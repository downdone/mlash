#pragma once
#include <QObject>
#include <functional>
class QDialog;
class QWidget;
namespace mlash {
// Only in-memory drafts; closing a parent must not silently destroy child editors.
class DialogDraftGuard final : public QObject {
    Q_OBJECT
public:
    static void protect(QDialog* dialog, std::function<bool()> dirty, std::function<bool()> pending = {});
    static bool hasDrafts(QWidget* root);
    static bool hasPending(QWidget* root);
    static bool allowLeave(QWidget* root);
    // 「取消」语义：有改动时询问是否放弃。窗口关闭不走这里（关闭直接丢弃草稿）。
    static bool confirmDiscard(QDialog* dialog);
protected:
    bool eventFilter(QObject*, QEvent*) override;
private:
    DialogDraftGuard(QDialog* dialog, std::function<bool()> dirty, std::function<bool()> pending);
    QDialog* dialog_;
    std::function<bool()> dirty_, pending_;
};
}
