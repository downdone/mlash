#include "TestPorts.h"
#include "ui/MainWindow.h"
#include "ui/ImportDialog.h"
#include "ui/NodePresentation.h"
#include <QtTest>
#include <QToolButton>
#include <QListWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QInputDialog>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QClipboard>
#include <QMenu>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTableWidget>
#include <QTableView>
#include <QComboBox>
#include <QAction>
#include <QDir>
#include <QTcpServer>
#include <QRandomGenerator>
#include <QPlainTextEdit>
#include <QTcpSocket>
#include "ui/GuiHost.h"
#include "ui/TrayController.h"
#include <QSpinBox>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QJsonDocument>
#include <QPointer>
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "MockHttp.h"
#include "ui/WidgetStyle.h"
#include "ui/UiTheme.h"
#include <QScreen>
#include <QVBoxLayout>
#include <QCryptographicHash>

using namespace mlash;
static bool seedDashboardFixture(const QString& root) {
    auto platform=createPlatformServices(); ProfileStore store(root,platform); if (!store.load().result.ok) return false;
    Settings settings; settings.startCore=false;
    Profile profile; profile.id=newId(); profile.name="Dashboard fixture"; profile.sourceType="file";
    auto prepared=store.prepare(profile,"proxies: []\nrules: ['MATCH,DIRECT']\n",settings,19090,QString(64,'a'));
    if (!prepared.result.ok) return false;
    profile=prepared.profile; profile.active=profile.candidate; settings.activeProfile=profile.id;
    return store.save(settings,{profile}).ok;
}
class UiTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Match the production QApplication font, including item delegate fallback.
        QApplication::setFont(QFont("Microsoft YaHei UI",9));
        WidgetStyle::install();
    }
    void popupSurfacesWithoutMainWindow() {
        for (auto effect:{Qt::UI_AnimateCombo,Qt::UI_AnimateMenu,Qt::UI_FadeMenu}) QApplication::setEffectEnabled(effect,true);
        WidgetStyle::install(); WidgetStyle::install();
        for (auto effect:{Qt::UI_AnimateCombo,Qt::UI_AnimateMenu,Qt::UI_FadeMenu}) QVERIFY(!QApplication::isEffectEnabled(effect));
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        TrayController tray(&controller); auto* root=tray.findChild<QSystemTrayIcon*>()->contextMenu(); root->popup(QPoint(200,100)); QTRY_VERIFY(root->isVisible()); QCOMPARE(QApplication::style()->objectName(),QString("mlash-refined")); QVERIFY(root->testAttribute(Qt::WA_TranslucentBackground)); QCOMPARE(root->grab().toImage().pixelColor(0,0).alpha(),0);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(root->grab().save(output+"/tray-popup.png")); } root->hide();
        QDialog dialog; dialog.move(80,80); auto* layout=new QVBoxLayout(&dialog); auto* combo=new QComboBox; QPointer<QAbstractItemView> originalView=combo->view(); auto* originalSelection=originalView->selectionModel(); WidgetStyle::prepareCombo(combo); QVERIFY(originalView); QCOMPARE(combo->view(),originalView.data()); QCOMPARE(combo->view()->selectionModel(),originalSelection); combo->addItem("规则模式","rule"); combo->addItem("全局模式","global"); combo->addItem("直连模式","direct"); combo->setFixedWidth(70); layout->addWidget(combo); dialog.resize(260,90); dialog.show(); QApplication::setActiveWindow(&dialog); QSignalSpy activated(combo,&QComboBox::activated);
        combo->clearFocus(); combo->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QVERIFY(!combo->property("keyboardFocus").toBool());
        combo->clearFocus(); combo->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY(combo->property("keyboardFocus").toBool());
        combo->showPopup(); QTRY_VERIFY(combo->view()->isVisible()); auto* popup=combo->view()->window(); QVERIFY(popup->geometry().top()>=combo->mapToGlobal(combo->rect().bottomLeft()).y()-2); QVERIFY(popup->width()>=QFontMetrics(combo->font()).horizontalAdvance(combo->itemText(0))+42); QCOMPARE(popup->grab().toImage().pixelColor(0,0).alpha(),0); const auto popupImage=popup->grab().toImage(); QCOMPARE(popupImage.pixelColor(popupImage.width()/2,popupImage.height()/2).alpha(),255);
        const auto initialGeometry=popup->geometry();
        for (int ms:{0,16,60,160}) { if (ms) QTest::qWait(ms); QCOMPARE(popup->geometry(),initialGeometry); QCOMPARE(popup->grab().toImage().pixelColor(0,0).alpha(),0); if (!output.isEmpty()) QVERIFY(popup->grab().save(output+QString("/combo-frame-%1.png").arg(ms))); }
        if (!output.isEmpty()) QVERIFY(popup->grab().save(output+"/combo-popup.png")); QTest::keyClick(combo->view(),Qt::Key_Down); if (!output.isEmpty()) QVERIFY(popup->grab().save(output+"/combo-keyboard.png")); QTest::keyClick(combo->view(),Qt::Key_Return); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(combo->currentData().toString(),QString("global")); QCOMPARE(activated.size(),1);
        combo->showPopup(); QTest::keyClick(combo->view(),Qt::Key_Escape); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(combo->currentData().toString(),QString("global")); QCOMPARE(activated.size(),1);
        combo->setEditable(true); for (int i=0;i<40;++i) combo->addItem(QString("节点 %1").arg(i),QString("n%1").arg(i)); combo->setEditText("自定义组"); combo->showPopup(); QVERIFY(combo->view()->window()->height()<400); QTest::keyClick(combo->view(),Qt::Key_Escape); QCOMPARE(combo->currentText(),QString("自定义组")); combo->showPopup(); QTest::keyClick(combo->view(),Qt::Key_End); QTest::keyClick(combo->view(),Qt::Key_Return); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(combo->currentData().toString(),QString("n39"));
        QMenu longMenu; QAction* last=nullptr; for (int i=0;i<500;++i) last=longMenu.addAction(QString("节点 %1").arg(i)); QSignalSpy triggered(&longMenu,&QMenu::triggered); longMenu.popup(QPoint(150,80)); QTRY_VERIFY(longMenu.isVisible()); QVERIFY2(longMenu.height()<=longMenu.screen()->availableGeometry().height(),qUtf8Printable(QString("menu %1x%2 screen %3 avail %4 scroll %5").arg(longMenu.width()).arg(longMenu.height()).arg(longMenu.screen()->geometry().height()).arg(longMenu.screen()->availableGeometry().height()).arg(longMenu.style()->styleHint(QStyle::SH_Menu_Scrollable)))); QCOMPARE(longMenu.actionGeometry(longMenu.actions().first()).left(),longMenu.actionGeometry(last).left()); QVERIFY(longMenu.width()<480);
        if (!output.isEmpty()) QVERIFY(longMenu.grab().save(output+"/long-menu.png")); QTest::keyClick(&longMenu,Qt::Key_End); QTest::keyClick(&longMenu,Qt::Key_Return); QTRY_VERIFY(!longMenu.isVisible()); QCOMPARE(triggered.size(),1); QCOMPARE(qvariant_cast<QAction*>(triggered.first()[0]),last);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void desktopPreferencesAndEnvironmentCopy() {
        QTemporaryDir root; ProfileStore store(root.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; QVERIFY(store.save(settings,{}).ok);
        AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(6); auto* startup=window.findChild<QComboBox*>("startupPage"); auto* click=window.findChild<QComboBox*>("trayClick"); auto* ipv6=window.findChild<QComboBox*>("ipv6Mode"); auto* unified=window.findChild<QComboBox*>("unifiedDelayMode"); QVERIFY(startup && click && ipv6 && unified);
        QCOMPARE(startup->currentIndex(),0); QCOMPARE(ipv6->currentData().toString(),QString("source")); startup->setCurrentIndex(2); click->setCurrentIndex(click->findData("proxy-page")); ipv6->setCurrentIndex(ipv6->findData("enabled")); unified->setCurrentIndex(unified->findData("disabled")); window.findChild<QPushButton*>("applySettings")->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); QCOMPARE(store.load().settings.startupPage,2); QCOMPARE(store.load().settings.unifiedDelayMode,QString("disabled"));
        window.findChild<QAction*>("copyProxyEnvironmentPowerShell")->trigger(); const auto powershell=QApplication::clipboard()->text(); QVERIFY(powershell.contains("$env:HTTPS_PROXY=\"http://127.0.0.1:")); QVERIFY(powershell.contains('\n')); QVERIFY(!powershell.contains("secret")); window.findChild<QAction*>("copyProxyEnvironmentCMD")->trigger(); QVERIFY(QApplication::clipboard()->text().startsWith("set \"HTTP_PROXY="));
        GuiHost fresh(&controller,true); fresh.show(); QCOMPARE(fresh.window()->page(),2); fresh.dispose();
        TrayController tray(&controller); auto* icon=tray.findChild<QSystemTrayIcon*>(); QVERIFY(icon); QSignalSpy show(&tray,&TrayController::showWindow),pages(&tray,&TrayController::showPage); icon->activated(QSystemTrayIcon::Trigger); QCOMPARE(pages.size(),1); QCOMPARE(pages.first()[0].toInt(),2);
        tray.measureMenuPreparation(); const auto menuCount=icon->contextMenu()->findChildren<QMenu*>().size(); for (int i=0;i<20;++i) tray.measureMenuPreparation(); QCOMPARE(icon->contextMenu()->findChildren<QMenu*>().size(),menuCount);
        auto desired=controller.settings(); desired.trayClick="toggle-proxy"; controller.updateSettings(desired); QTRY_COMPARE(finished.size(),2); icon->activated(QSystemTrayIcon::Trigger); icon->activated(QSystemTrayIcon::DoubleClick); QTest::qWait(QApplication::doubleClickInterval()+40); QCOMPARE(finished.size(),2); QCOMPARE(show.size(),1);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); window.resize(840,560); QVERIFY(window.grab().save(output+"/settings-enhanced.png")); }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void responsivePageGeometry() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(writeAtomic(output+"/render-context.json",QJsonDocument(QJsonObject{{"device_pixel_ratio",window.devicePixelRatioF()},{"logical_dpi",window.screen()->logicalDotsPerInch()},{"screen_scale_override",qEnvironmentVariable("QT_SCREEN_SCALE_FACTORS")},{"platform",QGuiApplication::platformName()}}).toJson()).ok); }
        for (const auto size:{QSize(840,560),QSize(940,620),QSize(1000,680),QSize(1180,810),QSize(1440,900)}) {
            window.resize(size);
            for (int page=0;page<8;++page) { window.setPage(page); QCoreApplication::processEvents(); if (page==4) { auto* connections=window.findChild<QTableView*>("connectionsTable"); QVERIFY(connections->columnWidth(0)>=150); QCOMPARE(connections->horizontalScrollBar()->maximum(),0); } if (!output.isEmpty()) QVERIFY(window.grab().save(output+QString("/size-%1x%2-page-%3.png").arg(size.width()).arg(size.height()).arg(page))); }
            auto* settings=window.findChild<QScrollArea*>("settingsScroll"); QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
            window.setPage(0); QVERIFY(window.findChild<QWidget*>("homeOnboarding")->isVisible()); QVERIFY(window.rect().contains(window.findChild<QWidget*>("homeOnboarding")->mapTo(&window,window.findChild<QWidget*>("homeOnboarding")->rect().bottomRight()))); for (auto* card:window.findChildren<QWidget*>("dashboardCard")) { if (!card->isVisible()) continue; QVERIFY(window.rect().contains(card->mapTo(&window,card->rect().bottomRight()))); for (auto* label:card->findChildren<QLabel*>()) if (label->isVisible()) QVERIFY2(card->rect().contains(label->geometry()),qUtf8Printable(label->objectName())); for (auto* combo:card->findChildren<QComboBox*>()) if (combo->isVisible()) QVERIFY2(card->rect().adjusted(0,0,0,-6).contains(combo->geometry()),qUtf8Printable(combo->objectName())); }
        }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void persistentStatusBarDoesNotMovePages() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(840,560); window.show(); auto* status=window.findChild<QStatusBar*>("operationStatusBar"); auto* message=window.findChild<QLabel*>("operationMessage"); auto* pages=window.findChild<QStackedWidget*>(); auto* details=window.findChild<QToolButton*>("statusDetails"); QVERIFY(status&&message&&pages&&details); QVERIFY(!message->wordWrap());
        const QString longMessage="连接失败 <原始文本>\n"+QString(4096,QChar(0x6D4B));
        for (int page=0;page<8;++page) {
            window.setPage(page); QCoreApplication::processEvents(); const auto central=window.centralWidget()->geometry(),pageRect=pages->geometry(),barRect=status->geometry();
            const auto stable=[&] { QCoreApplication::processEvents(); return status->isVisible()&&window.centralWidget()->geometry()==central&&pages->geometry()==pageRect&&status->geometry()==barRect; };
            controller.operationStarted("正在安全停止内核"); QVERIFY(stable()); controller.delayProgress(12,500); QVERIFY(stable());
            controller.operationFinished(false,longMessage); QVERIFY(stable()); QCOMPARE(message->toolTip(),longMessage); QVERIFY(message->text().contains("操作未完成"));
            controller.operationFinished(true,"操作完成"); QVERIFY(stable());
        }
        const auto before=window.centralWidget()->geometry(); QTRY_VERIFY_WITH_TIMEOUT(message->text().startsWith("mihomo"),4500); QCOMPARE(window.centralWidget()->geometry(),before); QVERIFY(status->isVisible());
        controller.operationFinished(false,longMessage); bool readable=false;
        QTimer::singleShot(0,&window,[&] { if (auto* dialog=window.findChild<QMessageBox*>("statusDetailsDialog")) { readable=dialog->textFormat()==Qt::PlainText&&dialog->text()==longMessage; dialog->accept(); } }); details->click(); QVERIFY(readable); QCOMPARE(window.centralWidget()->geometry(),before);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); for (int page:{0,2,6}) { window.setPage(page); QCoreApplication::processEvents(); QVERIFY(window.grab().save(output+QString("/status-page-%1.png").arg(page))); } }
    }
    void historyReadingAndDraftFeedback() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(5); auto* logs=window.findChild<QPlainTextEdit*>("logView"); auto* follow=window.findChild<QPushButton*>("followLatestLogs");
        for (int i=0;i<160;++i) controller.diagnostics()->append("info",QString("record-%1").arg(i));
        auto* scroll=logs->verticalScrollBar(); QTRY_VERIFY(scroll->maximum()>100); QCOMPARE(scroll->value(),scroll->maximum());
        scroll->setValue(20); auto cursor=logs->textCursor(); cursor.setPosition(20); cursor.setPosition(30,QTextCursor::KeepAnchor); logs->setTextCursor(cursor); scroll->setValue(20);
        const auto selection=logs->textCursor().selectedText(); const int position=scroll->value(); controller.diagnostics()->append("info","new-while-reading"); QCOMPARE(scroll->value(),position); QCOMPARE(logs->textCursor().selectedText(),selection); QVERIFY(follow->isEnabled());
        const auto raw=logs->toPlainText(); auto* wrap=window.findChild<QAction*>("wrapLogs"); QVERIFY(wrap); wrap->trigger(); QCOMPARE(logs->lineWrapMode(),QPlainTextEdit::WidgetWidth); QCOMPARE(logs->toPlainText(),raw); QCOMPARE(logs->textCursor().selectedText(),selection); wrap->trigger(); QCOMPARE(logs->lineWrapMode(),QPlainTextEdit::NoWrap); scroll->setValue(position);
        window.setPage(0); controller.diagnostics()->append("info","new-while-away"); window.setPage(5); QCOMPARE(logs->textCursor().selectedText(),selection); QCOMPARE(scroll->value(),position);
        QTest::mouseClick(follow,Qt::LeftButton); QCOMPARE(scroll->value(),scroll->maximum()); QVERIFY(!logs->textCursor().hasSelection()); controller.diagnostics()->append("info","latest"); QCOMPARE(scroll->value(),scroll->maximum());
        window.setPage(6); auto* hint=window.findChild<QLabel*>("settingsDraftHint"); auto* port=window.findChild<QSpinBox*>("mixedPort"); auto* light=window.findChild<QCheckBox*>("lightweightMode"); port->setValue(port->value()+1); light->toggle(); QCOMPARE(hint->text(),QString("有 2 项待应用"));
        window.resize(840,560); QCoreApplication::processEvents(); auto* settings=window.findChild<QScrollArea*>("settingsScroll"); QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
        window.setMinimumWidth(700); window.resize(700,560); QCoreApplication::processEvents(); QVERIFY(settings->widget()->property("singleColumn").toBool()); auto* left=window.findChild<QFrame*>("settingsApplication"); auto* right=window.findChild<QFrame*>("settingsCore"); QVERIFY(left->geometry().bottom()<right->geometry().top()); QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
        window.resize(940,620); QCoreApplication::processEvents(); QVERIFY(!settings->widget()->property("singleColumn").toBool());
        QSignalSpy finished(&controller,&AppController::operationFinished); window.findChild<QLineEdit*>("connectivityTestUrl")->setText("file:///invalid-test-address"); QTest::mouseClick(window.findChild<QPushButton*>("applySettings"),Qt::LeftButton); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(hint->text().startsWith("应用失败：")); QCOMPARE(port->value(),controller.settings().mixedPort+1);
        window.setPage(0); auto* message=window.findChild<QLabel*>("operationMessage"); QVERIFY(message->isVisible()); QVERIFY(message->text().contains("HTTP/HTTPS")); window.setPage(6); QVERIFY(message->isVisible());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void logPauseClearAndDiagnosticPrivacy() {
        QTemporaryDir data; ProfileStore store(data.path(), createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore = false;
        Profile p; p.id = newId(); p.candidate = newId(); p.name = "private-profile-name"; p.sourceType = "url"; p.source = "https://example.invalid/private-path-token?token=query-private"; QVERIFY(store.save(settings,{p}).ok);
        AppController controller(data.path(),true); QSignalSpy init(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(5); auto* logs = window.findChild<QPlainTextEdit*>("logView"); auto* level = window.findChild<QComboBox*>("logLevelFilter"); auto* pause = window.findChild<QPushButton*>("pauseLogs"); QVERIFY(logs && level && pause);
        controller.diagnostics()->setSecrets({"test-node-password"}); controller.diagnostics()->append("app",p.source+" test-node-password"); QVERIFY(!logs->toPlainText().contains("private-path-token")); QVERIFY(!logs->toPlainText().contains("test-node-password"));
        controller.diagnostics()->append("core","level=info info-only"); controller.diagnostics()->append("core","level=warning warning-only"); level->setCurrentIndex(level->findData("warning")); QVERIFY(logs->toPlainText().contains("warning-only")); QVERIFY(!logs->toPlainText().contains("info-only"));
        QTest::mouseClick(pause,Qt::LeftButton); const auto frozen = logs->toPlainText(); controller.diagnostics()->append("error","paused-error"); QCOMPARE(logs->toPlainText(),frozen); window.setPage(0); window.setPage(5); QCOMPARE(logs->toPlainText(),frozen);
        QTest::mouseClick(pause,Qt::LeftButton); level->setCurrentIndex(level->findData("error")); QVERIFY(logs->toPlainText().contains("paused-error"));
        const auto report = data.path()+"/diagnostics.json"; controller.exportDiagnostics(report); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); QByteArray exported; QVERIFY(readFile(report,exported).ok);
        for (const auto& privateValue : {"private-path-token","query-private","test-node-password","private-profile-name","source.yaml","subscription_info"}) QVERIFY(!exported.contains(privateValue)); QVERIFY(QJsonDocument::fromJson(exported).object().value("logs").isArray());
        QTRY_VERIFY(QFileInfo(data.path()+"/logs/app.log").size()>0); window.findChild<QAction*>("clearLogView")->trigger(); QVERIFY(controller.diagnostics()->lines().isEmpty()); QVERIFY(logs->toPlainText().isEmpty()); QVERIFY(QFileInfo(data.path()+"/logs/app.log").size()>0);
        window.setPage(0); window.setPage(5); QVERIFY(logs->toPlainText().isEmpty()); controller.diagnostics()->append("error","after-clear"); QVERIFY(logs->toPlainText().contains("after-clear"));
        window.setPage(6); auto* port = window.findChild<QSpinBox*>("mixedPort"); const auto draft = port->value()+1; port->setValue(draft); controller.renameProfile(p.id,"Renamed"); QTRY_COMPARE(finished.size(),2); QCOMPARE(port->value(),draft); QCOMPARE(controller.settings().mixedPort,settings.mixedPort);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void profilePolicyDialogPersistsOnlyOnSave() {
        QTemporaryDir data; ProfileStore store(data.path(), createPlatformServices()); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore = false; Profile profile; profile.id = newId(); profile.candidate = newId(); profile.name = "用量示例"; profile.sourceType = "url"; profile.source = "https://example.invalid/config";
        profile.subscriptionInfo = {{"upload","1024"},{"download","2048"},{"total","8192"},{"expire","2000000000"},{"observed_at","2026-10-13T12:00:00Z"}};
        QVERIFY(store.save(settings, {profile}).ok); AppController controller(data.path(), true); QSignalSpy init(&controller, &AppController::initialized), finished(&controller, &AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(init.size(), 1); MainWindow window(&controller); window.show(); window.setPage(1); window.findChild<QTableWidget*>("profilesTable")->selectRow(0);
        auto* action = window.findChild<QAction*>("profileUpdatePolicy"); QVERIFY(action->isEnabled()); action->trigger();
        QPointer<QDialog> dialog = window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(dialog);
        auto* automatic = dialog->findChild<QCheckBox*>("profileAutoUpdate"); auto* hours = dialog->findChild<QSpinBox*>("profileUpdateHours"); QVERIFY(!automatic->isChecked()); QVERIFY(!hours->isEnabled()); automatic->setChecked(true); hours->setValue(3);
        dialog->findChild<QComboBox*>("downloadRoute")->setCurrentIndex(1); dialog->findChild<QLineEdit*>("downloadUserAgent")->setText("Cancelled-Agent/1");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(finished.size(), 0); QVERIFY(!controller.profiles()[0].autoUpdate); QCOMPARE(controller.profiles()[0].download.route, QString("direct")); QVERIFY(controller.profiles()[0].download.userAgent.isEmpty());
        action->trigger(); dialog = window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(dialog); automatic = dialog->findChild<QCheckBox*>("profileAutoUpdate"); hours = dialog->findChild<QSpinBox*>("profileUpdateHours"); automatic->setChecked(true); hours->setValue(3);
        dialog->findChild<QComboBox*>("downloadRoute")->setCurrentIndex(1); dialog->findChild<QLineEdit*>("downloadUserAgent")->setText("Saved-Agent/1"); dialog->findChild<QSpinBox*>("downloadTimeout")->setValue(45);
        const auto screenshots = qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!screenshots.isEmpty()) { QDir().mkpath(screenshots); dialog->grab().save(screenshots+"/profile-policy.png"); }
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click(); QTRY_COMPARE(finished.size(), 1); QVERIFY(finished.last()[0].toBool()); QVERIFY(controller.profiles()[0].autoUpdate); QCOMPARE(controller.profiles()[0].updateIntervalHours, 3);
        const auto loaded = store.load(); QVERIFY(loaded.result.ok); QVERIFY(loaded.profiles[0].autoUpdate); QCOMPARE(loaded.profiles[0].subscriptionInfo, profile.subscriptionInfo); QCOMPARE(loaded.profiles[0].download.route, QString("core")); QCOMPARE(loaded.profiles[0].download.userAgent, QString("Saved-Agent/1")); QCOMPARE(loaded.profiles[0].download.timeoutSeconds, 45);
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(), 1);
    }
    void providerAndCompatibilityDialogsRespectVersions() {
        QTemporaryDir data; ProfileStore store(data.path(), createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore = false;
        Profile profile; profile.id = newId(); profile.name = "Provider"; profile.sourceType = "file"; profile.download.route = "core";
        const QByteArray source = "proxy-providers: {nodes: {type: http, url: 'https://example.invalid/nodes?token=hidden'}}\nproxy-groups: [{name: Test, type: select, use: [nodes]}]\nrules: [\"MATCH,Test\"]\n";
        auto prepared = store.prepare(profile, source, settings, 19090, QString(64, 'a')); QVERIFY(prepared.result.ok); const auto provider = prepared.result.data.value("providers").toArray()[0].toObject(); const QByteArray payload = "proxies: [{name: Sample, type: http, server: example.test, port: 443}]\n";
        QVERIFY(writeAtomic(prepared.workPath + '/' + provider.value("path").toString(), payload).ok); QVERIFY(store.recordProviderSnapshot(prepared.workPath, provider, payload).ok); QVERIFY(store.save(settings, {prepared.profile}).ok);
        AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized), finished(&controller, &AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(), 1);
        MainWindow window(&controller); window.resize(840,560); window.show(); window.setPage(1); window.findChild<QTableWidget*>("profilesTable")->selectRow(0);
        window.findChild<QAction*>("profileCompatibility")->trigger(); auto* report = window.findChild<QPlainTextEdit*>("compatibilityReport"); QVERIFY(report); QTRY_VERIFY(report->toPlainText().contains("静态兼容检查通过")); QVERIFY(!report->toPlainText().contains("token=hidden")); report->parentWidget()->close();
        window.findChild<QAction*>("profileProviders")->trigger(); QPointer<QDialog> dialog = window.findChild<QDialog*>("providersDialog"); QVERIFY(dialog); auto* table = dialog->findChild<QTableWidget*>("providersTable"); QTRY_COMPARE(table->rowCount(), 1); QCOMPARE(table->item(0,2)->text(), QString("1")); table->selectRow(0);
        auto* refresh = dialog->findChild<QPushButton*>("refreshProvider"); QTRY_VERIFY(refresh->isEnabled()); refresh->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished[0][0].toBool()); QCOMPARE(controller.profiles()[0].candidate, prepared.version);
        dialog->close(); QTRY_VERIFY(dialog.isNull()); window.findChild<QAction*>("profileProviders")->trigger(); dialog = window.findChild<QDialog*>("providersDialog"); QVERIFY(dialog); dialog->close(); QTRY_VERIFY(dialog.isNull());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void trayGroupDwellCacheAndSelection() {
        QTemporaryDir data; MockHttp proxy; QVERIFY(proxy.listen(QHostAddress::LocalHost,0)); int responseDelay=60;
        proxy.handler=[&](const QByteArray& request) { if (request.startsWith("CONNECT")) { HttpResponse response; response.tunnel=true; return response; } return HttpResponse{204,{},responseDelay}; };
        const auto requests=[&] { int count=0; for (const auto& request:proxy.requests) if (request.startsWith("HEAD")) ++count; return count; };
        ProfileStore store(data.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; settings.desiredProxy=false; settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; settings.testUrl="http://example.invalid/ping";
        settings.mixedPort=isolatedListenerPort(); QVERIFY(settings.mixedPort>0); QVERIFY(store.save(settings,{}).ok);
        QByteArray yaml="proxies:\n"; for (const auto& name:QStringList{"n1","n2","n3","n4","other"}) yaml+=QString("  - {name: %1, type: http, server: 127.0.0.1, port: %2}\n").arg(name).arg(proxy.serverPort()).toUtf8();
        yaml+="  - {name: bad, type: http, server: 127.0.0.1, port: 9}\nproxy-groups:\n  - {name: A, type: select, proxies: [n1, n2, n3, n4, bad, DIRECT, REJECT]}\n  - {name: B, type: select, proxies: [other]}\nrules: [\"MATCH,DIRECT\"]\n";
        const auto file=data.path()+"/tray.yaml"; QVERIFY(writeAtomic(file,yaml).ok); AppController controller(data.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished),delays(&controller,&AppController::nodeDelay); controller.initialize(); QTRY_COMPARE(init.size(),1); controller.importFile("Tray",file); QTRY_COMPARE(finished.size(),1); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); controller.activateProfile(controller.profiles().first().id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QTRY_VERIFY(controller.state().ready && controller.proxies().contains("A")); TrayController tray(&controller); tray.measureMenuPreparation(); auto* root=tray.findChild<QSystemTrayIcon*>()->contextMenu(); QMenu* group=nullptr; for (auto* menu:root->findChildren<QMenu*>()) if (menu->property("groupName").toString()=="A") group=menu; QVERIFY(group);
        group->popup(QPoint(100,100)); QTRY_VERIFY(group->isVisible()); group->hide(); QTest::qWait(340); QCOMPARE(requests(),0); QCOMPARE(delays.size(),0);
        const auto pid=controller.coreProcessId(); const auto member=controller.proxies().value("A").toObject().value("now").toString(); group->popup(QPoint(100,100)); QTRY_VERIFY_WITH_TIMEOUT(delays.size()==5,2500); QVERIFY(!controller.state().busy); QCOMPARE(requests(),4); QCOMPARE(controller.coreProcessId(),pid); QCOMPARE(controller.proxies().value("A").toObject().value("now").toString(),member); QCOMPARE(finished.size(),2);
        QVERIFY(controller.trayNodeDelay("n1").value("delay").toInt()>0); QVERIFY(controller.trayNodeDelay("bad").value("known").toBool()); QCOMPARE(controller.trayNodeDelay("bad").value("delay").toInt(),-1); QVERIFY(!controller.trayNodeDelay("other").value("known").toBool());
        QAction* node=nullptr; for (auto* action:group->actions()) if (action->property("memberName").toString()=="n1") node=action; QVERIFY(node && node->isEnabled()); QVERIFY(node->text().contains(" ms")); QVERIFY(!node->icon().isNull());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(group->grab().save(output+"/tray-group-delays.png")); }
        group->hide(); group->popup(QPoint(100,100)); QTest::qWait(340); QCOMPARE(requests(),4); QCOMPARE(delays.size(),5);
        auto* automatic=root->findChild<QAction*>("trayAutoTestGroups"); QVERIFY(automatic); automatic->setChecked(false); group->hide(); group->popup(QPoint(100,100)); QTest::qWait(340); QCOMPARE(requests(),4);
        responseDelay=1000; auto* retest=group->findChild<QAction*>("trayRetestGroup"); QVERIFY(retest && retest->isEnabled()); group->setActiveAction(retest); QTest::keyClick(group,Qt::Key_Return); QVERIFY(group->isVisible()); QVERIFY(!controller.state().busy); QTRY_VERIFY_WITH_TIMEOUT(requests()==7,1000); QVERIFY(controller.trayNodeDelay("n1").value("pending").toBool());
        controller.selectMember("A","REJECT"); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,3000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.proxies().value("A").toObject().value("now").toString(),QString("REJECT")); QVERIFY(!controller.trayNodeDelay("n1").value("pending").toBool()); QVERIFY(!controller.trayNodeDelay("n1").value("known").toBool()); group->hide(); QTest::qWait(100); QCOMPARE(delays.size(),5); QCOMPARE(requests(),7);
        automatic->setChecked(true); group->popup(QPoint(100,100)); QTRY_VERIFY_WITH_TIMEOUT(requests()==10,1000); group->hide(); const auto before=delays.size(); QTest::qWait(100); QCOMPARE(delays.size(),before); QVERIFY(!controller.trayNodeDelay("n1").value("pending").toBool()); QVERIFY(controller.state().ready);
        QPointer<QMenu> oldGroup=group; tray.measureMenuPreparation(); QVERIFY(oldGroup.isNull()); const auto count=root->findChildren<QMenu*>().size(); for (int i=0;i<10;++i) tray.measureMenuPreparation(); QCOMPARE(root->findChildren<QMenu*>().size(),count);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void trayAppearancePriorityAndResources() {
        const auto state = [](CoreState core, ProxyState proxy, QString mode = "rule", bool ready = true, bool exiting = false) { AppState value; value.core = core; value.proxy = proxy; value.mode = mode; value.ready = ready; value.exiting = exiting; return value; };
        const QList<QPair<AppState, QString>> cases{
            {state(CoreState::Stopped, ProxyState::Disabled, "rule", false), "idle"},
            {state(CoreState::Starting, ProxyState::Owned, "rule", false), "working"},
            {state(CoreState::Stopping, ProxyState::Owned), "working"},
            {state(CoreState::Failed, ProxyState::Unknown), "attention"},
            {state(CoreState::Running, ProxyState::Owned, "rule", false), "attention"},
            {state(CoreState::Running, ProxyState::Unknown), "checking"},
            {state(CoreState::Running, ProxyState::Owned, "unexpected"), "checking"},
            {state(CoreState::Failed, ProxyState::Owned, "rule", false, true), "working"},
            {state(CoreState::Running, ProxyState::Owned, "rule"), "rule"},
            {state(CoreState::Running, ProxyState::Owned, "global"), "global"},
            {state(CoreState::Running, ProxyState::Owned, "direct"), "direct"},
            {state(CoreState::Running, ProxyState::Disabled, "rule"), "ready-rule"},
            {state(CoreState::Running, ProxyState::Disabled, "global"), "ready-global"},
            {state(CoreState::Running, ProxyState::Disabled, "direct"), "ready-direct"},
            {state(CoreState::Running, ProxyState::External, "rule"), "external-rule"},
            {state(CoreState::Running, ProxyState::External, "global"), "external-global"},
            {state(CoreState::Running, ProxyState::External, "direct"), "external-direct"}
        };
        QHash<QString, QImage> images;
        for (const auto& item : cases) {
            QCOMPARE(TrayController::appearance(item.first), item.second);
            const auto icon = TrayController::statusIcon(item.first); QVERIFY(!icon.isNull());
            QCOMPARE(icon.cacheKey(), TrayController::statusIcon(item.first).cacheKey());
            auto busy = item.first; busy.busy = !busy.busy; QCOMPARE(TrayController::appearance(busy), item.second);
            const auto image = icon.pixmap(16, 16).toImage(); QVERIFY(!image.isNull()); QVERIFY(!icon.pixmap(24, 24).isNull()); QVERIFY(!icon.pixmap(32, 32).isNull());
            if (!images.contains(item.second)) { for (const auto& previous : images) QVERIFY(previous != image); images.insert(item.second, image); }
        }
        QCOMPARE(images.size(), 13);
        QVERIFY(TrayController::statusTooltip(state(CoreState::Running, ProxyState::Disabled, "global")).contains("系统代理未开启"));
        QVERIFY(TrayController::statusTooltip(state(CoreState::Running, ProxyState::External, "direct")).contains("其他程序管理"));
        QVERIFY(TrayController::statusTooltip(state(CoreState::Running, ProxyState::Owned)).contains("内核模式：规则"));
    }
    void lightweightLifecycleAndQuickReopen() {
        QApplication::setQuitOnLastWindowClosed(false);
        QTemporaryDir data; AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.count(), 1);
        QSignalSpy finished(&controller, &AppController::operationFinished); QSignalSpy exit(&controller, &AppController::exitReady);
        GuiHost host(&controller, true); host.show(6); auto* light = host.window()->findChild<QCheckBox*>("lightweightMode"); QVERIFY(light->isChecked());
        host.window()->setMemberView(1); QPointer<MainWindow> old = host.window(); QList<QPointer<QObject>> resources; for (auto* object : old->findChildren<QObject*>()) resources.append(object); QVERIFY(resources.size() > 100);
        old->close(); host.show(4); QTRY_VERIFY(old.isNull()); QTRY_VERIFY(host.window() && host.window()->isVisible()); QCOMPARE(host.window()->page(), 4); QCOMPARE(exit.count(), 0);
        for (const auto& resource : resources) QVERIFY(resource.isNull()); QCOMPARE(host.window()->memberView(),1);
        for (int i = 0; i < 6; ++i) { QPointer<MainWindow> previous = host.window(); previous->close(); QTRY_VERIFY(previous.isNull()); QVERIFY(!host.window()); QVERIFY(!controller.connectionsVisible()); host.show(i % 7); QVERIFY(host.window()); }
        auto settings = controller.settings(); settings.lightweight = false; controller.updateSettings(settings); QTRY_COMPARE(finished.count(), 1); QVERIFY(finished.last()[0].toBool());
        QPointer<MainWindow> retained = host.window(); retained->close(); QVERIFY(retained); QVERIFY(!retained->isVisible()); host.show(); QCOMPARE(host.window(), retained.data()); QVERIFY(retained->isVisible());
        settings.lightweight = true; controller.updateSettings(settings); QTRY_COMPARE(finished.count(), 2); QVERIFY(finished.last()[0].toBool());
        host.show(6); auto* port = retained->findChild<QSpinBox*>("mixedPort"); port->setValue(port->value() + 1);
        QTimer::singleShot(0, [] { auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if (dialog) dialog->button(QMessageBox::No)->click(); });
        retained->close(); QVERIFY(retained); QVERIFY(retained->isVisible());
        QTimer::singleShot(0, [] { auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if (dialog) dialog->button(QMessageBox::Yes)->click(); });
        retained->close(); QTRY_VERIFY(retained.isNull()); QCOMPARE(exit.count(), 0);
        ProfileStore persisted(data.path(), createPlatformServices()); const auto loaded = persisted.load(); QVERIFY(loaded.result.ok); QVERIFY(loaded.settings.lightweight); QCOMPARE(loaded.settings.mixedPort, settings.mixedPort);
        GuiHost noTray(&controller, false); noTray.show(); noTray.window()->close(); QTRY_COMPARE(exit.count(), 1);
    }
    void importValidationAndSecretVisibility() {
        ImportDialog dialog;
        auto* url = dialog.findChild<QLineEdit*>("subscriptionUrl"); auto* submit = dialog.findChild<QPushButton*>("importSubmit");
        QCOMPARE(dialog.findChild<QLabel*>("importHint")->textFormat(),Qt::PlainText);
        QCOMPARE(url->echoMode(), QLineEdit::Password); QVERIFY(!submit->isEnabled());
        url->setText("invalid"); QVERIFY(!submit->isEnabled());
        url->setText("https://user:pass@example.test/sub"); QVERIFY(!submit->isEnabled());
        url->setText("https://example.test/sub#fragment"); QVERIFY(!submit->isEnabled());
        url->setText("https://example.test/sub?token=private"); QVERIFY(submit->isEnabled());
        dialog.findChild<QLineEdit*>("subscriptionName")->setText("  我的订阅  "); QCOMPARE(dialog.profileName(), QString("我的订阅"));
        dialog.findChild<QCheckBox*>()->setChecked(true); QCOMPARE(url->echoMode(), QLineEdit::Normal);
        dialog.findChild<QLineEdit*>("downloadUserAgent")->setText("无效代理头"); QVERIFY(!submit->isEnabled());
        dialog.setSourceTab(false); QVERIFY(!dialog.isUrl()); QVERIFY(!submit->isEnabled());
        dialog.findChild<QLineEdit*>("subscriptionFile")->setText("  C:/test/config.yaml  "); QVERIFY(submit->isEnabled()); QCOMPARE(dialog.source(), QString("C:/test/config.yaml"));
        dialog.setSourceTab(true); QVERIFY(!submit->isEnabled());
    }
    void failedImportRetainsDraftUntilExplicitRetry() {
        QTemporaryDir data; auto platform=createPlatformServices(); ProfileStore store(data.path(),platform);
        QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe";
        QVERIFY(store.save(settings,{}).ok);
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        QPointer<ImportDialog> dialog=new ImportDialog; dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->bindController(&controller);
        dialog->setPrefill(data.path()+"/missing.yaml",false); dialog->show();
        auto* submit=dialog->findChild<QPushButton*>("importSubmit");
        auto* name=dialog->findChild<QLineEdit*>("subscriptionName"); name->setText("保留名称");
        submit->click(); controller.startCore();
        QVERIFY(!dialog->findChild<QTabWidget*>()->isEnabled());
        QTRY_COMPARE(finished.size(),2); QVERIFY(!finished.last()[0].toBool());
        QVERIFY(dialog); QVERIFY(dialog->isVisible()); QVERIFY(submit->isEnabled());
        QCOMPARE(name->text(),QString("保留名称")); QVERIFY(dialog->source().endsWith("missing.yaml"));
        const auto error=dialog->findChild<QLabel*>("importHint")->text(); QVERIFY(!error.isEmpty());
        emit controller.stateChanged(); QCOMPARE(dialog->findChild<QLabel*>("importHint")->text(),error);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(dialog->grab().save(output+"/import-failure-retained.png")); }

        QTest::qWait(60); QCOMPARE(finished.size(),2); QVERIFY(controller.profiles().isEmpty());
        const auto path=data.path()+"/valid.yaml";
        const auto before=dialog->size();
        const auto hintHeight=dialog->findChild<QLabel*>("importHint")->height(); const auto submitPosition=submit->mapTo(dialog,QPoint());
        const auto invalid=QString("proxies: []\nrules: ['IP-CIDR,<b>%1</b>,DIRECT']\n").arg(QString(1200,'x')).toUtf8();
        QVERIFY(writeAtomic(path,invalid).ok); dialog->setPrefill(path,false); submit->click();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,10000); QVERIFY(!finished.last()[0].toBool());
        auto* feedback=dialog->findChild<QLabel*>("importHint");
        QCOMPARE(feedback->textFormat(),Qt::PlainText); QCOMPARE(feedback->accessibleDescription(),finished.last()[1].toString());
        QVERIFY2(feedback->accessibleDescription().size()>500,qUtf8Printable(feedback->accessibleDescription())); QCOMPARE(feedback->text(),feedback->accessibleDescription()); QVERIFY(!feedback->wordWrap()); QVERIFY(feedback->fontMetrics().horizontalAdvance(feedback->text())>feedback->width());
        QCoreApplication::processEvents(); QCOMPARE(dialog->size(),before);
        QCOMPARE(feedback->height(),hintHeight); QCOMPARE(submit->mapTo(dialog,QPoint()),submitPosition);
        QHelpEvent tooltip(QEvent::ToolTip,QPoint(2,2),feedback->mapToGlobal(QPoint(2,2)));
        QApplication::sendEvent(feedback,&tooltip);
        QVERIFY(!QToolTip::text().contains("&lt;qt&gt;"));
        QVERIFY(!QToolTip::text().contains("&amp;lt;"));
        QCOMPARE(feedback->text(),feedback->accessibleDescription()); QToolTip::hideText();
        // Import validates a scalar rule with the actual pinned core.
        QVERIFY(writeAtomic(path,"proxies: []\nrules: ['MATCH,DIRECT']\n").ok);
        submit->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,10000);
        QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QTRY_VERIFY(dialog.isNull()); QCOMPARE(controller.profiles().size(),1);
        QCOMPARE(controller.profiles().first().name,QString("保留名称")); QVERIFY(!controller.state().ready);
    }
    void unchangedSnapshotsPreserveRowsAndChoices() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true; state.mode="rule";
        auto& proxies=const_cast<QJsonObject&>(controller.proxies());
        proxies={{"Test",QJsonObject{{"type","Selector"},{"all",QJsonArray{"HK 01","JP 02"}},{"now","HK 01"}}},
                 {"HK 01",QJsonObject{{"type","Http"}}},{"JP 02",QJsonObject{{"type","Http"}}}};
        MainWindow window(&controller); window.show(); window.setPage(2);
        auto* groups=window.findChild<QListWidget*>("proxyGroups");
        for (int i=0;i<groups->count();++i) if (groups->item(i)->text()=="Test") groups->setCurrentRow(i);
        auto* quick=window.findChild<QComboBox*>("homeQuickMember"); QTRY_COMPARE(quick->count(),2);
        auto* nodes=window.findChild<QTableView*>("nodesTable");
        auto* filter=qobject_cast<QSortFilterProxyModel*>(nodes->model()); QVERIFY(filter);
        auto* model=qobject_cast<ProxyTableModel*>(filter->sourceModel()); QVERIFY(model);
        nodes->selectRow(1); const QPersistentModelIndex selection=nodes->currentIndex();
        QSignalSpy rows(quick->model(),&QAbstractItemModel::rowsInserted),reset(model,&QAbstractItemModel::modelReset),changed(model,&QAbstractItemModel::dataChanged);
        for (int i=0;i<3;++i) { emit controller.proxiesChanged(); emit controller.stateChanged(); }
        QCOMPARE(rows.size(),0); QCOMPARE(reset.size(),0); QCOMPARE(changed.size(),0); QVERIFY(selection.isValid()); QCOMPARE(nodes->currentIndex(),QModelIndex(selection));
        proxies["Test"]=QJsonObject{{"type","Selector"},{"all",QJsonArray{"HK 01","JP 02"}},{"now","JP 02"}};
        emit controller.proxiesChanged(); QCOMPARE(changed.size(),2); QCOMPARE(rows.size(),0); QCOMPARE(quick->currentData().toString(),QString("JP 02"));
        auto leaf=proxies.value("JP 02").toObject(); leaf["type"]="Socks5"; proxies["JP 02"]=leaf;
        emit controller.proxiesChanged(); QCOMPARE(changed.size(),3);
        model->setDelay("JP 02",99,{},QDateTime::currentDateTimeUtc().addMSecs(-59950)); changed.clear();
        QTest::qWait(100); model->setGroup(proxies,"Test"); QCOMPARE(changed.size(),1);
        QVERIFY(model->data(model->index(1,3)).toString().contains("已过期"));
        model->setGroup(proxies,"Test"); QCOMPARE(changed.size(),1);
        window.setPage(7);
        auto* website=window.findChild<QComboBox*>("websiteNode"); QVERIFY(website);
        website->setCurrentIndex(website->findData("JP 02"));
        QSignalSpy websiteReset(website->model(),&QAbstractItemModel::modelReset),websiteRows(website->model(),&QAbstractItemModel::rowsInserted);
        website->showPopup(); QTRY_VERIFY(website->view()->isVisible());
        const QPersistentModelIndex popupIndex=website->model()->index(website->currentIndex(),0);
        for (int i=0;i<3;++i) { emit controller.proxiesChanged(); emit controller.stateChanged(); }
        QCOMPARE(websiteReset.size(),0); QCOMPARE(websiteRows.size(),0);
        QVERIFY(popupIndex.isValid()); QVERIFY(website->view()->isVisible());
        QCOMPARE(website->currentData().toString(),QString("JP 02"));
        website->hidePopup();
    }
    void emptyStateCannotIssueInvalidCommands() {
        QTemporaryDir data; AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.count(), 1);
        MainWindow window(&controller); window.show();
        QVERIFY(!window.findChild<QPushButton*>("activateProfile")->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("updateProfile")->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("chooseMember")->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("automaticAction")->isEnabled());
        QVERIFY(!window.findChild<QTableWidget*>("homeGroups")); QCOMPARE(window.findChildren<QWidget*>("dashboardCard").size(),6);
        auto* coreAction=window.findChild<QAction*>("coreAction"); QVERIFY(coreAction->isEnabled()); QCOMPARE(coreAction->text(),QString("添加订阅")); QSignalSpy noStart(&controller,&AppController::operationStarted); coreAction->trigger(); auto* import=window.findChild<QDialog*>("importDialog"); QVERIFY(import); QCOMPARE(noStart.size(),0); QVERIFY(!controller.state().ready); import->reject();
        QVERIFY(!window.findChild<QCheckBox*>("systemProxy")->isEnabled());
        QVERIFY(!window.findChild<QAction*>("rollbackProfile")->isEnabled());
        QVERIFY(!window.findChild<QAction*>("renameProfile")->isEnabled());
        QVERIFY(!window.findChild<QAction*>("deleteProfile")->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("saveCustomRules")->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("addCustomRule")->isEnabled()); QVERIFY(!window.findChild<QComboBox*>("ruleType")->isEnabled());
        for (int page = 0; page < 8; ++page) { window.setPage(page); QCoreApplication::processEvents(); }
        auto* startup = window.findChild<QCheckBox*>("startCore");
        QVERIFY(startup); const auto checked = startup->isChecked(); QTest::mouseClick(startup, Qt::LeftButton, {}, QPoint(startup->width() - 5, startup->height() / 2)); QCOMPARE(startup->isChecked(), !checked); QTest::mouseClick(startup, Qt::LeftButton, {}, QPoint(startup->width() - 5, startup->height() / 2)); QCOMPARE(startup->isChecked(), checked);
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.count(), 1);
    }
    void refinedTaskGeometryAndSemantics() {
        {
            QDialog dialog; auto* layout=new QVBoxLayout(&dialog); auto* input=new QLineEdit; auto* tool=new QPushButton("维护工具"); auto* primary=new QPushButton("保存"); primary->setProperty("primary",true); primary->setDefault(true); layout->addWidget(input); layout->addWidget(tool); layout->addWidget(primary);
            QSignalSpy tools(tool,&QPushButton::clicked),saved(primary,&QPushButton::clicked); dialog.show(); QApplication::setActiveWindow(&dialog); QVERIFY(!tool->autoDefault()); input->setFocus(); QTRY_VERIFY(input->hasFocus()); QTest::keyClick(input,Qt::Key_Return); QCOMPARE(saved.size(),1); QCOMPARE(tools.size(),0);
            tool->setFocus(Qt::TabFocusReason); QTest::keyClick(tool,Qt::Key_Return); QCOMPARE(tools.size(),1); QCOMPARE(saved.size(),1);
        }
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true; state.mode="rule";
        auto& proxies=const_cast<QJsonObject&>(controller.proxies()); QJsonArray members;
        for (int i=0;i<500;++i) { const auto name=QString("JP 日本节点 %1").arg(i,3,10,QChar('0')); members.append(name); proxies.insert(name,QJsonObject{{"type","Http"}}); }
        proxies.insert("Test",QJsonObject{{"type","Selector"},{"all",members},{"now",members.first()}});
        MainWindow window(&controller); window.resize(940,620); window.show(); window.setPage(2); window.setMemberView(1); QCoreApplication::processEvents();
        auto* mode=window.findChild<QComboBox*>("nodeViewMode"); QStyleOptionComboBox modeOption; modeOption.initFrom(mode); const auto field=mode->style()->subControlRect(QStyle::CC_ComboBox,&modeOption,QStyle::SC_ComboBoxEditField,mode); QVERIFY(mode->fontMetrics().horizontalAdvance(mode->currentText())<=field.width()-4);
        auto* cards=window.findChild<QListView*>("nodeCards"); auto* content=window.findChild<QWidget*>("nodeContent"); auto* page=content->parentWidget();
        auto* title=page->findChild<QLabel*>("pageTitle"); auto* footer=window.findChild<QLabel*>("currentMember");
        QVERIFY(title&&footer); const int span=footer->mapTo(page,QPoint()).y()-title->mapTo(page,title->rect().bottomLeft()).y();
        QVERIFY2(cards->viewport()->height()*100>=span*60,qPrintable(QString("node viewport %1 / content %2").arg(cards->viewport()->height()).arg(span)));
        QCOMPARE(cards->model()->rowCount(),500); const auto viewport=cards->viewport()->geometry();
        emit controller.delayProgress(1,500); QCoreApplication::processEvents(); QCOMPARE(cards->viewport()->geometry(),viewport);
        auto* group=window.findChild<QComboBox*>("homeQuickGroup"); QVERIFY(group); QVERIFY(!qobject_cast<QPushButton*>(group->parentWidget()));
        window.setPage(0); const auto homePage=window.page(); group->showPopup(); QTRY_VERIFY(group->view()->isVisible()); QTest::keyClick(group->view(),Qt::Key_Escape); QCOMPARE(window.page(),homePage);
        window.setPage(7); QCoreApplication::processEvents(); auto* scroll=window.findChild<QScrollArea*>("websiteScroll"); auto* surface=window.findChild<QWidget*>("websiteSurface");
        const int columns=surface->property("columns").toInt(); QVERIFY(columns>=2&&columns<=3);
        for (auto* details:window.findChildren<QToolButton*>("websiteResultDetails")) { QVERIFY(details->isVisible()); QVERIFY(details->focusPolicy()!=Qt::NoFocus); QVERIFY(details->property("quiet").toBool()); }
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); const QJsonObject geometry{{"node_viewport_height",cards->viewport()->height()},{"content_span",span},{"website_columns",columns},{"website_viewport_width",scroll->viewport()->width()},{"dpr",window.devicePixelRatioF()}}; QVERIFY(writeAtomic(output+"/refinement-geometry.json",QJsonDocument(geometry).toJson()).ok); QVERIFY(window.grab().save(output+"/refined-website.png")); window.setPage(2); QVERIFY(window.grab().save(output+"/refined-nodes.png")); }
        window.resize(840,560); QCoreApplication::processEvents(); QVERIFY(cards->viewport()->height()>=108);
        state.ready=false;
    }
    void refinedContextFitsAutomaticGroupsAndLargeText() {
        struct ResetAppearance { ~ResetAppearance() { UiTheme::setPreferences("system",100); } } resetAppearance;
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto settings=controller.settings(); settings.textScale=120; settings.appearance="light"; controller.updateSettings(settings); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.first()[0].toBool());
        const_cast<QVector<Profile>&>(controller.profiles())[0].name=QString(80,QChar(0x8BA2));
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true;
        auto& proxies=const_cast<QJsonObject&>(controller.proxies()); proxies={{"自动选择组",QJsonObject{{"type","URLTest"},{"all",QJsonArray{"香港节点","日本节点"}},{"now","香港节点"}}},{"香港节点",QJsonObject{{"type","Http"}}},{"日本节点",QJsonObject{{"type","Http"}}}};
        {
            MainWindow window(&controller); window.resize(840,560); window.show(); window.setPage(2); window.setMemberView(1); QCoreApplication::processEvents(); QCoreApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest); QCoreApplication::processEvents();
            QCOMPARE(window.size(),QSize(840,560));
            QList<QPair<QString,QRect>> rectangles;
            for (const auto& name:{"proxyProfilesCombo","groupSwitcher","automaticAction","nodeFilterDisclosure","proxyManagement","nodeSearch","memberSort","nodeViewMode","testGroupButton"}) {
                auto* control=window.findChild<QWidget*>(name); QVERIFY(control&&control->isVisible()); const QRect rect(control->mapTo(&window,QPoint()),control->size()); QVERIFY2(window.rect().contains(rect),name); rectangles.append({QString::fromUtf8(name),rect});
            }
            for (int i=0;i<rectangles.size();++i) for (int j=i+1;j<rectangles.size();++j) QVERIFY2(!rectangles[i].second.intersects(rectangles[j].second),qPrintable(rectangles[i].first+" intersects "+rectangles[j].first));
            QVERIFY(window.findChild<QListView*>("nodeCards")->viewport()->height()>=108);
            const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(window.grab().save(output+"/refined-automatic-long-120.png")); }
        }
        state.ready=false;
    }
    void fileProfileOffersSourceSyncAndReplacement() {
        QTemporaryDir data; auto platform = createPlatformServices(); ProfileStore store(data.path(), platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore = false; settings.desiredProxy = false;
        Profile p; p.id = newId(); p.name = "本地配置"; p.sourceType = "file"; p.source=data.path()+"/original.yaml"; QVERIFY(writeAtomic(p.source,"proxies: []\nproxy-groups: []\nrules: [\"MATCH,DIRECT\"]\n").ok);
        auto prepared = store.prepare(p, "proxies: []\nproxy-groups: []\nrules: [\"MATCH,DIRECT\"]\n", settings, 19090, QString(64, 'a')); QVERIFY(prepared.result.ok);
        QVERIFY(store.save(settings, {prepared.profile}).ok);
        AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.count(), 1);
        MainWindow window(&controller); window.setPage(1); window.show();
        QVERIFY(window.findChild<QPushButton*>("updateProfile")->isEnabled());
        auto* table = window.findChild<QTableWidget*>("profilesTable"); QCOMPARE(table->rowCount(), 1);
        const auto buttons = window.findChildren<QPushButton*>();
        auto* replace=window.findChild<QAction*>("replaceProfileSource"); QVERIFY(replace&&replace->isVisible()&&replace->isEnabled());
        bool foundUpdate = false;
        for (auto* button : buttons) {
            if (button->text() == "同步原文件") { QVERIFY(button->isEnabled()); foundUpdate = true; }
        }
        QVERIFY(foundUpdate);
        QVERIFY(window.findChild<QPushButton*>("updateProfile")->isVisible());
        QVERIFY(window.findChild<QToolButton*>("profilePolicyButton")->isVisible());
        QVERIFY(window.findChild<QAction*>("renameProfile")->isEnabled());
        QVERIFY(window.findChild<QAction*>("deleteProfile")->isEnabled());
        auto* add = window.findChild<QPushButton*>("addSubscription"); QVERIFY(add); QTest::mouseClick(add, Qt::LeftButton);
        auto* dialog = window.findChild<ImportDialog*>(); QTRY_VERIFY(dialog && dialog->isVisible()); dialog->setSourceTab(false); QVERIFY(!dialog->isUrl()); dialog->reject();
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.count(), 1);
    }
    void coherentActionsAndBilingualRuleTokens() {
        QTemporaryDir data; ProfileStore store(data.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        Profile profile; profile.id=newId(); profile.name="规则示例"; profile.sourceType="file"; auto prepared=store.prepare(profile,"proxies: []\nproxy-groups: []\nrules: [\"MATCH,DIRECT\"]\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings,{prepared.profile}).ok);
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.resize(840,560); QVERIFY(window.findChild<QToolButton*>("homeCoreControl")->isVisible()); QVERIFY(!window.findChild<QComboBox*>("homeQuickGroup")->isVisible()); QVERIFY(window.findChild<QWidget*>("homeOnboarding")->isVisible());
        window.setPage(3); auto* type=window.findChild<QComboBox*>("ruleType"); auto* target=window.findChild<QComboBox*>("ruleTarget"); auto* match=window.findChild<QLineEdit*>("ruleMatch"); auto* add=window.findChild<QPushButton*>("addCustomRule"); auto* editor=window.findChild<QPlainTextEdit*>("customRulesEditor");
        auto* save=window.findChild<QPushButton*>("saveCustomRules"); editor->setPlainText("DOMAIN-SUFFIX,example.com,,no-resolve"); QVERIFY(!save->isEnabled()); QVERIFY(save->toolTip().contains("第 1 行")); editor->clear(); QVERIFY(!save->isEnabled());
        QVERIFY(type->currentText().contains("域名后缀")); QVERIFY(target->currentText().contains("直连")); match->setText("example.com"); add->click(); QCOMPARE(editor->toPlainText(),QString("DOMAIN-SUFFIX,example.com,DIRECT"));
        type->setCurrentIndex(type->findData("MATCH")); QVERIFY(!match->isEnabled()); target->setCurrentIndex(target->findData("REJECT")); add->click(); QVERIFY(editor->toPlainText().endsWith("MATCH,REJECT"));
        type->setCurrentIndex(type->findData("PROCESS-NAME")); match->setText("sample.exe"); target->setEditText("自定义组"); add->click(); QVERIFY(editor->toPlainText().endsWith("PROCESS-NAME,sample.exe,自定义组"));
        QTRY_VERIFY(target->findData("GLOBAL")>=0); QVERIFY(!window.findChild<QPushButton*>("saveCustomRules")->isEnabled()); QCOMPARE(finished.size(),0); QVERIFY(!editor->isReadOnly()); QVERIFY(editor->toPlainText().contains("自定义组"));
        window.setPage(6); auto* apply=window.findChild<QPushButton*>("applySettings"); QVERIFY(!apply->isEnabled()); auto* port=window.findChild<QSpinBox*>("mixedPort"); port->setValue(port->value()+1); QVERIFY(apply->isEnabled()); QPushButton* reset=nullptr; for (auto* b:window.findChildren<QPushButton*>()) if (b->text()=="还原修改") reset=b; QVERIFY(reset); reset->click(); QCOMPARE(port->value(),settings.mixedPort); QVERIFY(!apply->isEnabled()); QCOMPARE(finished.size(),0);
        QVERIFY(apply->isVisible()); QVERIFY(window.rect().contains(apply->mapTo(&window,apply->rect().bottomRight())));
        window.setPage(4); QVERIFY(window.findChild<QToolButton*>("closeAllConnectionsButton")->isVisible()); QVERIFY(window.findChild<QToolButton*>("connectionDetailsButton")->isVisible()); QVERIFY(!window.findChild<QToolButton*>("connectionDetailsButton")->isEnabled()); QVERIFY(!window.findChild<QAction*>("defaultConnectionOrder")->isEnabled());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { window.setPage(3); QVERIFY(window.grab().save(output+"/rules-bilingual.png")); window.setPage(6); QVERIFY(window.grab().save(output+"/settings-footer.png")); }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void nodeLocationHints() {
        QCOMPARE(NodePresentation::icon("HK one",true).cacheKey(),NodePresentation::icon("香港 two").cacheKey());
        QCOMPARE(NodePresentation::icon("unknown one").cacheKey(),NodePresentation::icon("unknown two").cacheKey());
        NodePresentation::clearTransientCache();
        QCOMPARE(NodePresentation::location("Hong Kong fresh").code,QString("HK"));
        QCOMPARE(NodePresentation::location("HK 香港 中转 01").code,QString("HK"));
        QCOMPARE(NodePresentation::location("Japan Tokyo 02").code,QString("JP"));
        QCOMPARE(NodePresentation::location("🇸🇬 Singapore 03").code,QString("SG"));
        QCOMPARE(NodePresentation::location("🇯🇵 香港转发").code,QString("JP"));
        QCOMPARE(NodePresentation::location("US-01").label,QString("美国"));
        QVERIFY(NodePresentation::location("AUS route").code.isEmpty());
        QVERIFY(NodePresentation::location("VLESS unknown").code.isEmpty());
        QVERIFY(NodePresentation::location("made in nowhere").code.isEmpty());
        QVERIFY(!NodePresentation::icon("香港").isNull()); QVERIFY(!NodePresentation::icon("unknown").isNull());
        QCOMPARE(NodePresentation::iconKey("HK",true),QString("HK")); QCOMPARE(NodePresentation::iconKey("JP",true),QString("JP")); QCOMPARE(NodePresentation::iconKey("视频",true),QString("group")); QCOMPARE(NodePresentation::location("ES 马德里").code,QString("ES")); QCOMPARE(NodePresentation::location("匈牙利 01").code,QString("HU"));
        QFile manifest(QStringLiteral(MLASH_SOURCE_DIR "/resources/flags/manifest.json")); QVERIFY(manifest.open(QIODevice::ReadOnly)); const auto assets=QJsonDocument::fromJson(manifest.readAll()).object().value("files").toArray(); QCOMPARE(assets.size(),51);
        for (const auto& asset:assets) { const auto object=asset.toObject(); const auto code=object.value("code").toString(); const auto icon=NodePresentation::icon(code,true); QVERIFY(!icon.isNull()); QFile resource(":/flags/"+code+".png"); QVERIFY(resource.open(QIODevice::ReadOnly)); QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(resource.readAll(),QCryptographicHash::Sha256).toHex()),object.value("sha256").toString()); }
        QVERIFY(NodePresentation::icon("HK",true).pixmap(24,18).toImage()!=NodePresentation::icon("JP",true).pixmap(24,18).toImage());
        QCOMPARE(NodePresentation::icon("HK",true).pixmap(24,18).toImage(),NodePresentation::icon("香港节点").pixmap(24,18).toImage());
        QCOMPARE(NodePresentation::icon("视频",true).pixmap(24,18).toImage(),NodePresentation::icon("unknown",true).pixmap(24,18).toImage());
    }
    void dashboardTabsAndSettingsFocus() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(840,560); window.show(); QApplication::setActiveWindow(&window); const auto cards=window.findChildren<QWidget*>("dashboardCard"); QCOMPARE(cards.size(),6);
        window.findChild<QToolButton*>("homeDetailsDisclosure")->click();
        for (auto* card:cards) { window.setPage(0); window.findChild<QScrollArea*>("homeScroll")->ensureWidgetVisible(card); QCoreApplication::processEvents(); QVERIFY(!controller.connectionsVisible()); auto* target=card->property("page").toInt()<3?static_cast<QWidget*>(card->findChild<QPushButton*>("summaryNavigate")):card; QVERIFY(target); QTest::mouseClick(target,Qt::LeftButton); QCOMPARE(window.page(),card->property("page").toInt()); }
        window.setPage(4); auto* tabs=window.findChild<QTabBar*>("connectionView"); QVERIFY(tabs); QTest::mouseClick(tabs,Qt::LeftButton,{},tabs->tabRect(1).center()); QCOMPARE(tabs->currentIndex(),1); window.setPage(0); QVERIFY(!controller.connectionsVisible()); QVERIFY(controller.connections().isEmpty());
        window.setPage(6); auto* scroll=window.findChild<QScrollArea*>("settingsScroll"); auto* light=window.findChild<QCheckBox*>("lightweightMode"); scroll->ensureWidgetVisible(light); light->clearFocus(); light->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QVERIFY(light->hasFocus()); const auto mouse=light->grab().toImage(); light->clearFocus(); light->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); const auto keyboard=light->grab().toImage(); QVERIFY(mouse!=keyboard); const auto before=light->isChecked(); QTest::keyClick(light,Qt::Key_Space); QCOMPARE(light->isChecked(),!before);
        QTest::mouseClick(light,Qt::LeftButton,{},QPoint(light->width()-10,light->height()/2)); QCOMPARE(light->isChecked(),before); light->clearFocus(); const auto plain=light->grab().toImage(); QTest::mouseClick(light,Qt::LeftButton,{},QPoint(light->width()-10,light->height()/2)); QTest::mouseClick(light,Qt::LeftButton,{},QPoint(light->width()-10,light->height()/2)); QCOMPARE(light->grab().toImage(),plain);
        auto* apply=window.findChild<QPushButton*>("applySettings"); const auto position=apply->mapTo(&window,QPoint()); scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); QCOMPARE(apply->mapTo(&window,QPoint()),position); QVERIFY(window.rect().contains(apply->mapTo(&window,apply->rect().bottomRight()))); QVERIFY(window.findChildren<QWidget*>("settingRow").size()>=20);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(window.grab().save(output+"/settings-tools.png")); scroll->verticalScrollBar()->setValue(0); QVERIFY(window.grab().save(output+"/settings-list.png")); }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void quietMouseFocusAndTwoColumnSettings() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(840,560); window.show(); QApplication::setActiveWindow(&window);
        for (int page=0;page<8;++page) {
            window.setPage(page); QCoreApplication::processEvents();
            for (auto* widget:window.findChildren<QWidget*>()) {
                if (!widget->isVisible() || !widget->isEnabled() || widget->focusPolicy()==Qt::NoFocus || !(qobject_cast<QComboBox*>(widget) || qobject_cast<QPushButton*>(widget) || qobject_cast<QToolButton*>(widget) || qobject_cast<QLineEdit*>(widget) || qobject_cast<QSpinBox*>(widget))) continue;
                widget->clearFocus(); widget->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY2(widget->property("keyboardFocus").toBool(),qUtf8Printable(QString("page %1: %2 %3 focus %4").arg(page).arg(widget->metaObject()->className()).arg(widget->objectName()).arg(widget->hasFocus()))); widget->clearFocus(); widget->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QVERIFY(!widget->property("keyboardFocus").toBool());
            }
        }
        window.setPage(2); auto* combo=window.findChild<QComboBox*>("memberSort"); combo->clearFocus(); const auto neutral=combo->grab().toImage(); combo->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QCOMPARE(combo->grab().toImage(),neutral); combo->clearFocus(); combo->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY(combo->grab().toImage()!=neutral);
        QTest::mouseClick(combo,Qt::LeftButton); QTRY_VERIFY(combo->view()->isVisible()); QTest::qWait(100); const auto item=combo->model()->index(1,0); const auto point=combo->view()->visualRect(item).center(); QCOMPARE(combo->view()->indexAt(point),item); QMouseEvent move(QEvent::MouseMove,QPointF(point),QPointF(combo->view()->viewport()->mapToGlobal(point)),Qt::NoButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(combo->view()->viewport(),&move); QTest::mouseClick(combo->view()->viewport(),Qt::LeftButton,{},point); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(combo->currentIndex(),1); QVERIFY(!combo->property("keyboardFocus").toBool());
        window.setPage(6); auto* scroll=window.findChild<QScrollArea*>("settingsScroll"); scroll->verticalScrollBar()->setValue(0); QFrame *left=nullptr,*right=nullptr; for (auto* frame:scroll->widget()->findChildren<QFrame*>()) if (frame->property("settingsColumn").isValid()) { if (frame->property("settingsColumn").toInt()==0) left=frame; else right=frame; } QVERIFY(left && right); QCOMPARE(left->y(),right->y()); QVERIFY(left->height()>=left->minimumSizeHint().height()); QVERIFY(right->height()>=right->minimumSizeHint().height()); QVERIFY(left->geometry().right()<right->geometry().left()); QVERIFY(qAbs(left->width()-right->width())<=3); QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        window.setPage(0); const auto cards=window.findChildren<QWidget*>("dashboardCard"); QCOMPARE(cards.size(),6); QVERIFY(window.findChild<QWidget*>("homeOnboarding")->isVisible()); for (auto* card:cards) QVERIFY(!card->isVisible());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void currentGroupSelectionAndNativePreview() {
        const auto corePath = QString(MLASH_SOURCE_DIR) + "/cores/mihomo.exe";
        QVERIFY(QFile::exists(corePath)); QTemporaryDir data;
        auto platform = createPlatformServices(); ProfileStore store(data.path(), platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.corePath = corePath; settings.startCore = false; settings.desiredProxy = false;
        QTcpServer reservation;
        for (int attempt = 0; attempt < 100; ++attempt) if (reservation.listen(QHostAddress::LocalHost, QRandomGenerator::global()->bounded(20000, 40000))) break;
        QVERIFY(reservation.isListening()); settings.mixedPort = reservation.serverPort(); reservation.close();
        QVERIFY(store.save(settings, {}).ok);
        QByteArray yaml = "proxies:\n  - {name: 香港示例, type: http, server: 127.0.0.1, port: 9}\n  - {name: 日本示例, type: http, server: 127.0.0.1, port: 9}\n  - {name: 新加坡示例, type: http, server: 127.0.0.1, port: 9}\nproxy-groups:\n  - {name: 节点选择, type: select, proxies: [DIRECT, REJECT, 香港示例, 日本示例, 新加坡示例, HK]}\n  - {name: HK, type: select, proxies: [香港示例, DIRECT]}\nrules: [\"MATCH,节点选择\"]\n";
        QByteArray services; for (int i=0;i<22;++i) services+=QString("\n  - {name: 服务 %1, type: select, proxies: [DIRECT]}").arg(i,2,10,QChar('0')).toUtf8(); yaml.replace("\nrules:",services+"\nrules:");
        const auto file = data.path() + "/sample.yaml"; QVERIFY(writeAtomic(file, yaml).ok);
        AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized); QSignalSpy finished(&controller, &AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.count(), 1); controller.importFile("交互验证示例", file); QTRY_COMPARE(finished.count(), 1); QVERIFY(finished.last()[0].toBool());
        controller.activateProfile(controller.profiles().first().id); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 15000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString())); QVERIFY(controller.state().ready);
        MainWindow window(&controller); window.setPage(2); window.show();
        auto* groups = window.findChild<QListWidget*>("proxyGroups"); QTRY_VERIFY(groups->count() >= 3);
        const auto selectGroupRow = [groups](const QString& name) { for (int row = 0; row < groups->count(); ++row) if (groups->item(row)->text() == name) { groups->setCurrentRow(row); return true; } return false; };
        QVERIFY(selectGroupRow("节点选择"));
        auto* table = window.findChild<QTableView*>("nodesTable"); QVERIFY(table->model()->rowCount() >= 5);
        auto* switcher=window.findChild<QToolButton*>("groupSwitcher"); auto* nodeViews=window.findChild<QStackedWidget*>("nodeViews"); QVERIFY(switcher && nodeViews); QVERIFY(!groups->isVisible()); QCoreApplication::processEvents(); QCoreApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest); QCoreApplication::processEvents(); const int expandedSpace=nodeViews->height(); QTest::mouseClick(switcher,Qt::LeftButton); QVERIFY(groups->isVisible()); QCoreApplication::processEvents(); QVERIFY(nodeViews->height()<expandedSpace-80); QVERIFY(selectGroupRow("HK")); QVERIFY(groups->isVisible()); QTest::mouseClick(groups->viewport(),Qt::LeftButton,{},groups->visualItemRect(groups->currentItem()).center()); QVERIFY(!groups->isVisible()); QVERIFY(selectGroupRow("节点选择")); QCoreApplication::processEvents(); QTRY_VERIFY_WITH_TIMEOUT(nodeViews->height()>=expandedSpace,500); QVERIFY(nodeViews->height()>window.height()/2);
        auto* sort = window.findChild<QComboBox*>("memberSort"); QVERIFY(sort); QCOMPARE(sort->currentIndex(), 0);
        auto* sorted = qobject_cast<QSortFilterProxyModel*>(table->model()); QVERIFY(sorted);
        auto* members = qobject_cast<ProxyTableModel*>(sorted->sourceModel()); QVERIFY(members);
        const auto visibleNames = [sorted, members] { QStringList names; for (int row = 0; row < sorted->rowCount(); ++row) names.append(members->nameAt(sorted->mapToSource(sorted->index(row, 0)).row())); return names; };
        const auto original = members->names(); QCOMPARE(visibleNames(), original);
        table->selectRow(2); const auto selected = original[2];
        sort->showPopup(); QTRY_VERIFY(sort->view()->isVisible()); QTest::keyClick(sort->view(),Qt::Key_Escape); QTRY_VERIFY(!sort->view()->isVisible()); QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),selected);
        sort->setCurrentIndex(1); auto alphabetical = original; alphabetical.sort(Qt::CaseInsensitive); QCOMPARE(visibleNames(), alphabetical);
        QCOMPARE(members->nameAt(sorted->mapToSource(table->selectionModel()->selectedRows().first()).row()), selected);
        members->setDelay(original[3], 150, {}); members->setDelay(original[2], 50, {}); members->setDelay(original[1], 0, "timeout");
        sort->setCurrentIndex(2); QCOMPARE(visibleNames().first(), original[2]); QCOMPARE(visibleNames().last(), original[1]);
        members->setDelay(original[3], 20, {}); QCOMPARE(visibleNames().first(), original[3]);
        QCOMPARE(members->nameAt(sorted->mapToSource(table->selectionModel()->selectedRows().first()).row()), selected);
        members->setGroup(controller.proxies(), "节点选择"); QCOMPARE(visibleNames().first(), original[3]);
        sorted->setFilterFixedString("示例"); QCOMPARE(sorted->rowCount(), 3); sort->setCurrentIndex(0);
        sorted->setFilterFixedString({}); QCOMPARE(visibleNames(), original); members->clearDelays();
        int rejectRow = -1; for (int row = 0; row < table->model()->rowCount(); ++row) if (table->model()->data(table->model()->index(row, 0)).toString().contains("REJECT")) rejectRow = row;
        QVERIFY(rejectRow >= 0); auto* cards=window.findChild<QListView*>("nodeCards"); auto* viewMode=window.findChild<QComboBox*>("nodeViewMode"); QVERIFY(cards && viewMode); QCOMPARE(cards->model(),table->model()); QCOMPARE(cards->selectionModel(),table->selectionModel()); viewMode->setCurrentIndex(1); QCoreApplication::processEvents(); QVERIFY(cards->isVisible()); QVERIFY(!table->isVisible()); const auto rejectIndex=sorted->index(rejectRow,0); cards->scrollTo(rejectIndex); QVERIFY(cards->visualRect(rejectIndex).width()>200); QVERIFY(cards->visualRect(rejectIndex).height()>=50); QTest::mouseClick(cards->viewport(),Qt::LeftButton,{},cards->visualRect(rejectIndex).center()); QTRY_VERIFY(!table->selectionModel()->selectedRows().isEmpty()); QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),QString("REJECT")); QCOMPARE(finished.count(),2);
        sort->setCurrentIndex(1); QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),QString("REJECT")); sorted->setFilterFixedString("REJECT"); QCOMPARE(cards->model()->rowCount(),1); sorted->setFilterFixedString({}); sort->setCurrentIndex(0); members->setDelay("REJECT",432,{}); QCOMPARE(cards->model()->index(rejectRow,2).data().toString(),QString("432 ms")); members->clearDelays(); window.activateWindow(); QApplication::setActiveWindow(&window); cards->setFocus(); QTest::qWait(100); QVERIFY(window.findChild<QPushButton*>("chooseMember")->isEnabled());
        QTest::keyClick(cards, Qt::Key_Return); QTRY_COMPARE(finished.count(), 3); QVERIFY(finished.last()[0].toBool());
        QTRY_COMPARE(controller.proxies().value("节点选择").toObject().value("now").toString(), QString("REJECT"));
        QTest::keyClick(cards,Qt::Key_Escape); QVERIFY(table->selectionModel()->selectedRows().isEmpty()); QCOMPARE(controller.proxies().value("节点选择").toObject().value("now").toString(),QString("REJECT")); QCOMPARE(finished.count(),3); table->selectRow(rejectRow); QVERIFY(!window.findChild<QPushButton*>("chooseMember")->isEnabled());
        if (cards->viewport()->contentsRect().width()>=661) QTRY_COMPARE(cards->visualRect(sorted->index(2,0)).top(),cards->visualRect(sorted->index(0,0)).top());
        const auto cardOutput=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!cardOutput.isEmpty()) { QDir().mkpath(cardOutput); QVERIFY(window.grab().save(cardOutput+"/nodes-cards.png")); const QJsonObject geometry{{"viewport_width",cards->viewport()->width()},{"contents_width",cards->viewport()->contentsRect().width()},{"cell_width",cards->gridSize().width()},{"first_x",cards->visualRect(sorted->index(0,0)).x()},{"third_y",cards->visualRect(sorted->index(2,0)).y()},{"first_y",cards->visualRect(sorted->index(0,0)).y()}}; QVERIFY(writeAtomic(cardOutput+"/cards-geometry.json",QJsonDocument(geometry).toJson()).ok); window.resize(840,560); QTest::qWait(50); QVERIFY(window.grab().save(cardOutput+"/nodes-cards-compact.png")); } viewMode->setCurrentIndex(0); QCOMPARE(finished.count(),3); QVERIFY(table->isVisible()); QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),QString("REJECT"));
        QVERIFY(window.findChild<QPushButton*>("recentNodes")->isEnabled()); QCOMPARE(controller.profiles()[0].recentSelections[0].toObject().value("member").toString(),QString("REJECT"));
        auto* recent=window.findChild<QPushButton*>("recentNodes"); auto* recentMenu=recent->menu(); QVERIFY(recentMenu); window.activateWindow(); QApplication::setActiveWindow(&window);
        QVERIFY(recent->width()>=recent->fontMetrics().horizontalAdvance(recent->text())+36);
        QSignalSpy recentOpened(recentMenu,&QMenu::aboutToShow); QTimer::singleShot(80,recentMenu,[recentMenu] { QTest::keyClick(recentMenu,Qt::Key_Escape); }); QTest::mouseClick(recent,Qt::LeftButton); QTRY_COMPARE(recentOpened.size(),1); QTRY_VERIFY(!recentMenu->isVisible()); QCOMPARE(finished.count(),3);
        const auto recentImage=recent->grab().toImage(); const qreal dpr=recentImage.devicePixelRatio(); int arrowTop=recentImage.height(),arrowBottom=-1;
        for (int y=2;y<recentImage.height()-2;++y) for (int x=qRound((recent->width()-17)*dpr);x<qRound((recent->width()-6)*dpr);++x) { const auto color=recentImage.pixelColor(x,y); if (color.red()<150 && color.green()<160 && color.blue()<180 && color.alpha()>200) { arrowTop=qMin(arrowTop,y); arrowBottom=qMax(arrowBottom,y); } }
        QVERIFY(arrowBottom>=arrowTop); QVERIFY(qAbs((arrowTop+arrowBottom)/2.0-recentImage.height()/2.0)<=3*dpr);
        const auto recentOutput=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!recentOutput.isEmpty()) QVERIFY(recent->grab().save(recentOutput+"/recent-button.png"));
        QVERIFY(window.findChild<QLabel*>("currentMember")->toolTip().contains("REJECT"));
        QVERIFY(!window.findChild<QPushButton*>("automaticAction")->isEnabled());
        QWidget* home=nullptr; for (auto* card:window.findChildren<QWidget*>("dashboardCard")) if (card->property("page").toInt()==2) home=card; QVERIFY(home); QVERIFY(!qobject_cast<QPushButton*>(home)); QVERIFY(home->findChild<QPushButton*>("summaryNavigate")); QVERIFY(home->findChild<QLabel*>("homeProxyCounts")->text().contains("策略组")); QCOMPARE(home->findChild<QLabel*>("statValue")->text(),QString("REJECT")); QVERIFY(!home->findChild<QLabel*>("homeMemberIcon")->isHidden()); auto* profileMeta=window.findChild<QLabel*>("homeProfileMeta"); QVERIFY(profileMeta->text().contains("最近获取")); QVERIFY(!profileMeta->text().contains(file)); bool hasCurrent=false; for (auto* label:home->findChildren<QLabel*>()) if (label->text().contains("REJECT")) hasCurrent=true; QVERIFY(hasCurrent);
        window.setPage(0); window.resize(940,620); QCoreApplication::processEvents();
        auto* homeScroll=window.findChild<QScrollArea*>("homeScroll"); homeScroll->verticalScrollBar()->setValue(0);
        auto* metrics=window.findChild<QWidget*>("homeMetrics"); QVERIFY(metrics->isVisible());
        QVERIFY(!window.findChild<QWidget*>("homeAdditionalInfo")->isVisible());
        QVERIFY2(metrics->mapTo(homeScroll->viewport(),metrics->rect().bottomRight()).y()<=homeScroll->viewport()->height(),"default first screen must include all metrics");
        for (auto* card:window.findChildren<QWidget*>("dashboardCard")) if (card->isVisible()) {
            for (auto* label:card->findChildren<QLabel*>()) if (label->isVisible()) QVERIFY(card->rect().contains(label->geometry()));
            for (auto* combo:card->findChildren<QComboBox*>()) if (combo->isVisible()) QVERIFY(card->rect().adjusted(0,0,0,-6).contains(combo->geometry()));
        }
        window.setPage(2);
        controller.importFile("第二份示例", file); QTRY_COMPARE(finished.count(), 4); QVERIFY(finished.last()[0].toBool());
        auto* sources = window.findChild<QComboBox*>("proxyProfilesCombo"); QCOMPARE(sources->count(), 2); sources->setCurrentIndex(1);
        QCOMPARE(table->model()->rowCount(), 0); QCOMPARE(groups->count(), 0); QVERIFY(!window.findChild<QPushButton*>("chooseMember")->isEnabled());
        auto* use = window.findChild<QPushButton*>("useProxyProfile"); QVERIFY(use->isEnabled()); QTest::mouseClick(use, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 5, 15000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString()));
        QTRY_VERIFY(table->model()->rowCount() >= 5); QCOMPARE(controller.settings().activeProfile, sources->currentData().toString());
        QVERIFY(selectGroupRow("节点选择"));
        window.setPage(5); QCoreApplication::processEvents(); QVERIFY(!home->isVisible()); window.setPage(0); QCoreApplication::processEvents(); QVERIFY(home->isVisible());
        window.setPage(2); int childRow = -1; for (int row = 0; row < table->model()->rowCount(); ++row) if (table->model()->data(table->model()->index(row, 0)).toString().trimmed() == "HK") childRow = row;
        QVERIFY(childRow >= 0); table->selectRow(childRow); auto* enterGroup = window.findChild<QPushButton*>("enterGroup"); QVERIFY(enterGroup->isVisible()); QVERIFY(enterGroup->isEnabled());
        const auto beforeBrowse = finished.count(); QTest::mouseClick(enterGroup, Qt::LeftButton); QCOMPARE(table->model()->rowCount(), 2); QCOMPARE(finished.count(), beforeBrowse);
        auto* backGroup = window.findChild<QPushButton*>("backGroup"); QVERIFY(backGroup->isVisible()); QTest::mouseClick(backGroup, Qt::LeftButton); QVERIFY(table->model()->rowCount() >= 6); QCOMPARE(finished.count(), beforeBrowse);
        window.setPage(3); auto* rulesProfiles = window.findChild<QComboBox*>("rulesProfiles"); rulesProfiles->setCurrentIndex(rulesProfiles->findData(controller.state().activeProfile));
        auto* editor = window.findChild<QPlainTextEdit*>("customRulesEditor"); auto* saveRules = window.findChild<QPushButton*>("saveCustomRules");
        const QString rule = "IP-CIDR,127.0.0.1/32,DIRECT,no-resolve"; editor->setPlainText(rule); QVERIFY(saveRules->isEnabled());
        const auto activeId = controller.state().activeProfile; rulesProfiles->setCurrentIndex(0); QCOMPARE(rulesProfiles->currentData().toString(), activeId); QCOMPARE(editor->toPlainText(), rule);
        QTest::mouseClick(saveRules, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 6, 15000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString()));
        auto* rulesTabs = window.findChild<QTabWidget*>("rulesTabs"); rulesTabs->setCurrentIndex(1);
        QTRY_VERIFY(!controller.activeRules().isEmpty()); QCOMPARE(controller.activeRules()[0].toObject().value("payload").toString(), QString("127.0.0.1/32")); QCOMPARE(controller.activeRules()[0].toObject().value("proxy").toString(), QString("DIRECT"));
        QVERIFY(controller.profiles()[0].customRules.isEmpty()); QCOMPARE(controller.profiles()[1].customRules, QStringList{rule});
        auto* ruleSearch = window.findChild<QLineEdit*>("activeRulesSearch"); auto* ruleTable = window.findChild<QTableView*>("activeRulesTable"); ruleSearch->setText("no-rule-match-example"); QCOMPARE(ruleTable->model()->rowCount(),0); ruleSearch->setText("127.0.0.1/32"); QCOMPARE(ruleTable->model()->rowCount(),1);
        const auto rulesPreview = qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!rulesPreview.isEmpty()) { QDir().mkpath(rulesPreview); window.grab().save(rulesPreview+"/rules-readonly.png"); }
        ruleSearch->clear(); rulesTabs->setCurrentIndex(0); QVERIFY(controller.activeRules().isEmpty());
        const auto previousVersion = controller.profiles()[1].active; editor->setPlainText("IP-CIDR,invalid,DIRECT"); QTest::mouseClick(saveRules, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 7, 10000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(controller.profiles()[1].active, previousVersion); QCOMPARE(controller.profiles()[1].customRules, QStringList{rule}); QVERIFY(controller.state().ready);
        editor->setPlainText(rule); QVERIFY(!saveRules->isEnabled()); QCOMPARE(finished.count(),7);
        editor->setPlainText(rule+"\nDOMAIN-SUFFIX,update-test.example,DIRECT"); QTest::mouseClick(saveRules, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 8, 15000); QVERIFY(finished.last()[0].toBool());
        QTcpServer destination; QVERIFY(destination.listen(QHostAddress::LocalHost, 0)); QTcpSocket tunnel; tunnel.connectToHost(QHostAddress::LocalHost, controller.settings().mixedPort); QTRY_COMPARE(tunnel.state(), QAbstractSocket::ConnectedState);
        const auto address = QByteArray("127.0.0.1:") + QByteArray::number(destination.serverPort()); tunnel.write("CONNECT " + address + " HTTP/1.1\r\nHost: " + address + "\r\n\r\n"); QTRY_VERIFY(destination.hasPendingConnections()); auto* remote = destination.nextPendingConnection(); QVERIFY(remote); remote->setParent(&destination);
        window.setPage(4); QTRY_VERIFY_WITH_TIMEOUT(!controller.connections().isEmpty(), 5000); QVERIFY(controller.connectionsVisible());
        bool matched = false; for (const auto& value : controller.connections()) if (value.toObject().value("rulePayload").toString() == "127.0.0.1/32") matched = true; QVERIFY(matched);
        window.hide(); QVERIFY(!controller.connectionsVisible()); QVERIFY(controller.connections().isEmpty()); QSignalSpy hiddenUpdates(&controller, &AppController::connectionsChanged); QTest::qWait(2200); QCOMPARE(hiddenUpdates.count(), 0);
        window.show(); QTRY_VERIFY_WITH_TIMEOUT(!controller.connections().isEmpty(), 5000);
        const auto output = qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QVERIFY(writeAtomic(output+"/render-context.json",QJsonDocument(QJsonObject{{"device_pixel_ratio",window.devicePixelRatioF()},{"screen_device_pixel_ratio",window.screen()->devicePixelRatio()},{"logical_dpi",window.screen()->logicalDotsPerInch()},{"screen_scale_override",qEnvironmentVariable("QT_SCREEN_SCALE_FACTORS")},{"scale_factor",qEnvironmentVariable("QT_SCALE_FACTOR")},{"platform",QGuiApplication::platformName()}}).toJson()).ok);
            QVERIFY(QDir().mkpath(output));
            for (int page = 0; page < 8; ++page) { window.setPage(page); QTest::qWait(100); if (page == 4) QTRY_VERIFY(!controller.connections().isEmpty()); QVERIFY(window.grab().save(output + QString("/page-%1.png").arg(page))); if (page==0) { auto* mode=window.findChild<QComboBox*>("homeQuickGroup"); mode->showPopup(); QTRY_VERIFY(mode->view()->isVisible()); QVERIFY(mode->view()->window()->grab().save(output+"/home-group-popup.png")); QTest::keyClick(mode->view(),Qt::Key_Escape); QTRY_VERIFY(!mode->view()->isVisible()); } }
            for (const auto size:{QSize(1000,680),QSize(1180,810),QSize(1440,900)}) { window.resize(size); for (int page=0;page<8;++page) { window.setPage(page); QTest::qWait(20); QVERIFY(window.grab().save(output+QString("/size-%1x%2-page-%3.png").arg(size.width()).arg(size.height()).arg(page))); } }
            window.setPage(2); window.resize(840, 560); QTest::qWait(100); QVERIFY(window.grab().save(output + "/compact.png"));
            for (int page : {0, 1, 3, 4, 5, 6}) { window.setPage(page); QTest::qWait(100); if (page == 4) QTRY_VERIFY(!controller.connections().isEmpty()); QVERIFY(window.grab().save(output + QString("/compact-%1.png").arg(page))); }
            ImportDialog dialog(&window); dialog.show(); QTest::qWait(100); QVERIFY(dialog.grab().save(output + "/import.png"));
        }
        window.setPage(0); window.hide(); QVERIFY(!controller.connectionsVisible());
        GuiHost lightweight(&controller, true); lightweight.show(4); QTRY_VERIFY(!controller.connections().isEmpty()); QPointer<MainWindow> released = lightweight.window();
        released->close(); QTRY_VERIFY(released.isNull()); QVERIFY(controller.state().ready); QCOMPARE(controller.state().activeProfile, activeId); QVERIFY(!controller.connectionsVisible()); QVERIFY(controller.connections().isEmpty()); QVERIFY(controller.activeRules().isEmpty());
        tunnel.write("ping"); QTRY_VERIFY(remote->bytesAvailable() >= 4); QCOMPARE(remote->read(4), QByteArray("ping"));
        lightweight.show(2); QVERIFY(lightweight.window()->findChild<QTableView*>("nodesTable")->model()->rowCount() > 0); QVERIFY(controller.state().ready); lightweight.dispose();
        TrayController tray(&controller);
        int completed = finished.count();
        for (const auto& mode : {QString("global"), QString("direct"), QString("rule")}) { ++completed; controller.setMode(mode); QTRY_COMPARE(finished.count(), completed); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.state().mode, mode); QCOMPARE(tray.visualKey(), TrayController::appearance(controller.state())); }
        window.show(); window.setPage(4); QTRY_VERIFY(!controller.connections().isEmpty()); auto* connections = window.findChild<QTableView*>("connectionsTable"); connections->selectRow(0); auto* close = window.findChild<QPushButton*>("closeSelectedConnection"); QVERIFY(close->isEnabled()); ++completed; QTest::mouseClick(close,Qt::LeftButton); QTRY_COMPARE(finished.count(),completed); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_VERIFY(controller.connections().isEmpty()); QTRY_COMPARE(tunnel.state(),QAbstractSocket::UnconnectedState); QVERIFY(controller.state().ready); QVERIFY(!close->isEnabled());
        QVERIFY(!controller.closedConnections().isEmpty()); auto* view=window.findChild<QTabBar*>("connectionView"); view->setCurrentIndex(1); QVERIFY(connections->model()->rowCount()>0); connections->selectRow(0); QVERIFY(!close->isEnabled()); window.findChild<QAction*>("connectionDetailsAction")->trigger(); QPointer<QDialog> detail=window.findChild<QDialog*>("connectionDetails"); QVERIFY(detail); QVERIFY(detail->findChild<QLabel*>("connectionField_destination")->text().contains("127.0.0.1"));
        window.setPage(0); QTRY_VERIFY(detail.isNull()); QVERIFY(controller.closedConnections().isEmpty()); QVERIFY(controller.connectionTotals().isEmpty()); window.setPage(4); view->setCurrentIndex(0);
        QTcpSocket second,third; for (auto* socket:{&second,&third}) { socket->connectToHost(QHostAddress::LocalHost,controller.settings().mixedPort); QTRY_COMPARE(socket->state(),QAbstractSocket::ConnectedState); socket->write("CONNECT "+address+" HTTP/1.1\r\nHost: "+address+"\r\n\r\n"); }
        QTRY_VERIFY_WITH_TIMEOUT(controller.connections().size()>=2,5000); auto* closeAll=window.findChild<QAction*>("closeAllConnections");
        QTimer::singleShot(0,&window,[&window] { auto* box=window.findChild<QMessageBox*>(); QVERIFY(box); box->button(QMessageBox::No)->click(); }); closeAll->trigger(); QCOMPARE(finished.size(),completed); QVERIFY(!controller.connections().isEmpty());
        ++completed; QTimer::singleShot(0,&window,[&window] { auto* box=window.findChild<QMessageBox*>(); QVERIFY(box); box->button(QMessageBox::Yes)->click(); }); closeAll->trigger(); QTRY_COMPARE(finished.size(),completed); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(controller.connections().isEmpty()); QTRY_COMPARE(second.state(),QAbstractSocket::UnconnectedState); QTRY_COMPARE(third.state(),QAbstractSocket::UnconnectedState); QVERIFY(controller.closedConnections().size()>=2); QVERIFY(controller.state().ready);
        window.setPage(6); window.findChild<QAction*>("previewActiveConfig")->trigger(); QPointer<QDialog> config; QTRY_VERIFY((config=window.findChild<QDialog*>("configPreview"))); QVERIFY(config->findChild<QPlainTextEdit*>()->isReadOnly()); QVERIFY(config->findChild<QPlainTextEdit*>()->toPlainText().contains("[redacted]")); window.hide(); QTRY_VERIFY(config.isNull());
        window.show(); window.setPage(1); auto* profiles=window.findChild<QTableWidget*>("profilesTable"); auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(profiles && menu); const auto target=profiles->item(0,0)->data(Qt::UserRole).toString(),active=controller.state().activeProfile; const auto corePid=controller.coreProcessId(); const int beforeRename=finished.size();
        window.activateWindow(); QApplication::setActiveWindow(&window); QTest::qWait(50);
        const auto rowPoint=profiles->visualRect(profiles->model()->index(0,0)).center(); QContextMenuEvent rowMenu(QContextMenuEvent::Mouse,rowPoint,profiles->viewport()->mapToGlobal(rowPoint)); QApplication::sendEvent(profiles->viewport(),&rowMenu); QTRY_VERIFY(menu->isVisible()); QCOMPARE(profiles->currentRow(),0); menu->hide(); QTimer::singleShot(0,&window,[&window] { auto* dialog=window.findChild<QInputDialog*>(); QVERIFY(dialog); dialog->setTextValue("右键重命名示例"); dialog->accept(); }); window.findChild<QAction*>("renameProfile")->trigger(); QTRY_COMPARE(finished.size(),beforeRename+1); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.state().activeProfile,active); QCOMPARE(controller.coreProcessId(),corePid); bool renamed=false; for (const auto& profile:controller.profiles()) if (profile.id==target) { QCOMPARE(profile.name,QString("右键重命名示例")); renamed=true; } QVERIFY(renamed);
        QApplication::setActiveWindow(&window); profiles->setFocus(); QCoreApplication::processEvents(); QTest::keyClick(profiles,Qt::Key_F10,Qt::ShiftModifier); QTRY_VERIFY(menu->isVisible()); menu->hide(); const QPoint blankPoint(20,profiles->viewport()->height()-5); QContextMenuEvent blankMenu(QContextMenuEvent::Mouse,blankPoint,profiles->viewport()->mapToGlobal(blankPoint)); QApplication::sendEvent(profiles->viewport(),&blankMenu); QVERIFY(!menu->isVisible());
        window.setPage(2); QVERIFY(selectGroupRow("节点选择")); controller.testNodes({"香港示例","日本示例","新加坡示例"}); QVERIFY(controller.state().busy); QVERIFY(controller.testingNodes()); QVERIFY(table->isEnabled()); QVERIFY(groups->isEnabled()); QVERIFY(window.findChild<QLineEdit*>("nodeSearch")->isEnabled()); QVERIFY(!window.findChild<QPushButton*>("chooseMember")->isEnabled()); QVERIFY(window.findChild<QToolButton*>("cancelNodeTestButton")->isVisible());
        sort->setCurrentIndex(1); window.findChild<QLineEdit*>("nodeSearch")->setText("示例"); QVERIFY(sorted->rowCount()>0); QVERIFY(selectGroupRow("HK")); QVERIFY(table->isEnabled()); if (!output.isEmpty()) QVERIFY(window.grab().save(output+"/proxy-testing.png")); window.findChild<QToolButton*>("cancelNodeTestButton")->click(); QTRY_VERIFY(!controller.state().busy); QVERIFY(!controller.testingNodes()); QVERIFY(window.findChild<QToolButton*>("cancelNodeTestButton")->isHidden()); QVERIFY(controller.state().lastError.isEmpty()); QVERIFY(finished.last()[0].toBool()); QCOMPARE(finished.last()[1].toString(),QString("测速已取消"));
        QVERIFY(selectGroupRow("节点选择")); viewMode->setCurrentIndex(1); sort->setCurrentIndex(1); window.findChild<QLineEdit*>("nodeSearch")->clear(); table->selectRow(0); const auto selectedBefore=table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(); const auto nowBefore=controller.proxies().value("节点选择").toObject().value("now").toString(); const auto pidBefore=controller.coreProcessId();
        int hoverRow=-1; for (int row=0;row<sorted->rowCount();++row) if (sorted->index(row,0).data(Qt::UserRole).toString()=="香港示例") hoverRow=row; QVERIFY(hoverRow>=0); QVERIFY(cards->viewport()->hasMouseTracking()); const auto hoverIndex=sorted->index(hoverRow,0); cards->scrollTo(hoverIndex); const auto cardRect=cards->visualRect(hoverIndex); const QPoint delayPoint(cardRect.right()-16,cardRect.center().y()); QMouseEvent hoverMove(QEvent::MouseMove,QPointF(delayPoint),QPointF(cards->viewport()->mapToGlobal(delayPoint)),Qt::NoButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(cards->viewport(),&hoverMove); QTRY_COMPARE(cards->property("nodeDelayHover").toString(),QString("香港示例")); QCOMPARE(cards->viewport()->cursor().shape(),Qt::PointingHandCursor); QVERIFY(cards->property("nodeTestEnabled").toBool()); if (!output.isEmpty()) QVERIFY(window.grab().save(output+"/nodes-delay-hover.png"));
        QSignalSpy measured(&controller,&AppController::nodeDelay); const int beforeTest=finished.size(); QTest::mouseClick(cards->viewport(),Qt::LeftButton,{},delayPoint); QVERIFY(controller.testingNodes()); QVERIFY(!cards->property("nodeTestEnabled").toBool()); QTest::mouseDClick(cards->viewport(),Qt::LeftButton,{},delayPoint); QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),selectedBefore); QTRY_COMPARE_WITH_TIMEOUT(measured.size(),1,6000); QTRY_COMPARE(finished.size(),beforeTest+1); QCOMPARE(measured.first()[0].toString(),QString("香港示例")); QCOMPARE(controller.proxies().value("节点选择").toObject().value("now").toString(),nowBefore); QCOMPARE(controller.coreProcessId(),pidBefore); QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),selectedBefore);
        const int beforeSortPress=finished.size(); QTest::mousePress(cards->viewport(),Qt::LeftButton,{},delayPoint); members->setDelay("香港示例",2000,{}); sort->setCurrentIndex(2); QCoreApplication::processEvents(); QTest::mouseRelease(cards->viewport(),Qt::LeftButton,{},delayPoint); QCOMPARE(finished.size(),beforeSortPress); QCOMPARE(measured.size(),1); QVERIFY(!controller.state().busy); QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),selectedBefore); sort->setCurrentIndex(1);
        cards->setCurrentIndex(sorted->index(hoverRow,0)); table->selectRow(hoverRow); QApplication::setActiveWindow(&window); cards->setFocus(); QTest::keyClick(cards,Qt::Key_F5); QVERIFY(controller.testingNodes()); controller.cancelTests(); QTRY_VERIFY(!controller.state().busy);
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.count(), 1, 5000);
    }
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
