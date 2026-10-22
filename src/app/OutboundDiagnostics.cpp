#include "OutboundDiagnostics.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QRegularExpression>
#include <cmath>
namespace mlash {
OutboundDiagnostics::OutboundDiagnostics(QObject* parent,bool allowPrivateForTests,QUrl ipTarget,QMap<QString,QUrl> serviceUrls) : QObject(parent),ipTarget_(std::move(ipTarget)),allowPrivateForTests_(allowPrivateForTests),serviceUrls_(std::move(serviceUrls)) {}
quint64 OutboundDiagnostics::prepare(const QJsonObject& route) { cancel(); route_=route; busy_=true; emit changed(); return token_; }
void OutboundDiagnostics::cancel() { ++token_; busy_=false; route_={}; result_={}; serviceQueue_.clear(); const auto old=downloader_; downloader_=nullptr; if (old) { old->cancel(); old->deleteLater(); } emit changed(); }
void OutboundDiagnostics::fail(quint64 token,const QString& error) { if (token!=token_) return; busy_=false; result_=route_; result_["error"]=error.left(512); if (route_.value("kind")!="services") result_["target"]=ipTarget_.toString(); result_["measured_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); emit changed(); }
Result OutboundDiagnostics::parseIp(const QByteArray& bytes) {
    if (bytes.isEmpty()||bytes.size()>64*1024) return Result::fail("出口响应为空或超过64KiB");
    const auto doc=QJsonDocument::fromJson(bytes); if (!doc.isObject()) return Result::fail("出口响应不是JSON对象");
    const auto object=doc.object(); const auto ip=QHostAddress(object.value("ip").toString()); if (!SubscriptionDownloader::publicAddress(ip)) return Result::fail("未返回有效公网IP");
    const auto text=[&](const QString& key,int maximum) { auto value=object.value(key).toString().left(maximum); value.remove(QRegularExpression("[\\x00-\\x1f\\x7f]")); return value; };
    QJsonObject data{{"ip",ip.toString()},{"country",text("country",128)},{"region",text("region",128)},{"country_code",text("country_code",2)},{"asn_organization",text("asn_organization",200)},{"isp",text("isp",200)}};
    const auto number=object.value("asn"); const auto asn=number.toDouble(-1); if (number.isDouble()&&std::isfinite(asn)&&asn>=0&&asn<=4294967295.0&&std::floor(asn)==asn) data["asn"]=QString("AS%1").arg(quint64(asn));
    return {true,{},data};
}
void OutboundDiagnostics::runIp(quint64 token,int corePort) {
    if (token!=token_||!busy_) return;
    const auto route=route_; const bool core=route.value("route").toString()=="core"; DownloadPolicy policy; policy.route=core?"core":"direct"; policy.timeoutSeconds=15;
    auto* downloader=new SubscriptionDownloader(this,allowPrivateForTests_); downloader_=downloader;
    downloader->download(ipTarget_,[this,token,route,downloader](Result downloaded,QByteArray bytes) {
        downloader->deleteLater(); if (token!=token_) return; downloader_=nullptr;
        auto parsed=downloaded.ok?parseIp(bytes):downloaded;
        if (!parsed.ok) { fail(token,parsed.error); return; }
        busy_=false; result_=route; for (auto it=parsed.data.begin();it!=parsed.data.end();++it) result_[it.key()]=it.value(); result_["target"]=ipTarget_.toString(); result_["measured_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); result_["destination_ip"]=downloaded.data.value("response_address"); result_["response_host"]=downloaded.data.value("response_host"); emit changed();
    },policy,corePort,64*1024);
}
void OutboundDiagnostics::runServices(quint64 token,int corePort,const QString& id) {
    if (token!=token_||!busy_) return;
    serviceQueue_.clear(); for (const auto& target:serviceTargets()) if (id.isEmpty()||id==target.id) serviceQueue_.append(target);
    if (serviceQueue_.isEmpty()) { fail(token,"服务目标无效"); return; }
    result_=route_; result_["kind"]="services"; result_["services"]=QJsonArray{}; runNextService(token,corePort);
}
void OutboundDiagnostics::runNextService(quint64 token,int corePort) {
    if (token!=token_||!busy_) return;
    if (serviceQueue_.isEmpty()) { busy_=false; result_.remove("current_service"); emit changed(); return; }
    auto target=serviceQueue_.takeFirst(); if (serviceUrls_.contains(target.id)) target.url=serviceUrls_.value(target.id); result_["current_service"]=target.id; emit changed();
    DownloadPolicy policy; policy.route=route_.value("route")=="core"?"core":"direct"; policy.timeoutSeconds=10;
    auto* downloader=new SubscriptionDownloader(this,allowPrivateForTests_); downloader_=downloader;
    downloader->download(target.url,[this,token,target,corePort,downloader](Result response,QByteArray bytes) {
        downloader->deleteLater(); if (token!=token_) return; downloader_=nullptr;
        auto row=response.ok?analyzeService(target.id,response.data.value("http_status").toInt(),bytes,QUrl(response.data.value("location").toString())):QJsonObject{{"state","unknown"},{"evidence",response.error.left(512)}};
        row["id"]=target.id; row["name"]=target.name; row["target"]=target.url.toString(); row["measured_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); row["destination_ip"]=response.data.value("response_address"); row["http_status"]=response.data.value("http_status"); auto rows=result_.value("services").toArray(); rows.append(row); result_["services"]=rows; result_["measured_utc"]=row.value("measured_utc"); emit changed(); runNextService(token,corePort);
    },policy,corePort,2*1024*1024,true);
}
}
