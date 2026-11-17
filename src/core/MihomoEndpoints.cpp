#include "MihomoClient.h"

namespace mlash {
void MihomoClient::versionInfo(std::function<void(CoreObservation<CoreVersionInfo>)> done) {
    version([done = std::move(done)](Result result) mutable { done(parseMihomoVersion(std::move(result))); });
}
void MihomoClient::configInfo(std::function<void(CoreObservation<CoreConfigInfo>)> done) {
    config([done = std::move(done)](Result result) mutable { done(parseMihomoConfig(std::move(result))); });
}
void MihomoClient::proxyGroups(std::function<void(CoreObservation<CoreProxyGroups>)> done) {
    proxies([done = std::move(done)](Result result) mutable { done(parseMihomoProxies(std::move(result))); });
}
// Endpoint names and escaping belong to the core adapter, not orchestration/UI.
QNetworkReply* MihomoClient::dnsQuery(const QString& name, const QString& type, Completion done) {
    QUrlQuery query; query.addQueryItem("name",name); query.addQueryItem("type",type);
    return request("GET","/dns/query",{},std::move(done),4000,query);
}
QNetworkReply* MihomoClient::rules(Completion done) { return request("GET","/rules",{},std::move(done)); }
QNetworkReply* MihomoClient::connections(Completion done, int timeoutMs) { return request("GET","/connections",{},std::move(done),timeoutMs); }
QNetworkReply* MihomoClient::proxy(const QString& group, Completion done) { return request("GET","/proxies/"+segment(group),{},std::move(done)); }
QNetworkReply* MihomoClient::setMode(const QString& mode, Completion done) { return request("PATCH","/configs",{{"mode",mode}},std::move(done)); }
QNetworkReply* MihomoClient::releaseAutomaticGroup(const QString& group, Completion done) { return request("DELETE","/proxies/"+segment(group),{},std::move(done)); }
QNetworkReply* MihomoClient::closeConnection(const QString& id, Completion done, int timeoutMs) { return request("DELETE","/connections/"+segment(id),{},std::move(done),timeoutMs); }
QNetworkReply* MihomoClient::closeAllConnections(Completion done) { return request("DELETE","/connections",{},std::move(done)); }
}
