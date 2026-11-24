#pragma once
#include <QObject>
#include <QPointer>
#include "MainWindow.h"

namespace mlash {
// Owns only a live window and a small session bookmark; Controller and tray outlive it.
class GuiHost final : public QObject {
public:
    GuiHost(AppController* controller, bool trayAvailable, QObject* parent = nullptr);
    ~GuiHost() override;
    void show(int page = -1);
    // 路由入口：设置分类与首页锚点都随 Route 一起保存/恢复。
    void show(Route route);
    MainWindow* window() const { return window_; }
    void dispose();
    void requestExit();
private:
    AppController* controller_;
    bool trayAvailable_, retiring_ = false, reopen_ = false;
    Route route_{0, -1, HomeAnchor::Control};
    bool routeKnown_ = false;
    int memberView_ = -1;
    QByteArray geometry_;
    QJsonObject nodeBookmark_;
    WorkspaceBookmark workspaceBookmark_;
    QPointer<MainWindow> window_;
    quint64 scheduleGeneration_ = 0;
};
}
