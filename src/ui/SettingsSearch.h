#pragma once
#include <QWidget>
#include <QPointer>
#include <QTimer>
class QLineEdit; class QMenu; class QPushButton; class QScrollArea; class QShortcut;
namespace mlash {
class SettingsSearch final : public QWidget {
    Q_OBJECT
public:
    SettingsSearch(QWidget* catalog,QScrollArea* scroll,QWidget* parent=nullptr);
    void addRoute(const QString& title,const QString& terms,int page);
signals:
    void pageRequested(int page);
    void anchorRequested(QWidget* anchor);
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
private:
    struct Entry { QString title,terms; QPointer<QWidget> anchor; int page=-1; };
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
    QShortcut* shortcut_;
    QPointer<QWidget> marked_;
    QTimer markTimer_;
};
}
