#include "GuiHost.h"
#include <QTimer>
#include <QPixmapCache>

namespace mlash {
GuiHost::GuiHost(AppController* controller, bool trayAvailable, QObject* parent) : QObject(parent), controller_(controller), trayAvailable_(trayAvailable) {}
GuiHost::~GuiHost() { dispose(); }
void GuiHost::requestExit() { if (!window_ || window_->confirmDiscardEdits()) controller_->requestExit(); }
void GuiHost::show(int page) {
    if (page >= 0 && page < 8) page_ = page;
    if (page_<0) page_=controller_->settings().startupPage;
    if (retiring_) { reopen_ = true; return; }
    if (!window_) {
        window_ = new MainWindow(controller_); window_->setAttribute(Qt::WA_DeleteOnClose); window_->setTrayAvailable(trayAvailable_);
        if (!geometry_.isEmpty()) window_->restoreGeometry(geometry_);
        if (memberView_>=0) window_->setMemberView(memberView_);
        window_->restoreNodeBookmark(nodeBookmark_);
        window_->setPage(page_);
        connect(window_, &MainWindow::guiReleaseRequested, this, [this] { geometry_ = window_->saveGeometry(); page_ = window_->page(); memberView_=window_->memberView(); nodeBookmark_=window_->nodeBookmark(); retiring_ = true; });
        connect(window_, &QObject::destroyed, this, [this] {
            retiring_ = false; QPixmapCache::clear();
            if (reopen_) { reopen_ = false; const auto token = scheduleGeneration_; QTimer::singleShot(0, this, [this, token] { if (token == scheduleGeneration_) show(page_); }); }
        });
    } else if (page >= 0) window_->setPage(page_);
    window_->show(); window_->raise(); window_->activateWindow();
}
void GuiHost::dispose() {
    reopen_ = false;
    ++scheduleGeneration_;
    if (window_) { disconnect(window_, nullptr, this, nullptr); delete window_; }
    retiring_ = false; QPixmapCache::clear();
}
}
