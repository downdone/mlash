#pragma once
#include "app/AppController.h"
#include <QDialog>

class QCheckBox; class QPlainTextEdit; class QLineEdit; class QComboBox; class QPushButton; class QLabel;
namespace mlash {
class YamlFormEditor;
class DnsDialog final : public QDialog {
    Q_OBJECT
public:
    DnsDialog(AppController* controller, const QString& profileId, QWidget* parent = nullptr);
    ~DnsDialog() override;
private:
    void updateActions();
    AppController* controller_;
    QString id_, saved_;
    QJsonObject expected_;
    bool savedEnabled_ = false, saving_ = false;
    quint64 queryToken_ = 0;
    QCheckBox* enabled_;
    YamlFormEditor* fields_;
    QPlainTextEdit *editor_, *source_, *effective_, *answer_;
    QLineEdit* name_;
    QComboBox* type_;
    QPushButton *save_, *query_, *inherit_;
    QLabel* hint_;
};
}
