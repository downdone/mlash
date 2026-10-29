#pragma once
#include "app/AppController.h"
#include <QDialog>
class QLineEdit; class QPushButton; class QLabel; class QPlainTextEdit; class QTableWidget;
namespace mlash {
class CoreUpdateDialog final:public QDialog {
    Q_OBJECT
public:
    CoreUpdateDialog(AppController* controller,QWidget* parent=nullptr);
    ~CoreUpdateDialog() override;
private:
    void inspect(); void updateActions(); void presentPreview(Result result); void rebuildHistory();
    AppController* controller_; QString token_; bool previewing_=false,saving_=false,cleaning_=false,officialChecked_=false;
    QLineEdit* path_; QPushButton *browse_,*preview_,*apply_,*rollback_,*clean_,*cancel_,*checkOfficial_,*downloadOfficial_,*useHistory_; QPlainTextEdit* details_; QLabel* hint_; QTableWidget* history_;
};
}
