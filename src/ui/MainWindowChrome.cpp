#include "MainWindow.h"
#include "app/AppController.h"
#include "design/ShadowrocketStyle.h"
#include "ProfileCardDelegate.h"
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
    if (profiles_ && watched==profiles_->viewport() && event->type()==QEvent::Resize) updateProfileGrid();
    if (profiles_ && watched==profiles_->viewport() &&
        (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonRelease || event->type()==QEvent::MouseButtonDblClick)) {
        auto* mouse=static_cast<QMouseEvent*>(event);
        if (mouse->button()==Qt::LeftButton) {
            const auto index=profiles_->indexAt(mouse->pos());
            if (index.isValid()) {
                if (!ProfileCardDelegate::card(profiles_->visualRect(index)).contains(mouse->pos())) return true;
                const auto hit=ProfileCardDelegate::hit(profiles_->visualRect(index),mouse->pos(),index);
                if (hit!=ProfileCardDelegate::Hit::Use) {
                    if (event->type()==QEvent::MouseButtonRelease) {
                        const auto id=profileAt(index);
                        profiles_->setCurrentIndex(index);
                        switch (hit) {
                        case ProfileCardDelegate::Hit::Refresh:
                            if (!controller_->state().busy && !controller_->state().readOnly) controller_->updateProfile(id);
                            break;
                        case ProfileCardDelegate::Hit::Menu:
                            showProfileMenu(id,profiles_->viewport()->mapToGlobal(mouse->pos())); break;
                        case ProfileCardDelegate::Hit::Review:
                            openProfileTask(id,"candidate"); break;
                        case ProfileCardDelegate::Hit::Apply:
                            if (!controller_->state().busy && !controller_->state().readOnly &&
                                index.data(ProfileRowModel::HasCandidate).toBool())
                                controller_->activateProfile(id);
                            break;
                        default: break;
                        }
                    }
                    return true;
                }
            }
        }
    }
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
