#pragma once
#include "app/AppController.h"
#include <QDialog>
class QTableWidget; class QLabel; class QPushButton; class QComboBox;
namespace mlash {
class GeoDataDialog final : public QDialog {
    Q_OBJECT
public:
    explicit GeoDataDialog(AppController* controller,QWidget* parent=nullptr);
private:
    void reload();
    void updateActions();
    AppController* controller_;
    QTableWidget* table_;
    QLabel* hint_;
    QPushButton *refresh_,*update_,*import_;
    QComboBox* file_;
    bool loading_=false,writing_=false;
};
}
