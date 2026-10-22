#pragma once
#include "app/AppController.h"
#include <QDialog>
class QComboBox; class QLabel; class QPushButton; class QToolButton;
namespace mlash {
class ConnectionDetailsDialog final : public QDialog {
    Q_OBJECT
public:
    ConnectionDetailsDialog(AppController* controller,QJsonObject row,QString profile,QString version,QWidget* parent=nullptr);
    static QJsonArray ruleSuggestions(const QJsonObject& row);
signals:
    void browseGroup(const QString& group);
    void createRule(const QJsonObject& suggestion);
private:
    bool contextCurrent() const;
    QString target() const;
    void updateContext();
    AppController* controller_;
    QJsonObject row_;
    QString profile_,version_,feedback_;
    QComboBox* groups_;
    QLabel* hint_;
    QPushButton *locate_,*rule_,*disconnect_;
    QList<QToolButton*> copies_;
    bool disconnecting_=false;
};
}
