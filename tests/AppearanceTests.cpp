#include "TestPorts.h"
#include "ui/MainWindow.h"
#include "ui/UiTheme.h"
#include "ui/UiIcons.h"
#include "ui/WidgetStyle.h"
#include "ui/ImportDialog.h"
#include "ui/TrayController.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "MockHttp.h"
#include <QtTest>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QScreen>
#include <QAbstractItemView>
#include <cmath>
#include <QFontInfo>
#include <QTextLayout>
#include <QGlyphRun>

using namespace mlash;
namespace {
double luminance(QColor color) { const auto channel=[](double x) { return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4); }; return .2126*channel(color.redF())+.7152*channel(color.greenF())+.0722*channel(color.blueF()); }
double contrast(const char* foreground,const char* background) { const double a=luminance(QColor(foreground)),b=luminance(QColor(background)); return (qMax(a,b)+.05)/(qMin(a,b)+.05); }
int freePort() { return isolatedListenerPort(); }
void save(QWidget* widget,const QString& name) { const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(widget->grab().save(output+"/"+name+".png")); } }
}
class AppearanceTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WidgetStyle::install(); }
    void cleanup() { UiTheme::applyAppearance(Qt::ColorScheme::Light); }
    void semanticContrastAndArtwork() {
        for (const auto scheme:{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}) for (bool high:{false,true}) {
            UiTheme::applyAppearance(scheme,high);
            for (const auto foreground:{UiTheme::Ink,UiTheme::Secondary,UiTheme::Muted,UiTheme::Brand,UiTheme::Success,UiTheme::Warning,UiTheme::Error}) for (const auto background:{UiTheme::Surface,UiTheme::Window,UiTheme::Sidebar}) QVERIFY2(contrast(foreground,background)>=4.5,qUtf8Printable(QString("%1 on %2: %3").arg(foreground,background).arg(contrast(foreground,background))));
            QVERIFY(contrast(UiTheme::OnPrimary,UiTheme::Primary)>=4.5); QVERIFY(contrast(UiTheme::Ink,UiTheme::Selection)>=4.5); if (high) QVERIFY(contrast(UiTheme::Border,UiTheme::Surface)>=3.0);
            QCOMPARE(QApplication::palette().color(QPalette::Window),QColor(UiTheme::Window)); QVERIFY(!UiTheme::styleSheet().contains('@'));
        }
        QFile manifest(QStringLiteral(MLASH_SOURCE_DIR "/resources/services/manifest.json")); QVERIFY(manifest.open(QIODevice::ReadOnly)); const auto files=QJsonDocument::fromJson(manifest.readAll()).object().value("files").toArray(); QCOMPARE(files.size(),12);
        QSet<QString> unique;
        for (const auto& entry:files) { const auto object=entry.toObject(); const auto id=object.value("id").toString(); QVERIFY(!unique.contains(id)); unique.insert(id); QVERIFY(!UiIcons::service(id).isNull()); QFile asset(":/services/"+id+".png"); QVERIFY(asset.open(QIODevice::ReadOnly)); QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(asset.readAll(),QCryptographicHash::Sha256).toHex()),object.value("sha256").toString()); QImage image; QVERIFY(image.loadFromData(asset.seek(0)?asset.readAll():QByteArray{})); QCOMPARE(image.size(),QSize(96,96)); }
        QVERIFY(!UiIcons::service("not-a-service").isNull());
        QVERIFY(UiIcons::service("not-a-service").pixmap(24,24).toImage()!=UiIcons::service("youtube").pixmap(24,24).toImage());
    }
    void themeChangesPreserveDraftAndPopupFirstFrame() {
        QTemporaryDir dir; AppController controller(dir.path(),true); QSignalSpy init(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&controller); window.show(); window.setPage(6); auto* port=window.findChild<QSpinBox*>("mixedPort"); const int draft=port->value()+1; port->setValue(draft);
        TrayController tray(&controller); auto* menu=tray.findChild<QSystemTrayIcon*>()->contextMenu();
        for (const auto scheme:{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}) {
            UiTheme::applyAppearance(scheme); QCoreApplication::processEvents(); QCOMPARE(port->value(),draft); QCOMPARE(controller.settings().mixedPort,draft-1); QCOMPARE(window.page(),6); QCOMPARE(window.findChild<QLabel*>("settingsDraftHint")->text(),QString("有 1 项待应用"));
            menu->popup(QPoint(180,100)); QTRY_VERIFY(menu->isVisible()); const auto before=menu->geometry();
            for (int wait:{0,16,160}) { if (wait) QTest::qWait(wait); const auto frame=menu->grab().toImage(); QCOMPARE(frame.pixelColor(0,0).alpha(),0); QCOMPARE(menu->geometry(),before); } save(menu,scheme==Qt::ColorScheme::Dark?"tray-dark":"tray-light"); menu->hide();
            auto* combo=window.findChild<QComboBox*>("startupPage"); combo->showPopup(); QTRY_VERIFY(combo->view()->isVisible()); QCOMPARE(combo->view()->window()->grab().toImage().pixelColor(0,0).alpha(),0); save(combo->view()->window(),scheme==Qt::ColorScheme::Dark?"combo-dark":"combo-light"); QTest::keyClick(combo->view(),Qt::Key_Escape); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(port->value(),draft);
        }
        window.setPage(5); auto* logs=window.findChild<QPlainTextEdit*>("logView");
        for (int i=0;i<150;++i) controller.diagnostics()->append("info",QString("read-history-%1").arg(i));
        QTRY_VERIFY(logs->verticalScrollBar()->maximum()>10);
        auto cursor=logs->textCursor(); cursor.setPosition(20); cursor.setPosition(30,QTextCursor::KeepAnchor); logs->setTextCursor(cursor); logs->verticalScrollBar()->setValue(10); const auto selection=logs->textCursor().selectedText();
        for (const auto scheme:{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}) { UiTheme::applyAppearance(scheme); QCoreApplication::processEvents(); QCOMPARE(logs->textCursor().selectedText(),selection); QCOMPARE(logs->verticalScrollBar()->value(),10); }
        QSignalSpy exit(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exit.size(),1);
    }
    void allPagesAndDialogsAcrossAppearances() {
        QTemporaryDir dir; AppController controller(dir.path(),true); QSignalSpy init(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&controller); window.setScreen(QGuiApplication::primaryScreen()); window.move(QGuiApplication::primaryScreen()->availableGeometry().topLeft()+QPoint(20,20)); window.show();
        const auto font=window.findChild<QLabel*>("websiteName")->font();
        QStringList families;
        // Offscreen QPA has dummy font engines without family metadata. Shape
        // and inspect real platform faces only in the native Windows run.
        if (QGuiApplication::platformName()=="windows") {
            QTextLayout text(QStringLiteral("设置中文 YouTube"),font); text.beginLayout(); text.createLine(); text.endLayout();
            for (const auto& run:text.glyphRuns()) { const auto raw=run.rawFont(); if (raw.isValid() && !families.contains(raw.familyName())) families.append(raw.familyName()); }
            QVERIFY2(families.join(',').contains("YaHei",Qt::CaseInsensitive) || families.join(',').contains(QStringLiteral("微软雅黑")),qUtf8Printable(families.join(',')));
        }
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(writeAtomic(output+"/appearance-context.json",QJsonDocument(QJsonObject{{"device_pixel_ratio",window.devicePixelRatioF()},{"platform",QGuiApplication::platformName()},{"font_requested",font.family()},{"font_resolved",QFontInfo(font).family()},{"glyph_families",QJsonArray::fromStringList(families)}}).toJson()).ok); }
        for (const auto scheme:{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}) for (const auto size:{QSize(840,560),QSize(940,620),QSize(1180,810)}) {
            UiTheme::applyAppearance(scheme); window.resize(size);
            for (int page=0;page<8;++page) { window.setPage(page); QCoreApplication::processEvents(); save(&window,QString("%1-%2x%3-page-%4").arg(scheme==Qt::ColorScheme::Dark?"dark":"light").arg(size.width()).arg(size.height()).arg(page)); }
            for (auto* scroll:window.findChildren<QScrollArea*>()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
            window.setPage(0); QVERIFY(window.findChild<QWidget*>("homeOnboarding")->isVisible()); for (auto* card:window.findChildren<QWidget*>("dashboardCard")) { if (!card->isVisible()) continue; QVERIFY(window.rect().contains(card->mapTo(&window,card->rect().bottomRight()))); for (auto* label:card->findChildren<QLabel*>()) if (label->isVisible()) QVERIFY2(card->rect().contains(label->geometry()),qUtf8Printable(label->objectName())); }
        }
        ImportDialog dialog(&window); dialog.show(); for (const auto scheme:{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}) { UiTheme::applyAppearance(scheme); QCoreApplication::processEvents(); QCOMPARE(dialog.findChild<QLineEdit*>("subscriptionUrl")->font().pointSize(),UiTheme::BodyPoints); save(&dialog,scheme==Qt::ColorScheme::Dark?"import-dark":"import-light"); }
        UiTheme::applyAppearance(Qt::ColorScheme::Light,true); window.setPage(6); save(&window,"contrast-light"); UiTheme::applyAppearance(Qt::ColorScheme::Dark,true); save(&window,"contrast-dark");
        QSignalSpy exit(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exit.size(),1);
    }
    void websiteResponseCardsUseRealFixtureAndButtons() {
        QTemporaryDir dir; MockHttp proxy; QVERIFY(proxy.listen(QHostAddress::LocalHost,0)); proxy.handler=[](const QByteArray& request) { if (request.startsWith("CONNECT")) { HttpResponse r; r.tunnel=true; return r; } const bool blocked=request.contains("chatgpt.com"),slow=request.contains("netflix.com"); return HttpResponse{blocked?403:204,{},slow?5300:45}; };
        ProfileStore store(dir.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; settings.desiredProxy=false; settings.mixedPort=freePort(); settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; QVERIFY(store.save(settings,{}).ok);
        const auto yaml=QString("proxies:\n  - {name: 'JP 日本 测试节点', type: http, server: 127.0.0.1, port: %1}\nproxy-groups:\n  - {name: 默认节点, type: select, proxies: ['JP 日本 测试节点', DIRECT]}\nrules: [\"MATCH,DIRECT\"]\n").arg(proxy.serverPort()).toUtf8(); const auto path=dir.path()+"/source.yaml"; QVERIFY(writeAtomic(path,yaml).ok);
        auto targets=WebsiteTests::defaults(); for (auto& target:targets) target.url.replace("https://","http://");
        AppController controller(dir.path(),true,nullptr,{},targets); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished),delays(&controller,&AppController::nodeDelay); controller.initialize(); QTRY_COMPARE(init.size(),1); controller.importFile("本地渲染夹具",path); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); controller.activateProfile(controller.profiles().first().id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        MainWindow window(&controller); window.resize(1180,810); window.show(); window.setPage(7); auto* start=window.findChild<QPushButton*>("startWebsiteTests"); QTRY_VERIFY(start->isEnabled()); QVERIFY(!start->icon().isNull()); const auto actionRect=start->geometry(); QTest::mouseClick(start,Qt::LeftButton); QVERIFY(controller.websiteTests()->running()); QVERIFY(!controller.state().busy); QCOMPARE(start->text(),QString("停止测试")); QVERIFY(start->isEnabled()); QCoreApplication::processEvents(); QCOMPARE(start->geometry(),actionRect);
        UiTheme::applyAppearance(Qt::ColorScheme::Dark); QVERIFY(controller.websiteTests()->running()); QVERIFY(!controller.state().busy);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.websiteTests()->running(),8000); QCOMPARE(controller.websiteTests()->result("chatgpt").status,WebsiteResult::Responded); QCOMPARE(controller.websiteTests()->result("netflix").status,WebsiteResult::Failed); QCOMPARE(delays.size(),0); QVERIFY(!window.findChild<QLabel*>("lastWebsiteTest")->text().isEmpty());
        for (const auto scheme:{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}) { UiTheme::applyAppearance(scheme); QCoreApplication::processEvents(); save(&window,scheme==Qt::ColorScheme::Dark?"website-results-dark":"website-results-light"); }
        QPushButton* single=nullptr; for (auto* button:window.findChildren<QPushButton*>("testWebsite")) if (button->property("targetId").toString()=="google") single=button; QVERIFY(single); const auto other=controller.websiteTests()->result("github").measured; QTest::mouseClick(single,Qt::LeftButton); QTRY_VERIFY(!controller.websiteTests()->running()); QCOMPARE(controller.websiteTests()->result("github").measured,other);
        QTest::mouseClick(window.findChild<QPushButton*>("clearWebsiteTests"),Qt::LeftButton); QVERIFY(controller.websiteTests()->node().isEmpty());
        window.setPage(2); QPushButton* mode=nullptr; for (auto* button:window.findChildren<QPushButton*>()) if (button->property("mode").toString()=="global") mode=button; QVERIFY(mode && mode->isEnabled());
        mode->clearFocus(); mode->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); const auto mouse=mode->grab().toImage(); mode->clearFocus(); mode->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY(mode->property("keyboardFocus").toBool()); QVERIFY(mode->grab().toImage()!=mouse);
        QSignalSpy exit(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exit.size(),1);
    }
};
QTEST_MAIN(AppearanceTests)
#include "AppearanceTests.moc"
