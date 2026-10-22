#include "TestPorts.h"
#include "profile/YamlFields.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/YamlFormEditor.h"
#include "ui/ProxyEditorDialog.h"
#include "ui/DnsDialog.h"
#include "ui/UiTheme.h"
#include "ui/WidgetStyle.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTabWidget>
#include <QJsonDocument>
#include <QLayout>
#include <yaml-cpp/yaml.h>
using namespace mlash;
class FormsPlatform final : public PlatformServices {
public:
    int writes=0; bool failSeal=false;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("proxy write prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("Run write prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& bytes,QByteArray& out) override { if (failSeal) return Result::fail("injected save failure"); out="sealed:"+bytes.toBase64(); return {}; }
    Result open(const QByteArray& bytes,QByteArray& out) override { if (!bytes.startsWith("sealed:")) return Result::fail("bad index"); out=QByteArray::fromBase64(bytes.mid(7)); return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};

class YamlFormsTests final : public QObject {
    Q_OBJECT
    Settings settings() const { static const int port=isolatedListenerPort(); Settings s; s.mixedPort=port; s.startCore=false; s.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; return s; }
    void screenshot(QWidget* widget,const QString& name) { const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (output.isEmpty()) return; QVERIFY(QDir().mkpath(output)); QCoreApplication::processEvents(); QVERIFY(widget->grab().save(output+'/'+name+".png")); QJsonArray geometry; const auto add=[&](QWidget* item) { geometry.append(QJsonObject{{"hfw",item->heightForWidth(item->width())},{"layout_hfw",item->layout()?item->layout()->totalHeightForWidth(item->width()):-1},{"class",item->metaObject()->className()},{"name",item->objectName()},{"height",item->height()},{"min_height",item->minimumHeight()},{"min_hint",item->minimumSizeHint().height()},{"hint",item->sizeHint().height()},{"layout_min",item->layout()?item->layout()->minimumSize().height():0}}); }; add(widget); for (auto* child:widget->findChildren<QWidget*>()) add(child); QVERIFY(writeAtomic(output+'/'+name+"-layout.json",QJsonDocument(geometry).toJson()).ok); }
private slots:
    void initTestCase() { WidgetStyle::install(); UiTheme::install(); }
    void typedPatchesPreserveUntouchedValuesAndRejectDamagedInput() {
        const QString source="name: Test\ntype: vless\nserver: old.test\nport: 443\nuuid: private-fixture\ntls: false\nreality-opts: {public-key: preserve-me, short-id: abc}\n";
        auto result=patchYamlFields(source,"nodes",{{"server","new.test"},{"port","0"},{"udp","false"}}); QVERIFY2(result.ok,qUtf8Printable(result.error)); auto parsed=YAML::Load(result.data["yaml"].toString().toStdString()); QCOMPARE(parsed["server"].as<std::string>(),std::string("new.test")); QCOMPARE(parsed["port"].as<int>(),0); QVERIFY(!parsed["tls"].as<bool>()); QVERIFY(!parsed["udp"].as<bool>()); QCOMPARE(parsed["uuid"].as<std::string>(),std::string("private-fixture")); QCOMPARE(parsed["reality-opts"]["public-key"].as<std::string>(),std::string("preserve-me"));
        result=patchYamlFields(result.data["yaml"].toString(),"nodes",{{"tls",QJsonValue(QJsonValue::Null)}}); QVERIFY(result.ok); QVERIFY(!YAML::Load(result.data["yaml"].toString().toStdString())["tls"]);
        result=patchYamlFields("name: Test\ntype: select\nproxies: [A, B]\nuse: [Remote]\nstrategy: round-robin\n","groups",{{"proxies","B\nA"}}); QVERIFY(result.ok); parsed=YAML::Load(result.data["yaml"].toString().toStdString()); QCOMPARE(parsed["proxies"][0].as<std::string>(),std::string("B")); QCOMPARE(parsed["use"][0].as<std::string>(),std::string("Remote")); QCOMPARE(parsed["strategy"].as<std::string>(),std::string("round-robin"));
        for (const auto& input:QStringList{"server: [PRIVATE_FIXTURE", "server: one\nserver: two", "x: 1\n---\nx: 2", "x: &loop [*loop]", "x: !custom PRIVATE_FIXTURE", "<<: {x: one}"}) { result=inspectYamlFields(input,"nodes"); QVERIFY(!result.ok); QVERIFY(!result.error.contains("PRIVATE_FIXTURE")); }
        result=inspectYamlFields("nameserver: {unexpected: kept}\nnameserver-policy: {example.test: https://dns.example/query}\n","dns"); QVERIFY(result.ok); QVERIFY(result.data["unsupported"].toArray().contains("nameserver")); result=patchYamlFields("nameserver: {unexpected: kept}\nnameserver-policy: {example.test: https://dns.example/query}\n","dns",{{"ipv6","false"}}); QVERIFY(result.ok); parsed=YAML::Load(result.data["yaml"].toString().toStdString()); QVERIFY(parsed["nameserver"].IsMap()); QVERIFY(parsed["nameserver-policy"].IsMap());
    }
    void sharedDraftQueuesEditsAndKeepsUnparseableAdvancedText() {
        auto* raw=new QPlainTextEdit; raw->setPlainText("name: Test\ntype: http\nserver: old.test\nport: 80\npassword: private-fixture\nnested: {keep: true}\n"); YamlFormEditor form("nodes",raw); form.resize(620,460); form.show(); QTRY_VERIFY(form.valid()); auto* server=form.findChild<QLineEdit*>("yamlField_server"); auto* port=form.findChild<QLineEdit*>("yamlField_port"); QCOMPARE(form.findChild<QLineEdit*>("yamlField_password")->echoMode(),QLineEdit::Password); server->setText("new.test"); port->setText("1080"); QTRY_VERIFY(form.valid()); auto parsed=YAML::Load(raw->toPlainText().toStdString()); QCOMPARE(parsed["server"].as<std::string>(),std::string("new.test")); QCOMPARE(parsed["port"].as<int>(),1080); QVERIFY(parsed["nested"]["keep"].as<bool>()); QCOMPARE(parsed["password"].as<std::string>(),std::string("private-fixture"));
        form.findChild<QTabWidget*>("yamlFormTabs")->setCurrentIndex(1); raw->setPlainText("server: [PRIVATE_FIXTURE"); QTRY_VERIFY(!form.busy()); QVERIFY(!form.valid()); QCOMPARE(raw->toPlainText(),QString("server: [PRIVATE_FIXTURE")); QVERIFY(!form.findChild<QLabel*>("yamlFormHint")->text().contains("PRIVATE_FIXTURE")); QCOMPARE(form.findChild<QTabWidget*>("yamlFormTabs")->currentIndex(),1);
        raw->setPlainText("name: Fixed\ntype: http\nserver: corrected.test\nport: 443"); QTRY_VERIFY(form.valid()); QCOMPARE(server->text(),QString("corrected.test"));
        const auto large=QString("name: Large\ntype: http\nblob: '")+QString(64000,'x')+"'\n"; raw->setPlainText(large); QTRY_VERIFY(form.valid()); form.findChild<QTabWidget*>("yamlFormTabs")->setCurrentIndex(0); server->setText(QString(3000,'y')); QTRY_VERIFY(form.findChild<QPushButton*>("undoUnappliedYamlFields")->isVisible()); QCOMPARE(raw->toPlainText(),large); QCOMPARE(server->text().size(),3000); QVERIFY(!form.valid()); form.findChild<QPushButton*>("undoUnappliedYamlFields")->click(); QTRY_VERIFY(form.valid()); QCOMPARE(raw->toPlainText(),large); QVERIFY(server->text().isEmpty());
    }
    void groupReferencesComplexOriginsAndLongSecretsRemainIntact() {
        auto* raw=new QPlainTextEdit; raw->setPlainText("name: Test\ntype: select\nproxies: [B, A]\nuse: [Remote]\nstrategy: round-robin\n"); YamlFormEditor group("groups",raw); group.resize(620,480); group.setReferenceOptions({"A","B","DIRECT"},{"Remote"}); group.show(); QTRY_VERIFY(group.valid()); auto* choices=group.findChild<QComboBox*>("yamlReference_proxies"); choices->setCurrentIndex(choices->findData("DIRECT")); group.findChild<QPushButton*>("addYamlReference_proxies")->click(); QTRY_VERIFY(group.valid()); auto parsed=YAML::Load(raw->toPlainText().toStdString()); QCOMPARE(parsed["proxies"].size(),size_t(3)); QCOMPARE(parsed["proxies"][2].as<std::string>(),std::string("DIRECT")); group.findChild<QPushButton*>("addYamlReference_proxies")->click(); QTest::qWait(200); parsed=YAML::Load(raw->toPlainText().toStdString()); QCOMPARE(parsed["proxies"].size(),size_t(3)); screenshot(&group,"group-common-fields");
        auto* dnsRaw=new QPlainTextEdit; dnsRaw->setPlainText("nameserver: {complex: kept}\nipv6: false\n"); YamlFormEditor dns("dns",dnsRaw); dns.setSourceFields({"nameserver"}); dns.show(); QTRY_VERIFY(dns.valid()); QCOMPARE(dns.findChild<QLabel*>("yamlOrigin_nameserver")->text(),QString("草稿覆盖")); QVERIFY(!dns.findChild<QPlainTextEdit*>("yamlField_nameserver")->isEnabled()); dns.setOverrideEnabled(false); QCOMPARE(dns.findChild<QLabel*>("yamlOrigin_nameserver")->text(),QString("草稿已填 · 覆盖关闭"));
        auto* privateRaw=new QPlainTextEdit; const auto secret=QString(9000,'p'); privateRaw->setPlainText("name: Private\ntype: http\nserver: old.test\nport: 443\npassword: '"+secret+"'\n"); YamlFormEditor node("nodes",privateRaw); node.show(); QTRY_VERIFY(node.valid()); auto* password=node.findChild<QLineEdit*>("yamlField_password"); QCOMPARE(password->text().size(),secret.size()); QVERIFY(password->isReadOnly()); QVERIFY(node.findChild<QLineEdit*>("yamlField_sni")->isVisible()); QVERIFY(!node.findChild<QLineEdit*>("yamlField_servername")->isVisible()); node.findChild<QLineEdit*>("yamlField_server")->setText("new.test"); QTRY_VERIFY(node.valid()); QCOMPARE(YAML::Load(privateRaw->toPlainText().toStdString())["password"].as<std::string>().size(),size_t(9000));
        auto* cancelledRaw=new QPlainTextEdit("name: Cancel\ntype: http\nserver: test\nport: 80"); QPointer<YamlFormEditor> cancelled=new YamlFormEditor("nodes",cancelledRaw); cancelledRaw->setPlainText("name: Changed\ntype: http"); delete cancelled; QVERIFY(cancelled.isNull()); QTest::qWait(200);
    }
    void formsStageOnceSaveFailureAndDnsOriginsStaleProtection() {
        QTemporaryDir data; auto platform=std::make_shared<FormsPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Forms"; p.sourceType="file"; p.dnsOverride="ipv6: false\nnameserver-policy: {example.test: https://dns.example/query}\n"; p.dnsOverrideEnabled=true;
        const QByteArray source="proxies: [{name: Leaf, type: http, server: 127.0.0.1, port: 9, username: fixture, password: fixture-private}]\nproxy-groups: [{name: Test, type: select, proxies: [Leaf, DIRECT]}]\ndns: {enable: true, nameserver: ['https://1.1.1.1/dns-query']}\nrules: ['MATCH,Test']\n"; const auto prepared=store.prepare(p,source,settings(),19090,QString(64,'a')); QVERIFY2(prepared.result.ok,qUtf8Printable(prepared.result.error)); QVERIFY(store.save(settings(),{prepared.profile}).ok); AppController controller(data.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        QPointer<ProxyEditorDialog> parent=new ProxyEditorDialog(&controller,p.id); parent->show(); auto* nodes=parent->findChild<QListWidget*>("editableNodes"); QTRY_COMPARE(nodes->count(),1); nodes->setCurrentRow(0); parent->findChild<QPushButton*>("editProxyEntry")->click(); QPointer<QDialog> entry=parent->findChild<QDialog*>("proxyEntryDialog"); QVERIFY(entry); auto* form=entry->findChild<YamlFormEditor*>(); QTRY_VERIFY(form->valid()); form->findChild<QComboBox*>("yamlField_type")->setCurrentIndex(form->findChild<QComboBox*>("yamlField_type")->findData("socks5")); form->findChild<QLineEdit*>("yamlField_port")->setText("1080"); QTRY_VERIFY(form->valid()); screenshot(entry,"node-common-fields"); entry->findChild<QPushButton*>("acceptProxyEntry")->click(); QTRY_VERIFY(entry.isNull()); QVERIFY(controller.profiles()[0].proxyEdits.isEmpty()); auto* save=parent->findChild<QPushButton*>("saveProxyEdits"); QVERIFY(save->isEnabled()); platform->failSeal=true; save->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(!finished.last()[0].toBool()); QVERIFY(parent); platform->failSeal=false; save->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_VERIFY(parent.isNull()); const auto edited=YAML::Load(controller.profiles()[0].proxyEdits["nodes"].toObject()["Leaf"].toString().toStdString()); QCOMPARE(edited["password"].as<std::string>(),std::string("fixture-private")); QCOMPARE(edited["port"].as<int>(),1080);
        QPointer<DnsDialog> dns=new DnsDialog(&controller,p.id); dns->show(); auto* dnsForm=dns->findChild<YamlFormEditor*>(); QTRY_VERIFY(dnsForm->valid()); QTRY_COMPARE(dns->findChild<QLabel*>("yamlOrigin_nameserver")->text(),QString("沿用订阅")); QCOMPARE(dns->findChild<QLabel*>("yamlOrigin_ipv6")->text(),QString("草稿覆盖")); auto* upstream=dnsForm->findChild<QPlainTextEdit*>("yamlField_nameserver"); upstream->setPlainText("https://1.1.1.1/dns-query\nhttps://dns.google/dns-query"); QTRY_VERIFY(dnsForm->valid()); QTRY_COMPARE(dns->findChild<QLabel*>("yamlOrigin_nameserver")->text(),QString("草稿覆盖")); screenshot(dns,"dns-common-fields"); QCOMPARE(dns->size(),QSize(640,500)); dns->resize(560,400); QTest::qWait(50); QCOMPARE(dns->size(),QSize(560,400)); auto* dnsSave=dns->findChild<QPushButton*>("saveDnsOverride"); QVERIFY(dns->rect().contains(QRect(dnsSave->mapTo(dns,QPoint()),dnsSave->size()))); screenshot(dns,"dns-compact"); dns->resize(640,500);
        QPointer<DnsDialog> stale=new DnsDialog(&controller,p.id); stale->show(); auto* staleForm=stale->findChild<YamlFormEditor*>(); QTRY_VERIFY(staleForm->valid()); auto* staleRaw=stale->findChild<QPlainTextEdit*>("dnsOverrideEditor"); staleRaw->setPlainText("ipv6: true"); QTRY_VERIFY(staleForm->valid()); dns->findChild<QPushButton*>("saveDnsOverride")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_VERIFY(dns.isNull()); stale->findChild<QPushButton*>("saveDnsOverride")->click(); QTRY_COMPARE(finished.size(),4); QVERIFY(!finished.last()[0].toBool()); QVERIFY(stale); QCOMPARE(staleRaw->toPlainText(),QString("ipv6: true")); QVERIFY(stale->findChild<QLabel*>("dnsHint")->text().contains("已变化")); screenshot(stale,"dns-stale-draft"); stale->reject(); QCOMPARE(platform->writes,0);
        const auto final=YAML::Load(controller.profiles()[0].dnsOverride.toStdString()); QCOMPARE(final["nameserver"].size(),size_t(2)); QVERIFY(final["nameserver-policy"].IsMap()); QVERIFY(!controller.state().ready);
    }
};
QTEST_MAIN(YamlFormsTests)
#include "YamlFormsTests.moc"
