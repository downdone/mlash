#pragma once
#include "app/AppController.h"
#include <QWidget>

class QDialog; class QToolButton; class QComboBox; class QLabel; class QPushButton; class QGridLayout; class QFrame;
namespace mlash {
class WebsiteTestsPage final : public QWidget {
    Q_OBJECT
public:
    explicit WebsiteTestsPage(AppController* controller,QWidget* parent=nullptr);
    void setActive(bool active,const QString& suggestedGroup={});
    void suggestGroup(const QString& group);
protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    void updateNodes();
    void updateResults();
    void reflow();
    void rebuildCards();
    void showResult(const QString& id);
    QPointer<QDialog> resultDetails_;
    AppController* controller_;
    bool active_=false;
    bool userPicked_=false;
    QString suggestedGroup_;
    QComboBox* nodes_;
    QPushButton *start_,*clear_;
    QLabel *progress_,*lastTest_,*lastTestIcon_;
    QWidget* cards_;
    QGridLayout* grid_;
    struct Card { QString id; QFrame* box; QLabel *value,*state,*dot; QPushButton* test; QToolButton* details; };
    QList<Card> cardWidgets_;
    int columns_=0;
};
}
