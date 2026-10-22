#pragma once
#include "app/AppController.h"
#include <QDialog>
class QLineEdit; class QComboBox; class QSpinBox; class QCheckBox; class QPushButton; class QLabel; class QTableWidget;
namespace mlash {
class RemoteBackupDialog final:public QDialog {
    Q_OBJECT
public:
    RemoteBackupDialog(AppController* controller,QWidget* parent=nullptr);
private:
    QJsonObject draft() const; void updateActions(); void loadSaved();
    AppController* controller_; QJsonObject saved_,displayed_; bool saving_=false,listing_=false;
    QLineEdit *url_,*user_,*password_,*backupPassword_,*confirm_;
    QComboBox* route_; QSpinBox* hours_; QCheckBox *enabled_,*private_;
    QPushButton *save_,*clear_,*upload_,*list_,*download_,*cancel_; QLabel *hint_,*last_;
    QTableWidget* files_;
};
}
