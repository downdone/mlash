#include "app/AppController.h"
#include "storage/Storage.h"
#include "ui/ProxyBypassDialog.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
using namespace mlash;
class BypassPlatform final:public PlatformServices {
public:
    QJsonObject current{{"flags",13},{"server","previous:8080"},{"bypass","old.internal"},{"pac","https://previous.test/proxy.pac?token=private-value"}};
    int writes=0,autoWrites=0; bool failWrite=false,failReadsAfterWrite=false,blockReads=false,failStateSave=false,failJournal=false; std::function<void()> onWrite;
    Result readProxy() override { return blockReads?Result::fail("injected read failure"):Result{true,{},current}; }
    Result writeProxy(const QJsonObject& value) override { ++writes; if (onWrite) onWrite(); if (failReadsAfterWrite) blockReads=true; if (failWrite) return Result::fail("injected write failure"); current=value; return {}; }
    QJsonObject proxyTarget(int port) const override { return {{"flags",3},{"server",QString("127.0.0.1:%1").arg(port)},{"bypass","platform-default"},{"pac",""}}; } bool proxyEnabled(const QJsonObject& snapshot) const override { return snapshot.value("flags").toInt()!=1; }
    Result autoStart(bool,const QString&) override { ++autoWrites; return Result::fail("prohibited"); } Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { const auto object=QJsonDocument::fromJson(in).object(); if ((failStateSave&&object.contains("settings"))||(failJournal&&object.contains("written"))) return Result::fail("injected seal failure"); out=in; return {}; } Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); } Result attachChild(qint64) override { return {}; } Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
class ProxyBypassTests final:public QObject {
    Q_OBJECT
private slots:
    void strictPatternsAndOldSettings() {
        const auto checked=normalizeProxyBypass(" localhost\n127.*\n[::1];<local>;*.example.test;LOCALHOST"); QVERIFY(checked.ok); QCOMPARE(checked.data.value("bypass").toString(),QString("localhost;127.*;[::1];<local>;*.example.test")); QVERIFY(normalizeProxyBypass("").ok); for (const auto& bad:QStringList{"https://example.test","user:pass@host","C:/private","<-loopback>",QString(2049,'a'),QStringList(65,"host").join(';')+";other"}) { if (bad.startsWith("host;")) continue; QVERIFY(!normalizeProxyBypass(bad).ok); }
        QStringList many; for (int i=0;i<65;++i) many.append("host"+QString::number(i)); QVERIFY(!normalizeProxyBypass(many.join(';')).ok); auto old=Settings{}.toJson(); old.remove("proxy_bypass"); QCOMPARE(Settings::fromJson(old).proxyBypass,Settings{}.proxyBypass);
        QTemporaryDir data; auto platform=std::make_shared<BypassPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Settings s; s.proxyBypass="*.example.test;<local>"; QVERIFY(store.save(s,{}).ok); QCOMPARE(store.load().settings.proxyBypass,s.proxyBypass); s.proxyBypass="https://bad"; QVERIFY(!store.save(s,{}).ok); QCOMPARE(store.load().settings.proxyBypass,QString("*.example.test;<local>"));
    }
    void ownedReplacementKeepsOriginalPacAndExactConflict() {
        QTemporaryDir data; auto platform=std::make_shared<BypassPlatform>(); const auto original=platform->current; ProxyCoordinator coordinator(data.path(),platform); QVERIFY(coordinator.acquire(17890,true,QString("*.first.test")).ok); const auto first=platform->current; bool journalBeforeWrite=false; platform->onWrite=[&] { QByteArray bytes; if (readFile(data.path()+"/recovery/proxy-journal.dat",bytes).ok) { const auto object=QJsonDocument::fromJson(bytes).object(); journalBeforeWrite=object.value("previous_written").toObject()==first&&object.value("written").toObject().value("bypass").toString()=="*.second.test"&&object.value("before").toObject()==original; } }; QVERIFY(coordinator.acquire(17890,false,QString("*.second.test")).ok); QVERIFY(journalBeforeWrite); platform->onWrite={}; QVERIFY(coordinator.release().ok); QCOMPARE(platform->current,original); QVERIFY(!coordinator.hasJournal());
        QVERIFY(coordinator.acquire(17890,true,QString("")).ok); QCOMPARE(platform->current.value("bypass").toString(),QString()); platform->current["bypass"]="foreign-only-change"; const auto external=platform->current; const auto count=platform->writes; const auto released=coordinator.release(); QVERIFY(!released.ok); QVERIFY(released.data.value("conflict").toBool()); QCOMPARE(platform->current,external); QCOMPARE(platform->writes,count); QVERIFY(coordinator.hasJournal());
    }
    void interruptedTransitionRecoversPreviousCompleteSnapshot() {
        QTemporaryDir data; auto platform=std::make_shared<BypassPlatform>(); const auto original=platform->current; ProxyCoordinator coordinator(data.path(),platform); QVERIFY(coordinator.acquire(17890,true,QString("*.first.test")).ok); const auto first=platform->current; platform->failWrite=true; platform->failReadsAfterWrite=true; QVERIFY(!coordinator.acquire(17890,false,QString("*.second.test")).ok); QVERIFY(coordinator.hasJournal()); QByteArray bytes; QVERIFY(readFile(data.path()+"/recovery/proxy-journal.dat",bytes).ok); const auto object=QJsonDocument::fromJson(bytes).object(); QCOMPARE(object.value("previous_written").toObject(),first); QCOMPARE(object.value("before").toObject(),original); QCOMPARE(platform->current,first); platform->failWrite=false; platform->failReadsAfterWrite=false; platform->blockReads=false; ProxyCoordinator recovery(data.path(),platform); QVERIFY(recovery.release().ok); QCOMPARE(platform->current,original); QVERIFY(!recovery.hasJournal());
        QVERIFY(coordinator.acquire(17890,true,QString("*.first.test")).ok); const auto count=platform->writes; platform->failJournal=true; QVERIFY(!coordinator.acquire(17890,false,QString("*.second.test")).ok); QCOMPARE(platform->writes,count); platform->failJournal=false; QVERIFY(coordinator.release().ok); QCOMPARE(platform->current,original);
    }
    void controllerIndexFailureAndDraft() {
        QTemporaryDir data; auto platform=std::make_shared<BypassPlatform>(); const auto original=platform->current; AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); c.updateProxyBypass("*.saved.test"); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); QCOMPARE(platform->writes,0); ProxyCoordinator coordinator(data.path(),platform); QVERIFY(coordinator.acquire(17890,true,c.settings().proxyBypass).ok); const auto owned=platform->current; QByteArray before,after; QVERIFY(readFile(data.path()+"/state.dat",before).ok); platform->failStateSave=true; c.updateProxyBypass("*.new.test"); QTRY_COMPARE(finished.size(),2); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.settings().proxyBypass,QString("*.saved.test")); QCOMPARE(platform->current,owned); QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,before); platform->failStateSave=false;
        QPointer<ProxyBypassDialog> dialog=new ProxyBypassDialog(&c); dialog->show(); auto* editor=dialog->findChild<QPlainTextEdit*>("proxyBypassEditor"); auto* save=dialog->findChild<QPushButton*>("saveProxyBypass"); QVERIFY(!save->isEnabled()); editor->setPlainText("https://private-value"); save->click(); QTRY_COMPARE(finished.size(),3); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog); QCOMPARE(editor->toPlainText(),QString("https://private-value")); QVERIFY(!dialog->findChild<QLabel*>("proxyBypassHint")->text().contains("private-value")); editor->setPlainText("*.new.test\n<local>"); save->click(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(c.settings().proxyBypass,QString("*.new.test;<local>")); QCOMPARE(platform->current.value("bypass").toString(),c.settings().proxyBypass); c.updateProxyBypass("*.stale.test",QString("*.saved.test")); QCOMPARE(finished.size(),5); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(platform->current.value("bypass").toString(),QString("*.new.test;<local>")); QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE(exit.size(),1); QCOMPARE(platform->current,original); QCOMPARE(platform->autoWrites,0);
    }
};
QTEST_MAIN(ProxyBypassTests)
#include "ProxyBypassTests.moc"
