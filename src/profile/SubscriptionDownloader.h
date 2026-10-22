#pragma once
#include "core/MihomoClient.h"
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QElapsedTimer>
#include <functional>

namespace mlash {
class SubscriptionDownloader : public QObject {
    Q_OBJECT
public:
    explicit SubscriptionDownloader(QObject* parent = nullptr, bool allowPrivateForTests = false);
    virtual void download(const QUrl& url, std::function<void(Result, QByteArray)> done, const DownloadPolicy& policy = {}, int corePort = 0, qint64 maximumBytes = 8*1024*1024, bool inspectResponse = false);
    void checkPublicUrl(const QUrl& url, Completion done);
    virtual void cancel();
    static bool publicAddress(const QHostAddress& address);
private:
    void resolve(const QUrl& url, std::function<void(Result, QHostAddress)> done);
    void fetch(const QUrl& url, int redirects, std::function<void(Result, QByteArray)> done);
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> reply_;
    bool allowPrivateForTests_;
    bool downloading_ = false;
    qint64 maximumBytes_ = 8*1024*1024;
    bool inspectResponse_ = false;
    DownloadPolicy policy_;
    QElapsedTimer clock_;
    quint64 generation_ = 0;
};
}
