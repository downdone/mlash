#include "ConnectionSession.h"
#include "storage/Storage.h"
#include <QHash>
#include <QDateTime>
#include <cmath>
namespace mlash {
void ConnectionSession::invalidate() { active_={}; totals_={}; previousMs_=-1; }
void ConnectionSession::reset() { invalidate(); closed_={}; observedAt_.clear(); }
void ConnectionSession::suspend() {
    previousMs_=-1;
    QJsonArray frozen; for (const auto& value:active_) { auto row=value.toObject(); row["_paused"]=true; row["_upload_rate"]=QJsonValue(); row["_download_rate"]=QJsonValue(); frozen.append(row); } active_=frozen;
    totals_["_paused"]=true; totals_["_uploadTotal_rate"]=QJsonValue(); totals_["_downloadTotal_rate"]=QJsonValue();
}
Result ConnectionSession::update(const QJsonObject& snapshot,qint64 nowMs) {
    const auto field=snapshot.value("connections");
    if (!snapshot.contains("connections") || (!field.isArray() && !field.isNull())) return Result::fail("内核连接快照格式错误");
    const auto counter=[](QJsonValue value) { return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble()>=0 && value.toDouble()<=9007199254740991.0 && std::floor(value.toDouble())==value.toDouble(); };
    QJsonObject totals; for (const auto* key:{"uploadTotal","downloadTotal"}) if (snapshot.contains(key)) { if (!counter(snapshot.value(key))) return Result::fail("连接流量计数格式错误"); totals[key]=snapshot.value(key); }
    const auto values=field.toArray(); QHash<QString,QJsonObject> before,seen;
    for (const auto& value:active_) { const auto row=value.toObject(); before.insert(row.value("id").toString(),row); }
    const double seconds=previousMs_<0 ? 0 : (nowMs-previousMs_)/1000.0;
    QJsonArray next; const auto observed=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    for (int i=0;i<qMin(5000,int(values.size()));++i) {
        auto row=values[i].toObject(); const auto id=row.value("id").toString();
        if (!isId(id) || seen.contains(id) || !row.value("metadata").isObject() || !counter(row.value("upload")) || !counter(row.value("download"))) return Result::fail("连接条目格式错误");
        const auto old=before.value(id); row["_active"]=true; row["_paused"]=false; row["_observed_at"]=observed;
        for (const auto* key:{"upload","download"}) row[QString("_%1_rate").arg(key)]=(seconds>0 && seconds<=10 && !old.isEmpty() && row.value(key).toDouble()>=old.value(key).toDouble()) ? QJsonValue((row.value(key).toDouble()-old.value(key).toDouble())/seconds) : QJsonValue();
        seen.insert(id,row); next.append(row);
    }
    // Never infer closure from a truncated list: an omitted row may still be active.
    if (previousMs_>=0&&values.size()<=5000) for (const auto& value:active_) {
        auto row=value.toObject(); if (!seen.contains(row.value("id").toString())) { row["_active"]=false; row["_left_at"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); row["_upload_rate"]=QJsonValue(); row["_download_rate"]=QJsonValue(); closed_.prepend(row); }
    }
    QJsonArray closed; for (const auto& value:closed_) { if (!seen.contains(value.toObject().value("id").toString()) && closed.size()<1000) closed.append(value); } closed_=closed;
    for (const auto* key:{"uploadTotal","downloadTotal"}) totals[QString("_%1_rate").arg(key)]=(seconds>0 && seconds<=10 && totals.contains(key) && totals_.contains(key) && totals.value(key).toDouble()>=totals_.value(key).toDouble()) ? QJsonValue((totals.value(key).toDouble()-totals_.value(key).toDouble())/seconds) : QJsonValue();
    totals["active_count"]=values.size(); totals["truncated"]=values.size()>5000; totals["_observed_at"]=observed; totals["_paused"]=false; active_=next; totals_=totals; previousMs_=nowMs; observedAt_=observed; return {};
}
}
