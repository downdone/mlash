#include "WebsiteTests.h"
#include "diagnostics/Diagnostics.h"
#include <QNetworkReply>
#include <QTimer>
#include <utility>
#include <QRegularExpression>
#include <QSet>

namespace mlash {
QList<WebsiteTarget> WebsiteTests::defaults() {
    return {{"youtube","YouTube","https://www.youtube.com/","YT"},
        {"chatgpt","ChatGPT","https://chatgpt.com/","AI"},
        {"google","Google","https://www.google.com/generate_204","G"},
        {"github","GitHub","https://github.com/","GH"},
        {"x","X (Twitter)","https://x.com/","X"},
        {"netflix","Netflix","https://www.netflix.com/","N"},
        {"disney","Disney+","https://www.disneyplus.com/","D+"},
        {"tiktok","TikTok","https://www.tiktok.com/","TT"},
        {"facebook","Facebook","https://www.facebook.com/","f"},
        {"instagram","Instagram","https://www.instagram.com/","IG"},
        {"telegram","Telegram","https://web.telegram.org/","TG"},
        {"cloudflare","Cloudflare","https://www.cloudflare.com/cdn-cgi/trace","CF"}};
}
WebsiteTests::WebsiteTests(MihomoClient* api,QObject* parent,QList<WebsiteTarget> targets):QObject(parent),api_(api),targets_(std::move(targets)) {
    // Definitions come from compiled-in defaults (or isolated test fixtures).
    if (targets_.size()>24) targets_=targets_.mid(0,24);
    initialTargets_ = targets_;
}
Result WebsiteTests::validateTargets(const QJsonArray& targets) {
    if (targets.size() > 24) return Result::fail("测速目标最多 24 个");
    QSet<QString> ids; static const QRegularExpression idPattern("^[a-z0-9-]{1,64}$");
    for (int i = 0; i < targets.size(); ++i) {
        const auto item = targets[i].toObject(); const auto id = item.value("id").toString(), name = item.value("name").toString(), address = item.value("url").toString(); const QUrl url(address,QUrl::StrictMode);
        if (!targets[i].isObject() || !idPattern.match(id).hasMatch() || ids.contains(id) || name.trimmed().isEmpty() || name.size()>64 || name.contains(QChar::LineFeed) || name.contains(QChar::CarriageReturn) || address.size()>2048 || !url.isValid() || url.scheme()!="https" || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment() || (item.contains("mark") && (!item.value("mark").isString() || item.value("mark").toString().size()>8))) return Result::fail(QString("第 %1 个目标无效：名称 1–64 字符，HTTPS 地址不得含用户凭据/片段，目标标识不得重复").arg(i+1));
        ids.insert(id);
    }
    return {};
}
void WebsiteTests::configureTargets(const QJsonArray& definitions) {
    if (!validateTargets(definitions).ok) return;
    QList<WebsiteTarget> next = initialTargets_;
    if (!definitions.isEmpty()) { next.clear(); for (const auto& value : definitions) { const auto item = value.toObject(); next.append({item.value("id").toString(),item.value("name").toString(),item.value("url").toString(),item.value("mark").toString()}); } }
    if (next == targets_) return;
    clear(); targets_ = next; emit targetsChanged(); emit changed();
}
WebsiteTests::~WebsiteTests() { cancel(); }
void WebsiteTests::cancel() {
    ++generation_; queue_.clear(); const auto requests=requests_; requests_.clear();
    for (auto it=results_.begin();it!=results_.end();++it) if (it->status==WebsiteResult::Queued || it->status==WebsiteResult::Testing) { it->status=WebsiteResult::Cancelled; it->error.clear(); }
    for (const auto& reply:requests) if (reply) reply->abort();
    emit changed();
}
void WebsiteTests::clear() { cancel(); results_.clear(); node_.clear(); emit changed(); }
void WebsiteTests::start(const QString& node,const QString& onlyId) {
    if (running() || node.isEmpty() || node.size()>512) return;
    if (!onlyId.isEmpty()) { bool found=false; for (const auto& t:targets_) found|=t.id==onlyId; if (!found) return; }
    if (node_!=node) clear();
    node_=node; ++generation_;
    for (const auto& target:targets_) if (onlyId.isEmpty() || target.id==onlyId) { WebsiteResult result; result.status=WebsiteResult::Queued; results_[target.id]=result; queue_.append(target.id); }
    emit changed(); dispatch();
}
void WebsiteTests::dispatch() {
    const auto token=generation_;
    while (requests_.size()<3 && !queue_.isEmpty()) {
        const auto id=queue_.takeFirst(); WebsiteTarget target;
        for (const auto& item:targets_) if (item.id==id) target=item;
        requests_.insert(id,{}); results_[id].status=WebsiteResult::Testing;
        auto* reply=api_->delay(node_,[this,id,token,guard=QPointer<WebsiteTests>(this)](Result response) {
            if (!guard || token!=generation_) return;
            requests_.remove(id); auto& result=results_[id]; const auto value=response.data.value("delay");
            const bool valid=response.ok && value.isDouble() && value.toDouble()==value.toInt(-1) && value.toInt()>=0 && value.toInt()<=65535;
            result.status=valid?WebsiteResult::Responded:WebsiteResult::Failed; result.delay=valid?value.toInt():-1;
            result.error=valid?QString():Diagnostics::redact(response.error.isEmpty()?QString("目标没有返回有效响应延迟"):response.error).left(512); result.measured=QDateTime::currentDateTime();
            emit changed(); QTimer::singleShot(0,this,[this,token] { if (token==generation_) dispatch(); });
        },5000,target.url);
        if (token==generation_ && requests_.contains(id)) requests_[id]=reply;
        if (token!=generation_) return;
    }
    emit changed();
}
}
