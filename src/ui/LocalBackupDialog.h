#pragma once
#include "app/AppController.h"
#include <QDialog>
class QCheckBox; class QSpinBox; class QLineEdit; class QPushButton; class QLabel; class QTableWidget;
namespace mlash {
class LocalBackupDialog final : public QDialog {
    Q_OBJECT
public:
    explicit LocalBackupDialog(AppController* controller,QWidget* parent=nullptr);
private:
    QJsonObject draft() const;
    void updateActions();
    void refresh();
    AppController* controller_;
    QJsonObject saved_,displayed_;
    bool saving_=false,working_=false,listing_=false;
    QCheckBox *enabled_,*before_;
    QSpinBox *hours_,*retain_;
    QLineEdit *password_,*confirm_;
    QPushButton *save_,*clear_,*run_,*refresh_,*restore_;
    QLabel* hint_;
    QTableWidget* files_;
};
}
