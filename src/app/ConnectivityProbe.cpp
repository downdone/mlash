#include "ConnectivityProbe.h"
#include <QDateTime>
#include <QNetworkReply>
namespace mlash {

namespace {
constexpr qint64 kStaleSeconds = 600;   // 10 分钟：超过即视为过期，界面需标注
}

ConnectivityProbe::ConnectivityProbe(CoreApi* api, QObject* parent) : QObject(parent), api_(api) {}

ConnectivityProbe::~ConnectivityProbe() {
    ++generation_;
    if (request_) request_->abort();
}

void ConnectivityProbe::setUrl(const QString& url) {
    if (url_ == url) return;
    url_ = url;
    if (!running()) { result_ = {}; emit changed(); }
}

void ConnectivityProbe::run(const QString& node) {
    if (node.isEmpty() || node.size() > 512 || request_) return;
    const auto token = ++generation_;
    node_ = node;
    result_ = {{"status", "running"}, {"node", node}, {"url", url_}};
    emit changed();
    request_ = api_->delay(node, [this, token, node](Result observed) {
        if (token != generation_) return;
        request_ = nullptr;
        const int delay = observed.data.value("delay").toInt(-1);
        const bool ok = observed.ok && delay >= 0;
        // 0 ms 是有效响应，不是「未测」；只有失败才把 delay 记为 -1。
        finish({{"status", ok ? QString("ok") : QString("failed")},
                {"node", node},
                {"url", url_},
                {"delay", ok ? delay : -1},
                {"error", ok ? QString() : (observed.error.isEmpty() ? QString("本次探测失败") : observed.error.left(256))},
                {"measured_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    }, 5000, url_);
}

void ConnectivityProbe::cancel() {
    ++generation_;
    const auto request = request_;
    request_ = nullptr;
    if (request) request->abort();
    if (!result_.isEmpty()) { result_ = {}; emit changed(); }
}

void ConnectivityProbe::finish(const QJsonObject& result) {
    if (result_ == result) return;
    result_ = result;
    emit changed();
}

bool ConnectivityProbe::stale() const {
    const auto measured = QDateTime::fromString(result_.value("measured_utc").toString(), Qt::ISODate);
    if (!measured.isValid()) return false;
    return measured.secsTo(QDateTime::currentDateTimeUtc()) > kStaleSeconds;
}
} // namespace mlash
