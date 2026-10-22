#include "app/ProtocolInbox.h"
#include "ui/ProtocolImportQueue.h"
#include "ui/ProtocolSettingsDialog.h"
#include "platform/ProtocolTestSupport.h"
#include "storage/Storage.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QPushButton>
#include <QUrlQuery>
using namespace mlash;
class ProtocolPlatform final:public PlatformServices {
public:
    int writes=0,associations=0; bool associated=false,conflict=false,failWrite=false;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; } Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); } QJsonObject proxyTarget(int) const override { return {}; } bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); } Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; } Result seal(const QByteArray& in,QByteArray& out) override { out=in; return {}; } Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; } Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); } Result attachChild(qint64) override { return {}; } Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
    Result readUrlProtocol(const QString&) override { return {true,{},{{"enabled",associated},{"conflict",conflict}}}; } Result setUrlProtocol(bool enabled,const QString&) override { ++associations; if (conflict||failWrite) return Result::fail("injected association conflict"); associated=enabled; return {}; }
};
class ProtocolTests final:public QObject {
    Q_OBJECT
    QString link(const QString& source="https://example.test/sub?token=private-token") const { QUrl url("mlash://import"); QUrlQuery query; query.addQueryItem("url",source); query.addQueryItem("name","链接订阅"); url.setQuery(query); return url.toString(QUrl::FullyEncoded); }
private slots:
    void boundedParsingAndUserOnlyForwarding() {
        const auto encoded=parseProtocolLink("mlash://import?url=https%3A%2F%2Fexample.test%2Fsub%3Ftoken%3Da%2526b%253D"); QVERIFY(encoded.ok); QCOMPARE(encoded.data.value("url").toString(),QString("https://example.test/sub?token=a%26b%3D"));
        const auto parsed=parseProtocolLink(link()); QVERIFY(parsed.ok); QCOMPARE(parsed.data.value("url").toString(),QString("https://example.test/sub?token=private-token")); QCOMPARE(parsed.data.value("name").toString(),QString("链接订阅"));
        for (const auto& invalid:QStringList{"clash://import?url=x","mlash://import?url=https%3A%2F%2Fexample.test&url=x","mlash://import?url=https%3A%2F%2Fexample.test&activate=true","mlash://import?url=https%3A%2F%2Fexample.test#fragment",link("file:///private.yaml"),link("http://example.test"),link("https://user:pass@example.test/sub"),link("https://example.test/#private"),QString(9000,'x')}) QVERIFY(!parseProtocolLink(invalid).ok);
        QTemporaryDir root; ProtocolInbox inbox; QVERIFY(inbox.listen(root.path()).ok); QSignalSpy received(&inbox,&ProtocolInbox::received); int completed=0; Result forwarded; ProtocolInbox::forward(this,root.path(),link(),[&](Result r) { forwarded=r; ++completed; }); QTRY_COMPARE(completed,1); QVERIFY2(forwarded.ok,qUtf8Printable(forwarded.error)); QCOMPARE(received.size(),1); QCOMPARE(received[0][0].toString(),link()); ProtocolInbox::forward(this,root.path()+"/other",link(),[&](Result r) { forwarded=r; ++completed; }); QTRY_COMPARE(completed,2); QVERIFY(!forwarded.ok); QCOMPARE(received.size(),1);
    }
    void confirmQueueNoImportOnReceiveOrCancel() {
        QTemporaryDir data; auto platform=std::make_shared<ProtocolPlatform>(); AppController c(data.path(),true,nullptr,platform); GuiHost host(&c,true); ProtocolImportQueue queue(&c,&host); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); QVERIFY(queue.enqueue(link()).ok); QVERIFY(!host.window()); c.initialize(); QTRY_COMPARE(init.size(),1); QTRY_VERIFY(host.window()); QTRY_VERIFY(host.window()->findChild<ImportDialog*>()); auto* dialog=host.window()->findChild<ImportDialog*>(); QVERIFY(dialog->isVisible()); QCOMPARE(dialog->source(),QString("https://example.test/sub?token=private-token")); QCOMPARE(dialog->findChild<QLineEdit*>("subscriptionUrl")->echoMode(),QLineEdit::Password); QVERIFY(dialog->findChild<QPushButton*>("importSubmit")->isEnabled()); QTest::qWait(80); QVERIFY(c.profiles().isEmpty()); QCOMPARE(finished.size(),0); QVERIFY(!c.state().ready); QVERIFY(queue.enqueue(link("https://other.test/sub")).ok); dialog->reject(); QTRY_VERIFY(host.window()->findChild<ImportDialog*>()&&host.window()->findChild<ImportDialog*>()->source().contains("other.test")); host.window()->findChild<ImportDialog*>()->reject(); QTest::qWait(30); QVERIFY(c.profiles().isEmpty()); QCOMPARE(finished.size(),0); host.dispose(); QCOMPARE(platform->writes,0); QCOMPARE(platform->associations,0);
    }
    void linkLaunchSuppressesLastCoreAndClickIsRequired() {
        QTemporaryDir data; auto platform=std::make_shared<ProtocolPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Last"; p.sourceType="file"; Settings s; s.corePath="does-not-exist.exe"; s.startCore=true; s.activeProfile=p.id; auto prepared=store.prepare(p,"proxies: []\nproxy-groups: [{name: Test, type: select, proxies: [DIRECT]}]\nrules: [\"MATCH,Test\"]\n",s,19090,QString(64,'a')); QVERIFY(prepared.result.ok); prepared.profile.active=prepared.version; QVERIFY(store.save(s,{prepared.profile}).ok); AppController c(data.path(),false,nullptr,platform); c.suppressAutomaticStartup(); GuiHost host(&c,true); ProtocolImportQueue queue(&c,&host); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished),started(&c,&AppController::operationStarted); QVERIFY(queue.enqueue(link("https://127.0.0.1:1/sub")).ok); c.initialize(); QTRY_COMPARE(init.size(),1); QTRY_VERIFY(host.window()&&host.window()->findChild<ImportDialog*>()); QTest::qWait(50); QCOMPARE(c.state().core,CoreState::Stopped); QCOMPARE(c.coreProcessId(),qint64(0)); QCOMPARE(started.size(),0); auto* dialog=host.window()->findChild<ImportDialog*>(); dialog->findChild<QPushButton*>("importSubmit")->click(); QTRY_COMPARE(started.size(),1); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished[0][0].toBool()); QVERIFY(dialog->isVisible()); QVERIFY(dialog->findChild<QPushButton*>("importSubmit")->isEnabled()); QCOMPARE(dialog->source(),QString("https://127.0.0.1:1/sub")); QCOMPARE(c.profiles().size(),1); QCOMPARE(c.profiles()[0].id,p.id); QCOMPARE(c.state().core,CoreState::Stopped); QCOMPARE(platform->writes,0); host.dispose();
    }
    void explicitAssociationAndNativeOwnership() {
        QTemporaryDir data; auto platform=std::make_shared<ProtocolPlatform>(); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); QPointer<ProtocolSettingsDialog> dialog=new ProtocolSettingsDialog(&c); dialog->show(); auto* enable=dialog->findChild<QPushButton*>("enableUrlProtocol"); QTRY_VERIFY(enable->isEnabled()); QCOMPARE(platform->associations,0); enable->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); auto* disable=dialog->findChild<QPushButton*>("disableUrlProtocol"); QTRY_VERIFY(disable->isEnabled()); disable->click(); QTRY_COMPARE(finished.size(),2); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(enable->isEnabled()); platform->failWrite=true; enable->click(); QTRY_COMPARE(finished.size(),3); QVERIFY(!finished.last()[0].toBool()); QVERIFY(!platform->associated); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(platform->writes,0);
#ifdef Q_OS_WIN
        const auto native=testIsolatedProtocolRegistry(); QVERIFY2(native.ok,qUtf8Printable(native.error));
#endif
    }
};
QTEST_MAIN(ProtocolTests)
#include "ProtocolTests.moc"
