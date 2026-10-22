#include "TestPorts.h"
#include "app/AppController.h"
#include "diagnostics/ProcessMetrics.h"
#include "platform/ProxyCoordinator.h"
#include "storage/Storage.h"
#include "WindowsReleaseSupport.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QProcess>
#include <QTcpServer>
using namespace mlash;
static QByteArray fixture() { return "mode: rule\nproxies: []\nproxy-groups: [{name: Test, type: select, proxies: [DIRECT, REJECT]}]\nrules: [\"MATCH,Test\"]\n"; }
static QString corePath() { return qEnvironmentVariable("MLASH_TEST_CORE"); }
int freePort() { return isolatedListenerPort(); }
static Settings seed(const QString& root) {
    ProfileStore store(root,createPlatformServices()); store.load(); Settings settings; settings.corePath=corePath(); settings.startCore=false; settings.restoreProxy=false; settings.mixedPort=freePort(); store.save(settings,{}); writeAtomic(root+"/source.yaml",fixture()); return settings;
}
class WindowsReleaseTests final: public QObject {
    Q_OBJECT
    std::shared_ptr<PlatformServices> platform_=createPlatformServices();
    QJsonObject original_;
    QVector<QJsonObject> testWrites_;
    QTemporaryDir recovery_;
    bool startupOwned_=false;
    QString appPath_;
    Result write(const QJsonObject& value) { testWrites_.append(value); return platform_->writeProxy(value); }
private slots:
    void initTestCase() {
        QVERIFY(measurementEnvironment().value("controlled_vm_detected").toBool()); QVERIFY(QFileInfo::exists(corePath()));
        appPath_=qEnvironmentVariable("MLASH_TEST_APP"); QVERIFY(QFileInfo::exists(appPath_));
        const auto startup=platform_->readAutoStart(appPath_); QVERIFY(startup.ok); QVERIFY2(!startup.data.value("enabled").toBool() && !startup.data.value("conflict").toBool(),"测试 VM 必须无现有 Mlash 自启项");
        const auto snapshot=platform_->readProxy(); QVERIFY(snapshot.ok); original_=snapshot.data;
        recovery_.setAutoRemove(false); QVERIFY(platform_->secureDirectory(recovery_.path()).ok); QByteArray sealed; QVERIFY(platform_->seal(QJsonDocument(original_).toJson(),sealed).ok); QVERIFY(writeAtomic(recovery_.path()+"/original-proxy.dat",sealed).ok);
    }
    void actualPacRestoreAndExternalConflict() {
        QTemporaryDir root; ProxyCoordinator coordinator(root.path(),platform_);
        const QJsonObject pac{{"flags",5},{"server","old-proxy.invalid:8080"},{"bypass","localhost;<local>"},{"pac","http://127.0.0.1:9/release-test.pac"}};
        QVERIFY(write(pac).ok); QCOMPARE(platform_->readProxy().data,pac); const int port=freePort(); QVERIFY(port>=1024); testWrites_.append(platform_->proxyTarget(port));
        QVERIFY(coordinator.acquire(port,true).ok); QCOMPARE(platform_->readProxy().data,platform_->proxyTarget(port)); QVERIFY(coordinator.release().ok); QCOMPARE(platform_->readProxy().data,pac);
        QVERIFY(coordinator.acquire(port,true).ok); const auto external=platform_->proxyTarget(freePort()); QVERIFY(write(external).ok); const auto conflict=coordinator.release(); QVERIFY(!conflict.ok); QVERIFY(conflict.data.value("conflict").toBool()); QCOMPARE(platform_->readProxy().data,external); QVERIFY(coordinator.hasJournal()); QVERIFY(write(original_).ok);
    }
    void actualAutoStartReadback() {
        QVERIFY(platform_->autoStart(true,appPath_).ok); startupOwned_=true; QVERIFY(platform_->readAutoStart(appPath_).data.value("enabled").toBool());
        QVERIFY(platform_->autoStart(false,appPath_).ok); startupOwned_=false; QVERIFY(!platform_->readAutoStart(appPath_).data.value("enabled").toBool());
    }
    void actualCoreCrashReleasesProxy() {
        QTemporaryDir root; seed(root.path()); AppController controller(root.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        controller.importFile("fixture",root.path()+"/source.yaml"); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(finished.last()[0].toBool()); controller.activateProfile(controller.profiles().first().id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished.last()[0].toBool());
        testWrites_.append(platform_->proxyTarget(controller.settings().mixedPort)); controller.setSystemProxy(true,true); QTRY_COMPARE(finished.size(),3); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.state().proxy,ProxyState::Owned);
        for (int attempt=0;attempt<3;++attempt) {
            const auto pid=controller.coreProcessId(); QVERIFY(terminateTestCore(pid,corePath()).ok); QTRY_VERIFY_WITH_TIMEOUT(platform_->readProxy().data==original_,15000);
            if (attempt<2) { QTRY_VERIFY_WITH_TIMEOUT(controller.state().ready && controller.coreProcessId()!=pid,15000); QCOMPARE(controller.state().proxy,ProxyState::Owned); }
            else { QTest::qWait(2200); QVERIFY(!controller.state().ready); QVERIFY(controller.state().core!=CoreState::Running); }
        }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.size(),1,15000);
    }
    void actualControllerKillAndStartupRecovery() {
        QTemporaryDir root; QProcess helper; helper.setProgram(QCoreApplication::applicationFilePath()); helper.setArguments({"--system-proxy-test","--proxy-crash-helper",root.path()}); helper.start(); QTRY_VERIFY(helper.state()==QProcess::Running);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(root.path()+"/ready.json"),20000); QByteArray bytes; QVERIFY(readFile(root.path()+"/ready.json",bytes).ok); const auto ready=QJsonDocument::fromJson(bytes).object(); testWrites_.append(platform_->proxyTarget(ready.value("port").toInt()));
        helper.kill(); QTRY_VERIFY(helper.state()==QProcess::NotRunning); QVERIFY(QFileInfo::exists(root.path()+"/recovery/proxy-journal.dat")); QCOMPARE(platform_->readProxy().data,platform_->proxyTarget(ready.value("port").toInt()));
        AppController restarted(root.path(),true); QSignalSpy initialized(&restarted,&AppController::initialized); restarted.initialize(); QTRY_COMPARE(initialized.size(),1); QVERIFY(platform_->readProxy().data==original_); QVERIFY(!QFileInfo::exists(root.path()+"/recovery/proxy-journal.dat"));
        QSignalSpy exited(&restarted,&AppController::exitReady); restarted.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void cleanupTestCase() {
        if (startupOwned_) QVERIFY(platform_->autoStart(false,appPath_).ok);
        if (original_.isEmpty()) return; const auto observed=platform_->readProxy(); QVERIFY(observed.ok);
        if (observed.data!=original_) { QVERIFY2(testWrites_.contains(observed.data),"测试期间存在其他写入者，保留快照并禁止覆盖"); QVERIFY(platform_->writeProxy(original_).ok); }
        QVERIFY(platform_->readProxy().data==original_); QVERIFY(QDir(recovery_.path()).removeRecursively());
    }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv); QThreadPool::globalInstance()->setMaxThreadCount(2); auto args=app.arguments();
    if (!args.removeAll("--system-proxy-test") || !measurementEnvironment().value("controlled_vm_detected").toBool()) { qWarning("OS write tests disabled: explicit --system-proxy-test and controlled VM required"); return 77; }
    const int index=args.indexOf("--proxy-crash-helper");
    if (index>=0) {
        if (index+1>=args.size() || !QFileInfo::exists(corePath())) return 4; const auto root=args[index+1]; seed(root); AppController controller(root,true); int phase=0;
        QObject::connect(&controller,&AppController::initialized,&controller,[&] { phase=1; controller.importFile("fixture",root+"/source.yaml"); });
        QObject::connect(&controller,&AppController::operationFinished,&controller,[&](bool ok,const QString&) { if (!ok) { app.exit(5); return; } if (phase==1) { phase=2; controller.activateProfile(controller.profiles().first().id); } else if (phase==2) { phase=3; controller.setSystemProxy(true,true); } else if (phase==3) { phase=4; writeAtomic(root+"/ready.json",QJsonDocument(QJsonObject{{"port",controller.settings().mixedPort}}).toJson()); } });
        controller.initialize(); return app.exec();
    }
    WindowsReleaseTests tests; return QTest::qExec(&tests,args);
}
#include "WindowsReleaseTests.moc"
