#include "SubscriptionDownloader.h"
#include "SubscriptionInfo.h"
#include <QHostInfo>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QTimer>

namespace mlash {
SubscriptionDownloader::SubscriptionDownloader(QObject* parent, bool allowPrivateForTests)
    : QObject(parent), allowPrivateForTests_(allowPrivateForTests) { network_.setProxy(QNetworkProxy::NoProxy); }
bool SubscriptionDownloader::publicAddress(const QHostAddress& address) {
    if (address.isNull() || address.isLoopback() || address.isMulticast()) return false;
    bool ipv4 = false; const auto numeric = address.toIPv4Address(&ipv4);
    const QHostAddress normalized = ipv4 ? QHostAddress(numeric) : address;
    const QStringList denied = ipv4 ? QStringList{"0.0.0.0/8","10.0.0.0/8","100.64.0.0/10","127.0.0.0/8","169.254.0.0/16",
        "172.16.0.0/12","192.168.0.0/16","192.0.0.0/24","192.0.2.0/24","192.88.99.0/24","198.18.0.0/15",
        "198.51.100.0/24","203.0.113.0/24","224.0.0.0/4","240.0.0.0/4"}
        : QStringList{"2001::/23","2001:db8::/32","2002::/16","3fff::/20"};
    if (!ipv4) {
        if (normalized.isInSubnet(QHostAddress::parseSubnet("64:ff9b::/96"))) {
            const auto bytes = normalized.toIPv6Address();
            const quint32 embedded = (quint32(bytes[12]) << 24) | (quint32(bytes[13]) << 16) | (quint32(bytes[14]) << 8) | bytes[15];
            return publicAddress(QHostAddress(embedded));
        }
        // Conservatively support ordinary global unicast, not special transition/protocol assignments.
        if (!normalized.isInSubnet(QHostAddress::parseSubnet("2000::/3"))) return false;
    }
    for (const auto& subnet : denied) if (normalized.isInSubnet(QHostAddress::parseSubnet(subnet))) return false;
    return true;
}
void SubscriptionDownloader::resolve(const QUrl& url, std::function<void(Result, QHostAddress)> done) {
    if (url.toEncoded().size() > 4096 || !url.isValid() || !QStringList{"http","https"}.contains(url.scheme()) || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment()) {
        done(Result::fail("仅接受无用户凭据和片段的 HTTP/HTTPS URL"), {}); return;
    }
    auto deliver = [this, done = std::move(done)](const QList<QHostAddress>& addresses) mutable {
        if (addresses.isEmpty()) { done(Result::fail("订阅地址 DNS 解析失败"), {}); return; }
        for (const auto& address : addresses) if (!allowPrivateForTests_ && !publicAddress(address)) {
            done(Result::fail("订阅/Provider URL 指向回环、内网或保留地址，已拒绝"), {}); return;
        }
        done({}, addresses.first());
    };
    QHostAddress literal;
    if (literal.setAddress(url.host())) { deliver({literal}); return; }
    const auto generation = generation_;
    auto shared = std::make_shared<std::function<void(const QList<QHostAddress>&)>>(std::move(deliver));
    auto* timer = new QTimer(this); timer->setSingleShot(true);
    const int lookup = QHostInfo::lookupHost(url.host(), this, [this, timer, generation, shared](const QHostInfo& info) {
        timer->stop(); timer->deleteLater();
        auto callback = std::move(*shared); if (callback) callback(generation == generation_ ? info.addresses() : QList<QHostAddress>{});
    });
    connect(timer, &QTimer::timeout, this, [timer, lookup, shared] {
        QHostInfo::abortHostLookup(lookup); timer->deleteLater(); auto callback = std::move(*shared); if (callback) callback({});
    }); timer->start(downloading_ ? qMax(1, qMin(10000, policy_.timeoutSeconds * 1000 - int(clock_.elapsed()))) : 10000);
}
void SubscriptionDownloader::checkPublicUrl(const QUrl& url, Completion done) {
    resolve(url, [done = std::move(done)](Result r, QHostAddress) mutable { done(r); });
}
void SubscriptionDownloader::cancel() { ++generation_; if (reply_) reply_->abort(); }
void SubscriptionDownloader::download(const QUrl& url, std::function<void(Result, QByteArray)> done, const DownloadPolicy& policy, int corePort, qint64 maximumBytes, bool inspectResponse) {
    if (downloading_) { done(Result::fail("已有下载正在执行"), {}); return; }
    if (!policy.valid() || (policy.route == "core" && (corePort < 1024 || corePort > 65535))) { done(Result::fail("下载设置无效，或本地代理尚未就绪"), {}); return; }
    if (maximumBytes<1 || maximumBytes>128*1024*1024) { done(Result::fail("下载大小预算无效"),{}); return; }
    inspectResponse_=inspectResponse; maximumBytes_=maximumBytes; policy_ = policy; clock_.start(); downloading_ = true;
    QNetworkProxy proxy=policy.route == "core" ? QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", quint16(corePort)) : QNetworkProxy(QNetworkProxy::NoProxy);
    // HTTP must also use CONNECT to the validated IP. A caching HTTP proxy
    // may otherwise dial Host again, invalidating our DNS/address check.
    if (policy.route=="core") proxy.setCapabilities(QNetworkProxy::TunnelingCapability);
    network_.setProxy(proxy);
    fetch(url, 0, [this, done = std::move(done), route = policy.route](Result result, QByteArray bytes) mutable {
        downloading_ = false;
        result.data["download_route"] = route;
        if (!result.ok) result.error += route == "core" ? "（经本地代理）" : "（直连）";
        done(result, std::move(bytes));
    });
}
void SubscriptionDownloader::fetch(const QUrl& url, int redirects, std::function<void(Result, QByteArray)> done) {
    const auto generation = generation_;
    resolve(url, [this, url, redirects, generation, done = std::move(done)](Result r, QHostAddress address) mutable {
        if (generation != generation_) { done(Result::fail("下载已取消"), {}); return; }
        if (clock_.elapsed() >= policy_.timeoutSeconds * 1000) { done(Result::fail("订阅下载超时"), {}); return; }
        if (!r.ok) { done(r, {}); return; }
        QUrl pinned = url; pinned.setHost(address.toString());
        QNetworkRequest request(pinned);
        const QString host = url.host().contains(':') ? "[" + url.host() + "]" : url.host();
        const auto authority = host + (url.port() > 0 ? ":" + QString::number(url.port()) : QString());
        request.setRawHeader("Host", authority.toUtf8()); request.setPeerVerifyName(url.host());
        request.setRawHeader("User-Agent", policy_.userAgent.isEmpty() ? QByteArray("Mlash/" MLASH_VERSION) : policy_.userAgent.toLatin1());
        request.setRawHeader("Accept", "application/yaml,text/yaml,text/plain,*/*");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(15000);
        auto* reply = network_.get(request); reply_ = reply;
        auto bytes = std::make_shared<QByteArray>();
        auto* timer = new QTimer(reply); timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, reply, [reply] { reply->setProperty("timedOut", true); reply->abort(); }); timer->start(qMax(1, policy_.timeoutSeconds * 1000 - int(clock_.elapsed())));
        connect(reply, &QNetworkReply::readyRead, this, [reply, bytes,maximum=maximumBytes_] {
            if (reply->isOpen()) bytes->append(reply->readAll());
            if (bytes->size() > maximum) { reply->setProperty("oversize", true); reply->abort(); }
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, url, address, redirects, generation, done = std::move(done)]() mutable {
            if (reply_ == reply) reply_ = nullptr;
            if (reply->isOpen()) bytes->append(reply->readAll());
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QUrl location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            Result result;
            if (generation != generation_) result = Result::fail("下载已取消");
            else if (reply->property("oversize").toBool() || bytes->size() > maximumBytes_) result = Result::fail(QString("下载超过 %1 MiB").arg(maximumBytes_/(1024*1024)));
            else if (reply->property("timedOut").toBool()) result = Result::fail("订阅下载超时");
            else if (inspectResponse_ && status>=100 && status<=599 && (reply->error()==QNetworkReply::NoError || (status>=400 && (reply->error()==QNetworkReply::ContentAccessDenied || reply->error()==QNetworkReply::ContentNotFoundError || reply->error()==QNetworkReply::AuthenticationRequiredError || reply->error()==QNetworkReply::ContentOperationNotPermittedError || reply->error()==QNetworkReply::UnknownContentError || reply->error()==QNetworkReply::InternalServerError || reply->error()==QNetworkReply::ServiceUnavailableError || reply->error()==QNetworkReply::UnknownServerError)))) {
                result.data["http_status"]=status; result.data["location"]=url.resolved(location).toString(QUrl::RemoveUserInfo|QUrl::RemoveQuery|QUrl::RemoveFragment);
            }
            else if (status >= 300 && status < 400 && !location.isEmpty()) {
                const auto next = url.resolved(location); reply->deleteLater();
                if (redirects >= 4 || (url.scheme() == "https" && next.scheme() != "https")) { done(Result::fail("重定向过多或 HTTPS 降级"), {}); return; }
                fetch(next, redirects + 1, std::move(done)); return;
            } else if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) result = Result::fail(QString("订阅下载失败 (HTTP %1)，检查网络与证书").arg(status));
            else if (bytes->isEmpty()) result = Result::fail("订阅内容为空");
            if (result.ok) { result.data["subscription_info"] = SubscriptionInfo::parse(reply->rawHeader("subscription-userinfo")); result.data["response_host"]=url.host(); result.data["response_address"]=address.toString(); }
            reply->deleteLater(); done(result, result.ok ? *bytes : QByteArray{});
        });
    });
}
}
