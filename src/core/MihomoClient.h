#pragma once
#include "CoreApi.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSet>
#include <QUrlQuery>
#include <functional>

namespace mlash {
class MihomoClient : public QObject, public CoreApi {
    Q_OBJECT
public:
    explicit MihomoClient(QObject* parent = nullptr);
    void configure(int port, const QString& secret) override;
    QNetworkReply* request(const QByteArray& method, const QByteArray& path, const QJsonObject& body, Completion done,
        int timeoutMs = 4000, const QUrlQuery& query = {});
    void version(Completion done) override;
    void config(Completion done) override;
    void proxies(Completion done) override;
    void versionInfo(std::function<void(CoreObservation<CoreVersionInfo>)> done) override;
    void configInfo(std::function<void(CoreObservation<CoreConfigInfo>)> done) override;
    void proxyGroups(std::function<void(CoreObservation<CoreProxyGroups>)> done) override;
    void select(const QString& group, const QString& member, Completion done) override;
    QNetworkReply* dnsQuery(const QString& name, const QString& type, Completion done) override;
    QNetworkReply* rules(Completion done) override;
    QNetworkReply* connections(Completion done, int timeoutMs = 4000) override;
    QNetworkReply* proxy(const QString& group, Completion done) override;
    QNetworkReply* setMode(const QString& mode, Completion done) override;
    QNetworkReply* releaseAutomaticGroup(const QString& group, Completion done) override;
    QNetworkReply* closeConnection(const QString& id, Completion done, int timeoutMs = 4000) override;
    QNetworkReply* closeAllConnections(Completion done) override;
    QNetworkReply* delay(const QString& name, Completion done, int timeoutMs = 5000, const QString& url = "https://www.gstatic.com/generate_204") override;
    void setLogsVisible(bool visible, const QString& level = "info") override;
    void cancelAll() override;
    static QByteArray segment(const QString& name);
signals:
    void logReceived(const QString& line);
private:
    QNetworkAccessManager network_;
    // Two independent HTTP/1 pools allow a bounded twelve-probe batch to
    // reach the local core without queuing behind Qt's per-host pool limit.
    QNetworkAccessManager delayNetworkA_, delayNetworkB_;
    bool nextDelayPoolB_ = false;
    QUrl base_;
    QString secret_;
    QSet<QNetworkReply*> pending_;
    QPointer<QNetworkReply> logReply_;
    QByteArray logBuffer_;
    quint64 generation_ = 0;
};
}
