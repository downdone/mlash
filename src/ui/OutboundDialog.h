#pragma once
#include "app/AppController.h"
#include <QDialog>
class QPushButton; class QComboBox; class QPlainTextEdit; class QLabel; class QTabWidget; class QTableWidget;
namespace mlash {
class OutboundDialog final : public QDialog {
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
