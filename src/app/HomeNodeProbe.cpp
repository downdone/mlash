#include "HomeNodeProbe.h"
#include <QDateTime>
#include <QNetworkReply>
namespace mlash {
HomeNodeProbe::HomeNodeProbe(MihomoClient* api,QObject* parent,int unit):QObject(parent),api_(api),unit_(qMax(1,unit)) { timer_.setSingleShot(true); connect(&timer_,&QTimer::timeout,this,&HomeNodeProbe::refresh); }
HomeNodeProbe::~HomeNodeProbe() { ++token_; timer_.stop(); if (request_) request_->abort(); }
void HomeNodeProbe::reset() { ++token_; timer_.stop(); const auto request=request_; request_=nullptr; if (request) request->abort(); if (!result_.isEmpty()) { result_={}; emit changed(); } }
void HomeNodeProbe::setVisible(bool visible) { if (visible_==visible) return; visible_=visible; reset(); schedule(); }
void HomeNodeProbe::setContext(bool ready,const QString& node,quint64 generation,const QString& url,int interval) { if (ready_==ready&&node_==node&&generation_==generation&&url_==url&&interval_==interval) return; ready_=ready; node_=node; generation_=generation; url_=url; interval_=QList<int>{0,60,300}.contains(interval)?interval:0; reset(); schedule(); }
void HomeNodeProbe::schedule() { if (visible_&&ready_&&!node_.isEmpty()&&interval_>0) timer_.start(result_.isEmpty()?0:interval_*unit_); }
void HomeNodeProbe::refresh() {
    if (!visible_||!ready_||node_.isEmpty()||!interval_||request_) return; timer_.stop(); const auto token=++token_; const auto node=node_; result_={{"node",node},{"pending",true}}; emit changed();
    request_=api_->delay(node,[this,token,node](Result observed) { if (token!=token_) return; request_=nullptr; const int delay=observed.data.value("delay").toInt(-1); const bool ok=observed.ok&&delay>0; const auto error=ok?QString():observed.error.isEmpty()?QString("本次探测失败"):observed.error.left(256); result_={{"node",node},{"pending",false},{"delay",ok?delay:-1},{"error",error},{"measured_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}}; emit changed(); emit measured(node,ok?delay:-1,error); schedule(); },5000,url_);
}
}
