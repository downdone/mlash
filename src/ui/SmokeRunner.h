#pragma once
// CONTRACT owner=ui layer=ui
// invariant: 冒烟串行切页；每页先校验实际路由，再截图与记录
// forbidden: 不得在冒烟中执行配置写入、内核启停或系统代理接管
// lifecycle: 仅在 --smoke-test 下由 main 创建；结束时发出 finished 并由调用方退出
// tests: gui_smoke
// spec: docs/engineering.md
#include "domain/Types.h"
#include <QJsonArray>
#include <QObject>
#include <functional>

namespace mlash {

class GuiHost;

// 把「切页 → 校验路由 → 截图 → 记录」的编排从 main.cpp 移出。
// 写文件由调用方注入，避免界面层直接依赖 storage。
class SmokeRunner final : public QObject {
    Q_OBJECT
public:
    using Writer = std::function<Result(const QString& path, const QByteArray& bytes)>;

    struct Options {
        QString outputDir;
        QString runtimeReportPath;
        quint64 readyMs = 0;
        Writer write;
    };

    SmokeRunner(GuiHost* gui, Options options, QObject* parent = nullptr);

    // 旧页号总量：0..LegacyPageCount-1。路由清单条数必须等于它。
    static int legacyPageCount();

    void start();

signals:
    void finished(bool ok);

private:
    void capturePage(int page);
    bool verifyRoute(int page);
    bool captureScreenshot(const QString& name, QByteArray* hashOut = nullptr);
    bool checkSettingsSearch();
    void finish(bool ok);

    GuiHost* gui_ = nullptr;
    Options options_;
    QJsonArray pages_;
    bool ok_ = true;
};

} // namespace mlash