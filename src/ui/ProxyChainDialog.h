#pragma once
#include "app/AppController.h"
#include <QDialog>
class QListWidget; class QComboBox; class QPushButton; class QLabel;
namespace mlash {
class ProxyChainDialog final:public QDialog {
    Q_OBJECT
public:
    ProxyChainDialog(AppController* controller,const QString& id,QWidget* parent=nullptr);
private:
    void updateSelection(); void updateActions(); void updatePreview();
    AppController* controller_; QString id_; QJsonObject saved_,draft_,observed_,sourceDialers_,groupMembers_;
    bool loaded_=false,saving_=false,changing_=false;
    QListWidget* nodes_; QComboBox* target_; QLabel *hint_,*preview_; QPushButton *save_,*reset_;
};
}
