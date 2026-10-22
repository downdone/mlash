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
protected:
    bool eventFilter(QObject*, QEvent*) override;
private:
    DialogDraftGuard(QDialog* dialog, std::function<bool()> dirty, std::function<bool()> pending);
    QDialog* dialog_;
    std::function<bool()> dirty_, pending_;
};
}
