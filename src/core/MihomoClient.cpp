#include "MihomoClient.h"
#include "diagnostics/Diagnostics.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QTimer>

namespace mlash {
MihomoClient::MihomoClient(QObject* parent) : QObject(parent) { network_.setProxy(QNetworkProxy::NoProxy); }
QByteArray MihomoClient::segment(const QString& name) { return QUrl::toPercentEncoding(name); }
void MihomoClient::configure(int port, const QString& secret) {
    cancelAll(); base_ = QUrl(QString("http://127.0.0.1:%1").arg(port)); secret_ = secret;
}
void MihomoClient::cancelAll() {
    ++generation_; setLogsVisible(false);
    const auto pending = pending_;
    for (auto* reply : pending) reply->abort();
}
QNetworkReply* MihomoClient::request(const QByteArray& method, const QByteArray& path, const QJsonObject& body, Completion done, int timeoutMs, const QUrlQuery& query) {
    if (!base_.isValid() || base_.port() <= 0) { done(Result::fail("内核 API 尚未配置")); return nullptr; }
    QUrl url = QUrl::fromEncoded(base_.toEncoded(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment) + path);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + secret_.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(timeoutMs);
    auto* reply = network_.sendCustomRequest(request, method, body.isEmpty() ? QByteArray{} : QJsonDocument(body).toJson(QJsonDocument::Compact));
    pending_.insert(reply);
    const auto generation = generation_;
    auto* timer = new QTimer(reply); timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply] { reply->setProperty("timedOut", true); reply->abort(); });
    timer->start(timeoutMs);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 16 * 1024 * 1024) { reply->setProperty("oversize", true); reply->abort(); }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, done = std::move(done)]() mutable {
        pending_.remove(reply);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->isOpen() ? reply->readAll() : QByteArray{};
        Result result;
        if (generation != generation_) result = Result::fail("操作已取消或内核已切换");
        else if (reply->property("timedOut").toBool()) result = {false, "内核请求超时", {{"timeout", true}}};
        else if (reply->property("oversize").toBool()) result = Result::fail("内核响应超过限制");
        else if (status < 200 || status >= 300 || reply->error() != QNetworkReply::NoError) {
            QJsonObject error = QJsonDocument::fromJson(bytes).object();
            result = Result::fail(QString("内核 API %1：%2").arg(status).arg(Diagnostics::redact(error.value("message").toString(reply->errorString()))));
        } else if (status != 204 && !bytes.trimmed().isEmpty()) {
            QJsonParseError error; const auto doc = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) result = Result::fail("内核返回无效 JSON 对象");
            else result.data = doc.object();
        } else if (status != 204) result = Result::fail("内核返回空响应");
        reply->deleteLater(); done(std::move(result));
    });
    return reply;
}
void MihomoClient::version(Completion done) { request("GET", "/version", {}, std::move(done)); }
void MihomoClient::config(Completion done) { request("GET", "/configs", {}, std::move(done)); }
void MihomoClient::proxies(Completion done) { request("GET", "/proxies", {}, std::move(done)); }
void MihomoClient::select(const QString& group, const QString& member, Completion done) { request("PUT", "/proxies/" + segment(group), {{"name", member}}, std::move(done)); }
QNetworkReply* MihomoClient::delay(const QString& name, Completion done, int timeoutMs, const QString& url) {
    QUrlQuery query; query.addQueryItem("url", url); query.addQueryItem("timeout", QString::number(timeoutMs));
    return request("GET", "/proxies/" + segment(name) + "/delay", {}, std::move(done), timeoutMs + 1000, query);
}
void MihomoClient::setLogsVisible(bool visible, const QString& level) {
    const auto desired = QStringList{"debug", "info", "warning", "error", "silent"}.contains(level) ? level : QString("info");
    if (visible && logReply_ && logReply_->property("logLevel").toString() == desired) return;
    if (logReply_) { auto* old = logReply_.data(); logReply_ = nullptr; old->abort(); old->deleteLater(); }
    logBuffer_.clear();
    if (!visible || !base_.isValid() || base_.port() <= 0) return;
    QUrl url = base_; url.setPath("/logs"); url.setQuery("level=" + desired);
    QNetworkRequest request(url); request.setRawHeader("Authorization", "Bearer " + secret_.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto* reply = network_.get(request); reply->setProperty("logLevel", desired); logReply_ = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (logReply_ != reply) return;
        logBuffer_ += reply->readAll();
        if (logBuffer_.size() > 256 * 1024) { logBuffer_.clear(); reply->abort(); emit logReceived("日志流超出缓冲限制，已停止"); return; }
        int end;
        while ((end = logBuffer_.indexOf('\n')) >= 0) {
            const auto line = logBuffer_.left(end); logBuffer_.remove(0, end + 1);
            const auto object = QJsonDocument::fromJson(line).object();
            if (!object.isEmpty()) emit logReceived(object.value("type").toString() + " " + object.value("payload").toString());
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { if (logReply_ == reply) { logReply_ = nullptr; logBuffer_.clear(); } reply->deleteLater(); });
}
}
