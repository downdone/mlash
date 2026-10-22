#pragma once
#include "app/AppController.h"
#include <QDialog>
class QLabel; class QPushButton; class QComboBox; class QCheckBox; class QPlainTextEdit; class QSpinBox;
namespace mlash {
class TunDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TunDialog(AppController* controller,QWidget* parent=nullptr);
private:
    void inspect();
    void updateActions();
    Settings draft() const;
    void showInterfaces();
    AppController* controller_;
    QLabel *status_,*hint_,*validation_;
    QPlainTextEdit *routes_,*details_;
    QSpinBox* mtu_;
    QComboBox* stack_;
    QCheckBox *enabled_,*strict_,*dnsHijack_;
    QPushButton *install_,*remove_,*refresh_,*apply_,*repair_,*cleanup_;
    Settings initial_;
    QJsonObject service_;
    bool installed_=false,known_=false,loading_=false,waiting_=false,applyingOptions_=false,cleaning_=false;
};
}
