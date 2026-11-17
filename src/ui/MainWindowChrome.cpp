#include "MainWindow.h"
#include "app/AppController.h"
#include "design/ShadowrocketStyle.h"
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
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
    chromeLeading_->setFixedWidth(slotWidth);
    chromeTrailing_->setFixedWidth(slotWidth);
    for (const char* name:{"windowMinimize","windowMaximize","windowClose"})
        if (auto* button=findChild<QToolButton*>(name)) button->setVisible(desired);
    for (const char* name:{"chromeMark","chromeBrand"})
        if (auto* label=findChild<QLabel*>(name)) label->setVisible(desired);
    if (bool(windowFlags() & Qt::FramelessWindowHint)==desired) return;
    const auto oldGeometry=geometry(); const bool maximized=isMaximized(); const bool visible=isVisible();
    setWindowFlag(Qt::FramelessWindowHint,desired);
    if (visible) { maximized?showMaximized():showNormal(); if (!maximized) setGeometry(oldGeometry); }
}

bool MainWindow::eventFilter(QObject* watched,QEvent* event) {
    if (integratedChrome_ && (watched==chromeDragArea_ || watched==chromeLeading_ || watched==topbarTitle_ || watched->property("chromeDrag").toBool())) {
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
