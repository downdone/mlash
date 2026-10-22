#include "DialogDraftGuard.h"
#include <QDialog>
#include <QEvent>
#include <QCloseEvent>
#include <QMessageBox>
namespace mlash {
DialogDraftGuard::DialogDraftGuard(QDialog* dialog, std::function<bool()> dirty, std::function<bool()> pending)
    : QObject(dialog), dialog_(dialog), dirty_(std::move(dirty)), pending_(std::move(pending)) { dialog->installEventFilter(this); }
void DialogDraftGuard::protect(QDialog* dialog, std::function<bool()> dirty, std::function<bool()> pending) { new DialogDraftGuard(dialog,std::move(dirty),std::move(pending)); }
bool DialogDraftGuard::hasDrafts(QWidget* root) { for (auto* guard:root->findChildren<DialogDraftGuard*>()) if (!guard->dialog_->isHidden()&&guard->dirty_&&guard->dirty_()) return true; return false; }
bool DialogDraftGuard::hasPending(QWidget* root) { for (auto* guard:root->findChildren<DialogDraftGuard*>()) if (!guard->dialog_->isHidden()&&guard->pending_&&guard->pending_()) return true; return false; }
bool DialogDraftGuard::eventFilter(QObject*,QEvent* event) {
    if (event->type()!=QEvent::Close) return false;
    if (pending_&&pending_()) { static_cast<QCloseEvent*>(event)->ignore(); return true; }
    if (dirty_&&dirty_()&&QMessageBox::question(dialog_,"未保存的修改","关闭会放弃此窗口的修改，是否继续？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) { static_cast<QCloseEvent*>(event)->ignore(); return true; }
    return false;
}
}
