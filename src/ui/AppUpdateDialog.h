#pragma once
#include "TaskDialog.h"
#include <QJsonObject>
class QLabel; class QPushButton;
namespace mlash {
class AppController;
class AppUpdateDialog final : public TaskDialog {
    Q_OBJECT
public:
    explicit AppUpdateDialog(AppController* controller,QWidget* parent=nullptr);
    ~AppUpdateDialog() override;
private:
    void check();
    void download();
    void showCandidate(const QJsonObject& candidate);
    AppController* controller_;
    QJsonObject available_;
    QLabel *status_,*version_,*detail_;
    QPushButton *check_,*download_,*open_,*release_,*cancel_;
    bool working_=false;
};
}
