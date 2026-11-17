#pragma once
#include "domain/Types.h"
namespace mlash { class AppController; }
#include "TaskDialog.h"
class QPushButton; class QComboBox; class QPlainTextEdit; class QLabel; class QTabWidget; class QTableWidget;
namespace mlash {
class OutboundDialog final : public TaskDialog {
    Q_OBJECT
public:
    explicit OutboundDialog(AppController* controller,QWidget* parent=nullptr);
    ~OutboundDialog() override;
private:
    void updateView();
    void showServiceEvidence();
    QPointer<QDialog> serviceEvidence_;
    AppController* controller_;
    QComboBox* route_;
    QPushButton *start_,*cancel_;
    QLabel* value_;
    QPlainTextEdit* details_;
    QTabWidget* tabs_;
    QTableWidget* services_;
    QPushButton *retest_,*evidence_;
    QLabel* serviceHint_;
};
}
