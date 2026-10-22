#pragma once
#include "app/AppController.h"
#include <QWidget>
#include <QSortFilterProxyModel>
#include <QTimer>
class QTableView; class QLineEdit; class QToolButton; class QLabel; class QAction;
namespace mlash {
class ConnectionOptions final : public QWidget {
    Q_OBJECT
public:
    ConnectionOptions(AppController* controller,QTableView* table,QSortFilterProxyModel* filter,QLineEdit* search,QLabel* error,QWidget* parent=nullptr);
    ~ConnectionOptions() override;
    QJsonObject preferences() const;
    void flushPreferences();
    void addViewAction(QAction* action);
signals:
    void searchFeedback(const QString& error);
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    void applySearch();
    void reportSearchError(const QString& error);
    void changed();
    void save();
    void fitDefault();
    void defaults();
    AppController* controller_;
    QTableView* table_;
    QSortFilterProxyModel* filter_;
    QLineEdit* search_;
    QLabel* error_;
    QToolButton *case_,*word_,*regex_;
    QMap<int,int> widths_;
    QTimer debounce_;
    bool applying_=true,custom_=false,dirty_=false;
    QString searchError_;
};
}
