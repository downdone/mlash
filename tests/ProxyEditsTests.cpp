#include "TestPorts.h"
#include <QTcpServer>
#include "app/AppController.h"
#include "profile/ProxyEdits.h"
#include "profile/ProxyImpact.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/ProxyEditorDialog.h"
#include "ui/WidgetStyle.h"
#include "ui/UiTheme.h"
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QtTest>
#include <QTemporaryDir>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
using namespace mlash;
class EditPlatform final:public PlatformServices {
public:
    bool failSeal=false; int writes=0;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; } bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); } Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { if (failSeal) return Result::fail("injected index failure"); out=in; return {}; } Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); } Result attachChild(qint64) override { return {}; } Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
class ProxyEditsTests final:public QObject {
    Q_OBJECT
    const QByteArray source="proxies: [{name: Leaf, type: http, server: 127.0.0.1, port: 9}]\nproxy-groups: [{name: Test, type: select, proxies: [Leaf, DIRECT]}]\nrules: [\"MATCH,Test\"]\n";
    Settings settings() const { static const int isolatedPort=isolatedListenerPort(); Settings s; s.mixedPort=isolatedPort; s.startCore=false; s.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; return s; }
    QJsonObject edits() const { return {{"nodes",QJsonObject{{"New","name: New\ntype: socks5\nserver: 127.0.0.1\nport: 9"}}},{"groups",QJsonObject{{"Test","name: Test\ntype: select\nproxies: [New, DIRECT]"}}},{"remove_nodes",QJsonArray{"Leaf"}}}; }
private slots:
    void initTestCase() { WidgetStyle::install(); }
    void impactsUseCurrentDraftAndBoundUncertainty() {
        const auto original=YAML::Load("proxies: [{name: Leaf, type: http, server: example.test, port: 80}, {name: Hop, type: http, server: example.test, port: 80, dialer-proxy: Leaf}]\nproxy-groups: [{name: Test, type: select, proxies: [Leaf, Leaf]}, {name: Outer, type: select, proxies: [Test]}]\n");
        const auto effective=YAML::Load("rules: ['MATCH,Leaf', 'DOMAIN,private.example,Test']");
        auto impact=inspectProxyReferences(original,effective,{},"Leaf"); QVERIFY(impact.ok); QCOMPARE(impact.data.value("total").toInt(),3); QVERIFY(!impact.data.value("incomplete").toBool());
        QJsonObject draft{{"groups",QJsonObject{{"Test","name: Test\ntype: select\nproxies: [DIRECT]"}}},{"dialers",QJsonObject{{"Hop",""}}}};
        impact=inspectProxyReferences(original,effective,draft,"Leaf"); QVERIFY(impact.ok); QCOMPARE(impact.data.value("total").toInt(),1);
        draft["nodes"]=QJsonObject{{"Leaf","password: private-fixture ["},{"Bad","password: [private-fixture"}};
        impact=inspectProxyReferences(original,effective,draft,"Leaf"); QVERIFY(impact.ok); QCOMPARE(impact.data.value("total").toInt(),1); QVERIFY(impact.data.value("incomplete").toBool()); QVERIFY(!QJsonDocument(impact.data).toJson().contains("private-fixture"));
        impact=inspectProxyReferences(original,effective,{},"Test"); QVERIFY(impact.ok); QCOMPARE(impact.data.value("total").toInt(),2); QVERIFY(!QJsonDocument(impact.data).toJson().contains("private.example"));
        auto withDynamic=YAML::Clone(original); withDynamic["proxy-providers"]["Remote"]["type"]="http";
        impact=inspectProxyReferences(withDynamic,YAML::Load("rules: ['AND,((NETWORK,TCP),(DST-PORT,443)),Leaf']"),{},"Leaf"); QVERIFY(impact.ok); QVERIFY(impact.data.value("incomplete").toBool());
        QJsonObject many; for (int i=0;i<130;++i) { const auto name="G"+QString::number(i); many[name]="name: "+name+"\ntype: select\nproxies: [Leaf]"; }
        impact=inspectProxyReferences(original,effective,{{"groups",many}},"Leaf"); QVERIFY(impact.ok); QCOMPARE(impact.data.value("total").toInt(),133); QCOMPARE(impact.data.value("references").toArray().size(),100);
        impact=inspectProxyReferences(original,effective,{{"dialers",QJsonObject{{"MissingOwner","Leaf"}}}},"Leaf"); QVERIFY(impact.ok); QVERIFY(impact.data.value("incomplete").toBool()); QCOMPARE(impact.data.value("total").toInt(),4);
        auto implicit=YAML::Clone(original); implicit["proxy-groups"][0]["include-all-proxies"]=true; impact=inspectProxyReferences(implicit,effective,{},"Leaf"); QVERIFY(impact.ok); QVERIFY(impact.data.value("incomplete").toBool());
        QVERIFY(!inspectProxyReferences(original,effective,{},"Missing").ok);
    }
    void removalCancellationFreshnessAndOutgoingOverride() {
        QTemporaryDir data; auto platform=std::make_shared<EditPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok);
        Profile p; p.id=newId(); p.name="Impact"; p.sourceType="file"; p.proxyEdits={{"dialers",QJsonObject{{"Solo","DIRECT"}}}};
        const auto withSolo=source+"# isolated fixture\n"; auto yaml=YAML::Load(withSolo.toStdString()); yaml["proxies"].push_back(YAML::Load("{name: Solo, type: http, server: 127.0.0.1, port: 9}")); const auto input=QByteArray::fromStdString(YAML::Dump(yaml));
        const auto prepared=store.prepare(p,input,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        QPointer<ProxyEditorDialog> parent=new ProxyEditorDialog(&c,p.id); parent->show(); auto* nodes=parent->findChild<QListWidget*>("editableNodes"); QTRY_COMPARE(nodes->count(),2);
        const auto select=[nodes](const QString& name) { for (int i=0;i<nodes->count();++i) if (nodes->item(i)->data(Qt::UserRole).toString()==name) nodes->setCurrentRow(i); };
        select("Solo"); parent->findChild<QPushButton*>("removeProxyEntry")->click(); QPointer<QDialog> removal=parent->findChild<QDialog*>("proxyRemovalDialog"); QVERIFY(removal); QTRY_VERIFY(removal->findChild<QPushButton*>("stageProxyRemoval")->isEnabled()); QCOMPARE(removal->findChild<QListWidget*>("proxyRemovalReferences")->count(),0); QVERIFY(removal->findChild<QLabel*>("proxyRemovalBoundary")->text().contains("自身")); QTest::keyClick(removal,Qt::Key_Escape); QTRY_VERIFY(removal.isNull()); QVERIFY(!parent->findChild<QPushButton*>("saveProxyEdits")->isEnabled()); QCOMPARE(c.profiles()[0].proxyEdits,p.proxyEdits);
        parent->findChild<QPushButton*>("removeProxyEntry")->click(); removal=parent->findChild<QDialog*>("proxyRemovalDialog"); QTRY_VERIFY(removal->findChild<QPushButton*>("stageProxyRemoval")->isEnabled()); removal->findChild<QPushButton*>("stageProxyRemoval")->click(); QTRY_VERIFY(removal.isNull()); parent->findChild<QPushButton*>("saveProxyEdits")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished[0][0].toBool(),qUtf8Printable(finished[0][1].toString())); QTRY_VERIFY(parent.isNull()); QVERIFY(c.profiles()[0].proxyEdits.value("dialers").toObject().isEmpty()); QCOMPARE(c.profiles()[0].proxyEdits.value("remove_nodes").toArray(),QJsonArray{"Solo"});
        parent=new ProxyEditorDialog(&c,p.id); parent->show(); nodes=parent->findChild<QListWidget*>("editableNodes"); QTRY_COMPARE(nodes->count(),2); for (int i=0;i<nodes->count();++i) if (nodes->item(i)->data(Qt::UserRole).toString()=="Leaf") nodes->setCurrentRow(i);
        parent->findChild<QPushButton*>("removeProxyEntry")->click(); removal=parent->findChild<QDialog*>("proxyRemovalDialog"); QTRY_VERIFY(removal->findChild<QPushButton*>("stageProxyRemoval")->isEnabled()); c.updateProxyEdits(p.id,{}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished[1][0].toBool()); removal->findChild<QPushButton*>("stageProxyRemoval")->click(); QVERIFY(removal); QVERIFY(!removal->findChild<QPushButton*>("stageProxyRemoval")->isEnabled()); QVERIFY(removal->findChild<QLabel*>("proxyRemovalSummary")->text().contains("变化")); removal->reject(); QTRY_VERIFY(removal.isNull()); parent->reject(); QTRY_VERIFY(parent.isNull()); QVERIFY(c.profiles()[0].proxyEdits.isEmpty());
        const auto current=c.profiles()[0]; bool inspected=false; Result inspection; c.inspectProxyImpact(p.id,{},"Leaf",{{"candidate",current.candidate},{"active",current.active}},[&](Result result) { inspection=std::move(result); inspected=true; }); c.updateProxyEdits(p.id,{{"dialers",QJsonObject{{"Leaf","DIRECT"}}}}); QTRY_VERIFY(inspected); QVERIFY(!inspection.ok); QVERIFY(inspection.error.contains("变化")); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY(finished[2][0].toBool());
        bool done=false; c.inspectProxyImpact(p.id,{},"Leaf",{},[&](Result result) { QVERIFY(!result.ok); QVERIFY(result.error.contains("变化")); done=true; }); QVERIFY(done); QCOMPARE(platform->writes,0);
    }
    void finalGraphAndPersistence() {
        Profile p; p.proxyEdits=edits(); auto build=[&] { return ConfigBuilder::buildProfile(source,settings(),19090,QString(64,'a'),p); }; auto b=build(); QVERIFY2(b.result.ok,qUtf8Printable(b.result.error)); const auto yaml=YAML::Load(b.yaml.toStdString()); QCOMPARE(yaml["proxies"].size(),size_t(1)); QCOMPARE(yaml["proxies"][0]["name"].as<std::string>(),std::string("New"));
        p.proxyEdits={{"remove_nodes",QJsonArray{"Leaf"}}}; QVERIFY(!build().result.ok); p.proxyEdits={{"groups",QJsonObject{{"Test","name: Test\ntype: select\nproxies: [Other]"},{"Other","name: Other\ntype: select\nproxies: [Test]"}}}}; QVERIFY(build().result.error.contains("循环")); p.proxyEdits={{"groups",QJsonObject{{"Test","name: Test\ntype: select\nuse: [missing]"}}}}; QVERIFY(!build().result.ok);
        p.proxyEdits={{"nodes",QJsonObject{{"New","name: Other\ntype: http"}}}}; QVERIFY(!build().result.ok); p.proxyEdits={{"nodes",QJsonObject{{"New","name: New\ntype: http\nserver: example.test\nport: 80\nca-file: private-path"}}}}; b=build(); QVERIFY(!b.result.ok); QVERIFY(!b.result.error.contains("private-path")); p.proxyEdits={{"nodes",QJsonObject{{"New","name: New\ntype: http\nserver: secret-server\nport: [invalid"}}}}; b=build(); QVERIFY(!b.result.ok); QVERIFY(!b.result.error.contains("secret-server"));
        QVERIFY(!validateProxyEdits({{"nodes",QJsonObject{{"DIRECT","test"}}}}).ok); QVERIFY(!validateProxyEdits({{"nodes",QJsonObject{{"A","test"}}},{"remove_nodes",QJsonArray{"A"}}}).ok); QVERIFY(!validateProxyEdits({{"nodes",QJsonArray{}}}).ok);
        QTemporaryDir data; auto platform=std::make_shared<EditPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); p.id=newId(); p.sourceType="file"; p.name="Edit"; p.proxyEdits=edits(); const auto prepared=store.prepare(p,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok); const auto loaded=store.load(); QVERIFY(loaded.result.ok); QCOMPARE(loaded.profiles[0].proxyEdits,p.proxyEdits); QByteArray original; QVERIFY(readFile(store.versionRoot(p.id,prepared.version)+"/source.yaml",original).ok); QCOMPARE(original,source); auto old=p.toJson(); old.remove("proxy_edits"); QVERIFY(Profile::fromJson(old).proxyEdits.isEmpty());
    }
    void realCoreCandidateAndFailureRollback() {
        QTemporaryDir data; auto platform=std::make_shared<EditPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Live"; p.sourceType="file"; auto prepared=store.prepare(p,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); c.activateProfile(p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(finished[0][0].toBool()); const auto file=data.path()+"/next.yaml"; QVERIFY(writeAtomic(file,source+"# independent candidate\n").ok); c.importFile("",file,p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished[1][0].toBool()); const auto pending=c.profiles()[0].candidate;
        c.updateProxyEdits(p.id,edits()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY2(finished[2][0].toBool(),qUtf8Printable(finished[2][1].toString())); QTRY_VERIFY(c.proxies().contains("New")); QVERIFY(!c.proxies().contains("Leaf")); QCOMPARE(c.proxies().value("Test").toObject().value("all").toArray(),QJsonArray({"New","DIRECT"})); QCOMPARE(c.profiles()[0].candidate,pending);
        QByteArray before; QVERIFY(readFile(data.path()+"/state.dat",before).ok); const auto pid=c.coreProcessId(); c.updateProxyEdits(p.id,{{"remove_nodes",QJsonArray{"Leaf"}}}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,15000); QVERIFY(!finished[3][0].toBool()); QCOMPARE(c.coreProcessId(),pid);
        platform->failSeal=true; c.updateProxyEdits(p.id,{}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),5,15000); QVERIFY(!finished[4][0].toBool()); QTRY_VERIFY(c.state().ready); QTRY_VERIFY(c.proxies().contains("New")); QVERIFY(!c.proxies().contains("Leaf")); QCOMPARE(c.profiles()[0].proxyEdits,edits()); QByteArray after; QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,before); platform->failSeal=false;
        c.updateProxyEdits(p.id,{}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),6,15000); QVERIFY(finished[5][0].toBool()); QTRY_VERIFY(c.proxies().contains("Leaf")); QVERIFY(!c.proxies().contains("New")); QCOMPARE(c.profiles()[0].candidate,pending); QSignalSpy exit(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000); QCOMPARE(platform->writes,0);
    }
    void staleEditorDoesNotReplaceNewerEdits() {
        QTemporaryDir data; auto platform=std::make_shared<EditPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Stale"; p.sourceType="file"; p.proxyEdits=edits(); const auto prepared=store.prepare(p,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); QByteArray before,after; QVERIFY(readFile(data.path()+"/state.dat",before).ok); c.updateProxyEdits(p.id,{},QJsonObject{}); QCOMPARE(finished.size(),1); QVERIFY(!finished[0][0].toBool()); QVERIFY(finished[0][1].toString().contains("其他窗口")); QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,before); QCOMPARE(c.profiles()[0].proxyEdits,p.proxyEdits);
    }
    void draftErrorsCancelAndSave() {
        QTemporaryDir data; auto platform=std::make_shared<EditPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Draft"; p.sourceType="file"; auto prepared=store.prepare(p,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok); AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        QPointer<ProxyEditorDialog> d=new ProxyEditorDialog(&c,p.id); d->show(); auto* nodes=d->findChild<QListWidget*>("editableNodes"); QTRY_COMPARE(nodes->count(),1); auto* save=d->findChild<QPushButton*>("saveProxyEdits"); QVERIFY(!save->isEnabled()); nodes->setCurrentRow(0); d->findChild<QPushButton*>("removeProxyEntry")->click(); auto* removal=d->findChild<QDialog*>("proxyRemovalDialog"); QVERIFY(removal); auto* stageRemoval=removal->findChild<QPushButton*>("stageProxyRemoval"); QTRY_VERIFY(stageRemoval->isEnabled()); QVERIFY(!save->isEnabled()); QCOMPARE(removal->findChild<QListWidget*>("proxyRemovalReferences")->count(),1); QVERIFY(removal->findChild<QListWidget*>("proxyRemovalReferences")->item(0)->text().contains("Test")); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(removal->grab().save(output+"/node-removal-impact.png")); } stageRemoval->click(); QTRY_VERIFY(d->findChild<QDialog*>("proxyRemovalDialog")==nullptr); QVERIFY(save->isEnabled()); save->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished[0][0].toBool()); QVERIFY(d); QVERIFY(d->findChild<QLabel*>("proxyEditorHint")->text().contains("引用")); d->findChild<QPushButton*>("resetProxyEdits")->click(); QVERIFY(!save->isEnabled());
        nodes->setCurrentRow(0); d->findChild<QPushButton*>("editProxyEntry")->click(); auto* editor=d->findChild<QDialog*>("proxyEntryDialog"); QVERIFY(editor); editor->findChild<QPlainTextEdit*>("proxyEntryYaml")->setPlainText("name: Leaf\ntype: socks5\nserver: 127.0.0.1\nport: 9"); QTRY_VERIFY(editor->findChild<QPushButton*>("acceptProxyEntry")->isEnabled()); editor->findChild<QPushButton*>("acceptProxyEntry")->click(); save->click(); QTRY_VERIFY_WITH_TIMEOUT(d.isNull(),15000); QVERIFY(finished.last()[0].toBool()); QVERIFY(!c.profiles()[0].proxyEdits.isEmpty()); d=new ProxyEditorDialog(&c,p.id); d->show(); QTRY_COMPARE(d->findChild<QListWidget*>("editableNodes")->count(),1); d->findChild<QPushButton*>("resetProxyEdits")->click(); d->reject(); QTRY_VERIFY(d.isNull()); QVERIFY(!c.profiles()[0].proxyEdits.isEmpty()); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(ProxyEditsTests)
#include "ProxyEditsTests.moc"
