#include "TestPorts.h"
#include "app/AppController.h"
#include "core/CoreUpdate.h"
#include "storage/Storage.h"
#include "ui/CoreUpdateDialog.h"
#include <QtTest>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QtEndian>
#include <QTableWidget>
using namespace mlash;
class UpdatePlatform final:public PlatformServices {
public:
    int writes=0; bool failState=false;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; } Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); } QJsonObject proxyTarget(int) const override { return {}; } bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); } Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; } Result seal(const QByteArray& in,QByteArray& out) override { if (failState) return Result::fail("injected index failure"); out=in; return {}; } Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; } Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); } Result attachChild(qint64) override { return {}; } Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
class CoreUpdateTests final:public QObject {
    Q_OBJECT
    const QString original=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe";
    const QByteArray source="proxies: []\nproxy-groups: [{name: Test, type: select, proxies: [DIRECT]}]\nrules: [\"MATCH,Test\"]\n";
    QString fixture() const { return QCoreApplication::applicationDirPath()+"/fake_core_update.exe"; }
    Result preview(AppController& c,const QString& path) { Result result; bool done=false; c.previewCoreUpdate(path,[&](Result r) { result=r; done=true; }); QElapsedTimer clock; clock.start(); while (!done&&clock.elapsed()<6000) QTest::qWait(20); if (!done) return Result::fail("test preview deadline"); return result; }
private slots:
    void copyBoundsHashAndExplicitCleanup() {
        QTemporaryDir data; auto platform=std::make_shared<UpdatePlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); QByteArray pe(128,'\0'); pe.replace(0,2,"MZ"); qToLittleEndian<quint32>(64,pe.data()+0x3c); pe.replace(64,4,QByteArray("PE\0\0",4)); qToLittleEndian<quint16>(0x8664,pe.data()+68); const auto input=data.path()+"/input.exe"; QVERIFY(writeAtomic(input,"not an executable").ok); QVERIFY(!CoreUpdate::stage(store,input).ok); auto x86=pe; qToLittleEndian<quint16>(0x14c,x86.data()+68); QVERIFY(writeAtomic(input,x86).ok); QVERIFY(!CoreUpdate::stage(store,input).ok);
        QJsonObject first; for (int i=0;i<8;++i) { QVERIFY(writeAtomic(input,pe+QByteArray::number(i)).ok); const auto stage=CoreUpdate::stage(store,input); QVERIFY2(stage.ok,qUtf8Printable(stage.error)); QVERIFY(CoreUpdate::verify(data.path(),stage.data).ok); if (i==0) first=stage.data; } QVERIFY(writeAtomic(input,pe+"ninth").ok); QVERIFY(!CoreUpdate::stage(store,input).ok); const auto kept=first.value("path").toString(); const auto clean=CoreUpdate::clean(store,{kept}); QVERIFY(clean.ok); QCOMPARE(clean.data.value("removed").toInt(),7); QVERIFY(QFileInfo::exists(kept)); QVERIFY(CoreUpdate::verify(data.path(),first).ok); QVERIFY(writeAtomic(kept,pe+"modified").ok); QVERIFY(!CoreUpdate::verify(data.path(),first).ok); QVERIFY(CoreUpdate::clean(store,{kept}).ok); QVERIFY(QFileInfo::exists(kept)); QVERIFY(CoreUpdate::clean(store,{}).ok); QVERIFY(!QFileInfo::exists(kept));
    }
    void versionBoundCancelAndUnknownOutput() {
        CoreVersionProbe probe; bool done=false; Result result; probe.inspect(QCoreApplication::applicationDirPath()+"/fake_core.exe",[&](Result r) { result=r; done=true; }); QTRY_VERIFY(done); QVERIFY(!result.ok); done=false; qputenv("MLASH_TEST_CORE_VERSION_SLOW","1"); probe.inspect(fixture(),[&](Result r) { result=r; done=true; }); qunsetenv("MLASH_TEST_CORE_VERSION_SLOW"); QTest::qWait(50); probe.cancel(); QTRY_VERIFY(done); QVERIFY(!result.ok); QVERIFY(result.error.contains("取消")); done=false; qputenv("MLASH_TEST_CORE_VERSION_SLOW","1"); probe.inspect(fixture(),[&](Result r) { result=r; done=true; }); qunsetenv("MLASH_TEST_CORE_VERSION_SLOW"); QTRY_VERIFY_WITH_TIMEOUT(done,4500); QVERIFY(!result.ok); QVERIFY(result.error.contains("3秒"));
    }
    void realCoreApplyRollbackAndMismatchedRuntime() {
        QTemporaryDir data; auto platform=std::make_shared<UpdatePlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Settings s; s.mixedPort=isolatedListenerPort(); QVERIFY(s.mixedPort>0); s.startCore=false; s.corePath=original; Profile p; p.id=newId(); p.name="Core update"; p.sourceType="file"; const auto prepared=store.prepare(p,source,s,19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(s,{prepared.profile}).ok); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); c.activateProfile(p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); const auto originalPid=c.coreProcessId(); const auto originalVersion=c.state().coreVersion; const auto pendingFile=data.path()+"/pending.yaml"; QVERIFY(writeAtomic(pendingFile,source+"# pending remains\n").ok); c.importFile("",pendingFile,p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); const auto pending=c.profiles()[0].candidate; QByteArray originalBytes; QVERIFY(readFile(original,originalBytes,128*1024*1024).ok); const auto variant=data.path()+"/variant.exe"; QVERIFY(writeAtomic(variant,originalBytes+"\nMlash core-update isolated fixture\n").ok); auto inspected=preview(c,variant); QVERIFY2(inspected.ok,qUtf8Printable(inspected.error)); QCOMPARE(c.coreProcessId(),originalPid); QCOMPARE(inspected.data.value("version").toString(),originalVersion); finished.clear(); c.applyCoreUpdate(inspected.data.value("token").toString()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QVERIFY(c.coreProcessId()!=originalPid); const auto variantPath=c.settings().corePath,oldPath=c.settings().previousCorePath; QVERIFY(!oldPath.isEmpty()); QVERIFY(variantPath!=oldPath); QCOMPARE(c.profiles()[0].candidate,pending); QCOMPARE(c.state().coreVersion,originalVersion); QByteArray unchanged; QVERIFY(readFile(original,unchanged,128*1024*1024).ok); QCOMPARE(unchanged,originalBytes);
        inspected=preview(c,oldPath); QVERIFY(inspected.ok); finished.clear(); c.applyCoreUpdate(inspected.data.value("token").toString()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QCOMPARE(c.settings().corePath,oldPath); QCOMPARE(c.settings().previousCorePath,variantPath); QCOMPARE(c.profiles()[0].candidate,pending);
        inspected=preview(c,fixture()); QVERIFY(inspected.ok); QCOMPARE(inspected.data.value("version").toString(),QString("v9.99.0")); finished.clear(); c.applyCoreUpdate(inspected.data.value("token").toString()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(!finished.last()[0].toBool()); QVERIFY2(finished.last()[1].toString().contains("运行版本与预检不一致"),qUtf8Printable(finished.last()[1].toString())); QVERIFY(c.state().ready); QCOMPARE(c.state().coreVersion,originalVersion); QCOMPARE(c.settings().corePath,oldPath); QCOMPARE(c.profiles()[0].candidate,pending); QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000); QCOMPARE(platform->writes,0);
    }
    void changedPreviewAndStoppedDialogApply() {
        QTemporaryDir data; auto platform=std::make_shared<UpdatePlatform>(); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); auto inspected=preview(c,original); QVERIFY(inspected.ok); QVERIFY(writeAtomic(inspected.data.value("path").toString(),"tampered").ok); finished.clear(); c.applyCoreUpdate(inspected.data.value("token").toString()); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(!c.state().ready); c.cleanCoreCopies(); QTRY_COMPARE(finished.size(),2); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QPointer<CoreUpdateDialog> dialog=new CoreUpdateDialog(&c); dialog->show(); auto* apply=dialog->findChild<QPushButton*>("applyCoreUpdate"); QVERIFY(!apply->isEnabled()); dialog->findChild<QLineEdit*>("coreUpdatePath")->setText(original); dialog->findChild<QPushButton*>("previewCoreUpdate")->click(); QTRY_VERIFY_WITH_TIMEOUT(apply->isEnabled(),6000); QVERIFY(dialog->findChild<QPlainTextEdit*>("coreUpdateDetails")->toPlainText().contains("SHA-256")); apply->click(); QTRY_VERIFY_WITH_TIMEOUT(dialog.isNull(),15000); QVERIFY(!c.state().ready); QVERIFY(c.settings().corePath.contains("/core-updates/")); QCOMPARE(platform->writes,0);
    }
    void stoppedIndexFailureKeepsOldPathAndCleansValidation() {
        QTemporaryDir data; auto platform=std::make_shared<UpdatePlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Settings s; s.corePath=original; s.startCore=false; QVERIFY(store.save(s,{}).ok); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); const auto inspected=preview(c,original); QVERIFY(inspected.ok); QByteArray before,after; QVERIFY(readFile(data.path()+"/state.dat",before).ok); platform->failState=true; finished.clear(); c.applyCoreUpdate(inspected.data.value("token").toString()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.settings().corePath,original); QVERIFY(!c.state().ready); QCOMPARE(c.coreProcessId(),qint64(0)); QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,before); QVERIFY(QDir(data.path()).entryList({"core-check-*"},QDir::Dirs|QDir::NoDotAndDotDot).isEmpty()); QCOMPARE(platform->writes,0);
    }
    void coreHistoryKeepsBoundedSwitchableVersions() {
        QTemporaryDir data; auto platform=std::make_shared<UpdatePlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Settings s; s.startCore=false; s.corePath=original; QVERIFY(store.save(s,{}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        QCOMPARE(c.settings().coreHistory.size(),0);
        const auto applied=preview(c,original); QVERIFY2(applied.ok,qUtf8Printable(applied.error)); finished.clear(); c.applyCoreUpdate(applied.data.value("token").toString()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QCOMPARE(c.settings().coreHistory.size(),1); const auto first=c.settings().coreHistory.first();
        QCOMPARE(first.sha256,applied.data.value("sha256").toString()); QCOMPARE(first.version,applied.data.value("version").toString()); QCOMPARE(first.path,c.settings().corePath); QCOMPARE(first.source,QString("local")); QVERIFY(first.valid(CoreHistoryEntry::sources()));
        // Switching to the recorded entry reuses the same explicit preview/apply path.
        const auto switched=preview(c,first.path); QVERIFY2(switched.ok,qUtf8Printable(switched.error)); finished.clear(); c.applyCoreUpdate(switched.data.value("token").toString()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QCOMPARE(c.settings().corePath,first.path);
        // The manager lists recorded versions and can drive the same preview from a selected row.
        QPointer<CoreUpdateDialog> dialog=new CoreUpdateDialog(&c); dialog->show(); auto* table=dialog->findChild<QTableWidget*>("coreHistory"); auto* useHistory=dialog->findChild<QPushButton*>("previewHistoryCore"); QVERIFY(table&&useHistory);
        QTRY_VERIFY(table->rowCount()>=1); QCOMPARE(table->item(0,0)->data(Qt::UserRole).toString(),first.path); QCOMPARE(table->item(0,3)->text(),QString("当前")); table->selectRow(0); QVERIFY(useHistory->isEnabled());
        // Selecting the running version is refused in place instead of re-previewing it.
        useHistory->click(); QVERIFY(dialog->findChild<QLabel*>("coreUpdateHint")->text().contains("当前内核"));
        QVERIFY(!dialog->findChild<QPushButton*>("applyCoreUpdate")->isEnabled()); dialog->close(); QTRY_VERIFY(dialog.isNull());
        c.cancelCoreUpdatePreview();
        // One entry per digest, newest first, capped at the same bound as the copy store.
        auto history=c.settings().coreHistory; for (int i=0;i<10;++i) { const auto sha=QString(QStringLiteral("%1").arg(i,2,16,QChar('0')).repeated(32)); CoreHistoryEntry entry{sha,"v9."+QString::number(i),data.path()+"/core-updates/"+sha+"/mihomo.exe","local","2026-10-29T00:00:00Z",1024}; history=appendedCoreHistory(history,entry); }
        QCOMPARE(history.size(),8); QCOMPARE(history.first().version,QString("v9.9")); QCOMPARE(history.last().version,QString("v9.2"));
        auto next=store.load().settings; next.coreHistory=history; QVERIFY(store.save(next,{}).ok); QCOMPARE(ProfileStore(data.path(),platform).load().settings.coreHistory.size(),8);
        auto bad=next.toJson(); auto rows=bad.value("core_history").toArray(); auto row=rows[0].toObject(); row["sha256"]="not-a-digest"; rows[0]=row; bad["core_history"]=rows; QVERIFY(!ProfileStore(data.path(),platform).save(Settings::fromJson(bad),{}).ok);
        auto dup=next.toJson(); auto dupRows=dup.value("core_history").toArray(); dupRows[1]=dupRows[0]; dup["core_history"]=dupRows; QVERIFY(!ProfileStore(data.path(),platform).save(Settings::fromJson(dup),{}).ok);
        QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(CoreUpdateTests)
#include "CoreUpdateTests.moc"
