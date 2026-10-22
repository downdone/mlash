#pragma once
#include "app/AppController.h"
#include <QDialog>
class QLabel; class QPushButton;
namespace mlash {
class ProtocolSettingsDialog final:public QDialog {
    Q_OBJECT
public:
    ProtocolSettingsDialog(AppController* controller,QWidget* parent=nullptr);
private:
    void refresh(const QString& feedback={});
    AppController* controller_; QLabel* status_; QPushButton *enable_,*disable_; bool saving_=false,reading_=false;
};
}
