#include "app/AppController.h"
#include "app/Async.h"
#include "storage/Storage.h"
#include "ui/MainWindow.h"
#include "ui/TrayController.h"
#include "ui/GuiHost.h"
#include "ui/WidgetStyle.h"
#include "ui/UiTheme.h"
#include "app/ProtocolInbox.h"
#include "ui/ProtocolImportQueue.h"
#include "diagnostics/QualificationRun.h"
#include <QApplication>
#include <QFont>
#include <QCommandLineParser>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QPointer>
#include <QSessionManager>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSslSocket>
#include <QStackedWidget>
#include <functional>

int main(int argc, char** argv) {
    QElapsedTimer startup; startup.start();
#ifdef Q_OS_WIN
    // Choose Qt's gray-scale FreeType rasterizer before QPA initializes. Explicit
    // platform choices (including offscreen tests) retain their precedence.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");
#endif
    QApplication app(argc, argv);
    mlash::WidgetStyle::install();
    app.setApplicationName("Mlash"); app.setOrganizationName("Mlash"); app.setApplicationVersion(MLASH_VERSION); app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(mlash::TrayController::icon());
    QCommandLineParser parser; parser.setApplicationDescription("Mlash — lightweight mihomo client"); parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"silent", "Start in the tray when a profile is available"});
    parser.addOption({"import-url","Review a mlash:// subscription link without downloading until confirmed","link"});
    parser.addOption({"data-dir", "Explicit isolated data directory", "path"});
    parser.addOption({"smoke-test", "UI smoke test with no automatic core/proxy/autostart changes"});
    parser.addOption({"screenshots-dir", "Write smoke-test page screenshots", "path"});
    parser.addOption({"runtime-report", "Write smoke-test runtime readiness and TLS report", "path"});
    parser.addOption({"qualification-seconds", "Isolated qualification run (1..604800 seconds), blocks OS writes", "seconds"});
    parser.addOption({"qualification-report", "Atomic numeric qualification JSON report", "path"});
    parser.addOption({"qualification-gui", "Retain GUI during qualification instead of lightweight tray idle"}); parser.process(app);
    const bool smoke = parser.isSet("smoke-test");
    const auto importLink=parser.value("import-url"); if (parser.isSet("import-url")&&(!mlash::parseProtocolLink(importLink).ok||app.arguments().size()!=3||app.arguments()[1]!="--import-url")) { QMessageBox::warning(nullptr,"Mlash","导入链接无效；链接启动不能附带其他命令行选项"); return 4; }
    const bool qualification = parser.isSet("qualification-seconds"); bool validSeconds=false; const int seconds=parser.value("qualification-seconds").toInt(&validSeconds);
    const auto root = QDir::cleanPath(parser.isSet("data-dir") ? QFileInfo(parser.value("data-dir")).absoluteFilePath() : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Mlash");
    if (qualification && (smoke || !parser.isSet("data-dir") || !parser.isSet("qualification-report") || !validSeconds || seconds<1 || seconds>604800 ||
        QFileInfo::exists(parser.value("qualification-report")) || QFileInfo::exists(parser.value("qualification-report")+".samples.jsonl") ||
        !QDir(root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty())) return 4;
    if (!QDir().mkpath(root)) { QMessageBox::critical(nullptr, "Mlash", "无法创建数据目录"); return 2; }
    QLockFile lock(root + "/instance.lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) { if (!importLink.isEmpty()) { int forwarded=3; mlash::ProtocolInbox::forward(&app,root,importLink,[&](mlash::Result result) { forwarded=result.ok?0:3; if (!result.ok) QMessageBox::warning(nullptr,"Mlash",result.error); app.quit(); }); app.exec(); return forwarded; } if (!smoke) QMessageBox::information(nullptr, "Mlash", "同一数据目录已有 Mlash 实例，或锁文件无法访问。"); return 3; }
    mlash::ProtocolInbox protocolInbox; if (!smoke&&!qualification) { const auto inbox=protocolInbox.listen(root); if (!inbox.ok) { QMessageBox::warning(nullptr,"Mlash",inbox.error); return 3; } }
    QThreadPool::globalInstance()->setMaxThreadCount(2);
    mlash::AppController controller(root, smoke, nullptr, qualification ? mlash::createMeasurementPlatform() : nullptr);
    if (!importLink.isEmpty()) controller.suppressAutomaticStartup();
    QObject::connect(&controller,&mlash::AppController::stateChanged,&app,[&controller] { mlash::UiTheme::setPreferences(controller.settings().appearance,controller.settings().textScale); });
    mlash::TrayController tray(&controller);
    mlash::GuiHost gui(&controller, !smoke && tray.available());
    mlash::ProtocolImportQueue imports(&controller,&gui); QObject::connect(&protocolInbox,&mlash::ProtocolInbox::received,&imports,[&imports](const QString& link) { const auto accepted=imports.enqueue(link); if (!accepted.ok) QMessageBox::warning(nullptr,"Mlash",accepted.error); }); if (!importLink.isEmpty()) imports.enqueue(importLink);
    std::unique_ptr<mlash::QualificationRun> measurement;
    bool smokeUiOk = true;
    QJsonArray smokePages;
    std::function<void(int)> captureSmokePage;
    auto showWindow = [&] { gui.show(); };
    QObject::connect(&tray, &mlash::TrayController::showWindow, &app, showWindow);
    QObject::connect(&tray, &mlash::TrayController::showPage, &app, [&](int page) { gui.show(page); });
    QObject::connect(&tray, &mlash::TrayController::showRoute, &app, [&](mlash::Route route) { gui.show(route); });
    QObject::connect(&tray, &mlash::TrayController::exitRequested, &app, [&] { gui.requestExit(); });
    QObject::connect(&controller,&mlash::AppController::openPageRequested,&app,[&](int page) { gui.show(page); });
    QObject::connect(&controller, &mlash::AppController::exitReady, &app, &QApplication::quit);
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &controller, [&controller](QSessionManager&) { controller.requestExit(); });
    QObject::connect(&controller, &mlash::AppController::initialized, &app, [&] {
        if (qualification) { measurement=std::make_unique<mlash::QualificationRun>(&controller,&gui,&tray,parser.value("qualification-report"),seconds,parser.isSet("qualification-gui")); measurement->start(startup.elapsed()); return; }
        if (smoke || !tray.available() || controller.profiles().isEmpty() || controller.settings().activeProfile.isEmpty() || !controller.state().lastError.isEmpty() ||
            !(parser.isSet("silent") || controller.settings().silentStart)) gui.show(controller.settings().startupPage);
        if (smoke) {
            if (parser.isSet("runtime-report")) {
                QJsonObject report{{"qt_version", QT_VERSION_STR}, {"ready_ms", static_cast<double>(startup.elapsed())},
                    {"tls_available", QSslSocket::supportsSsl()}, {"tls_backend", QSslSocket::activeBackend()},
                    {"plugin_paths", QJsonArray::fromStringList(QCoreApplication::libraryPaths())}};
                const auto path = parser.value("runtime-report"); const auto bytes = QJsonDocument(report).toJson();
                mlash::runTask(&controller, [path, bytes] { return mlash::writeAtomic(path, bytes); }, [&controller](mlash::Result r) { if (!r.ok) controller.diagnostics()->append("error", r.error); });
            }
            const auto output = parser.value("screenshots-dir"); if (!output.isEmpty()) QDir().mkpath(output);
            // 切页、确认路由、截图必须串行。独立定时器会在慢布局时把下一页误存成上一页。
            captureSmokePage = [&,output](int page) {
                if (page == 8) {
                    if (output.isEmpty()) { controller.requestExit(); return; }
                    const auto bytes=QJsonDocument(smokePages).toJson();
                    mlash::runTask(&controller,[output,bytes] { return mlash::writeAtomic(output+"/page-routes.json",bytes); },
                        [&](mlash::Result r) { if (!r.ok) smokeUiOk=false; controller.requestExit(); });
                    return;
                }
                auto* window=gui.window();
                if (!window) { smokeUiOk=false; controller.requestExit(); return; }
                window->setPage(page);
                QTimer::singleShot(80,&app,[&,page,output] {
                    auto* readyWindow=gui.window();
                    const auto expected=mlash::fromLegacyPage(page);
                    if (!readyWindow || readyWindow->currentRoute()!=expected) {
                        if (readyWindow) {
                            const auto actual=readyWindow->currentRoute();
                            qWarning("Smoke route mismatch for page %d: expected %d/%d/%d/%d, actual %d/%d/%d/%d",
                                page,expected.view,expected.category,int(expected.anchor),expected.tab,
                                actual.view,actual.category,int(actual.anchor),actual.tab);
                        }
                        smokeUiOk=false; controller.requestExit(); return;
                    }
                    if (!output.isEmpty() && !readyWindow->grab().save(output+QString("/page-%1.png").arg(page))) {
                        smokeUiOk=false; controller.requestExit(); return;
                    }
                    const auto route=readyWindow->currentRoute();
                    smokePages.append(QJsonObject{{"page",page},{"view",route.view},{"tab",route.tab}});
                    if (page==6 && !output.isEmpty()) {
                        auto* search=readyWindow->findChild<QLineEdit*>("settingsSearch");
                        auto* stack=readyWindow->findChild<QStackedWidget*>("settingsTaskStack");
                        auto* directory=readyWindow->findChild<QWidget*>("settingsDirectory");
                        auto* appearance=readyWindow->findChild<QWidget*>("settingsAppearanceRow");
                        if (!search || !stack || !directory || !appearance || !search->isVisible() ||
                            stack->currentIndex()!=0 || !directory->isVisible()) {
                            smokeUiOk=false; controller.requestExit(); return;
                        }
                        search->setText(QStringLiteral("外观"));
                        QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
                        QApplication::sendEvent(search,&enter);
                        if (stack->currentIndex()!=0 || !directory->isVisible() ||
                            !appearance->isVisible() || !appearance->property("located").toBool() ||
                            !readyWindow->grab().save(output+"/settings-search.png")) {
                            smokeUiOk=false; controller.requestExit(); return;
                        }
                        readyWindow->setPage(0);
                        readyWindow->setPage(6);
                        if (readyWindow->currentRoute()!=expected || stack->currentIndex()!=0 ||
                            !directory->isVisible() || !search->isVisible() ||
                            !readyWindow->grab().save(output+"/settings-return.png")) {
                            smokeUiOk=false; controller.requestExit(); return;
                        }
                    }
                    QTimer::singleShot(100,&app,[&,page] { captureSmokePage(page+1); });
                });
            };
            QTimer::singleShot(250,&app,[&] { captureSmokePage(0); });
        }
    });
    controller.initialize(); const int result = app.exec();
    gui.dispose();
    QThreadPool::globalInstance()->waitForDone();
    if (smoke && !smokeUiOk) return 6;
    return qualification && (!measurement || !measurement->succeeded()) ? 5 : result;
}
