#pragma once
#include "app/AppController.h"
#include <QDialog>

class QTableWidget; class QLabel; class QTabWidget; class QPushButton;
namespace mlash {
class RuleOverlayDialog final : public QDialog {
    Q_OBJECT
public:
    RuleOverlayDialog(AppController* controller, const QString& profileId, QWidget* parent = nullptr,const QJsonObject& draft={});
private:
    QTableWidget* editableTable(const QString& name, const QStringList& rules);
    void appendRule(QTableWidget* table, const QString& rule);
    QStringList rules(QTableWidget* table) const;
    void moveRow(QTableWidget* table, int delta);
    void updateSave();
    void rebuildProviderDefinitions();
    void editProviderDefinition(bool adding);
    AppController* controller_;
    QString id_;
    QStringList savedDisabled_;
    QStringList savedPrepend_, savedAppend_;
    QTableWidget *prepend_, *append_, *source_;
    QTabWidget* tabs_;
    QLabel* hint_;
    QPushButton* save_;
    bool saving_ = false, loaded_ = false;
    QList<QWidget*> mutations_;
    QJsonObject expected_;
    QStringList targets_,providers_;
    QString sourceHint_;
    QString operationError_;
    QJsonObject providerEdits_,savedProviderEdits_,sourceProviderDefinitions_;
    QTableWidget* providerDefinitions_;
    QList<QWidget*> providerActions_;
    int sourceRuleCount_=0;
};
}
