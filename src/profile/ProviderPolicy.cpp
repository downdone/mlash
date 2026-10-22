#include "ProviderPolicy.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>
namespace mlash {
Result validateProviderPolicies(const QJsonObject& policies) {
    static const QRegularExpression id("^providers/[0-9a-f]{24}\\.cache$");
    if (policies.size()>64||QJsonDocument(policies).toJson(QJsonDocument::Compact).size()>64*1024) return Result::fail("每订阅最多64项提供者策略/64KiB");
    for (auto it=policies.begin();it!=policies.end();++it) {
        if (!id.match(it.key()).hasMatch()||!it.value().isObject()) return Result::fail("提供者策略标识或对象无效");
        const auto row=it.value().toObject(); for (auto field=row.begin();field!=row.end();++field) if (!QStringList{"download","automatic","hours"}.contains(field.key())) return Result::fail("提供者策略含未知字段");
        if (row.contains("automatic")&&!row.value("automatic").isBool()) return Result::fail("提供者自动更新必须为开关");
        if (row.contains("hours")&&(!row.value("hours").isDouble()||std::floor(row.value("hours").toDouble())!=row.value("hours").toDouble()||row.value("hours").toInt()<1||row.value("hours").toInt()>168)) return Result::fail("提供者更新周期须为1–168小时");
        if (row.contains("download")) { if (!row.value("download").isObject()) return Result::fail("提供者下载策略格式无效"); const auto object=row.value("download").toObject(); if (object.size()!=3||!object.value("route").isString()||!object.value("user_agent").isString()||!object.value("timeout_seconds").isDouble()||object.value("timeout_seconds").toDouble()!=object.value("timeout_seconds").toInt()||!DownloadPolicy::fromJson(object).valid()) return Result::fail("提供者下载路径、UA或超时无效"); }
    }
    return {};
}
QDateTime providerNextDue(const QJsonObject& policy,const QString& updated,const QJsonObject& attempt,const QDateTime& now) {
    if (!policy.value("automatic").toBool()) return {};
    const auto retry=QDateTime::fromString(attempt.value("retry_at").toString(),Qt::ISODate); if (attempt.value("failures").toInt()>0&&retry.isValid()) return retry;
    const auto last=QDateTime::fromString(updated,Qt::ISODate); return !last.isValid()||last>now?now:last.addSecs(policy.value("hours").toInt(12)*3600);
}
Result findDueProvider(ProfileStore& store,const QVector<Profile>& profiles,const Settings& settings,const QHash<QString,QJsonObject>& attempts,int cursor,const QDateTime& now) {
    for (int offset=0;offset<profiles.size();++offset) {
        const int index=(cursor+offset)%profiles.size(); const auto& profile=profiles[index]; bool automatic=false; for (auto it=profile.providerPolicies.begin();it!=profile.providerPolicies.end();++it) automatic|=it.value().toObject().value("automatic").toBool(); if (!automatic) continue;
        const auto inspected=store.inspectProviders(profile,profile.candidate,settings); if (!inspected.ok) continue;
        for (const auto& value:inspected.data.value("providers").toArray()) { const auto row=value.toObject(); const auto id=row.value("id").toString(); const auto config=profile.providerPolicies.value(id).toObject(); if (row.value("type")!="http"||!config.value("automatic").toBool()) continue; const auto due=providerNextDue(config,row.value("updated_utc").toString(),attempts.value(profile.id+'/'+id),now); if (due.isValid()&&due<=now) return {true,{},{{"profile",profile.id},{"provider",id},{"candidate",profile.candidate},{"policy",config},{"cursor",index+1}}}; }
    }
    return {};
}
}
