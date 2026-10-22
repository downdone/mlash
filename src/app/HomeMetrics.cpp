#include "HomeMetrics.h"
#include "Async.h"
#include "diagnostics/ProcessMetrics.h"
#include <QNetworkReply>
#include <QDateTime>
#include <cmath>
namespace mlash {
HomeMetrics::HomeMetrics(MihomoClient* api,QObject* parent,std::function<Result(qint64)> memoryReader) : QObject(parent),api_(api),memoryReader_(memoryReader?std::move(memoryReader):processMemory) {
    clock_.start(); timer_.setInterval(2000); connect(&timer_,&QTimer::timeout,this,&HomeMetrics::refresh);
}
void HomeMetrics::reset() { ++token_; timer_.stop(); snapshot_={}; history_={}; error_.clear(); previousMs_=-1; if (request_) request_->abort(); emit changed(); }
void HomeMetrics::setVisible(bool visible) { if (visible_==visible) return; visible_=visible; reset(); if (sampling()) { timer_.start(); refresh(); } }
void HomeMetrics::setContext(bool ready,qint64 pid,quint64 generation) { if (ready_==ready&&pid_==pid&&coreGeneration_==generation) return; ready_=ready; pid_=pid; coreGeneration_=generation; reset(); if (sampling()) { timer_.start(); refresh(); } }
void HomeMetrics::refresh() {
    if (!sampling()||pending_) return;
    pending_=true; const auto token=token_; const auto pid=pid_; const auto reader=memoryReader_;
    request_=api_->request("GET","/connections",{},[this,token,pid,reader](Result result) {
        request_=nullptr;
        if (token!=token_||!sampling()) { pending_=false; return; }
        const auto valid=[](const QJsonValue& value) { const auto n=value.toDouble(-1); return value.isDouble()&&std::isfinite(n)&&n>=0&&n<=9007199254740991.0&&std::floor(n)==n; };
        const auto connections=result.data.value("connections");
        if (!result.ok || (!connections.isArray()&&!connections.isNull()) || !valid(result.data.value("uploadTotal")) || !valid(result.data.value("downloadTotal"))) { pending_=false; previousMs_=-1; snapshot_={}; history_={}; error_="运行指标暂不可用"; emit changed(); return; }
        const auto now=clock_.elapsed(); QJsonObject next{{"active_count",connections.toArray().size()},{"measured_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"elapsed_ms",double(now)}};
        for (const auto* key:{"uploadTotal","downloadTotal"}) { const auto value=result.data.value(key); next[key]=value; const auto gap=now-previousMs_; next[QString(key)+"_rate"]=previousMs_>=0&&gap>0&&gap<=10000&&snapshot_.contains(key)&&value.toDouble()>=snapshot_.value(key).toDouble()?QJsonValue((value.toDouble()-snapshot_.value(key).toDouble())*1000/gap):QJsonValue(); }
        previousMs_=now;
        runTask(this,[reader,pid] { return reader(pid); },[this,token,next](Result memory) mutable {
            pending_=false; if (token!=token_||!sampling()) return;
            const auto used=memory.data.value("working_set"); memory.ok=memory.ok&&used.isDouble()&&std::isfinite(used.toDouble())&&used.toDouble()>=0&&used.toDouble()<=9007199254740991.0;
            if (memory.ok) next["core_working_set"]=used;
            snapshot_=next; error_=memory.ok?QString():QString("内核内存不可读取"); history_.append(QJsonObject{{"upload",next.value("uploadTotal_rate")},{"download",next.value("downloadTotal_rate")},{"elapsed_ms",next.value("elapsed_ms")}}); while (history_.size()>30 || (!history_.isEmpty()&&next.value("elapsed_ms").toDouble()-history_[0].toObject().value("elapsed_ms").toDouble()>60000)) history_.removeAt(0); emit changed();
        });
    },1500);
}
}
