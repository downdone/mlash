#pragma once
#include "app/AppController.h"
#include <QDialog>
class QLineEdit; class QPushButton; class QLabel; class QPlainTextEdit;
namespace mlash {
class CoreUpdateDialog final:public QDialog {
    Q_OBJECT
public:
    CoreUpdateDialog(AppController* controller,QWidget* parent=nullptr);
    ~CoreUpdateDialog() override;
private:
    void inspect(); void updateActions(); void presentPreview(Result result);
    AppController* controller_; QString token_; bool previewing_=false,saving_=false,cleaning_=false,officialChecked_=false;
    QLineEdit* path_; QPushButton *browse_,*preview_,*apply_,*rollback_,*clean_,*cancel_,*checkOfficial_,*downloadOfficial_; QPlainTextEdit* details_; QLabel* hint_;
};
}
