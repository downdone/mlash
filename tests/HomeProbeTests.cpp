#include "TestPorts.h"
#include "app/HomeNodeProbe.h"
#include "app/ConnectivityProbe.h"
#include "app/AppController.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/GuiHost.h"
#include "ui/MainWindow.h"
#include "MockHttp.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QUrlQuery>
#include "platform/GlobalHotkeys.h"
#include "profile/WebDavClient.h"
#include "profile/SubscriptionDownloader.h"
using namespace mlash;
class ProbePlatform final:public PlatformServices {
public:
    bool failSeal=false; int writes=0;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; } Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); } QJsonObject proxyTarget(int) const override { return {}; } bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); } Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; } Result seal(const QByteArray& in,QByteArray& out) override { if (failSeal) return Result::fail("injected failure"); out=in; return {}; } Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); } Result attachChild(qint64) override { return {}; } Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
class HomeProbeTests final:public QObject {
    Q_OBJECT
private slots:
    void visibleIntervalCancellationAndNodeContext() {
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); int delay=0; bool fail=false; server.handler=[&](const QByteArray&) { return HttpResponse{fail?500:200,fail?QByteArray("{\"message\":\"unavailable\"}"):QByteArray("{\"delay\":42}"),delay}; }; MihomoClient api; api.configure(server.serverPort(),"test"); HomeNodeProbe probe(&api,nullptr,2); QSignalSpy measured(&probe,&HomeNodeProbe::measured); probe.setContext(true,"A",1,"https://example.test",60); QTest::qWait(40); QCOMPARE(server.requests.size(),0); probe.setVisible(true); QTRY_COMPARE(measured.size(),1); QCOMPARE(probe.result().value("delay").toInt(),42); QTRY_VERIFY(measured.size()>=2); for (const auto& request:server.requests) QVERIFY(request.startsWith("GET /proxies/A/delay?")); probe.setVisible(false); QVERIFY(probe.result().isEmpty()); const auto requests=server.requests.size(); QTest::qWait(180); QCOMPARE(server.requests.size(),requests);
        delay=200; probe.setVisible(true); QTRY_VERIFY(server.requests.size()>requests); const auto completed=measured.size(); probe.setContext(false,"A",2,"https://example.test",60); QTest::qWait(260); QCOMPARE(measured.size(),completed); QVERIFY(probe.result().isEmpty()); fail=true; delay=0; probe.setContext(true,"B",3,"https://example.test",60); QTRY_VERIFY(probe.result().contains("delay")); QCOMPARE(probe.result().value("node").toString(),QString("B")); QCOMPARE(probe.result().value("delay").toInt(),-1); QVERIFY(!probe.result().value("error").toString().isEmpty()); probe.setContext(true,"B",3,"https://example.test",0); const auto count=server.requests.size(); QTest::qWait(180); QCOMPARE(server.requests.size(),count); QVERIFY(probe.result().isEmpty());
    }
    void realLeafUiLifecycleAndPreferenceFailure() {
        MockHttp target; QVERIFY(target.listen(QHostAddress::LocalHost)); target.handler=[](const QByteArray&) { return HttpResponse{204,{},30}; }; QTemporaryDir data; auto platform=std::make_shared<ProbePlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Settings s; s.mixedPort=isolatedListenerPort(); QVERIFY(s.mixedPort>0); s.startCore=false; s.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; s.testUrl=QString("http://127.0.0.1:%1/probe").arg(target.serverPort()); Profile p; p.id=newId(); p.name="Probe"; p.sourceType="file"; const auto prepared=store.prepare(p,"proxies: [{name: Leaf, type: direct}]\nproxy-groups: [{name: Inner, type: select, proxies: [Leaf]}, {name: Test, type: select, proxies: [Inner]}]\nrules: [\"MATCH,Test\"]\n",s,19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(s,{prepared.profile}).ok); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); c.activateProfile(p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished[0][0].toBool(),qUtf8Printable(finished[0][1].toString())); c.setHomeProbeGroup("Test"); c.setHomeVisible(true); QTest::qWait(80); QCOMPARE(target.requests.size(),0); const auto pid=c.coreProcessId(); const auto generation=c.state().generation; c.updateHomeProbe(60); QTRY_COMPARE(finished.size(),2); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(c.homeProbe()->result().contains("delay")); QCOMPARE(c.homeProbe()->result().value("node").toString(),QString("Leaf")); QVERIFY(c.homeProbe()->result().value("delay").toInt()>0); QCOMPARE(c.proxies().value("Test").toObject().value("now").toString(),QString("Inner")); QCOMPARE(c.coreProcessId(),pid); QCOMPARE(c.state().generation,generation);
        c.setHomeVisible(false); QVERIFY(c.homeProbe()->result().isEmpty()); const auto count=target.requests.size(); QTest::qWait(100); QCOMPARE(target.requests.size(),count); platform->failSeal=true; c.updateHomeProbe(300); QTRY_COMPARE(finished.size(),3); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.settings().homeProbeSeconds,60); platform->failSeal=false; QCOMPARE(store.load().settings.homeProbeSeconds,60);
        // K5：首页不再有「探测周期」下拉，周期只由设置与命令决定；按当前周期重新触发即可。
        { GuiHost host(&c,true); host.show(0); auto* w=host.window(); QVERIFY(w); QVERIFY(!w->findChild<QComboBox*>("homeProbeInterval")); c.setHomeVisible(true); QTRY_VERIFY_WITH_TIMEOUT(c.homeProbe()->result().contains("delay"),3000); w->setPage(4); QVERIFY(c.homeProbe()->result().isEmpty()); w->hide(); QVERIFY(c.homeProbe()->result().isEmpty()); host.dispose(); QVERIFY(c.homeProbe()->result().isEmpty()); }
        c.updateHomeProbe(61); QCOMPARE(finished.size(),4); QVERIFY(!finished.last()[0].toBool()); QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000); QCOMPARE(platform->writes,0);
    }
    void connectivityProbeRunsWithoutWebsitePageAndReportsFreshness() {
        // R3b：首页连通性行不受测速页可见性约束；只读，不改节点选择。
        MockHttp target; QVERIFY(target.listen(QHostAddress::LocalHost)); bool fail=false; target.handler=[&](const QByteArray&) { return HttpResponse{fail?500:200,fail?QByteArray("{\"message\":\"unavailable\"}"):QByteArray("{\"delay\":37}"),20}; };
        QTemporaryDir data; auto platform=std::make_shared<ProbePlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok);
        Settings s; s.startCore=false; s.mixedPort=isolatedListenerPort(); s.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; s.testUrl=QString("http://127.0.0.1:%1/connectivity").arg(target.serverPort()); QVERIFY(store.save(s,{}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        // 未就绪：拒绝并说明原因，且不发起任何请求。
        c.runConnectivityProbe(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(target.requests.size(),0);
        auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.core=CoreState::Running;
        QJsonArray members{"Leaf"}; QJsonObject group{{"type","Selector"},{"all",members},{"now","Leaf"}}; auto& proxies=const_cast<QJsonObject&>(c.proxies()); proxies.insert("G",group); proxies.insert("Leaf",QJsonObject{{"type","Http"}});
        c.configureApiForTests(target.serverPort(),"test");   // 与 CoreManager 在启动内核时做的一样
        c.setHomeProbeGroup("G");
        // 首页不可见也允许（这是与 startWebsiteTests 的关键差别）。
        QSignalSpy changed(const_cast<ConnectivityProbe*>(c.connectivity()),&ConnectivityProbe::changed);
        c.runConnectivityProbe(); QTRY_VERIFY(!changed.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(c.connectivity()->result().value("status").toString()!="running" && !c.connectivity()->result().isEmpty(),6000);
        QVERIFY2(c.connectivity()->result().value("status").toString()=="ok",qUtf8Printable(QJsonDocument(c.connectivity()->result()).toJson(QJsonDocument::Compact)));
        QVERIFY2(c.connectivity()->result().value("node").toString()==QString("Leaf"),qUtf8Printable(QJsonDocument(c.connectivity()->result()).toJson(QJsonDocument::Compact))); QCOMPARE(c.connectivity()->result().value("delay").toInt(),37);
        QVERIFY(!c.connectivity()->result().value("measured_utc").toString().isEmpty()); QVERIFY(!c.connectivity()->stale());
        QVERIFY(target.requests.size()>=1); QCOMPARE(c.proxies().value("G").toObject().value("now").toString(),QString("Leaf"));
        // 失败保留原因；取消清空结果。
        fail=true; c.runConnectivityProbe(); QTRY_COMPARE_WITH_TIMEOUT(c.connectivity()->result().value("status").toString(),QString("failed"),6000);
        QVERIFY(!c.connectivity()->result().value("error").toString().isEmpty());
        c.cancelConnectivityProbe(); QVERIFY(c.connectivity()->result().isEmpty()); QVERIFY(!c.connectivity()->running());
        QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(HomeProbeTests)
#include "HomeProbeTests.moc"
