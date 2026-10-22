#include "core/CoreManager.h"
#include "storage/Storage.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QRandomGenerator>
using namespace mlash;
class MemoryPlatform final : public PlatformServices {
public:
    QJsonObject current{{"flags", 1}, {"server", ""}, {"bypass", "original"}, {"pac", ""}};
    int writes = 0; bool failWrite = false, failRead = false, failSeal = false;
    bool autoEnabled = false;
    int autoWrites = 0;
    Result readProxy() override { return failRead ? Result::fail("read failure") : Result{true, {}, current}; }
    Result writeProxy(const QJsonObject& o) override { ++writes; if (failWrite) return Result::fail("write failure"); current = o; return {}; }
    QJsonObject proxyTarget(int port) const override { return {{"flags", 3}, {"server", QString("127.0.0.1:%1").arg(port)}, {"bypass", "local"}, {"pac", ""}}; }
    bool proxyEnabled(const QJsonObject& o) const override { return o.value("flags").toInt() != 1; }
    Result autoStart(bool enabled, const QString&) override { ++autoWrites; autoEnabled = enabled; return {}; }
    Result readAutoStart(const QString&) override { return {true, {}, {{"enabled", autoEnabled}}}; }
    Result seal(const QByteArray& b, QByteArray& out) override { if (failSeal) return Result::fail("seal failure"); out = "sealed:" + b.toBase64(); return {}; }
    Result open(const QByteArray& b, QByteArray& out) override { if (!b.startsWith("sealed:")) return Result::fail("invalid ciphertext"); out = QByteArray::fromBase64(b.mid(7)); return {}; }
    Result secureDirectory(const QString& root) override { return QDir().mkpath(root) ? Result{} : Result::fail("mkdir failure"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64, int) override { return {true, {}, {{"owned", true}}}; }
};
static int freePort() {
    // Fixed proxy ports should not race with automatic client ports in Windows' dynamic range.
    for (int i = 0; i < 100; ++i) {
        QTcpServer server; server.setProxy(QNetworkProxy::NoProxy); const int port = QRandomGenerator::global()->bounded(20000, 40000);
        if (server.listen(QHostAddress::LocalHost, port)) return port;
    }
    return 0;
}
static QByteArray minimalYaml() { return "mode: rule\nproxies: []\nproxy-groups:\n  - name: Test\n    type: select\n    proxies: [DIRECT, REJECT]\nrules: [\"MATCH,Test\"]\n"; }
static QByteArray runnableYaml(int mixed, int api, bool crash = false) {
    return "mixed-port: " + QByteArray::number(mixed) + "\nexternal-controller: 127.0.0.1:" + QByteArray::number(api) + "\nsecret: " + QByteArray(64, 'a') + "\n" + minimalYaml() + (crash ? "fake-crash: true\n" : "");
}
class CoreReadinessTests final : public QObject {
    Q_OBJECT
private slots:
    void readinessUsesDirectLocalSocketsAndSettlesOnce();
    void pinnedRealCoreRestartsWithoutLeakingListeners();
};
void CoreReadinessTests::pinnedRealCoreRestartsWithoutLeakingListeners() {
    const auto executable=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe";
    QVERIFY2(QFile::exists(executable),"Pinned release core is required for this verification");
    QTemporaryDir data; auto platform=createPlatformServices(); MihomoClient api; CoreManager core(&api,platform);
    const int mixed=freePort(); int port=freePort(); while (mixed==port) port=freePort();
    QVERIFY(mixed>0&&port>0); const auto path=data.path()+"/config.yaml";
    QVERIFY(writeAtomic(path,runnableYaml(mixed,port)).ok);
    for (int round=0;round<5;++round) {
        bool complete=false; Result result;
        core.start(executable,path,data.path(),mixed,port,QString(64,'a'),[&](Result r) { result=r; complete=true; });
        QTRY_VERIFY_WITH_TIMEOUT(complete,5000); QVERIFY2(result.ok,qUtf8Printable(result.error));
        QCOMPARE(result.data.value("version").toString(),QString("v1.19.32-halfclose.1"));
        complete=false;
        api.proxies([&](Result r) { result=r; complete=true; }); QTRY_VERIFY(complete);
        QVERIFY(result.ok); QVERIFY(result.data.value("proxies").toObject().contains("Test"));
        complete=false;
        core.stop([&](Result r) { result=r; complete=true; }); QTRY_VERIFY_WITH_TIMEOUT(complete,4000);
        QVERIFY(result.ok); QCOMPARE(core.state(),CoreState::Stopped);
        QTcpServer mixedReservation,apiReservation;
        mixedReservation.setProxy(QNetworkProxy::NoProxy); apiReservation.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(mixedReservation.listen(QHostAddress::LocalHost,mixed));
        QVERIFY(apiReservation.listen(QHostAddress::LocalHost,port));
    }
}
void CoreReadinessTests::readinessUsesDirectLocalSocketsAndSettlesOnce() {
    struct RestoreProxy { QNetworkProxy old=QNetworkProxy::applicationProxy(); ~RestoreProxy() { QNetworkProxy::setApplicationProxy(old); } } restore;
    // This is only the test process Qt proxy, never Windows/WinINet configuration.
    QTcpServer blackhole; blackhole.setProxy(QNetworkProxy::NoProxy); QVERIFY(blackhole.listen(QHostAddress::LocalHost));
    QTemporaryDir dir; const int mixed=freePort(); int port=freePort(); while (port==mixed && port!=0) port=freePort(); QVERIFY(mixed>0 && port>0); const auto config=dir.path()+"/config.yaml";
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy,"127.0.0.1",blackhole.serverPort()));
    QVERIFY(writeAtomic(config,runnableYaml(mixed,port)).ok);
    auto platform=std::make_shared<MemoryPlatform>(); MihomoClient api; CoreManager core(&api,platform);
    for (int round=0;round<3;++round) {
        int starts=0; Result result;
        core.start(QCoreApplication::applicationDirPath()+"/fake_core.exe",config,dir.path(),mixed,port,QString(64,'a'),[&](Result r) { ++starts; result=r; });
        QTRY_COMPARE_WITH_TIMEOUT(starts,1,5000); QVERIFY2(result.ok,qUtf8Printable(result.error)); QCOMPARE(core.state(),CoreState::Running);
        bool stopped=false; core.stop([&](Result r) { result=r; stopped=true; }); QTRY_VERIFY(stopped); QVERIFY(result.ok);
        QTest::qWait(850); QCOMPARE(starts,1); QCOMPARE(core.state(),CoreState::Stopped); QVERIFY(core.findChildren<QTcpSocket*>().isEmpty());
    }
    QCOMPARE(platform->writes,0); QCOMPARE(platform->autoWrites,0); QVERIFY(!blackhole.hasPendingConnections());
}
QTEST_GUILESS_MAIN(CoreReadinessTests)
#include "CoreReadinessTests.moc"
