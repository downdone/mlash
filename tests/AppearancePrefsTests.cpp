#include "TestPorts.h"
#include "app/AppController.h"
#include <QTcpServer>
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/MainWindow.h"
#include "ui/GuiHost.h"
#include "ui/UiTheme.h"
#include "ui/WidgetStyle.h"
#include "ui/BackupDialog.h"
#include "ui/LocalBackupDialog.h"
#include "ui/WebsiteTargetsDialog.h"
#include "ui/RuleOverlayDialog.h"
#include "ui/DnsDialog.h"
#include "ui/GeoDataDialog.h"
#include "ui/OutboundDialog.h"
#include "ui/HotkeysDialog.h"
#include "ui/HomeSummaryDialog.h"
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include "ui/ProxyEditorDialog.h"
#include "ui/ProxyChainDialog.h"
#include "ui/ProtocolSettingsDialog.h"
#include "ui/ProxyBypassDialog.h"
#include "ui/CoreUpdateDialog.h"
#include "ui/RemoteBackupDialog.h"
#include "MockHttp.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QToolButton>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleHints>
#include <QAccessibilityHints>
#include <QTextLayout>
#include <QGlyphRun>
#include <QFontInfo>
#include <QJsonDocument>
using namespace mlash;
class AppearancePlatform final : public PlatformServices {
public:
    int writes=0; bool failSeal=false;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { if (failSeal) return Result::fail("injected index failure"); out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
class AppearancePrefsTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WidgetStyle::install(); }
    void cleanup() { UiTheme::setPreferences("system",100); UiTheme::applyAppearance(Qt::ColorScheme::Light); }
    void draftApplyFailureAndPersistence() {
        QTemporaryDir root; auto platform=std::make_shared<AppearancePlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&c); window.show(); window.setPage(6); auto* theme=window.findChild<QComboBox*>("appearancePreference"); auto* scale=window.findChild<QComboBox*>("textScalePreference"); auto* apply=window.findChild<QPushButton*>("applySettings"); QVERIFY(!apply->isEnabled()); const bool before=UiTheme::isDark(); theme->setCurrentIndex(theme->findData("dark")); scale->setCurrentIndex(scale->findData(110)); QCOMPARE(UiTheme::isDark(),before); QVERIFY(apply->isEnabled()); const auto generation=c.state().generation; apply->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(finished[0][0].toBool()); QCOMPARE(c.settings().appearance,QString("dark")); QCOMPARE(c.settings().textScale,110); QCOMPARE(UiTheme::textScale(),110); if (qApp->styleHints()->accessibility()->contrastPreference()!=Qt::ContrastPreference::HighContrast) QVERIFY(UiTheme::isDark()); QCOMPARE(c.state().generation,generation); QVERIFY(!apply->isEnabled()); ProfileStore store(root.path(),platform); QCOMPARE(store.load().settings.textScale,110);
        auto* port=window.findChild<QSpinBox*>("mixedPort"); const int draft=port->value()+1; port->setValue(draft); c.updateAppearance("light",120); QTRY_COMPARE(finished.size(),2); QVERIFY(finished[1][0].toBool()); QCOMPARE(port->value(),draft); QCOMPARE(c.settings().mixedPort,draft-1); platform->failSeal=true; c.updateAppearance("dark",90); QTRY_COMPARE(finished.size(),3); QVERIFY(!finished[2][0].toBool()); QCOMPARE(c.settings().appearance,QString("light")); QCOMPARE(UiTheme::textScale(),120); const auto old=Settings::fromJson({}); QCOMPARE(old.appearance,QString("system")); QCOMPARE(old.textScale,100); QVERIFY(!validAppearance("custom",100)); QVERIFY(!validAppearance("dark",500)); QCOMPARE(platform->writes,0);
    }
    void finiteHomeSummaryDraftsPersistenceAndFailure() {
        QTemporaryDir root; auto platform=std::make_shared<AppearancePlatform>(); ProfileStore initialStore(root.path(),platform); QVERIFY(initialStore.load().result.ok); Settings initial; initial.startCore=false; Profile profile; profile.id=newId(); profile.name="Summary fixture"; profile.sourceType="file"; auto prepared=initialStore.prepare(profile,"proxies: []\nrules: ['MATCH,DIRECT']\n",initial,19090,QString(64,'a')); QVERIFY(prepared.result.ok); profile=prepared.profile; profile.active=profile.candidate; initial.activeProfile=profile.id; QVERIFY(initialStore.save(initial,{profile}).ok); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&c); window.resize(940,700); window.show(); QTest::qWait(50); const auto generation=c.state().generation; auto* dashboard=window.findChild<QWidget*>("dashboard"); window.findChild<QToolButton*>("homeDetailsDisclosure")->click(); QCoreApplication::processEvents(); const auto fullHeight=window.findChild<QScrollArea*>("homeScroll")->widget()->minimumSizeHint().height(); auto* port=window.findChild<QSpinBox*>("mixedPort"); const auto portDraft=port->value()+1; port->setValue(portDraft); window.setPage(6); window.findChild<QPushButton*>("configureHomeSummary")->click(); QPointer<HomeSummaryDialog> dialog=window.findChild<HomeSummaryDialog*>(); QVERIFY(dialog); for (auto* choice:dialog->findChildren<QCheckBox*>()) choice->setChecked(false); dialog->findChild<QPushButton*>("saveHomeSummary")->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(finished[0][0].toBool()); QTRY_VERIFY(dialog.isNull()); QCOMPARE(c.settings().homeSummaryMask,0); window.setPage(0); QCoreApplication::processEvents(); QVERIFY(window.findChild<QScrollArea*>("homeScroll")->widget()->minimumSizeHint().height()<fullHeight); const auto cards=window.findChildren<QWidget*>("dashboardCard"); QCOMPARE(cards.size(),6); for (auto* card:cards) QCOMPARE(card->isVisible(),card->property("page").toInt()<3); QCOMPARE(c.state().generation,generation); QCOMPARE(port->value(),portDraft);
        ProfileStore store(root.path(),platform); QCOMPARE(store.load().settings.homeSummaryMask,0); QByteArray before,after; QVERIFY(readFile(root.path()+"/state.dat",before).ok); c.updateHomeSummary(16); QCOMPARE(finished.size(),2); QVERIFY(!finished.last()[0].toBool()); c.updateHomeSummary(3,15); QCOMPARE(finished.size(),3); QVERIFY(!finished.last()[0].toBool()); platform->failSeal=true; c.updateHomeSummary(5,0); QTRY_COMPARE(finished.size(),4); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.settings().homeSummaryMask,0); QVERIFY(readFile(root.path()+"/state.dat",after).ok); QCOMPARE(after,before); platform->failSeal=false;
        c.updateHomeSummary(7,0); QTRY_COMPARE(finished.size(),5); QVERIFY(finished.last()[0].toBool()); QCoreApplication::processEvents(); QList<int> widths; for (auto* card:cards) if (card->property("page").toInt()>=3&&card->isVisible()) widths.append(card->width()); QCOMPARE(widths.size(),3); QVERIFY(qAbs(widths[0]-widths[1])<=1); QVERIFY(qAbs(widths[0]-widths[2])<=1); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(window.grab().save(output+"/home-three-summaries.png")); }
        dialog=new HomeSummaryDialog(&c,&window); dialog->show(); dialog->findChild<QPushButton*>("resetHomeSummary")->click(); c.updateHomeSummary(1,7); QTRY_COMPARE(finished.size(),6); QVERIFY(finished.last()[0].toBool()); dialog->findChild<QPushButton*>("saveHomeSummary")->click(); QTRY_COMPARE(finished.size(),7); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog); QVERIFY(dialog->findChild<QLabel*>("homeSummaryHint")->text().contains("其他窗口")); if (!output.isEmpty()) QVERIFY(dialog->grab().save(output+"/home-summary-stale-draft.png")); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(c.settings().homeSummaryMask,1); dialog=new HomeSummaryDialog(&c,&window); dialog->findChild<QPushButton*>("resetHomeSummary")->click(); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(c.settings().homeSummaryMask,1); window.hide(); { GuiHost host(&c,true); for (int round=0;round<2;++round) { host.show(0); auto* rebuilt=host.window(); QVERIFY(rebuilt); QVERIFY(!rebuilt->findChild<QWidget*>("homeAdditionalInfo")->isVisible()); rebuilt->findChild<QToolButton*>("homeDetailsDisclosure")->click(); int visible=0; for (auto* card:rebuilt->findChildren<QWidget*>("dashboardCard")) if (card->isVisible()&&card->property("page").toInt()>=3) { ++visible; QCOMPARE(card->property("page").toInt(),3); } QCOMPARE(visible,1); host.dispose(); QVERIFY(!host.window()); } } window.show();
        dialog=new HomeSummaryDialog(&c,&window); dialog->findChild<QPushButton*>("resetHomeSummary")->click(); dialog->findChild<QPushButton*>("saveHomeSummary")->click(); QTRY_COMPARE(finished.size(),8); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(dialog.isNull()); QCOMPARE(c.settings().homeSummaryMask,15); QCOMPARE(c.state().generation,generation); QCOMPARE(port->value(),portDraft); QCOMPARE(platform->writes,0); QCOMPARE(Settings::fromJson({}).homeSummaryMask,15); c.updateHomeSummary(3); QTRY_COMPARE(finished.size(),9); QVERIFY(finished.last()[0].toBool()); QCoreApplication::processEvents(); widths.clear(); for (auto* card:cards) if (card->property("page").toInt()>=3&&card->isVisible()) widths.append(card->width()); QCOMPARE(widths.size(),2); QVERIFY(qAbs(widths[0]-widths[1])<=1); c.updateHomeSummary(15,3); QTRY_COMPARE(finished.size(),10); QVERIFY(finished.last()[0].toBool()); QCOMPARE(c.state().generation,generation); QVERIFY(readFile(root.path()+"/state.dat",before).ok);
        for (const auto& invalid:QJsonArray{-1,16,1.5,"3",true,QJsonValue()}) { QTemporaryDir badRoot; auto index=QJsonDocument::fromJson(before).object(); auto settings=index.value("settings").toObject(); settings["home_summary_mask"]=invalid; index["settings"]=settings; QVERIFY(writeAtomic(badRoot.path()+"/state.dat",QJsonDocument(index).toJson()).ok); ProfileStore bad(badRoot.path(),platform); QVERIFY(!bad.load().result.ok); }
        { MainWindow rebuilt(&c); rebuilt.show(); QCOMPARE(rebuilt.findChildren<QWidget*>("dashboardCard").size(),6); for (auto* card:rebuilt.findChildren<QWidget*>("dashboardCard")) QCOMPARE(card->isVisible(),card->property("page").toInt()<3); rebuilt.findChild<QToolButton*>("homeDetailsDisclosure")->click(); for (auto* card:rebuilt.findChildren<QWidget*>("dashboardCard")) QVERIFY(card->isVisible()); }
    }
    void allPagesAtLimitedTextScales() {
        QTemporaryDir root; auto platform=std::make_shared<AppearancePlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&c); window.show(); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) QDir().mkpath(output);
        for (const auto& theme:QStringList{"light","dark"}) for (const int scale:{90,100,110,120}) {
            UiTheme::setPreferences(theme,scale); QCOMPARE(qApp->font().pointSizeF(),double(UiTheme::BodyPoints)*scale/100.0);
            for (const auto size:{QSize(840,560),QSize(1180,810)}) { window.resize(size); for (int page=0;page<8;++page) { window.setPage(page); QCoreApplication::processEvents(); for (auto* scroll:window.findChildren<QScrollArea*>()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); if (!output.isEmpty()&&size.width()==840&&(page==0||page==6||page==7)) QVERIFY(window.grab().save(output+QString("/%1-%2-page-%3.png").arg(theme).arg(scale).arg(page))); } }
        }
        UiTheme::applyAppearance(Qt::ColorScheme::Dark,true); QCOMPARE(QApplication::palette().color(QPalette::Window),QColor(UiTheme::Window)); QVERIFY(!UiTheme::styleSheet().contains('@')); QCOMPARE(platform->writes,0);
    }
    void addedDialogsFitSmallWindows() {
        QTemporaryDir root; auto platform=std::make_shared<AppearancePlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.mixedPort=isolatedListenerPort(); QVERIFY(settings.mixedPort>0); settings.startCore=false; Profile profile; profile.id=newId(); profile.name="视觉测试订阅"; profile.sourceType="file"; const auto prepared=store.prepare(profile,"proxies: [{name: JP Test, type: http, server: 127.0.0.1, port: 1080}]\nproxy-groups: [{name: Test, type: select, proxies: [JP Test]}]\nrules: ['MATCH,Test']\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings,{prepared.profile}).ok);
        AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&c); window.resize(840,560); window.show();
        const QList<std::function<QDialog*()>> factories={
            [&] { return new BackupDialog(&c,false,&window); },[&] { return new BackupDialog(&c,true,&window); },
            [&] { return new WebsiteTargetsDialog(&c,&window); },[&] { return new RuleOverlayDialog(&c,profile.id,&window); },
            [&] { return new DnsDialog(&c,profile.id,&window); },[&] { return new GeoDataDialog(&c,&window); },
            [&] { return new OutboundDialog(&c,&window); },[&] { return new HotkeysDialog(&c,&window); },[&] { return new HomeSummaryDialog(&c,&window); },
            [&] { return new ProxyEditorDialog(&c,profile.id,&window); },[&] { return new ProxyChainDialog(&c,profile.id,&window); },
            [&] { return new ProtocolSettingsDialog(&c,&window); },[&] { return new ProxyBypassDialog(&c,&window); },
            [&] { return new CoreUpdateDialog(&c,&window); },[&] { return new RemoteBackupDialog(&c,&window); },[&] { return new LocalBackupDialog(&c,&window); }
        };
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) QDir().mkpath(output);
        QStringList faces;
        if (QGuiApplication::platformName()=="windows") { QTextLayout text(QStringLiteral("设置中文 YouTube"),window.font()); text.beginLayout(); text.createLine(); text.endLayout(); for (const auto& run:text.glyphRuns()) if (run.rawFont().isValid()&&!faces.contains(run.rawFont().familyName())) faces.append(run.rawFont().familyName()); QVERIFY2(faces.join(',').contains("YaHei",Qt::CaseInsensitive)||faces.join(',').contains(QStringLiteral("微软雅黑")),qUtf8Printable(faces.join(','))); }
        if (!output.isEmpty()) QVERIFY(writeAtomic(output+"/context.json",QJsonDocument(QJsonObject{{"dpr",window.devicePixelRatioF()},{"platform",QGuiApplication::platformName()},{"font",QFontInfo(window.font()).family()},{"glyph_families",QJsonArray::fromStringList(faces)}}).toJson()).ok);
        for (const auto& theme:QStringList{"light","dark","contrast"}) for (const int scale:{100,120}) {
            UiTheme::setPreferences(theme=="contrast"?"light":theme,scale); if (theme=="contrast") UiTheme::applyAppearance(Qt::ColorScheme::Light,true);
            int index=0;
            for (const auto& create:factories) {
                QPointer<QDialog> dialog=create(); dialog->resize(qMin(dialog->width(),780),qMin(dialog->height(),500)); dialog->show(); QTest::qWait(30);
                QVERIFY2(dialog->width()<=800&&dialog->height()<=520,qUtf8Printable(QString("%1 %2 %3: %4x%5").arg(dialog->objectName(),theme).arg(scale).arg(dialog->width()).arg(dialog->height())));
                for (auto* button:dialog->findChildren<QPushButton*>()) if (button->isVisible()) QVERIFY2(button->width()>=button->minimumSizeHint().width(),qUtf8Printable(dialog->objectName()+"/"+button->text()+" clipped"));
                for (auto* scroll:dialog->findChildren<QScrollArea*>()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
                if (!output.isEmpty()&&scale==120) QVERIFY(dialog->grab().save(output+QString("/%1-dialog-%2-%3.png").arg(theme).arg(index).arg(dialog->objectName())));
                ++index; dialog->reject(); QTRY_VERIFY(dialog.isNull());
            }
        }
        QCOMPARE(platform->writes,0);
    }
    void pureAppearancePreservesLiveWebsiteTest() {
        QTemporaryDir root; MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); server.handler=[](const QByteArray& request) { if (request.startsWith("CONNECT")) { HttpResponse response; response.tunnel=true; return response; } return HttpResponse{204,{},350}; }; auto platform=std::make_shared<AppearancePlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.mixedPort=isolatedListenerPort(); QVERIFY(settings.mixedPort>0); settings.startCore=false; settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; QVERIFY(store.save(settings,{}).ok); const auto path=root.path()+"/source.yaml"; QVERIFY(writeAtomic(path,QString("proxies: [{name: n1, type: http, server: 127.0.0.1, port: %1}]\nproxy-groups: [{name: Test, type: select, proxies: [n1]}]\nrules: ['MATCH,DIRECT']\n").arg(server.serverPort()).toUtf8()).ok); AppController c(root.path(),true,nullptr,platform,{{"fixture","Fixture","http://example.test/fixture","F"}}); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1); c.importFile("Appearance",path); QTRY_COMPARE(finished.size(),1); QVERIFY(finished[0][0].toBool()); c.activateProfile(c.profiles()[0].id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished[1][0].toBool(),qUtf8Printable(finished[1][1].toString())); MainWindow window(&c); window.show(); window.setPage(7); QTRY_COMPARE(c.websiteNode("n1"),QString("n1")); c.startWebsiteTests("n1","fixture"); QVERIFY(c.websiteTests()->running()); const auto generation=c.state().generation; const auto pid=c.coreProcessId(); auto desired=c.settings(); desired.appearance="dark"; desired.textScale=110; c.updateSettings(desired); QTRY_COMPARE(finished.size(),3); QVERIFY(finished[2][0].toBool()); QCOMPARE(c.state().generation,generation); QCOMPARE(c.coreProcessId(),pid); QCOMPARE(c.websiteTests()->node(),QString("n1")); QTRY_VERIFY_WITH_TIMEOUT(!c.websiteTests()->running(),6000); QCOMPARE(c.websiteTests()->result("fixture").status,WebsiteResult::Responded); QSignalSpy exited(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.size(),1,10000); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(AppearancePrefsTests)
#include "AppearancePrefsTests.moc"
