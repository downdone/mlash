#pragma once
#include <QWidget>
#include <QPointer>
#include <QTimer>
#include "Route.h"
class QLineEdit; class QMenu; class QPushButton; class QScrollArea;
namespace mlash {
class SettingsSearch final : public QWidget {
    Q_OBJECT
public:
    SettingsSearch(QWidget* catalog,QScrollArea* scroll,QWidget* parent=nullptr);
    void addRoute(const QString& title,const QString& terms,int page);
    // 路由级条目：title/terms 用于检索，route 决定跳转目标（见 Route.h）。
    void addRoute(const QString& title,const QString& terms,Route route);
    // Directory-owned controls do not need a duplicate hidden editor row to be searchable.
    void addAnchor(const QString& title,const QString& terms,QWidget* anchor);
    void remapAnchor(const QString& settingName,QWidget* anchor);
signals:
    void pageRequested(int page);
    void routeRequested(Route route);
    void anchorRequested(QWidget* anchor);
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
private:
    struct Entry { QString title,terms; QPointer<QWidget> anchor; int page=-1; Route route; bool hasRoute=false; };
    void refresh();
    void locate(int match);
    void clearMark();
    void showMatches();
    QList<Entry> entries_;
    QList<int> matches_;
    int current_=-1;
    QLineEdit* search_;
    QPushButton* count_;
    QMenu* menu_;
    QScrollArea* scroll_;
    QPointer<QWidget> marked_;
    QTimer markTimer_;
};
}
