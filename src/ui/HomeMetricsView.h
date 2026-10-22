#pragma once
#include "app/HomeMetrics.h"
#include <QFrame>
class QLabel;
namespace mlash {
class HomeMetricsView final : public QFrame {
    Q_OBJECT
public:
    explicit HomeMetricsView(const HomeMetrics* metrics,QWidget* parent=nullptr);
signals:
    void inspectRequested();
private:
    void refresh();
    const HomeMetrics* metrics_;
    QList<QLabel*> values_;
    QLabel* totals_;
    QWidget* graph_;
};
}
