#pragma once
#include "domain/Types.h"
namespace mlash { class AppController; }
#include "TaskDialog.h"

class QCheckBox; class QPlainTextEdit; class QLineEdit; class QComboBox; class QPushButton; class QLabel; class QTabWidget;
namespace mlash {
class YamlFormEditor;
class DnsDialog final : public TaskDialog {
    Q_OBJECT
public:
    DnsDialog(AppController* controller, const QString& profileId, QWidget* parent = nullptr);
    ~DnsDialog() override;
signals:
    void compactModeChanged();
private:
    void updateActions();
    void updateSourceHint();
    void inspectSource();
    AppController* controller_;
    QString id_, saved_, sourceError_;
    QJsonObject expected_;
    bool savedEnabled_ = false, saving_ = false;
    bool inspected_ = false, observedOverridden_ = false;
    quint64 queryToken_ = 0;
    QCheckBox* enabled_;
    YamlFormEditor* fields_;
    QPlainTextEdit *editor_, *source_, *effective_, *answer_;
    QLineEdit* name_;
    QComboBox* type_;
    QPushButton *save_, *query_, *inherit_, *retry_;
    QLabel* hint_;
    QTabWidget* tabs_;
};
}
