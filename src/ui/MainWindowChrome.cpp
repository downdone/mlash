#include "MainWindow.h"
#include "app/AppController.h"
#include "design/ShadowrocketStyle.h"
#include <QEvent>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QWindow>
#ifdef Q_OS_WIN
#include "platform/windows/WindowChromeNative.h"
#endif

namespace mlash {
void MainWindow::applyWindowChrome() {
    const bool desired=controller_->settings().windowChrome=="integrated";
    integratedChrome_=desired;
    const int slotWidth=desired?176:design::ShadowrocketStyle::geometry.topbarActionSlotWidth;
    chromeTrailing_->setFixedWidth(slotWidth);
    for (const char* name:{"windowMinimize","windowMaximize","windowClose"})
        if (auto* button=findChild<QToolButton*>(name)) button->setVisible(desired);
    if (bool(windowFlags() & Qt::FramelessWindowHint)==desired) return;
    const auto oldGeometry=geometry(); const bool maximized=isMaximized(); const bool visible=isVisible();
    setWindowFlag(Qt::FramelessWindowHint,desired);
    if (visible) { maximized?showMaximized():showNormal(); if (!maximized) setGeometry(oldGeometry); }
}

bool MainWindow::eventFilter(QObject* watched,QEvent* event) {
    if (profiles_ && watched==profiles_ && event->type()==QEvent::KeyPress) {
        profiles_->setProperty("keyboardFocus",true); profiles_->viewport()->update();
    }
    if (profiles_ && watched==profiles_->viewport() && event->type()==QEvent::MouseButtonPress) {
        profiles_->setProperty("keyboardFocus",false); profiles_->viewport()->update();
    }
    if (profiles_ && watched==profiles_->viewport() && event->type()==QEvent::Resize) updateProfileGrid();
    if (integratedChrome_ && (watched==chromeDragArea_ || watched==topbarTitle_)) {
        if (event->type()==QEvent::MouseButtonDblClick) {
            auto* mouse=static_cast<QMouseEvent*>(event);
            if (mouse->button()==Qt::LeftButton) { isMaximized()?showNormal():showMaximized(); return true; }
        }
        if (event->type()==QEvent::MouseButtonPress) {
            auto* mouse=static_cast<QMouseEvent*>(event);
            if (mouse->button()==Qt::LeftButton && windowHandle()) { windowHandle()->startSystemMove(); return true; }
        }
    }
    return QMainWindow::eventFilter(watched,event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType,void* message,qintptr* result) {
#ifdef Q_OS_WIN
    if (integratedChrome_ && WindowsWindowChrome::resizeHitTest(message,result)) return true;
#endif
    return QMainWindow::nativeEvent(eventType,message,result);
}
}
