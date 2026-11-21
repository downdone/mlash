#pragma once
#include "domain/Types.h"
#include <functional>
namespace mlash { class AppController; }
#include "TaskDialog.h"
class QLabel; class QTableWidget; class QTextEdit; class QPushButton;
namespace mlash {
QString runningProfileVersion(const AppController& controller);
void confirmProfileVersion(QWidget* parent, const QString& action, const Profile& profile,
                           const QString& version, bool discardDraft, std::function<void()> confirmed);

class ProfileChangesDialog final : public TaskDialog {
    Q_OBJECT
public:
    ProfileChangesDialog(AppController* controller,const QString& id,QWidget* parent=nullptr);
private:
    void reload();
    void updateActions();
    void showSection(int row);
    void confirmApply();
    const Profile* currentProfile() const;

    AppController* controller_;
    QString id_;
    QLabel* versions_;
    QLabel* metadata_;
    QLabel* hint_;
    QTableWidget* table_;
    QTextEdit* details_;
    QPushButton* apply_;
    QPushButton* refresh_;
    QJsonObject report_;
    QString feedback_;
    bool loading_ = false;
    bool applying_ = false;
};
}
