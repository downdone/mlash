#include "SmokeRunner.h"
#include "GuiHost.h"
#include "MainWindow.h"
#include "Route.h"
#include "ViewIds.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStackedWidget>
#include <QSslSocket>
#include <QTimer>

namespace mlash {
namespace {
constexpr int kPageSettleMs = 80;
constexpr int kPageGapMs = 100;
constexpr int kFirstPageDelayMs = 250;
} // namespace

SmokeRunner::SmokeRunner(GuiHost* gui, Options options, QObject* parent)
    : QObject(parent), gui_(gui), options_(std::move(options)) {}

int SmokeRunner::legacyPageCount() { return 8; }

void SmokeRunner::start() {
    if (!options_.runtimeReportPath.isEmpty() && options_.write) {
        QJsonObject report{{"qt_version", QT_VERSION_STR},
                           {"ready_ms", static_cast<double>(options_.readyMs)},
                           {"tls_available", QSslSocket::supportsSsl()},
                           {"tls_backend", QSslSocket::activeBackend()},
                           {"plugin_paths", QJsonArray::fromStringList(QCoreApplication::libraryPaths())}};
        options_.write(options_.runtimeReportPath, QJsonDocument(report).toJson());
    }
    QTimer::singleShot(kFirstPageDelayMs, this, [this] { capturePage(0); });
}

void SmokeRunner::capturePage(int page) {
    if (page >= legacyPageCount()) {
        // 条数断言：少跑一页是这套编排最危险的失败模式，不能只看"整体正常"。
        if (pages_.size() != legacyPageCount()) {
            qWarning("Smoke captured %d pages, expected %d", int(pages_.size()), legacyPageCount());
            finish(false);
            return;
        }
        if (options_.outputDir.isEmpty()) {
            finish(ok_);
            return;
        }
        const auto bytes = QJsonDocument(pages_).toJson();
        const auto written = options_.write ? options_.write(options_.outputDir + "/page-routes.json", bytes)
                                            : Result::fail("未注入写文件回调");
        finish(ok_ && written.ok);
        return;
    }
    auto* window = gui_ ? gui_->window() : nullptr;
    if (!window) {
        finish(false);
        return;
    }
    window->setPage(page);
    // 切页、确认路由、截图必须串行；独立定时器会在慢布局时把下一页误存成上一页。
    QTimer::singleShot(kPageSettleMs, this, [this, page] {
        if (!verifyRoute(page)) {
            finish(false);
            return;
        }
        QByteArray screenshotHash;
        if (!captureScreenshot(QString("/page-%1.png").arg(page), &screenshotHash)) {
            finish(false);
            return;
        }
        const auto route = gui_->window()->currentRoute();
        QJsonObject record{{"page", page}, {"view", route.view}, {"tab", route.tab},
                           {"category", route.category}, {"anchor", int(route.anchor)},
                           {"area", int(route.area)}};
        if (!screenshotHash.isEmpty())
            record.insert("screenshot_sha256", QString::fromLatin1(screenshotHash.toHex()));
        pages_.append(record);
        if (page == 6 && !checkSettingsSearch()) {
            finish(false);
            return;
        }
        QTimer::singleShot(kPageGapMs, this, [this, page] { capturePage(page + 1); });
    });
}

bool SmokeRunner::verifyRoute(int page) {
    auto* window = gui_->window();
    const auto expected = fromLegacyPage(page);
    if (!window || window->currentRoute() != expected) {
        if (window) {
            const auto actual = window->currentRoute();
            qWarning("Smoke route mismatch for page %d: expected %d/%d/%d/%d, actual %d/%d/%d/%d",
                     page, expected.view, expected.category, int(expected.anchor), expected.tab,
                     actual.view, actual.category, int(actual.anchor), actual.tab);
        }
        return false;
    }
    return true;
}

bool SmokeRunner::captureScreenshot(const QString& name, QByteArray* hashOut) {
    if (options_.outputDir.isEmpty()) return true;
    auto* window = gui_ ? gui_->window() : nullptr;
    if (!window || !options_.write) return false;
    const auto pixmap = window->grab();
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !pixmap.save(&buffer, "PNG")) return false;
    buffer.close();
    if (!options_.write(options_.outputDir + name, bytes).ok) return false;
    if (hashOut) *hashOut = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return true;
}

bool SmokeRunner::checkSettingsSearch() {
    if (options_.outputDir.isEmpty()) return true;
    auto* window = gui_->window();
    if (!window) return false;
    auto* search = window->findChild<QLineEdit*>("settingsSearch");
    auto* stack = window->findChild<QStackedWidget*>("settingsTaskStack");
    auto* directory = window->findChild<QWidget*>("settingsDirectory");
    auto* appearance = window->findChild<QWidget*>("settingsAppearanceRow");
    if (!search || !stack || !directory || !appearance || !search->isVisible() ||
        stack->currentIndex() != 0 || !directory->isVisible())
        return false;
    search->setText(QStringLiteral("外观"));
    auto* results = window->findChild<QWidget*>("settingsInlineResults");
    if (!results || !results->isVisible() || directory->isVisible()) return false;
    if (!captureScreenshot("/settings-search.png")) return false;
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(search, &enter);
    if (stack->currentIndex() != 0 || !directory->isVisible() || !appearance->isVisible() ||
        !appearance->property("located").toBool())
        return false;
    const auto expected = fromLegacyPage(6);
    window->setPage(0);
    window->setPage(6);
    if (window->currentRoute() != expected || stack->currentIndex() != 0 || !directory->isVisible() ||
        !search->isVisible())
        return false;
    return captureScreenshot("/settings-return.png");
}

void SmokeRunner::finish(bool ok) {
    ok_ = ok;
    emit finished(ok_);
}

} // namespace mlash