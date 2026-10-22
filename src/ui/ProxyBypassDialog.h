#pragma once
#include "app/AppController.h"
#include <QDialog>
class QPlainTextEdit; class QPushButton; class QLabel;
namespace mlash {
class ProxyBypassDialog final:public QDialog {
    Q_OBJECT
public:
    ProxyBypassDialog(AppController* controller,QWidget* parent=nullptr);
private:
    void updateActions();
    AppController* controller_; QString saved_; bool saving_=false;
    QPlainTextEdit* editor_; QPushButton *save_,*defaults_; QLabel* hint_;
};
}
