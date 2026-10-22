#include "TestPorts.h"
#include <QTcpServer>
#include "app/AppController.h"
#include "profile/ConfigBuilder.h"
#include "profile/RuleDraft.h"
#include <QComboBox>
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/RuleOverlayDialog.h"
#include "ui/MainWindow.h"
#include "ui/UiTheme.h"
#include "ui/WidgetStyle.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QTableWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <yaml-cpp/yaml.h>

using namespace mlash;
class RulesPlatform final : public PlatformServices {
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
class RulesTests : public QObject {
    Q_OBJECT
    const QByteArray source="proxies: []\nproxy-groups: [{name: Test, type: select, proxies: [DIRECT, REJECT]}]\nrule-providers: {sites: {type: inline, behavior: domain, payload: [example.test]}}\nrules: [\"DOMAIN,source.test,DIRECT\", \"MATCH,Test\"]\n";
    Settings settings() const { static const int isolatedPort=isolatedListenerPort(); Settings settings; settings.mixedPort=isolatedPort; settings.startCore=false; settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; return settings; }
private slots:
    void initTestCase() { WidgetStyle::install(); UiTheme::install(); }
    void plainTextAndListShareTheSameDraft() {
        QTemporaryDir data; auto platform=std::make_shared<RulesPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile profile; profile.id=newId(); profile.sourceType="file"; profile.name="Shared"; const auto prepared=store.prepare(profile,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); profile=prepared.profile; QVERIFY(store.save(settings(),{profile}).ok);
        AppController controller(data.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&controller); window.show(); window.setPage(3); auto* editor=window.findChild<QPlainTextEdit*>("customRulesEditor"); auto* advanced=window.findChild<QPushButton*>("advancedRules");
        const auto original=QString("# local note\nDOMAIN-SUFFIX,task.test,DIRECT\n"); editor->setPlainText(original); QVERIFY(advanced->isEnabled()); advanced->click(); auto* dialog=window.findChild<RuleOverlayDialog*>("ruleOverlayDialog"); QVERIFY(dialog); auto* table=dialog->findChild<QTableWidget*>("prependRulesTable"); QCOMPARE(table->rowCount(),1); QCOMPARE(table->item(0,1)->text(),QString("task.test")); QTRY_COMPARE(dialog->findChild<QTableWidget*>("sourceRulesTable")->rowCount(),2); table->item(0,1)->setText("cancelled.test"); dialog->reject(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QCOMPARE(editor->toPlainText(),original); QCOMPARE(finished.size(),0);
        advanced->click(); QPointer<RuleOverlayDialog> shared=window.findChild<RuleOverlayDialog*>("ruleOverlayDialog"); QVERIFY(shared); QTRY_COMPARE(shared->findChild<QTableWidget*>("sourceRulesTable")->rowCount(),2); table=shared->findChild<QTableWidget*>("prependRulesTable"); table->item(0,1)->setText("form.test"); shared->findChild<QPushButton*>("saveRuleOverlay")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(shared.isNull()); QCOMPARE(editor->toPlainText(),QString("DOMAIN-SUFFIX,form.test,DIRECT"));
        editor->setPlainText("DOMAIN,private.test,Missing"); auto* target=window.findChild<QComboBox*>("ruleTarget"); QTRY_VERIFY(target->findData("GLOBAL")>=0); QVERIFY(!window.findChild<QPushButton*>("saveCustomRules")->isEnabled()); advanced->click(); shared=window.findChild<RuleOverlayDialog*>("ruleOverlayDialog"); QVERIFY(shared); QTRY_COMPARE(shared->findChild<QTableWidget*>("sourceRulesTable")->rowCount(),2); table=shared->findChild<QTableWidget*>("prependRulesTable"); QCOMPARE(table->item(0,2)->text(),QString("Missing")); QVERIFY(!shared->findChild<QPushButton*>("saveRuleOverlay")->isEnabled()); table->item(0,2)->setText("Test"); QVERIFY(shared->findChild<QPushButton*>("saveRuleOverlay")->isEnabled()); shared->findChild<QPushButton*>("saveRuleOverlay")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(shared.isNull()); QCOMPARE(editor->toPlainText(),QString("DOMAIN,private.test,Test")); QCOMPARE(controller.profiles()[0].customRules,QStringList{"DOMAIN,private.test,Test"});
        QByteArray preserved; QVERIFY(readFile(store.versionRoot(profile.id,controller.profiles()[0].candidate)+"/source.yaml",preserved).ok); QCOMPARE(preserved,source); QCOMPARE(platform->writes,0);
    }
    void sharedDraftChecksAndStaleEditors() {
        const QSet<QString> targets{"DIRECT","Test"},providers{"sites"};
        for (const auto& rule:QStringList{"MATCH,DIRECT","DOMAIN,example.test,Test","IP-CIDR,10.0.0.0/8,DIRECT,no-resolve","RULE-SET,sites,Test"}) { QCOMPARE(RuleDraft::compose(RuleDraft::fields(rule)),rule); QVERIFY(RuleDraft::check({rule},targets,providers).ok); }
        auto result=RuleDraft::check({"DOMAIN,example.test,DIRECT,no-resolve"},targets,providers); QVERIFY(!result.ok); QCOMPARE(result.data.value("code").toString(),QString("RULE_OPTION")); QCOMPARE(result.data.value("column").toInt(),3);
        result=RuleDraft::check({"MATCH,DIRECT","RULE-SET,missing,Test"},targets,providers); QCOMPARE(result.data.value("row").toInt(),2); QCOMPARE(result.data.value("column").toInt(),1); QCOMPARE(result.data.value("code").toString(),QString("RULE_SET_REFERENCE"));
        result=RuleDraft::check({"DOMAIN,PRIVATE_DOMAIN,Missing"},targets,providers); QVERIFY(!result.ok); QCOMPARE(result.data.value("column").toInt(),2); QVERIFY(!result.error.contains("PRIVATE_DOMAIN")); const auto built=ConfigBuilder::build(source,settings(),19090,QString(64,'a'),{"DOMAIN,PRIVATE_DOMAIN,Missing"}); QCOMPARE(built.result.data.value("code"),result.data.value("code")); QCOMPARE(built.result.data.value("path"),result.data.value("path"));
        QTemporaryDir data; auto platform=std::make_shared<RulesPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile profile; profile.id=newId(); profile.sourceType="file"; profile.name="Concurrent"; const auto prepared=store.prepare(profile,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); profile=prepared.profile; QVERIFY(store.save(settings(),{profile}).ok);
        AppController controller(data.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        QPointer<RuleOverlayDialog> first=new RuleOverlayDialog(&controller,profile.id),second=new RuleOverlayDialog(&controller,profile.id); first->show(); second->show(); QTRY_COMPARE(first->findChild<QTableWidget*>("sourceRulesTable")->rowCount(),2); QTRY_COMPARE(second->findChild<QTableWidget*>("sourceRulesTable")->rowCount(),2);
        first->findChild<QPushButton*>("addOverlayRule")->click(); second->findChild<QPushButton*>("addOverlayRule")->click(); auto* table=second->findChild<QTableWidget*>("prependRulesTable"); table->editItem(table->item(0,0)); QTRY_VERIFY(table->findChild<QComboBox*>("ruleCellEditor")); auto* editor=table->findChild<QComboBox*>("ruleCellEditor"); QCOMPARE(editor->currentData().toString(),QString("DOMAIN-SUFFIX")); QVERIFY(editor->currentText().contains("域名后缀")); QTest::keyClick(editor,Qt::Key_Return);
        first->findChild<QPushButton*>("saveRuleOverlay")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(first.isNull()); table->item(0,1)->setText("unsaved.test"); second->findChild<QPushButton*>("saveRuleOverlay")->click(); QCOMPARE(finished.size(),2); QVERIFY(!finished.last()[0].toBool()); QVERIFY(second); QVERIFY(finished.last()[1].toString().contains("其他窗口")); QVERIFY(second->findChild<QLabel*>("ruleOverlayHint")->text().contains("其他窗口")); QCOMPARE(table->item(0,1)->text(),QString("unsaved.test")); QCOMPARE(store.load().profiles[0].customRules,QStringList{"DOMAIN-SUFFIX,example.com,DIRECT"});
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QCoreApplication::processEvents(); QVERIFY(second->grab().save(output+"/rule-stale-draft.png")); } second->reject(); QCOMPARE(platform->writes,0);
    }
    void overlayOrderReferenceAndPersistence() {
        const auto built=ConfigBuilder::build(source,settings(),19090,QString(64,'a'),{"RULE-SET,sites,Test"},{"DOMAIN-SUFFIX,tail.test,REJECT"},{"MATCH,Test"}); QVERIFY2(built.result.ok,qUtf8Printable(built.result.error)); const auto config=YAML::Load(built.yaml.toStdString()); QCOMPARE(config["rules"].size(),size_t(3)); QCOMPARE(config["rules"][0].as<std::string>(),std::string("RULE-SET,sites,Test")); QCOMPARE(config["rules"][1].as<std::string>(),std::string("DOMAIN,source.test,DIRECT")); QCOMPARE(config["rules"][2].as<std::string>(),std::string("DOMAIN-SUFFIX,tail.test,REJECT")); QCOMPARE(built.result.data.value("rule_count").toInt(),3);
        auto bad=ConfigBuilder::build(source,settings(),19090,QString(64,'a'),{"RULE-SET,missing,DIRECT"}); QVERIFY(!bad.result.ok); QCOMPARE(bad.result.data.value("code").toString(),QString("RULE_SET_REFERENCE")); QVERIFY(!ConfigBuilder::build(source,settings(),19090,QString(64,'a'),{}, {"DOMAIN,x,Missing"}).result.ok);
        QTemporaryDir data; auto platform=std::make_shared<RulesPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile profile; profile.id=newId(); profile.sourceType="file"; profile.name="Rules"; profile.customRules={"RULE-SET,sites,Test"}; profile.appendRules={"DOMAIN,tail.test,DIRECT"}; profile.disabledRules={"MATCH,Test"}; auto prepared=store.prepare(profile,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok); const auto loaded=store.load(); QVERIFY(loaded.result.ok); QCOMPARE(loaded.profiles[0].appendRules,profile.appendRules); QCOMPARE(loaded.profiles[0].disabledRules,profile.disabledRules); QByteArray bytes; QVERIFY(readFile(store.versionRoot(profile.id,prepared.version)+"/source.yaml",bytes).ok); QCOMPARE(bytes,source);
        auto older=profile.toJson(); older.remove("append_rules"); older.remove("disabled_rules"); QVERIFY(Profile::fromJson(older).appendRules.isEmpty()); QVERIFY(Profile::fromJson(older).disabledRules.isEmpty());
    }
    void liveReadbackAndPendingCandidateProtection() {
        QTemporaryDir data; auto platform=std::make_shared<RulesPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile profile; profile.id=newId(); profile.sourceType="file"; profile.name="Live"; auto prepared=store.prepare(profile,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok);
        AppController controller(data.path(),true,nullptr,platform); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1); controller.activateProfile(profile.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished[0][0].toBool(),qUtf8Printable(finished[0][1].toString())); QVERIFY(controller.state().ready);
        const auto nextSource=source+"# pending source remains independent\n"; const auto file=data.path()+"/pending.yaml"; QVERIFY(writeAtomic(file,nextSource).ok); controller.importFile("",file,profile.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished[1][0].toBool()); const auto pending=controller.profiles()[0].candidate; QVERIFY(pending!=controller.profiles()[0].active);
        controller.updateRuleOverlay(profile.id,{"DOMAIN,prefix.test,REJECT"},{"DOMAIN-SUFFIX,tail.test,DIRECT"},{"MATCH,Test"}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY2(finished[2][0].toBool(),qUtf8Printable(finished[2][1].toString())); QVERIFY(controller.state().ready); QCOMPARE(controller.profiles()[0].candidate,pending); controller.setRulesVisible(true); controller.refreshRules(); QTRY_COMPARE(controller.activeRules().size(),3); QCOMPARE(controller.activeRules()[0].toObject().value("proxy").toString(),QString("REJECT")); QCOMPARE(controller.activeRules()[2].toObject().value("type").toString(),QString("DomainSuffix"));
        QByteArray candidate; QVERIFY(readFile(store.versionRoot(profile.id,pending)+"/source.yaml",candidate).ok); QCOMPARE(candidate,nextSource); QByteArray index; QVERIFY(readFile(data.path()+"/state.dat",index).ok); const auto pid=controller.coreProcessId(); controller.updateRuleOverlay(profile.id,{}, {"DOMAIN,bad.test,Missing"},{}); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,15000); QVERIFY(!finished[3][0].toBool()); QCOMPARE(controller.coreProcessId(),pid); QByteArray after; QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,index);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.size(),1,10000); QCOMPARE(platform->writes,0);
    }
    void structuredDraftFailureAndSave() {
        QTemporaryDir data; auto platform=std::make_shared<RulesPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile profile; profile.id=newId(); profile.sourceType="file"; profile.name="Draft"; auto prepared=store.prepare(profile,source,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok);
        AppController controller(data.path(),true,nullptr,platform); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        QPointer<RuleOverlayDialog> dialog=new RuleOverlayDialog(&controller,profile.id); dialog->show(); auto* original=dialog->findChild<QTableWidget*>("sourceRulesTable"); QTRY_COMPARE(original->rowCount(),2); auto* save=dialog->findChild<QPushButton*>("saveRuleOverlay"); QVERIFY(!save->isEnabled()); original->item(0,0)->setCheckState(Qt::Unchecked); QVERIFY(save->isEnabled()); dialog->findChild<QPushButton*>("addOverlayRule")->click(); auto* table=dialog->findChild<QTableWidget*>("prependRulesTable"); QCOMPARE(table->rowCount(),1); table->item(0,2)->setText("Missing"); QVERIFY(!save->isEnabled()); QCOMPARE(finished.size(),0); QCOMPARE(table->item(0,2)->text(),QString("Missing")); QVERIFY(dialog->findChild<QLabel*>("ruleOverlayHint")->text().contains("目标")); QVERIFY(!table->item(0,2)->toolTip().isEmpty()); table->item(0,2)->setText("DIRECT"); QVERIFY(save->isEnabled()); platform->failSeal=true; save->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished[0][0].toBool()); QVERIFY(dialog); QCOMPARE(table->item(0,2)->text(),QString("DIRECT")); QVERIFY(dialog->findChild<QLabel*>("ruleOverlayHint")->text().contains("injected")); platform->failSeal=false; save->click(); QTRY_VERIFY_WITH_TIMEOUT(dialog.isNull(),15000); QTRY_COMPARE(finished.size(),2); QVERIFY(finished[1][0].toBool()); QCOMPARE(controller.profiles()[0].customRules.size(),1); QCOMPARE(controller.profiles()[0].disabledRules,QStringList{"DOMAIN,source.test,DIRECT"}); QVERIFY(!controller.state().ready);
        dialog=new RuleOverlayDialog(&controller,profile.id); dialog->show(); QTRY_COMPARE(dialog->findChild<QTableWidget*>("sourceRulesTable")->rowCount(),2); QVERIFY(!dialog->findChild<QPushButton*>("saveRuleOverlay")->isEnabled()); dialog->findChild<QTabWidget*>("ruleOverlayTabs")->setCurrentIndex(2); dialog->findChild<QPushButton*>("addOverlayRule")->click(); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(finished.size(),2); QVERIFY(controller.profiles()[0].appendRules.isEmpty()); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(RulesTests)
#include "RulesTests.moc"
