#include "HomeMetricsView.h"
#include "ActivityTableModel.h"
#include "UiTheme.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
namespace mlash {
namespace {
class TrafficGraph final : public QWidget {
public:
    const HomeMetrics* metrics;
    explicit TrafficGraph(const HomeMetrics* source) : metrics(source) { setObjectName("homeTrafficGraph"); setFixedHeight(26); setAccessibleName("最近60秒流量曲线，上传与下载"); }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); const auto& history=metrics->history(); double maximum=1;
        for (const auto& value:history) for (const auto* key:{"upload","download"}) maximum=qMax(maximum,value.toObject().value(key).toDouble());
        for (const auto* key:{"upload","download"}) {
            QPainterPath path; bool segment=false; const auto end=history.isEmpty()?0:history.last().toObject().value("elapsed_ms").toDouble();
            for (int i=0;i<history.size();++i) { const auto row=history[i].toObject(); const auto value=row.value(key); if (!value.isDouble()) { segment=false; continue; } const QPointF point(1+(1-(end-row.value("elapsed_ms").toDouble())/60000)*qMax(1,width()-2),height()-2-value.toDouble()*(height()-4)/maximum); if (segment) path.lineTo(point); else { path.moveTo(point); segment=true; } }
            painter.setPen(QPen(QColor(QString(key)=="upload"?UiTheme::Warning:UiTheme::Brand),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); painter.drawPath(path);
        }
    }
};
}
HomeMetricsView::HomeMetricsView(const HomeMetrics* metrics,QWidget* parent) : QFrame(parent),metrics_(metrics) {
    setObjectName("homeMetrics"); setProperty("panel",true); setFrameShape(QFrame::NoFrame); setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(14,9,14,8); layout->setSpacing(4); auto* stats=new QHBoxLayout; stats->setSpacing(14); const QStringList names{"↑ 上传","↓ 下载","活动连接","内核工作集"};
    for (int i=0;i<names.size();++i) { auto* column=new QVBoxLayout; column->setSpacing(2); auto* title=new QLabel(names[i]); title->setObjectName("secondaryText"); column->addWidget(title); auto* value=new QLabel("—"); value->setObjectName("homeMetric"+QString::number(i)); value->setProperty("metricValue",true); value->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); auto font=value->font(); font.setPointSize(UiTheme::SectionPoints); font.setWeight(QFont::DemiBold); value->setFont(font); value->setAccessibleName(names[i]); values_.append(value); column->addWidget(value); stats->addLayout(column,1); } layout->addLayout(stats);
    graph_=new TrafficGraph(metrics); layout->addWidget(graph_); auto* footer=new QHBoxLayout; totals_=new QLabel; totals_->setObjectName("secondaryText"); totals_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); footer->addWidget(totals_,1); auto* inspect=new QPushButton("出口诊断…"); inspect->setObjectName("openOutboundDiagnostics"); connect(inspect,&QPushButton::clicked,this,&HomeMetricsView::inspectRequested); footer->addWidget(inspect); layout->addLayout(footer); connect(metrics,&HomeMetrics::changed,this,[this] { refresh(); }); connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,[this] { graph_->update(); }); refresh();
}
void HomeMetricsView::refresh() {
    const auto& snapshot=metrics_->snapshot(); const auto amount=[&](const QString& key,bool rate=false) { const auto value=snapshot.value(key); return value.isDouble()?ActivityTableModel::formatBytes(value.toDouble())+(rate?"/s":""):QString("—"); };
    values_[0]->setText(amount("uploadTotal_rate",true)); values_[1]->setText(amount("downloadTotal_rate",true)); values_[2]->setText(snapshot.contains("active_count")?QString::number(snapshot.value("active_count").toInt()):"—"); values_[3]->setText(amount("core_working_set")); values_[3]->setToolTip("受控内核进程的工作集；不含Mlash界面内存，也不是订阅流量用量");
    const QString detail="累计 ↑ "+amount("uploadTotal")+" · ↓ "+amount("downloadTotal")+"  ·  "+(metrics_->sampling()?QString("最近60秒 · 2秒刷新"):QString("首页可见且内核就绪时采样")); totals_->setText("累计 ↑ "+amount("uploadTotal")+" · ↓ "+amount("downloadTotal")); totals_->setToolTip(detail+(metrics_->error().isEmpty()?QString():"\n"+metrics_->error())); graph_->update();
}
}
