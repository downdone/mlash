#pragma once
#include "domain/Types.h"
namespace mlash { class AppController; }
#include "TaskDialog.h"
class QLabel; class QTableWidget; class QPlainTextEdit; class QPushButton;
namespace mlash {
class ProfileChangesDialog final : public TaskDialog {
    Q_OBJECT
public:
    ProfileChangesDialog(AppController* controller,const QString& id,QWidget* parent=nullptr);
private:
    void reload(); void updateActions(); void showSection(int row);
    AppController* controller_; QString id_,candidate_;
    QLabel* hint_; QTableWidget* table_; QPlainTextEdit* details_; QPushButton* apply_;
    QJsonObject report_; bool loading_=false,applying_=false;
};
}
