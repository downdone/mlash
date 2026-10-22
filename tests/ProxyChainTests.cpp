#include "TestPorts.h"
#include "ui/UiTheme.h"
#include "ui/WidgetStyle.h"
#include "ui/ChainPreview.h"
#include "app/AppController.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/ProxyChainDialog.h"
#include "MockHttp.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QUrl>
#include <yaml-cpp/yaml.h>
using namespace mlash;
class ChainPlatform final:public PlatformServices {
public:
    bool failSeal=false; int writes=0;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; } Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); } QJsonObject proxyTarget(int) const override { return {}; } bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); } Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; } Result seal(const QByteArray& in,QByteArray& out) override { if (failSeal) return Result::fail("injected index failure"); out=in; return {}; } Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); } Result attachChild(qint64) override { return {}; } Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
// Real local CONNECT relay, constrained to fixture loopback endpoints.
class ForwardProxy final:public QTcpServer {
public:
    QStringList connections;
    ForwardProxy() { connect(this,&QTcpServer::newConnection,this,[this] { while (hasPendingConnections()) { auto* client=nextPendingConnection(); auto* upstream=new QTcpSocket(client); auto buffer=std::make_shared<QByteArray>(); auto tunnel=std::make_shared<bool>(false);
        connect(client,&QTcpSocket::disconnected,client,&QObject::deleteLater); connect(upstream,&QTcpSocket::disconnected,client,&QTcpSocket::disconnectFromHost); connect(upstream,&QTcpSocket::readyRead,client,[client,upstream] { client->write(upstream->readAll()); });
        connect(upstream,&QTcpSocket::connected,client,[client,upstream,buffer,tunnel] { *tunnel=true; client->write("HTTP/1.1 200 Connection established\r\n\r\n"); upstream->write(*buffer); buffer->clear(); });
        connect(client,&QTcpSocket::readyRead,client,[this,client,upstream,buffer,tunnel] { if (*tunnel) { upstream->write(client->readAll()); return; } *buffer+=client->readAll(); const auto end=buffer->indexOf("\r\n\r\n"); if (end<0) return; const auto first=buffer->left(buffer->indexOf("\r\n")).split(' '); if (first.size()<2||first[0]!="CONNECT") { client->abort(); return; } const QUrl target("tcp://"+QString::fromLatin1(first[1])); if (target.host()!="127.0.0.1"||target.port()<1024) { client->abort(); return; } connections.append(QString::fromLatin1(first[1])); buffer->remove(0,end+4); upstream->connectToHost(target.host(),quint16(target.port())); });
    } }); }
};
class ProxyChainTests final:public QObject {
    Q_OBJECT
    QByteArray source(int a=1080,int b=1081) const { return QString("proxies: [{name: A, type: http, server: 127.0.0.1, port: %1}, {name: B, type: http, server: 127.0.0.1, port: %2}]\nproxy-groups: [{name: Test, type: select, proxies: [A, B]}]\nrules: [\"MATCH,Test\"]\n").arg(a).arg(b).toUtf8(); }
    Settings settings() const { static const int port=isolatedListenerPort(); Settings s; s.mixedPort=port; s.startCore=false; s.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; return s; }
private slots:
    void initTestCase() { WidgetStyle::install(); UiTheme::install(); }
    void flowPreviewDirectionAndDraftInheritance() {
        const QSet<QString> nodes{"A","B","C"},targets{"A","B","C","Test","DIRECT"}; const QJsonObject source{{"A","B"},{"B","C"}};
        auto result=chainPreview("A",source,{},nodes,targets); QVERIFY(result.ok); QCOMPARE(result.data.value("full").toString(),QString("应用 → C → B → A → 目标"));
        result=chainPreview("A",source,{{"A",""}},nodes,targets); QCOMPARE(result.data.value("full").toString(),QString("应用 → A → 目标"));
        result=chainPreview("A",source,{{"A","Test"}},nodes,targets); QVERIFY(result.ok); QVERIFY(result.data.value("preview").toString().contains("Test（策略组）")); QVERIFY(result.data.value("full").toString().contains("运行时选择"));
        QVERIFY(!chainPreview("A",source,{{"A","Test"}},nodes,targets,{{"Test",QJsonArray{"A","B"}}}).ok);
        QJsonObject longSource; QSet<QString> longNodes; for (int i=0;i<=64;++i) { const auto name=QString::number(i); longNodes.insert(name); if (i<64) longSource[name]=QString::number(i+1); } QVERIFY(chainPreview("0",longSource,{},longNodes,longNodes).ok); longSource["64"]="65"; longNodes.insert("65"); QVERIFY(!chainPreview("0",longSource,{},longNodes,longNodes).ok);
        QVERIFY(!chainPreview("A",source,{{"C","A"}},nodes,targets).ok); QVERIFY(!chainPreview("A",source,{{"A","Missing"}},nodes,targets).ok);
    }
    void inheritedFlowSurvivesSavedOverrideAndReset() {
        QTemporaryDir data; auto platform=std::make_shared<ChainPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Flow"; p.sourceType="file"; p.proxyEdits={{"dialers",QJsonObject{{"A",""}}}}; auto inherited=source(); inherited.replace("name: A, type: http","name: A, dialer-proxy: B, type: http"); const auto prepared=store.prepare(p,inherited,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok); AppController controller(data.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1); QPointer<ProxyChainDialog> dialog=new ProxyChainDialog(&controller,p.id); dialog->show(); QTRY_COMPARE(dialog->findChild<QListWidget*>("chainNodes")->count(),2); auto* preview=dialog->findChild<QLabel*>("chainFlowPreview"); QCOMPARE(preview->text(),QString("应用 → A → 目标")); dialog->findChild<QPushButton*>("resetProxyDialers")->click(); QCOMPARE(preview->text(),QString("应用 → B → A → 目标")); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QCoreApplication::processEvents(); QVERIFY(dialog->grab().save(output+"/chain-source-preview.png")); } dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(finished.size(),0); QCOMPARE(platform->writes,0);
    }
    void graphAndInheritedChains() {
        Profile p; auto build=[&](const QByteArray& bytes) { return ConfigBuilder::buildProfile(bytes,settings(),19090,QString(64,'a'),p); };
        p.proxyEdits={{"dialers",QJsonObject{{"A","B"}}}}; auto built=build(source()); QVERIFY2(built.result.ok,qUtf8Printable(built.result.error)); QCOMPARE(YAML::Load(built.yaml.toStdString())["proxies"][0]["dialer-proxy"].as<std::string>(),std::string("B"));
        p.proxyEdits={{"dialers",QJsonObject{{"A","A"}}}}; QVERIFY(build(source()).result.error.contains("循环")); p.proxyEdits={{"dialers",QJsonObject{{"A","Test"}}}}; QVERIFY(build(source()).result.error.contains("循环")); p.proxyEdits={{"dialers",QJsonObject{{"A","B"},{"B","A"}}}}; QVERIFY(!build(source()).result.ok); p.proxyEdits={{"dialers",QJsonObject{{"Missing","B"}}}}; QVERIFY(!build(source()).result.ok); p.proxyEdits={{"dialers",QJsonObject{{"A","Unknown"}}}}; QVERIFY(!build(source()).result.ok);
        auto inherited=source(); inherited.replace("name: A, type: http","name: A, dialer-proxy: B, type: http"); p.proxyEdits={{"dialers",QJsonObject{{"A",""}}}}; built=build(inherited); QVERIFY(built.result.ok); QVERIFY(!YAML::Load(built.yaml.toStdString())["proxies"][0]["dialer-proxy"]); p.proxyEdits={}; built=build(inherited); QVERIFY(built.result.ok); QCOMPARE(YAML::Load(built.yaml.toStdString())["proxies"][0]["dialer-proxy"].as<std::string>(),std::string("B"));
    }
    void realTwoHopAndIndexFailureRestore() {
        ForwardProxy a,b; MockHttp target; QVERIFY(a.listen(QHostAddress::LocalHost)); QVERIFY(b.listen(QHostAddress::LocalHost)); QVERIFY(target.listen(QHostAddress::LocalHost)); target.handler=[](const QByteArray&) { return HttpResponse{204,{},25}; };
        QTemporaryDir data; auto platform=std::make_shared<ChainPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); auto s=settings(); s.testUrl=QString("http://127.0.0.1:%1/probe").arg(target.serverPort()); Profile p; p.id=newId(); p.name="Chain"; p.sourceType="file"; const auto prepared=store.prepare(p,source(a.serverPort(),b.serverPort()),s,19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(s,{prepared.profile}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished),delays(&c,&AppController::nodeDelay); c.initialize(); QTRY_COMPARE(init.size(),1); c.activateProfile(p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); c.updateProxyDialers(p.id,{{"A","B"}},{}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_COMPARE(c.proxies().value("Test").toObject().value("now").toString(),QString("A"));
        c.testNodes({"A"}); QTRY_COMPARE_WITH_TIMEOUT(delays.size(),1,10000); QVERIFY2(delays.last()[1].toInt()>0,qUtf8Printable(delays.last()[2].toString())); QVERIFY(b.connections.contains(QString("127.0.0.1:%1").arg(a.serverPort()))); QVERIFY(a.connections.contains(QString("127.0.0.1:%1").arg(target.serverPort()))); QCOMPARE(target.requests.size(),1); QCOMPARE(c.proxies().value("Test").toObject().value("now").toString(),QString("A"));
        QTRY_COMPARE(finished.size(),3); QByteArray before,after; QVERIFY(readFile(data.path()+"/state.dat",before).ok); platform->failSeal=true; c.updateProxyDialers(p.id,{},{{"A","B"}}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,15000); QVERIFY(!finished.last()[0].toBool()); QTRY_VERIFY(c.state().ready); QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,before); const auto previous=b.connections.size(); c.testNodes({"A"}); QTRY_COMPARE_WITH_TIMEOUT(delays.size(),2,10000); QVERIFY(delays.last()[1].toInt()>0); QVERIFY(b.connections.size()>previous); QTRY_COMPARE(finished.size(),5); platform->failSeal=false;
        c.updateProxyDialers(p.id,{},{{"A","B"}}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),6,15000); QVERIFY(finished.last()[0].toBool()); const auto count=b.connections.size(); c.testNodes({"A"}); QTRY_COMPARE_WITH_TIMEOUT(delays.size(),3,10000); QVERIFY(delays.last()[1].toInt()>0); QCOMPARE(b.connections.size(),count); QTRY_COMPARE(finished.size(),7); QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000); QCOMPARE(platform->writes,0);
    }
    void explicitDraftCycleAndCancel() {
        QTemporaryDir data; auto platform=std::make_shared<ChainPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Draft"; p.sourceType="file"; const auto prepared=store.prepare(p,source(),settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        QPointer<ProxyChainDialog> d=new ProxyChainDialog(&c,p.id); d->show(); auto* nodes=d->findChild<QListWidget*>("chainNodes"); QTRY_COMPARE(nodes->count(),2); auto* combo=d->findChild<QComboBox*>("chainTarget"); auto* save=d->findChild<QPushButton*>("saveProxyDialers"); QVERIFY(!save->isEnabled()); combo->setCurrentIndex(combo->findData("Test")); QVERIFY(d->findChild<QLabel*>("chainFlowPreview")->text().contains("循环")); save->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(d); QVERIFY(d->findChild<QLabel*>("chainHint")->text().contains("循环")); combo->setCurrentIndex(combo->findData("B")); save->click(); QTRY_VERIFY_WITH_TIMEOUT(d.isNull(),15000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(c.profiles()[0].proxyEdits.value("dialers").toObject().value("A").toString(),QString("B")); d=new ProxyChainDialog(&c,p.id); d->show(); QTRY_COMPARE(d->findChild<QListWidget*>("chainNodes")->count(),2); d->findChild<QPushButton*>("resetProxyDialers")->click(); d->reject(); QTRY_VERIFY(d.isNull()); QVERIFY(c.profiles()[0].proxyEdits.contains("dialers"));
    }
};
QTEST_MAIN(ProxyChainTests)
#include "ProxyChainTests.moc"
