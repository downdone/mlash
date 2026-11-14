#include "DialogDraftGuard.h"
#include <QDialog>
#include <QEvent>
#include <QCloseEvent>
#include <QMessageBox>
namespace mlash {
DialogDraftGuard::DialogDraftGuard(QDialog* dialog, std::function<bool()> dirty, std::function<bool()> pending)
    : QObject(dialog), dialog_(dialog), dirty_(std::move(dirty)), pending_(std::move(pending)) { dialog->installEventFilter(this); }
void DialogDraftGuard::protect(QDialog* dialog, std::function<bool()> dirty, std::function<bool()> pending) { new DialogDraftGuard(dialog,std::move(dirty),std::move(pending)); }
bool DialogDraftGuard::hasDrafts(QWidget* root) { for (auto* guard:root->findChildren<DialogDraftGuard*>()) if ((!guard->dialog_->isHidden()||guard->dialog_->property("embeddedTask").toBool())&&guard->dirty_&&guard->dirty_()) return true; return false; }
bool DialogDraftGuard::hasPending(QWidget* root) { for (auto* guard:root->findChildren<DialogDraftGuard*>()) if ((!guard->dialog_->isHidden()||guard->dialog_->property("embeddedTask").toBool())&&guard->pending_&&guard->pending_()) return true; return false; }
bool DialogDraftGuard::allowLeave(QWidget* root) {
    if (hasPending(root)) return false;
    if (!hasDrafts(root)) return true;
    return QMessageBox::question(root,"未保存的修改","放弃此任务尚未保存的修改？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;
}
bool DialogDraftGuard::confirmDiscard(QDialog* dialog) {
    return QMessageBox::question(dialog,"未保存的修改","取消会放弃此窗口的修改，是否继续？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;
}
bool DialogDraftGuard::eventFilter(QObject*,QEvent* event) {
    // 生命周期契约（分层设计 4「工具窗口骨架」）：
    //  - 窗口「关闭」= 放弃未提交编辑并直接关闭，不询问；
    //  - 「取消」/Esc = 有改动时先询问。
    // 因此这里只拦「取消」与 Esc，窗口关闭交给 closeEvent 直接丢弃草稿。
    if (dialog_->property("embeddedTask").toBool()) return false;
    if (event->type()==QEvent::Close) return false;
    const bool cancelling = event->type()==QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape;
    const bool rejecting = event->type()==QEvent::ActionChanged
        && static_cast<QActionEvent*>(event)->action()
        && qobject_cast<QDialogButtonBox*>(static_cast<QActionEvent*>(event)->action()->parent());
    if (!cancelling && !rejecting) return false;
    if (pending_&&pending_()) return true;              // 写事务在跑：忽略取消，由事务安全结束
    if (dirty_&&dirty_()&&!confirmDiscard(dialog_)) return true;
    return false;
}
}
