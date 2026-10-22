#pragma once
#include "app/AppController.h"
#include <QWidget>
class QPlainTextEdit; class QTableWidget; class QPushButton; class QLabel;
namespace mlash {
class NodeLinksEditor final : public QWidget {
    Q_OBJECT
public:
    explicit NodeLinksEditor(AppController* controller,QWidget* parent=nullptr,const QStringList& reserved={});
    QString text() const;
    void setText(const QString& text);
    void bindController(AppController* controller) { controller_=controller; }
    bool ready() const { return !result_.isEmpty()&&!checking_; }
    bool checking() const { return checking_; }
    QJsonObject definitions() const { return result_.value("definitions").toObject(); }
signals:
    void changed();
private:
    void check();
    AppController* controller_;
    QStringList reserved_;
    QJsonObject result_;
    QPlainTextEdit* input_; QTableWidget* preview_; QPushButton* check_; QLabel* hint_;
    bool checking_=false;
    quint64 revision_=0;
};
}
