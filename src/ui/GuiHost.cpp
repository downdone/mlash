#include "GuiHost.h"
#include "ViewIds.h"
#include "app/AppController.h"
#include <QTimer>
#include <QPixmapCache>

namespace mlash {
GuiHost::GuiHost(AppController* controller, bool trayAvailable, QObject* parent) : QObject(parent), controller_(controller), trayAvailable_(trayAvailable) {}
GuiHost::~GuiHost() { dispose(); }
void GuiHost::requestExit() { if (!window_ || window_->confirmDiscardEdits()) controller_->requestExit(); }
void GuiHost::show(int page) {
    if (page<0) {
        if (window_&&!retiring_) route_=window_->currentRoute();
        else if (!routeKnown_) {
            const int startup=controller_->settings().startupPage;
            if (startup==0) route_=Route{ViewIds::Overview};
            else if (startup==2) route_=workspaceRoute(WorkspaceArea::Proxy,WorkspacePage::Members);
            else route_=fromLegacyPage(startup);
        }
    }
    else route_ = fromLegacyPage(page);
    routeKnown_ = true;
    show(route_);
}
void GuiHost::show(Route route) {
    if (isRoutableView(route)) route_ = route;
    routeKnown_ = true;
    if (retiring_) { reopen_ = true; return; }
    if (!window_) {
        window_ = new MainWindow(controller_); window_->setAttribute(Qt::WA_DeleteOnClose); window_->setTrayAvailable(trayAvailable_);
        if (!geometry_.isEmpty()) window_->restoreGeometry(geometry_);
        if (memberView_>=0) window_->setMemberView(memberView_);
        window_->restoreNodeBookmark(nodeBookmark_);
        window_->restoreWorkspaceBookmark(workspaceBookmark_);
        window_->navigate(route_);
        connect(window_, &MainWindow::guiReleaseRequested, this, [this] {
            geometry_=window_->saveGeometry(); route_=window_->currentRoute(); memberView_=window_->memberView();
            nodeBookmark_=window_->nodeBookmark(); workspaceBookmark_=window_->workspaceBookmark(); retiring_=true;
        });
        connect(window_, &QObject::destroyed, this, [this] {
            retiring_ = false; QPixmapCache::clear();
            if (reopen_) { reopen_ = false; const auto token = scheduleGeneration_; QTimer::singleShot(0, this, [this, token] { if (token == scheduleGeneration_) show(route_); }); }
        });
    } else window_->navigate(route_);
    window_->show(); window_->raise(); window_->activateWindow();
}
void GuiHost::dispose() {
    reopen_ = false;
    ++scheduleGeneration_;
    if (window_) { disconnect(window_, nullptr, this, nullptr); delete window_; }
    retiring_ = false; QPixmapCache::clear();
}
}
