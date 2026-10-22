#include "TestPorts.h"
#include "app/AppController.h"
#include "core/TunPolicy.h"
#include "core/TunApiGate.h"
#include "storage/Storage.h"
#include "ui/MainWindow.h"
#include "ui/TunDialog.h"
#include "ui/TrayController.h"
#include "ui/UiTheme.h"
#include "ui/WidgetStyle.h"
#include "MockHttp.h"
#ifdef Q_OS_WIN
#include "platform/windows/TunNative.h"
#endif
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QLocalServer>
#include <yaml-cpp/yaml.h>
using namespace mlash;
class TunPlatform final:public PlatformServices {
public:
    bool installed=true; int writes=0,installs=0; bool failState=false,verifyPeer=true,advanced=true;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; } bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { if (failState) return Result::fail("injected save failure"); out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64 pid,int) override { return {true,{},{{"owned",pid>0}}}; }
    Result tunServiceInfo(bool=false) override { return {true,{},{{"installed",installed},{"running",installed},{"advanced_options",advanced},{"core_matches",true}}}; }
    Result manageTunService(bool,const QString&) override { ++installs; return Result::fail("prohibited"); }
    Result verifyTunServer(qintptr) override { return verifyPeer?Result{}:Result::fail("injected service identity mismatch"); }
};
class FakeTun final:public TunSession {
public:
    explicit FakeTun(std::shared_ptr<PlatformServices> p):TunSession(p) {}
    QProcess child; bool bad=false,failStop=false; int starts=0,stops=0;
    qint64 processId() const override { return child.processId(); }
    qint64 apiProcessId() const override { return child.processId(); }
    bool ready() const override { return child.state()==QProcess::Running; }
    bool active() const override { return child.state()!=QProcess::NotRunning; }
    void start(const QString&,const QString& work,Completion done) override {
        ++starts; auto env=QProcessEnvironment::systemEnvironment(); if (bad) env.insert("MLASH_TEST_TUN_BAD_READBACK","1"); child.setProcessEnvironment(env);
        child.setProgram(QCoreApplication::applicationDirPath()+"/fake_core.exe"); child.setArguments({"-d",work,"-f",work+"/config.yaml"});
        auto connection=std::make_shared<QMetaObject::Connection>(); *connection=QObject::connect(&child,&QProcess::started,this,[connection,done=std::move(done)]() mutable { QObject::disconnect(*connection); done({}); }); child.start();
    }
    void stop(Completion done) override { ++stops; if (failStop) { done(Result::fail("injected cleanup not confirmed")); return; } if (child.state()==QProcess::NotRunning) { done({}); return; } auto connection=std::make_shared<QMetaObject::Connection>(); *connection=QObject::connect(&child,&QProcess::finished,this,[connection,done=std::move(done)](int,QProcess::ExitStatus) mutable { QObject::disconnect(*connection); done({}); }); child.kill(); }
    ~FakeTun() override { child.kill(); child.waitForFinished(2000); }
};
class TunTests final:public QObject {
    Q_OBJECT
    const QString core=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe";
    const QByteArray source="proxies: []\nproxy-groups: [{name: Test, type: select, proxies: [DIRECT, REJECT]}]\nrules: [\"MATCH,Test\"]\n";
    int freePort() { return isolatedListenerPort(); }
private slots:
    void initTestCase() { WidgetStyle::install(); UiTheme::install(); }
    void advancedOptionsAreCanonicalAndProtected() {
        const auto normalized=normalizeTunRoutes("192.168.1.25/24; 192.168.1.0/24\nfd00:1234::1/64"); QVERIFY(normalized.ok); QCOMPARE(normalized.data.value("routes").toArray(),(QJsonArray{"192.168.1.0/24","fd00:1234::/64"}));
        for (const auto& invalid:QStringList{"0.0.0.0/0","::/0","192.168.1.1/33","fd00::/129","host.example/24","127.0.0.1","::1%12/128","1.1.1.1/32\nPRIVATE_BAD_VALUE"}) QVERIFY(!normalizeTunRoutes(invalid).ok);
        Settings options; options.tunEnabled=true; options.tunMtu=1400; options.tunDnsHijack=false; options.tunExcludeRoutes={"192.168.1.0/24","fd00:1234::/64"}; QVERIFY(validTunOptions(options)); QCOMPARE(Settings::fromJson(options.toJson()).toJson(),options.toJson()); const auto built=ConfigBuilder::build(source,options,19091,QString(64,'a')); QVERIFY(built.result.ok);
        QByteArray output; QStringList files; QVERIFY(prepareTunRuntime(built.yaml,output,files).ok); const auto config=YAML::Load(output.toStdString()); QCOMPARE(config["tun"]["mtu"].as<int>(),1400); QCOMPARE(config["tun"]["route-exclude-address"].size(),size_t(2)); QCOMPARE(config["tun"]["dns-hijack"].size(),size_t(0));
        auto hostile=YAML::Clone(config); hostile["tun"]["route-exclude-address"][0]="0.0.0.0/0"; QVERIFY(!prepareTunRuntime(QByteArray::fromStdString(YAML::Dump(hostile)),output,files).ok); QVERIFY(output.isEmpty()); hostile=YAML::Clone(config); hostile["tun"]["mtu"]=100000; QVERIFY(!prepareTunRuntime(QByteArray::fromStdString(YAML::Dump(hostile)),output,files).ok);
        QTemporaryDir root; auto platform=std::make_shared<TunPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); auto good=options; good.tunEnabled=false; QVERIFY(store.save(good,{}).ok); QCOMPARE(store.load().settings.tunMtu,1400); auto invalid=good; invalid.tunMtu=100; QVERIFY(!store.save(invalid,{}).ok); QCOMPARE(store.load().settings.tunMtu,1400);
        const auto path=root.path()+"/check.yaml"; QVERIFY(writeAtomic(path,built.yaml).ok); MihomoClient api; CoreManager manager(&api,platform); bool validated=false; Result result; manager.validate(core,path,root.path(),[&](Result r) { result=r; validated=true; }); QTRY_VERIFY_WITH_TIMEOUT(validated,15000); QVERIFY2(result.ok,qUtf8Printable(result.error)); QCOMPARE(platform->writes,0);
#ifdef Q_OS_WIN
        const auto interfaces=tunNative::networkInterfaces(); QVERIFY(interfaces.ok); QVERIFY(interfaces.data.value("read_only").toBool()); QCOMPARE(interfaces.data.value("route_target").toString(),QString("1.1.1.1")); QVERIFY(interfaces.data.value("interfaces").toArray().size()<=64);
#endif
    }
    void advancedPreferencesRetainOrdinaryRuntimeAndRejectOldService() {
        QTemporaryDir data; auto platform=std::make_shared<TunPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Settings s; s.corePath=core; s.startCore=false; s.mixedPort=freePort(); QVERIFY(store.save(s,{}).ok); const auto file=data.path()+"/source.yaml"; QVERIFY(writeAtomic(file,source).ok);
        auto session=std::make_unique<FakeTun>(platform); AppController c(data.path(),true,nullptr,platform,WebsiteTests::defaults(),{}, {},std::move(session)); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); c.importFile("Advanced",file); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,12000); QVERIFY(finished.last()[0].toBool()); c.activateProfile(c.profiles()[0].id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,12000); QVERIFY(finished.last()[0].toBool()); const auto pid=c.coreProcessId(); auto options=c.settings(); options.tunMtu=1400; options.tunExcludeRoutes={"192.168.0.0/16"}; options.tunDnsHijack=false; const auto expected=tunOptionsJson(c.settings());
        c.updateTunSettings(options,expected); QTRY_COMPARE(finished.size(),3); QVERIFY(finished.last()[0].toBool()); QCOMPARE(c.coreProcessId(),pid); c.updateTunSettings(options,expected); QCOMPARE(finished.size(),4); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.coreProcessId(),pid);
        platform->advanced=false; c.setTunEnabled(true); QTRY_COMPARE(finished.size(),5); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.coreProcessId(),pid); QVERIFY(!c.settings().tunEnabled); platform->advanced=true; c.setTunEnabled(true); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),6,16000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QVERIFY(c.state().tunObserved); QCOMPARE(c.settings().tunMtu,1400); c.setTunEnabled(false); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),7,16000); QVERIFY(finished.last()[0].toBool()); QVERIFY(!c.state().tunObserved); QCOMPARE(c.settings().tunExcludeRoutes,QStringList{"192.168.0.0/16"});
        QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000); QCOMPARE(platform->writes,0); QCOMPARE(platform->installs,0);
    }
    void policyAndPinnedCore() {
        Settings s; QVERIFY(!s.tunEnabled); QVERIFY(!Settings::fromJson({}).tunEnabled); s.tunEnabled=true; s.tunStack="gvisor"; s.tunStrictRoute=true; QCOMPARE(Settings::fromJson(s.toJson()).toJson(),s.toJson());
        auto built=ConfigBuilder::build(source+"tun: {enable: true, device: Foreign, file-descriptor: 42}\n",s,18091,QString(64,'a'));
        QVERIFY2(built.result.ok,qUtf8Printable(built.result.error)); auto node=YAML::Load(built.yaml.toStdString()); QCOMPARE(node["tun"]["device"].as<std::string>(),std::string("Mlash")); QVERIFY(!node["tun"]["file-descriptor"]); QVERIFY(node["tun"]["auto-route"].as<bool>()); QVERIFY(node["dns"]["enable"].as<bool>());
        QByteArray result; QStringList paths; QVERIFY(prepareTunRuntime(built.yaml,result,paths).ok); QVERIFY(paths.isEmpty()); auto halfClose=YAML::Load(result.toStdString())["tcp-half-close-timeout"]; QCOMPARE(halfClose["client"].as<int>(),300); QCOMPARE(halfClose["server"].as<int>(),30); auto explicitOff=ConfigBuilder::build(source+"tcp-half-close-timeout: {client: 0, server: 0}\n",s,18091,QString(64,'a')); QVERIFY(explicitOff.result.ok); QVERIFY(prepareTunRuntime(explicitOff.yaml,result,paths).ok); halfClose=YAML::Load(result.toStdString())["tcp-half-close-timeout"]; QCOMPARE(halfClose["client"].as<int>(),0); QCOMPARE(halfClose["server"].as<int>(),0); s.tunEnabled=false; built=ConfigBuilder::build(source,s,18091,QString(64,'a')); QVERIFY(!prepareTunRuntime(built.yaml,result,paths).ok); QVERIFY(result.isEmpty());
        QByteArray bytes; QVERIFY(readFile(core,bytes,128*1024*1024).ok); QCOMPARE(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex(),QByteArray(TunCoreSha256));
    }
    void hostileSnapshotAndApiPolicy() {
        Settings s; s.tunEnabled=true; auto built=ConfigBuilder::build(source,s,18091,QString(64,'a')); QVERIFY(built.result.ok); const auto base=YAML::Load(built.yaml.toStdString());
        const QStringList attacks{"../x","C:/Windows/x","providers/../../x","providers/aaaaaaaaaaaaaaaaaaaaaaaa.cache:stream","providers/aaa.cache","\\\\server\\share"};
        for (const auto& path:attacks) { auto node=YAML::Clone(base); auto p=node["rule-providers"]["x"]; p["type"]="file"; p["path"]=path.toStdString(); p["behavior"]="domain"; QByteArray output; QStringList files; QVERIFY(!prepareTunRuntime(QByteArray::fromStdString(YAML::Dump(node)),output,files).ok); QVERIFY(output.isEmpty()); }
        auto node=YAML::Clone(base); auto p=node["rule-providers"]["x"]; p["type"]="file"; p["path"]="providers/aaaaaaaaaaaaaaaaaaaaaaaa.cache"; p["behavior"]="domain"; p["format"]="text"; QByteArray output; QStringList files; auto result=prepareTunRuntime(QByteArray::fromStdString(YAML::Dump(node)),output,files); QVERIFY2(result.ok,qUtf8Printable(result.error)); QCOMPARE(files,QStringList{"providers/aaaaaaaaaaaaaaaaaaaaaaaa.cache"});
        QVERIFY(checkTunProvider(output,files[0],"+.example.com\n").ok); QVERIFY(!checkTunProvider(output,"providers/bbbbbbbbbbbbbbbbbbbbbbbb.cache","+.example.com\n").ok);
        node=YAML::Clone(base); p=node["proxy-providers"]["x"]; p["type"]="file"; p["path"]="providers/aaaaaaaaaaaaaaaaaaaaaaaa.cache"; QVERIFY(prepareTunRuntime(QByteArray::fromStdString(YAML::Dump(node)),output,files).ok); QVERIFY(!checkTunProvider(output,files[0],"proxies: [{name: x, type: trojan, server: example.com, port: 443, private-key-file: C:/Windows/secret}]\n").ok);
        QVERIFY(!prepareTunRuntime("a: &a [*a]\n",output,files).ok); QVERIFY(!prepareTunRuntime(QByteArray(8*1024*1024+1,'x'),output,files).ok);
        QVERIFY(allowedTunApi("PATCH","/configs","{\"mode\":\"global\"}")); QVERIFY(allowedTunApi("PUT","/proxies/Test","{\"name\":\"DIRECT\"}")); QVERIFY(allowedTunApi("GET","/logs?level=warning",{}));
        QVERIFY(allowedTunApi("GET","/proxies/test%2Fnode/delay",{})); QVERIFY(allowedTunApi("PUT","/proxies/HK..01","{\"name\":\"DIRECT\"}")); QVERIFY(!allowedTunApi("GET","/proxies/%2e%2e%2Fupgrade",{}));
        for (const auto& path:QList<QByteArray>{"/upgrade","/restart","/configs","/providers/proxies/x","//host/x","/proxies/../upgrade","/proxies/%2e%2e/upgrade"}) QVERIFY(!allowedTunApi("POST",path,"{}"));
        QVERIFY(!allowedTunApi("PUT","/configs","{\"payload\":\"x\"}")); QVERIFY(!allowedTunApi("PATCH","/configs","{\"mode\":\"rule\",\"tun\":{\"enable\":true}}")); QVERIFY(!allowedTunApi("GET","/configs","{}"));
    }
    void gateAuthenticatesAndBlocksPrivilegeOperations() {
        MockHttp backend; QVERIFY(backend.listen(QHostAddress::LocalHost,0)); backend.handler=[](const QByteArray& request) { return request.contains("Bearer private-key")?HttpResponse{200,"{\"version\":\"test\"}"}:HttpResponse{401,{}}; };
        TunApiGate gate; const auto port=freePort(); QVERIFY(gate.listen(port,backend.serverPort(),"public-key","private-key").ok);
        gate.setBackendVerifier([](int client,int server) {
#ifdef Q_OS_WIN
            return tunNative::ownsCoreConnection(QCoreApplication::applicationPid(),client,server)&&!tunNative::ownsCoreConnection(0,client,server);
#else
            Q_UNUSED(client); Q_UNUSED(server); return true;
#endif
        });
        QNetworkAccessManager network; network.setProxy(QNetworkProxy::NoProxy);
        const auto call=[&](QByteArray verb,const QString& path,const QByteArray& key,QByteArray body={}) { QNetworkRequest r(QUrl(QString("http://127.0.0.1:%1").arg(port)+path)); r.setRawHeader("Authorization","Bearer "+key); r.setHeader(QNetworkRequest::ContentTypeHeader,"application/json"); return network.sendCustomRequest(r,verb,body); };
        auto* response=call("GET","/version","public-key"); QSignalSpy done(response,&QNetworkReply::finished); QTRY_COMPARE(done.size(),1); QCOMPARE(response->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),200); QVERIFY(response->readAll().contains("test")); QCOMPARE(backend.requests.size(),1); QVERIFY(!backend.requests[0].contains("public-key"));
        for (const auto& action:QList<QPair<QByteArray,QString>>{{"POST","/upgrade"},{"POST","/restart"},{"PUT","/configs"},{"GET","/version"}}) { response=call(action.first,action.second,action.second=="/version"?"wrong":"public-key","{}"); QSignalSpy completed(response,&QNetworkReply::finished); QTRY_COMPARE(completed.size(),1); QCOMPARE(response->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),403); }
        QCOMPARE(backend.requests.size(),1);
        gate.setBackendVerifier([](int,int) { return false; }); response=call("GET","/version","public-key"); QSignalSpy untrusted(response,&QNetworkReply::finished); QTRY_COMPARE(untrusted.size(),1); QCOMPARE(response->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),503); QCOMPARE(backend.requests.size(),1);
        gate.setBackendVerifier([](int,int) { return true; }); backend.handler=[](const QByteArray&) { return HttpResponse{200,"{\"type\":\"info\",\"payload\":\"stream-ok\"}\n",0,{},true}; }; response=call("GET","/logs?level=info","public-key"); QTRY_VERIFY(response->bytesAvailable()>0); QVERIFY(response->readAll().contains("stream-ok")); response->abort(); QTRY_COMPARE(backend.activeConnections(),0);
    }
    void sessionIdentityStopAcknowledgementAndLoss() {
        auto platform=std::make_shared<TunPlatform>(); QLocalServer server; const auto name="mlash-tun-test-"+newId(); QVERIFY(server.listen(name));
        QList<QByteArray> commands; QPointer<QLocalSocket> peer;
        const auto status=QJsonObject{{"protocol",1},{"ok",true},{"op","started"},{"pid",123},{"api_pid",456},{"adapter_up",true},{"routes",true}};
        QObject::connect(&server,&QLocalServer::newConnection,&server,[&] { peer=server.nextPendingConnection(); auto bytes=std::make_shared<QByteArray>(); QObject::connect(peer,&QLocalSocket::readyRead,&server,[&,bytes] { *bytes+=peer->readAll(); while (bytes->contains('\n')) { auto line=bytes->left(bytes->indexOf('\n')); bytes->remove(0,line.size()+1); commands.append(line); const auto request=QJsonDocument::fromJson(line).object(); if (request.value("op")=="start"||request.value("op")=="status") peer->write(QJsonDocument(status).toJson(QJsonDocument::Compact)+'\n'); else if (request.value("op")=="idle") { peer->write("{\"protocol\":1,\"ok\":true,\"op\":\"idle\",\"cleanup_confirmed\":true}\n"); } else if (request.value("op")=="stop") { peer->write(QJsonDocument(status).toJson(QJsonDocument::Compact)+'\n'); QTimer::singleShot(50,&server,[&,peer=peer] { if (peer) peer->write("{\"protocol\":1,\"ok\":true,\"op\":\"stopped\",\"cleanup_confirmed\":true}\n"); }); } } }); });
        TunSession session(platform,nullptr,name); Result started; int starts=0,stops=0; QSignalSpy lost(&session,&TunSession::lost); session.start(core,"isolated-snapshot",[&](Result r) { started=r; ++starts; }); QTRY_COMPARE(starts,1); QVERIFY2(started.ok,qUtf8Printable(started.error)); QVERIFY(session.ready()); QCOMPARE(session.apiProcessId(),qint64(456));
        session.stop([&](Result r) { QVERIFY(r.ok); ++stops; }); QTest::qWait(15); QCOMPARE(stops,0); QTRY_COMPARE(stops,1); QVERIFY(!session.active()); QCOMPARE(lost.size(),0);
        session.start(core,"isolated-snapshot",[&](Result r) { started=r; ++starts; }); QTRY_COMPARE(starts,2); QVERIFY(started.ok); QVERIFY(peer); peer->abort(); QTRY_COMPARE(lost.size(),1); QVERIFY(!session.active());
        session.stop([&](Result r) { QVERIFY(r.ok); ++stops; }); QTRY_COMPARE(stops,2);
        platform->verifyPeer=false; const auto count=commands.size(); session.start(core,"isolated-snapshot",[&](Result r) { started=r; ++starts; }); QTRY_COMPARE(starts,3); QVERIFY(!started.ok); QCOMPARE(commands.size(),count); QCOMPARE(lost.size(),1);
        platform->verifyPeer=true; session.start(core,"isolated-snapshot",[&](Result r) { started=r; ++starts; }); session.stop([&](Result r) { QVERIFY(r.ok); ++stops; }); QTRY_COMPARE(starts,4); QVERIFY(!started.ok); QCOMPARE(stops,3); bool idle=false; session.confirmCleanup([&](Result result) { QVERIFY(result.ok); idle=true; }); QTRY_VERIFY(idle); QVERIFY(!session.observation().value("cleanup_pending").toBool()); QVERIFY(commands.last().contains("idle")); QCOMPARE(platform->writes,0);
    }
    void controllerTransactionReadbackRollbackAndCandidate() {
        QTemporaryDir data; auto platform=std::make_shared<TunPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Settings s; s.corePath=core; s.startCore=false; s.mixedPort=freePort(); QVERIFY(store.save(s,{}).ok); const auto file=data.path()+"/source.yaml"; QVERIFY(writeAtomic(file,source).ok);
        auto session=std::make_unique<FakeTun>(platform); auto* fake=session.get(); AppController c(data.path(),true,nullptr,platform,WebsiteTests::defaults(),{}, {},std::move(session)); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); c.importFile("Test",file); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,12000); QVERIFY(finished.last()[0].toBool()); const auto id=c.profiles()[0].id; c.activateProfile(id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,12000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QVERIFY(c.state().ready); const auto originalPid=c.coreProcessId();
        QVERIFY(writeAtomic(file,source+"# new snapshot\n").ok); c.updateProfile(id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,12000); const auto candidate=c.profiles()[0].candidate;
        platform->installed=false; c.setTunEnabled(true); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,6000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.coreProcessId(),originalPid); QVERIFY(!c.settings().tunEnabled);
        platform->installed=true; fake->bad=true; c.setTunEnabled(true); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),5,16000); QVERIFY2(!finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QVERIFY2(c.state().ready,qUtf8Printable(finished.last()[1].toString())); QVERIFY(!c.settings().tunEnabled); QVERIFY(!c.state().tunObserved); QCOMPARE(c.profiles()[0].candidate,candidate);
        fake->bad=false; c.setTunEnabled(true); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),6,16000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QVERIFY(c.state().tunObserved); QVERIFY(c.settings().tunEnabled); QCOMPARE(c.profiles()[0].candidate,candidate);
        MainWindow* window=new MainWindow(&c); window->setTrayAvailable(true); window->resize(840,560); window->show(); QTest::qWait(50); auto* toggle=window->findChild<QCheckBox*>("homeTunSwitch"); QVERIFY(toggle->isChecked()); QVERIFY(window->rect().contains(toggle->mapTo(window,toggle->rect().bottomRight()))); const auto shots=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!shots.isEmpty()) { QDir().mkpath(shots); window->grab().save(shots+"/tun-home-minimum.png"); } delete window; QVERIFY(fake->active());
        c.setTunEnabled(false); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),7,16000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QVERIFY(c.state().ready); QVERIFY(!c.settings().tunEnabled); QVERIFY(!c.state().tunObserved); QCOMPARE(c.profiles()[0].candidate,candidate);
        const auto ordinaryPid=c.coreProcessId(); c.updateTunOptions(false,"mixed",true); QTRY_COMPARE(finished.size(),8); QVERIFY(finished.last()[0].toBool()); QCOMPARE(c.coreProcessId(),ordinaryPid); QVERIFY(c.settings().tunStrictRoute);
        c.setTunEnabled(true); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),9,16000); QVERIFY(finished.last()[0].toBool()); fake->failStop=true;
        QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),10,6000); QVERIFY(!finished.last()[0].toBool()); QVERIFY(!c.state().exiting); QCOMPARE(exit.size(),0); QVERIFY(c.settings().tunEnabled); QVERIFY(fake->active());
        fake->failStop=false; c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000); QCOMPARE(platform->writes,0); QCOMPARE(platform->installs,0); QVERIFY(fake->starts>=3); QVERIFY(!fake->active());
    }
    void uiServiceLifecycleWithoutOsWrites() {
        QTemporaryDir data; auto platform=std::make_shared<TunPlatform>(); platform->installed=false; AppController c(data.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1);
        QPointer<TunDialog> dialog=new TunDialog(&c); dialog->show(); QTRY_VERIFY(dialog->findChild<QLabel*>("tunServiceStatus")->text().contains("未安装")); QVERIFY(!dialog->findChild<QPushButton*>("applyTunOptions")->isEnabled()); dialog->findChild<QPushButton*>("installTunService")->click(); QCOMPARE(platform->installs,0); QVERIFY(!dialog->findChild<QLabel*>("tunServiceStatus")->text().isEmpty());
        c.repairTunService(); c.retryTunCleanup(); QCOMPARE(platform->installs,0); QSignalSpy saves(&c,&AppController::operationFinished); dialog->findChild<QPushButton*>("tunAdvancedToggle")->click(); auto* mtu=dialog->findChild<QSpinBox*>("tunMtu"); auto* routes=dialog->findChild<QPlainTextEdit*>("tunExcludeRoutes"); mtu->setValue(1400); routes->setPlainText("0.0.0.0/0"); QVERIFY(!dialog->findChild<QPushButton*>("applyTunOptions")->isEnabled()); QVERIFY(!dialog->findChild<QLabel*>("tunValidationError")->text().isEmpty()); routes->setPlainText("192.168.1.25/24"); platform->failState=true; dialog->findChild<QPushButton*>("applyTunOptions")->click(); QTRY_COMPARE(saves.size(),1); QVERIFY(!saves.last()[0].toBool()); QCOMPARE(mtu->value(),1400); QVERIFY(dialog->isVisible()); QCOMPARE(c.settings().tunMtu,1500); platform->failState=false; QTRY_VERIFY(dialog->findChild<QPushButton*>("applyTunOptions")->isEnabled()); dialog->findChild<QPushButton*>("applyTunOptions")->click(); QTRY_COMPARE(saves.size(),2); QVERIFY(saves.last()[0].toBool()); QCOMPARE(c.settings().tunMtu,1400); QCOMPARE(c.settings().tunExcludeRoutes,QStringList{"192.168.1.0/24"});
        QTRY_VERIFY(dialog->findChild<QPushButton*>("refreshTunService")->isEnabled()); const auto dir=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!dir.isEmpty()) { QDir().mkpath(dir); dialog->grab().save(dir+"/tun-dialog.png"); }
        dialog->close(); QTRY_VERIFY(dialog.isNull()); AppState state; state.core=CoreState::Running; state.ready=true; state.proxy=ProxyState::Disabled; state.tunObserved=true; QCOMPARE(TrayController::appearance(state),QString("rule")); QVERIFY(TrayController::statusTooltip(state).contains("TUN"));
        QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE(exit.size(),1); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(TunTests)
#include "TunTests.moc"
