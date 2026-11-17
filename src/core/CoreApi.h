#pragma once
#include "CoreDtos.h"
#include <QNetworkReply>
#include <functional>

namespace mlash {
using Completion = std::function<void(Result)>;

// Application-facing capabilities of the current core. MihomoClient owns the
// protocol mapping; callers retain their existing generation/readback checks.
class CoreApi {
public:
    virtual ~CoreApi() = default;
    virtual void configure(int port, const QString& secret) = 0;
    virtual void cancelAll() = 0;
    virtual void version(Completion done) = 0;
    virtual void config(Completion done) = 0;
    virtual void proxies(Completion done) = 0;
    virtual void versionInfo(std::function<void(CoreObservation<CoreVersionInfo>)> done) = 0;
    virtual void configInfo(std::function<void(CoreObservation<CoreConfigInfo>)> done) = 0;
    virtual void proxyGroups(std::function<void(CoreObservation<CoreProxyGroups>)> done) = 0;
    virtual void select(const QString& group, const QString& member, Completion done) = 0;
    virtual QNetworkReply* dnsQuery(const QString& name, const QString& type, Completion done) = 0;
    virtual QNetworkReply* rules(Completion done) = 0;
    virtual QNetworkReply* connections(Completion done, int timeoutMs = 4000) = 0;
    virtual QNetworkReply* proxy(const QString& group, Completion done) = 0;
    virtual QNetworkReply* setMode(const QString& mode, Completion done) = 0;
    virtual QNetworkReply* releaseAutomaticGroup(const QString& group, Completion done) = 0;
    virtual QNetworkReply* closeConnection(const QString& id, Completion done, int timeoutMs = 4000) = 0;
    virtual QNetworkReply* closeAllConnections(Completion done) = 0;
    virtual QNetworkReply* delay(const QString& name, Completion done, int timeoutMs = 5000,
                                 const QString& url = "https://www.gstatic.com/generate_204") = 0;
    virtual void setLogsVisible(bool visible, const QString& level = "info") = 0;
};
}
