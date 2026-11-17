#pragma once
#include "domain/Types.h"
#include "domain/WebsiteTargets.h"
namespace mlash { class AppController; }
#include "TaskDialog.h"

class QTableWidget; class QLabel;
namespace mlash {
class WebsiteTargetsDialog final : public TaskDialog {
    Q_OBJECT
public:
    explicit WebsiteTargetsDialog(AppController* controller, QWidget* parent = nullptr);
private:
    void load(const QList<WebsiteTarget>& targets);
    QJsonArray definitions() const;
    void moveRow(int delta);
    AppController* controller_;
    QTableWidget* table_;
    QLabel* hint_;
    bool saving_ = false;
    QList<QWidget*> mutations_;
};
}
