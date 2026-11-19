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
        QTemporaryDir root; auto platform=std::make_shared<AppearancePlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&c); window.show(); window.setPage(6); auto* theme=window.findChild<QComboBox*>("appearancePreference"); auto* scale=window.findChild<QComboBox*>("textScalePreference"); auto* apply=window.findChild<QPushButton*>("applySettings"); QVERIFY(!apply->isVisible());
        int before=UiTheme::isDark(); theme->setCurrentIndex(theme->findData("dark")); QCOMPARE(UiTheme::isDark(),before); QVERIFY(!apply->isVisible()); const auto generation=c.state().generation; QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000); QVERIFY(finished[0][0].toBool()); QTRY_VERIFY_WITH_TIMEOUT(!c.state().busy,10000); scale->setCurrentIndex(scale->findData(110)); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,10000); QVERIFY(finished[1][0].toBool()); QCOMPARE(c.settings().appearance,QString("dark")); QCOMPARE(c.settings().textScale,110); QCOMPARE(UiTheme::textScale(),110); if (qApp->styleHints()->accessibility()->contrastPreference()!=Qt::ContrastPreference::HighContrast) QVERIFY(UiTheme::isDark()); QCOMPARE(c.state().generation,generation); ProfileStore store(root.path(),platform); QCOMPARE(store.load().settings.textScale,110);
        // A restart-class edit stays pending and is untouched by later preference commits.
        auto* port=window.findChild<QSpinBox*>("mixedPort"); const int draft=port->value()+1; port->setValue(draft); c.updateAppearance("light",120); QTRY_COMPARE(finished.size(),3); QVERIFY(finished[2][0].toBool()); QCOMPARE(port->value(),draft); QCOMPARE(c.settings().mixedPort,draft-1); platform->failSeal=true; c.updateAppearance("dark",90); QTRY_COMPARE(finished.size(),4); QVERIFY(!finished[3][0].toBool()); QCOMPARE(c.settings().appearance,QString("light")); QCOMPARE(UiTheme::textScale(),120); const auto old=Settings::fromJson({}); QCOMPARE(old.appearance,QString("system")); QCOMPARE(old.textScale,100); QVERIFY(!validAppearance("custom",100)); QVERIFY(!validAppearance("dark",500)); QCOMPARE(platform->writes,0);
    }
    void allPagesAtLimitedTextScales() {
        QTemporaryDir root; auto platform=std::make_shared<AppearancePlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&c); window.show(); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) QDir().mkpath(output);
        for (const auto& theme:QStringList{"light","dark"}) for (const int scale:{90,100,110,120}) {
            UiTheme::setPreferences(theme,scale); QCOMPARE(qApp->font().pointSizeF(),double(UiTheme::BodyPoints)*scale/100.0);
            for (const auto size:{QSize(840,560),QSize(1180,810)}) { window.resize(size); for (int page=0;page<8;++page) { window.setPage(page); QCoreApplication::processEvents(); for (auto* scroll:window.findChildren<QScrollArea*>()) if (scroll->isVisible())
                QVERIFY2(scroll->horizontalScrollBar()->maximum()==0,
                         qUtf8Printable(QString("page %1 %2/%3 %4 %5%% horizontal overflow: %6")
                             .arg(page).arg(scroll->objectName(),theme).arg(size.width()).arg(scale)
                             .arg(scroll->horizontalScrollBar()->maximum())));
                if (!output.isEmpty()&&size.width()==840&&(page==0||page==6||page==7)) QVERIFY(window.grab().save(output+QString("/%1-%2-page-%3.png").arg(theme).arg(scale).arg(page))); } }
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
            [&] { return new OutboundDialog(&c,&window); },[&] { return new HotkeysDialog(&c,&window); },
            [&] { return new ProxyEditorDialog(&c,profile.id,&window); },[&] { return new ProxyChainDialog(&c,profile.id,&window); },
            [&] { return new ProtocolSettingsDialog(&c,&window); },[&] { return new ProxyBypassDialog(&c,&window); },
            [&] { return new CoreUpdateDialog(&c,&window); },[&] { return new RemoteBackupDialog(&c,&window); },[&] { return new LocalBackupDialog(&c,&window); }
        };
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) QDir().mkpath(output);
        QStringList faces;
        if (QGuiApplication::platformName()=="windows") { QTextLayout text(QStringLiteral("设置中文 YouTube"),window.font()); text.beginLayout(); text.createLine(); text.endLayout(); for (const auto& run:text.glyphRuns()) if (run.rawFont().isValid()&&!faces.contains(run.rawFont().familyName())) faces.append(run.rawFont().familyName()); QVERIFY2(faces.join(',').contains("Noto Sans SC",Qt::CaseInsensitive),qUtf8Printable(faces.join(','))); }
        if (!output.isEmpty()) QVERIFY(writeAtomic(output+"/context.json",QJsonDocument(QJsonObject{{"dpr",window.devicePixelRatioF()},{"platform",QGuiApplication::platformName()},{"font",QFontInfo(window.font()).family()},{"glyph_families",QJsonArray::fromStringList(faces)}}).toJson()).ok);
        for (const auto& theme:QStringList{"light","dark","contrast"}) for (const int scale:{100,120}) {
            UiTheme::setPreferences(theme=="contrast"?"light":theme,scale); if (theme=="contrast") UiTheme::applyAppearance(Qt::ColorScheme::Light,true);
            int index=0;
            for (const auto& create:factories) {
                QPointer<QDialog> dialog=create(); dialog->resize(qMin(dialog->width(),780),qMin(dialog->height(),500)); dialog->show(); QTest::qWait(30);
                QVERIFY2(dialog->width()<=800&&dialog->height()<=520,qUtf8Printable(QString("%1 %2 %3: %4x%5").arg(dialog->objectName(),theme).arg(scale).arg(dialog->width()).arg(dialog->height())));
                for (auto* button:dialog->findChildren<QPushButton*>()) if (button->isVisible()) QVERIFY2(button->width()>=button->minimumSizeHint().width(),qUtf8Printable(dialog->objectName()+"/"+button->text()+" clipped"));
                for (auto* scroll:dialog->findChildren<QScrollArea*>()) if (scroll->isVisible())
                    QVERIFY2(scroll->horizontalScrollBar()->maximum()==0,
                             qUtf8Printable(QString("%1/%2 %3 %4%% horizontal overflow: %5")
                                 .arg(dialog->objectName(),scroll->objectName(),theme).arg(scale)
                                 .arg(scroll->horizontalScrollBar()->maximum())));
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
