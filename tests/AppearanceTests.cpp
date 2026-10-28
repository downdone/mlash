#include "TestPorts.h"
#include "ui/MainWindow.h"
#include "ui/UiTheme.h"
#include "ui/UiIcons.h"
#include "ui/WidgetStyle.h"
#include "ui/ImportDialog.h"
#include "ui/GuiHost.h"
#include "ui/HotkeysDialog.h"
#include "ui/WebsiteTargetsDialog.h"
#include "ui/HomeSummaryDialog.h"
#include "ui/TunDialog.h"
#include "ui/CoreUpdateDialog.h"
#include "ui/GeoDataDialog.h"
#include "ui/BackupDialog.h"
#include "ui/LocalBackupDialog.h"
#include "ui/RemoteBackupDialog.h"
#include "ui/ProxyBypassDialog.h"
#include "ui/ProtocolSettingsDialog.h"
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
#include <QTableView>
#include <QTableWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTabBar>
#include <QTabWidget>
#include <QListWidget>

using namespace mlash;
namespace {
// UI-only fixtures never call the host platform or start a core.
class ReviewPlatform final : public PlatformServices {
public:
    int writes=0;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("UI fixture prohibits OS writes"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("UI fixture prohibits autostart writes"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return Result::fail("UI fixture prohibits child processes"); }
    Result ownsTcpPort(qint64,int) override { return Result::fail("No core in UI fixture"); }
};
double luminance(QColor color) { const auto channel=[](double x) { return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4); }; return .2126*channel(color.redF())+.7152*channel(color.greenF())+.0722*channel(color.blueF()); }
double contrast(const char* foreground,const char* background) { const double a=luminance(QColor(foreground)),b=luminance(QColor(background)); return (qMax(a,b)+.05)/(qMin(a,b)+.05); }
int freePort() { return isolatedListenerPort(); }
void save(QWidget* widget,const QString& name) { const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(widget->grab().save(output+"/"+name+".png")); } }
}
class AppearanceTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WidgetStyle::install(); }
    void cleanup() { UiTheme::setPreferences("system",100); UiTheme::applyAppearance(Qt::ColorScheme::Light); }
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
        QTemporaryDir dir; AppController controller(dir.path(),true,nullptr,std::make_shared<ReviewPlatform>()); QSignalSpy init(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&controller); window.show(); window.setPage(6); auto* port=window.findChild<QSpinBox*>("mixedPort"); const int draft=port->value()+1; port->setValue(draft);
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
        QTemporaryDir dir; AppController controller(dir.path(),true,nullptr,std::make_shared<ReviewPlatform>()); QSignalSpy init(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&controller); window.setScreen(QGuiApplication::primaryScreen()); window.move(QGuiApplication::primaryScreen()->availableGeometry().topLeft()+QPoint(20,20)); window.show();
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
    void groupedSettingsAndPopulatedPagesRemainUsable() {
        QTemporaryDir dir; auto platform=std::make_shared<ReviewPlatform>();
        AppController controller(dir.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(init.size(),1);
        Profile profile; profile.id=newId(); profile.name="日常网络"; profile.sourceType="file"; profile.candidate=newId(); profile.active=profile.candidate; profile.updated="2026-10-28T00:00:00Z";
        const_cast<QVector<Profile>&>(controller.profiles()).append(profile);
        auto& state=const_cast<AppState&>(controller.state()); state.activeProfile=profile.id; state.core=CoreState::Running; state.ready=true; state.mode="rule"; state.proxy=ProxyState::Disabled;
        const QJsonArray members{"香港 · 日常","日本 · 备用","新加坡 · 工作","美国 · 长名称节点用于检查省略与完整提示","DIRECT"};
        QJsonObject proxies{{"日常选择",QJsonObject{{"type","Selector"},{"all",members},{"now",members[0]}}},{"DIRECT",QJsonObject{{"type","Direct"}}}};
        for (const auto& member:members) if (member!="DIRECT") proxies[member.toString()]=QJsonObject{{"type","Shadowsocks"}};
        const_cast<QJsonObject&>(controller.proxies())=proxies;
        MockHttp api; QVERIFY(api.listen(QHostAddress::LocalHost));
        api.handler=[proxies](const QByteArray& request) {
            if (request.contains("/connections")) return HttpResponse{200,"{\"connections\":[],\"uploadTotal\":2048,\"downloadTotal\":8192}"};
            if (request.contains("/rules")) return HttpResponse{200,"{\"rules\":[{\"type\":\"Match\",\"payload\":\"\",\"proxy\":\"日常选择\"}]}"};
            if (request.contains("/proxies")) return HttpResponse{200,QJsonDocument(QJsonObject{{"proxies",proxies}}).toJson()};
            return HttpResponse{200,"{}"};
        };
        controller.findChild<MihomoClient*>()->configure(api.serverPort(),"UI fixture");
        MainWindow window(&controller); window.show();
        auto* port=window.findChild<QSpinBox*>("mixedPort"); const int original=port->value(); port->setValue(original+1);
        auto& settings=const_cast<Settings&>(controller.settings());
        for (int scale:{100,120}) for (const auto scheme:{Qt::ColorScheme::Light,Qt::ColorScheme::Dark}) {
            settings.appearance=scheme==Qt::ColorScheme::Dark?"dark":"light"; settings.textScale=scale;
            UiTheme::setPreferences(settings.appearance,scale);
            for (const auto size:{QSize(840,560),QSize(940,620),QSize(1180,810)}) {
                window.resize(size);
                for (int page=0;page<8;++page) {
                    window.setPage(page); QCoreApplication::processEvents(); QTest::qWait(20);
                    QCOMPARE(window.size(),size);
                    save(&window,QString("filled-%1-%2-%3x%4-page-%5").arg(settings.appearance).arg(scale).arg(size.width()).arg(size.height()).arg(page));
                    if (page==2) {
                        auto* view=window.findChild<QComboBox*>("nodeViewMode"); view->setCurrentIndex(1); QCoreApplication::processEvents();
                        save(&window,QString("cards-%1-%2-%3").arg(settings.appearance).arg(scale).arg(size.width())); view->setCurrentIndex(0);
                    }
                    for (auto* scroll:window.findChildren<QScrollArea*>()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
                    // Each visible button must fit its layout parent; controls may scroll vertically.
                    for (auto* button:window.findChildren<QAbstractButton*>()) {
                        if (!button->isVisible()||button->window()!=&window||button->parentWidget()->inherits("QAbstractItemView")) continue;
                        QVERIFY2(button->parentWidget()->rect().contains(button->geometry()),qUtf8Printable(QString("page %1 %2 %3 %4 parent %5 rect %6,%7 %8x%9 in %10x%11").arg(page).arg(button->metaObject()->className()).arg(button->objectName()).arg(button->text()).arg(button->parentWidget()->metaObject()->className()).arg(button->x()).arg(button->y()).arg(button->width()).arg(button->height()).arg(button->parentWidget()->width()).arg(button->parentWidget()->height())));
                    }
                }
                window.setPage(6); auto* scroll=window.findChild<QScrollArea*>("settingsScroll");
                auto* categories=window.findChild<QTabBar*>("settingsCategories");
                const QList<QFrame*> sections{window.findChild<QFrame*>("settingsApplication"),window.findChild<QFrame*>("settingsCore"),window.findChild<QFrame*>("settingsTools")};
                for (int category=0;category<sections.size();++category) {
                    categories->setCurrentIndex(category); QCoreApplication::processEvents();
                    for (int i=0;i<sections.size();++i) QCOMPARE(sections[i]->isVisible(),i==category);
                    QVERIFY(window.findChild<QWidget*>("settingsSurface")->width()<=760);
                    QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
                    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); QCoreApplication::processEvents();
                    QVERIFY(window.findChild<QPushButton*>("applySettings")->isVisible());
                    save(&window,QString("settings-category-%1-%2-%3-%4").arg(category).arg(settings.appearance).arg(scale).arg(size.width()));
                }
                categories->setCurrentIndex(0); scroll->verticalScrollBar()->setValue(0); QCOMPARE(port->value(),original+1);
            }
        }
        // Search in the scrolling catalog must reveal its row without applying the draft.
        window.setPage(6); auto* search=window.findChild<QLineEdit*>("settingsSearch"); search->setText("全局快捷键"); QTest::keyClick(search,Qt::Key_Return); QCoreApplication::processEvents();
        auto* control=window.findChild<QPushButton*>("configureHotkeys"); auto* scroll=window.findChild<QScrollArea*>("settingsScroll");
        QVERIFY(scroll->viewport()->rect().intersects(QRect(control->mapTo(scroll->viewport(),QPoint()),control->size())));
        QCOMPARE(port->value(),original+1); QCOMPARE(platform->writes,0);
        window.activateWindow(); QApplication::setActiveWindow(&window); window.setPage(2);
        auto* sort=window.findChild<QComboBox*>("memberSort"); sort->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY(sort->property("keyboardFocus").toBool());
        sort->clearFocus(); sort->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QVERIFY(!sort->property("keyboardFocus").toBool());
        auto* submit=window.findChild<QPushButton*>("applySettings"); window.setPage(6); submit->setEnabled(true); const auto released=submit->grab().toImage();
        QTest::mousePress(submit,Qt::LeftButton); QVERIFY(submit->grab().toImage()!=released); QTest::mouseRelease(submit,Qt::LeftButton,Qt::NoModifier,QPoint(-2,-2));
        const auto area=window.centralWidget()->geometry(); controller.operationFinished(false,"验证错误："+QString(2048,QChar(0x6d4b))); QCoreApplication::processEvents(); QCOMPARE(window.centralWidget()->geometry(),area);
        window.hide(); QVERIFY(!controller.connectionsVisible()); QVERIFY(controller.connections().isEmpty());
        state.core=CoreState::Stopped; state.ready=false;
        { GuiHost host(&controller,true); host.show(2); QTRY_VERIFY(host.window()); QPointer<MainWindow> old=host.window(); old->close(); QTRY_VERIFY(old.isNull()); host.show(2); QTRY_VERIFY(host.window()); QCOMPARE(host.window()->page(),2); host.dispose(); }
        QCOMPARE(platform->writes,0); settings.appearance="system"; settings.textScale=100; UiTheme::setPreferences("system",100);
    }
    void toolWindowsKeepActionsAndDraftsAccessible() {
        QTemporaryDir dir; auto platform=std::make_shared<ReviewPlatform>();
        AppController controller(dir.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(init.size(),1);
        for (int scale:{100,120}) for (const auto& appearance:QStringList{"light","dark"}) {
            UiTheme::setPreferences(appearance,scale);
            QList<QDialog*> dialogs{new ImportDialog,new HotkeysDialog(&controller),new WebsiteTargetsDialog(&controller),new HomeSummaryDialog(&controller),new TunDialog(&controller),new CoreUpdateDialog(&controller),new GeoDataDialog(&controller),new BackupDialog(&controller,false),new BackupDialog(&controller,true),new LocalBackupDialog(&controller),new RemoteBackupDialog(&controller),new ProxyBypassDialog(&controller),new ProtocolSettingsDialog(&controller)};
            for (auto* dialog:dialogs) {
                dialog->setAttribute(Qt::WA_DeleteOnClose,false); dialog->show(); QCoreApplication::processEvents(); QTest::qWait(20);
                if (auto* import=qobject_cast<ImportDialog*>(dialog)) {
                    auto* source=import->findChild<QTabWidget*>("importTabs"); auto* scroll=import->findChild<QScrollArea*>("importScroll");
                    for (int tab:{0,1,2,0}) { source->setCurrentIndex(tab); QCoreApplication::processEvents(); }
                    auto* name=import->findChild<QLineEdit*>("subscriptionName"); const QRect field(name->mapTo(scroll->viewport(),QPoint()),name->size()); QVERIFY2(scroll->viewport()->rect().contains(field),"URL import name must be visible without scrolling");
                }
                save(dialog,QString("tool-%1-%2-%3").arg(appearance).arg(scale).arg(dialog->objectName()));
                for (auto* scroll:dialog->findChildren<QScrollArea*>()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
                for (auto* box:dialog->findChildren<QDialogButtonBox*>()) for (auto* button:box->buttons()) if (button->isVisible()) {
                    QVERIFY2(box->rect().contains(button->geometry()),qUtf8Printable(dialog->objectName()+" "+button->text()));
                    QVERIFY2(dialog->rect().contains(button->mapTo(dialog,button->rect().bottomRight())),qUtf8Printable(dialog->objectName()+" "+button->text()));
                }
                dialog->hide(); delete dialog;
            }
        }
        QCOMPARE(platform->writes,0); UiTheme::setPreferences("system",100);
    }
    void quietDefaultsKeepErrorsDraftsAndHelp() {
        QTemporaryDir dir; auto platform=std::make_shared<ReviewPlatform>();
        AppController controller(dir.path(),true,nullptr,platform); QSignalSpy initialized(&controller,&AppController::initialized),operations(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&controller); window.show(); QCoreApplication::processEvents();
        auto* message=window.findChild<QLabel*>("operationMessage"); auto* details=window.findChild<QToolButton*>("statusDetails");
        QVERIFY(message->text().isEmpty()); QVERIFY(!details->isVisible()); QVERIFY(!window.findChild<QFrame*>("overview")->isVisible());
        window.setPage(6); auto* draft=window.findChild<QLabel*>("settingsDraftHint"); auto* port=window.findChild<QSpinBox*>("mixedPort"); QVERIFY(draft->text().isEmpty());
        port->setValue(port->value()+1); QVERIFY(draft->text().contains("待应用"));
        const auto geometry=window.centralWidget()->geometry();
        controller.operationStarted("正在导入…"); QVERIFY(details->isVisible());
        const QString error="配置校验失败 · "+QString(1024,QChar(0x6d4b)); controller.operationFinished(false,error);
        QCOMPARE(message->toolTip(),error); QVERIFY(message->text().contains("校验失败")); QVERIFY(details->isVisible());
        window.setPage(5); QCoreApplication::processEvents(); QVERIFY(message->text().contains("校验失败"));
        controller.operationFinished(true,"已完成"); QTRY_VERIFY_WITH_TIMEOUT(message->text().isEmpty(),4500); QVERIFY(!details->isVisible()); QCOMPARE(window.centralWidget()->geometry(),geometry);
        window.setPage(6); QVERIFY(draft->text().contains("待应用"));
        auto* dialog=new ProtocolSettingsDialog(&controller,&window); dialog->show(); QCoreApplication::processEvents();
        auto* help=dialog->findChild<QToolButton*>("contextHelp"); QVERIFY(help); const int before=operations.size();
        help->setFocus(Qt::TabFocusReason); QTest::keyClick(help,Qt::Key_Space);
        auto* explanation=dialog->findChild<QMessageBox*>("contextHelpDialog"); QTRY_VERIFY(explanation&&explanation->isVisible()); QVERIFY(explanation->text().contains("确认后才下载"));
        explanation->done(QMessageBox::Close); dialog->hide(); delete dialog; QCOMPARE(operations.size(),before); QCOMPARE(platform->writes,0);
    }
    void taskEntryPointsAndSettingsStayCoherent() {
        QTemporaryDir dir; auto platform=std::make_shared<ReviewPlatform>(); AppController controller(dir.path(),true,nullptr,platform);
        QSignalSpy initialized(&controller,&AppController::initialized),operations(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show();
        auto* navigation=window.findChild<QListWidget*>("navigation"); auto* section=window.findChild<QTabBar*>("sectionNavigation"); QCOMPARE(navigation->count(),4);
        for (int row=0;row<4;++row) QCOMPARE(navigation->item(row)->text(),QStringList({"首页","配置","数据","设置"})[row]);
        section->setCurrentIndex(1); QCOMPARE(window.page(),2); QCOMPARE(navigation->currentRow(),0);
        navigation->setCurrentRow(1); QCOMPARE(window.page(),1); section->setCurrentIndex(1); QCOMPARE(window.page(),3); QCOMPARE(navigation->currentRow(),1);
        navigation->setCurrentRow(2); QCOMPARE(window.page(),4); section->setCurrentIndex(1); QCOMPARE(window.page(),5);
        navigation->setCurrentRow(3); QCOMPARE(window.page(),6); QVERIFY(!section->isVisible()); navigation->setCurrentRow(0); QCOMPARE(window.page(),2); window.setPage(0);
        auto* next=window.findChild<QPushButton*>("homeNextStep"); QVERIFY(next->isVisible()); QVERIFY(!window.findChild<QToolButton*>("homeCoreControl")->isVisible());
        QTest::mouseClick(next,Qt::LeftButton); QTRY_VERIFY(window.findChild<ImportDialog*>()); auto* import=window.findChild<ImportDialog*>(); import->reject(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QCOMPARE(operations.size(),0); QVERIFY(!window.findChild<QPushButton*>("nodePreferences")->isEnabled()); QVERIFY(!window.findChild<QPushButton*>("ruleProviders")->isEnabled());
        auto& state=const_cast<AppState&>(controller.state()); state.busy=true; controller.stateChanged(); QVERIFY(!window.findChild<QAction*>("exportDiagnostics")->isEnabled()); state.busy=false; controller.stateChanged(); QVERIFY(window.findChild<QAction*>("exportDiagnostics")->isEnabled());
        for (const auto& route:QList<QPair<int,QString>>{{2,"nodeNextStep"},{4,"connectionNextStep"},{7,"websiteNextStep"}}) { window.setPage(route.first); auto* action=window.findChild<QPushButton*>(route.second); QVERIFY(action->isVisible()); QTest::mouseClick(action,Qt::LeftButton); QCOMPARE(window.page(),route.first==2?1:0); }
        window.setPage(6); auto* categories=window.findChild<QTabBar*>("settingsCategories"); auto* port=window.findChild<QSpinBox*>("mixedPort"); const int original=port->value(); categories->setCurrentIndex(1); port->setValue(original+1); categories->setCurrentIndex(2); categories->setCurrentIndex(0);
        auto* search=window.findChild<QLineEdit*>("settingsSearch"); search->setText("本地端口"); QTest::keyClick(search,Qt::Key_Return); QTRY_COMPARE(categories->currentIndex(),1); QCoreApplication::processEvents(); QVERIFY(port->isVisible()); QCOMPARE(port->value(),original+1);
        search->setText("WebDAV"); QTest::keyClick(search,Qt::Key_Return); QTRY_COMPARE(categories->currentIndex(),2); QVERIFY(window.findChild<QPushButton*>("configureRemoteBackup")->isVisible()); QCOMPARE(port->value(),original+1);
        search->setText("DNS"); QTest::keyClick(search,Qt::Key_Return); QCOMPARE(window.page(),1);
        QVERIFY(!window.findChild<QPushButton*>("configureDns")); QVERIFY(!window.findChild<QPushButton*>("openOutboundFromSettings")); QVERIFY(!window.findChild<QPushButton*>("proxyManagement"));
        Profile first; first.id=newId(); first.name="订阅甲"; first.candidate=newId(); Profile second=first; second.id=newId(); second.name="订阅乙"; const_cast<QVector<Profile>&>(controller.profiles())={first,second}; controller.profilesChanged();
        auto* profiles=window.findChild<QTableWidget*>("profilesTable"); profiles->selectAll(); QVERIFY(!window.findChild<QPushButton*>("profileEdit")->isEnabled()); QVERIFY(!window.findChild<QPushButton*>("profileChanges")->isEnabled()); QVERIFY(!window.findChild<QPushButton*>("activateProfile")->isEnabled()); profiles->clearSelection(); profiles->selectRow(0); QVERIFY(window.findChild<QPushButton*>("profileEdit")->isEnabled());
        window.findChild<QPushButton*>("addSubscription")->click(); QTRY_VERIFY(window.findChild<ImportDialog*>()); import=window.findChild<ImportDialog*>(); Profile added=first; added.id=newId(); added.name="新增订阅"; const_cast<QVector<Profile>&>(controller.profiles()).append(added); controller.profilesChanged(); import->done(QDialog::Accepted); QCOMPARE(window.page(),1); QCOMPARE(profiles->item(profiles->currentRow(),0)->data(Qt::UserRole).toString(),added.id);
        QCOMPARE(operations.size(),0); QCOMPARE(platform->writes,0);
    }
    void ruleListAndHomeSelectionRequireExplicitApply() {
        QTemporaryDir dir; auto platform=std::make_shared<ReviewPlatform>(); ProfileStore store(dir.path(),platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore=false; settings.desiredProxy=false; Profile profile; profile.id=newId(); profile.name="任务测试"; profile.sourceType="file";
        auto prepared=store.prepare(profile,"proxies: [{name: Node A, type: http, server: 127.0.0.1, port: 1080}, {name: Node B, type: http, server: 127.0.0.1, port: 1081}]\nproxy-groups: [{name: Test, type: select, proxies: [Node A, Node B]}]\nrules: ['MATCH,Test']\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); prepared.profile.active=prepared.profile.candidate; settings.activeProfile=profile.id; prepared.profile.customRules={"DOMAIN-SUFFIX,old.example,DIRECT"}; QVERIFY(store.save(settings,{prepared.profile}).ok);
        AppController controller(dir.path(),true,nullptr,platform); QSignalSpy initialized(&controller,&AppController::initialized),operations(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.core=CoreState::Running; state.ready=true; state.activeProfile=profile.id; state.mode="rule";
        QJsonObject proxies{{"Test",QJsonObject{{"type","Selector"},{"all",QJsonArray{"Node A","Node B"}},{"now","Node A"}}},{"Node A",QJsonObject{{"type","Http"}}},{"Node B",QJsonObject{{"type","Http"}}}}; const_cast<QJsonObject&>(controller.proxies())=proxies;
        MockHttp api; QVERIFY(api.listen(QHostAddress::LocalHost)); int writes=0; api.handler=[&](const QByteArray& request) { if (request.startsWith("PUT /proxies/")) { ++writes; auto group=proxies["Test"].toObject(); group["now"]=QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n")+4)).object().value("name"); proxies["Test"]=group; } if (request.startsWith("GET /proxies/Test ")) return HttpResponse{200,QJsonDocument(proxies["Test"].toObject()).toJson()}; if (request.contains("/proxies")) return HttpResponse{200,QJsonDocument(QJsonObject{{"proxies",proxies}}).toJson()}; return HttpResponse{200,"{}"}; }; controller.findChild<MihomoClient*>()->configure(api.serverPort(),"UI fixture");
        MainWindow window(&controller); window.show(); window.setPage(0); auto* member=window.findChild<QComboBox*>("homeQuickMember"); QTRY_COMPARE(member->count(),2); member->setCurrentIndex(member->findData("Node B")); member->activated(member->currentIndex());
        QCOMPARE(writes,0); QCOMPARE(operations.size(),0); QCOMPARE(controller.proxies()["Test"].toObject()["now"].toString(),QString("Node A")); auto* change=window.findChild<QPushButton*>("homeSwitchNode"); QVERIFY(change->isEnabled()); QTest::mouseClick(change,Qt::LeftButton); QTRY_COMPARE(writes,1); QTRY_COMPARE(operations.size(),1); QVERIFY(operations.last()[0].toBool()); QCOMPARE(controller.proxies()["Test"].toObject()["now"].toString(),QString("Node B"));
        window.setPage(3); auto* text=window.findChild<QPlainTextEdit*>("customRulesEditor"); auto* list=window.findChild<QTableWidget*>("customRulesList"); QTRY_COMPARE(list->rowCount(),1); QVERIFY(list->isVisible()); QVERIFY(!text->isVisible());
        auto* match=window.findChild<QLineEdit*>("ruleMatch"); auto* type=window.findChild<QComboBox*>("ruleType"); type->setCurrentIndex(type->findData("DOMAIN-SUFFIX")); match->setText("new.example"); QTest::mouseClick(window.findChild<QPushButton*>("addCustomRule"),Qt::LeftButton); QCOMPARE(list->rowCount(),2); QVERIFY(text->toPlainText().contains("new.example"));
        list->selectRow(1); QTest::mouseClick(window.findChild<QPushButton*>("moveQuickRuleUp"),Qt::LeftButton); QVERIFY(text->toPlainText().startsWith("DOMAIN-SUFFIX,new.example")); QTest::mouseClick(window.findChild<QPushButton*>("removeQuickRule"),Qt::LeftButton); QCOMPARE(list->rowCount(),1); QVERIFY(!text->toPlainText().contains("new.example"));
        auto* advanced=window.findChild<QToolButton*>("ruleTextMode"); QTest::mouseClick(advanced,Qt::LeftButton); QVERIFY(text->isVisible()); text->setPlainText("DOMAIN-SUFFIX,draft.example,DIRECT"); QTest::mouseClick(advanced,Qt::LeftButton); QCOMPARE(list->item(0,1)->text(),QString("draft.example")); QCOMPARE(controller.profiles().first().customRules,QStringList{"DOMAIN-SUFFIX,old.example,DIRECT"});
        window.setPage(1); window.setPage(3); QCOMPARE(list->item(0,1)->text(),QString("draft.example")); QTest::mouseClick(window.findChild<QPushButton*>("resetCustomRules"),Qt::LeftButton); QCOMPARE(list->item(0,1)->text(),QString("old.example"));
        text->setPlainText("IP-CIDR,192.0.2.0/24,DIRECT,no-resolve"); QVERIFY(!list->isColumnHidden(3)); QCOMPARE(list->item(0,3)->text(),QString("no-resolve"));
        QStringList tooMany; for (int i=0;i<257;++i) tooMany.append(QString("DOMAIN-SUFFIX,site%1.example,DIRECT").arg(i)); text->setPlainText(tooMany.join('\n')); QCOMPARE(list->rowCount(),256); QCOMPARE(text->toPlainText().split('\n').size(),257); QVERIFY(!window.findChild<QPushButton*>("saveCustomRules")->isEnabled());
        QTest::mouseClick(window.findChild<QPushButton*>("resetCustomRules"),Qt::LeftButton);
        auto automatic=controller.proxies()["Test"].toObject(); automatic["type"]="URLTest"; automatic["fixed"]=""; const_cast<QJsonObject&>(controller.proxies())["Test"]=automatic; controller.proxiesChanged(); window.setPage(2);
        QCOMPARE(window.findChild<QPushButton*>("chooseMember")->text(),QString("固定节点")); auto* restore=window.findChild<QPushButton*>("automaticAction"); QVERIFY(!restore->isEnabled()); QCOMPARE(restore->text(),QString("自动选择中"));
        automatic["fixed"]="Node B"; const_cast<QJsonObject&>(controller.proxies())["Test"]=automatic; controller.proxiesChanged(); QVERIFY(restore->isEnabled()); QCOMPARE(restore->text(),QString("恢复自动选择")); QCOMPARE(writes,1); QCOMPARE(platform->writes,0);
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
