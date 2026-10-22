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
#include <QSslSocket>

int main(int argc, char** argv) {
    QElapsedTimer startup; startup.start();
    QApplication app(argc, argv);
    app.setFont(QFont("Microsoft YaHei UI", 9));
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
    auto showWindow = [&] { gui.show(); };
    QObject::connect(&tray, &mlash::TrayController::showWindow, &app, showWindow);
    QObject::connect(&tray, &mlash::TrayController::showPage, &app, [&](int page) { gui.show(page); });
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
            for (int page = 0; page < 8; ++page) QTimer::singleShot(250 + page * 200, &app, [&, page, output] {
                if (auto* window = gui.window()) { window->setPage(page); if (!output.isEmpty()) window->grab().save(output + QString("/page-%1.png").arg(page)); }
            });
            QTimer::singleShot(2000, &controller, &mlash::AppController::requestExit);
        }
    });
    controller.initialize(); const int result = app.exec();
    gui.dispose();
    QThreadPool::globalInstance()->waitForDone();
    return qualification && (!measurement || !measurement->succeeded()) ? 5 : result;
}
