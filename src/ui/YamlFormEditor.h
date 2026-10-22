#pragma once
#include "profile/YamlFields.h"
#include <QWidget>
#include <QTimer>
#include <QSet>
#include <QHash>
class QFormLayout; class QLineEdit; class QPlainTextEdit; class QTabWidget; class QLabel; class QPushButton;
namespace mlash {
class YamlFormEditor final : public QWidget {
    Q_OBJECT
public:
    YamlFormEditor(QString kind,QPlainTextEdit* editor,QWidget* parent=nullptr,QLineEdit* identity=nullptr);
    bool busy() const { return active_||!changes_.isEmpty()||readQueued_; }
    bool valid() const { return parsed_&&!busy(); }
    bool readyToStage() const { return valid()&&stageIssue().isEmpty(); }
    QString stageIssue() const;
    QString identity() const { return values_.value("name").toString(); }
    bool unapplied() const { return activePatch_||!changes_.isEmpty(); }
    void setReferenceOptions(const QStringList& members,const QStringList& providers);
    void setSourceFields(const QSet<QString>& fields);
    void setOverrideEnabled(bool enabled);
signals:
    void activityChanged();
private:
    void enqueue(const YamlField& field,const QString& value);
    void pump(); void fill(const QJsonObject& result); void updateState(); void updateOrigins(); void updateFieldVisibility(); void updateHint();
    QString kind_,baseHint_; QFormLayout* form_; QLineEdit* identity_; QPlainTextEdit* editor_; QTabWidget* tabs_; QWidget* formBody_; QLabel* hint_; QPushButton* undo_;
    QHash<QString,QWidget*> controls_,referenceRows_,fieldRows_; QHash<QString,QLabel*> origins_; QSet<QString> sourceFields_; QJsonObject changes_,values_;
    QTimer timer_; quint64 revision_=0; bool active_=false,activePatch_=false,readQueued_=true,parsed_=false,updating_=false,settingText_=false,failed_=false,overrideEnabled_=true;
};
}
