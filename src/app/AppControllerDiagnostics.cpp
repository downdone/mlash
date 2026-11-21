#include "profile/YamlFields.h"
#include "profile/ProxyImpact.h"
#include "profile/VersionChanges.h"
#include "profile/RuleDraft.h"
#include "profile/ProviderEdits.h"
#include "profile/ProviderPolicy.h"
#include "AppController.h"
#include "Async.h"
#include "storage/Storage.h"
#include "profile/BackupArchive.h"
#include "profile/GeoData.h"
#include "core/TunPolicy.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QUrlQuery>
#include <utility>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QNetworkProxy>
#include <QSet>
#include <yaml-cpp/yaml.h>
#include <QRegularExpression>
#include <QNetworkReply>

namespace mlash {
void AppController::updateDiagnosticStream() {
    const bool coreLogs=diagnosticsLevel_!="app"&&diagnosticsLevel_!="wireguard";
    api_.setLogsVisible(diagnosticsVisible_&&state_.ready&&coreLogs,diagnosticsLevel_=="all"||!coreLogs?"debug":diagnosticsLevel_);
}
void AppController::setDiagnosticsVisible(bool visible) { if (diagnosticsVisible_ == visible) return; diagnosticsVisible_ = visible; updateDiagnosticStream(); }
void AppController::setDiagnosticsLevel(const QString& level) {
    if (!QStringList{"all", "debug", "info", "warning", "error", "app","wireguard"}.contains(level) || level == diagnosticsLevel_) return;
    diagnosticsLevel_ = level; updateDiagnosticStream();
}
void AppController::clearLogView() { diagnostics_.clearView(); }
void AppController::exportDiagnostics(const QString& path) {
    if (QFileInfo(path).suffix().compare("json", Qt::CaseInsensitive) != 0) { emit operationFinished(false, "请选择 .json 诊断文件"); return; }
    const auto lastError = diagnostics_.sanitize(state_.lastError);
    if (!begin("正在导出脱敏诊断", false, true)) return;
    const auto snapshot = diagnostics_.snapshot();
    const QJsonObject report{{"report_format",1},{"client_version",MLASH_VERSION},{"qt_version",QT_VERSION_STR},{"core_version",state_.coreVersion},
        {"core_state",coreStateText(state_.core)},{"core_ready",state_.ready},{"proxy_state",proxyStateText(state_.proxy)},
        {"mode",state_.mode},{"mixed_port",settings_.mixedPort},{"profile_count",profiles_.size()},{"read_only",readOnly_},
        {"auto_start_known",state_.autoStartKnown},{"auto_start_enabled",state_.autoStartEnabled},
        {"last_error",lastError}};
    runTask(this, [path, report = report, snapshot]() mutable { QStringList logs; for (const auto& line : snapshot.lines) logs.append(Diagnostics::sanitize(line, snapshot.secrets)); report["logs"] = QJsonArray::fromStringList(logs); return writeAtomic(path, QJsonDocument(report).toJson()); }, [this](Result r) { finish(r); });
}
void AppController::closeConnection(const QString& id) {
    const OperationScope operation(this, OperationId::ConnectionClose);
    bool present = false; for (const auto& value : connections_) if (value.toObject().value("id").toString() == id) present = true;
    if (!connectionsCurrent() || !state_.ready || !isId(id) || !present) { emit operationFinished(false, connectionsPaused_?"连接已暂停，请恢复并等待新快照后再关闭":"连接已离开当前快照，请重新选择"); return; }
    if (!begin("正在关闭选中连接")) return;
    const auto token = connectionsGeneration_;
    api_.closeConnection(id, [this, id, token](Result written) {
        api_.connections( [this, id, token, written](Result observed) {
            if (!observed.ok) { finish(observed); return; }
            const auto field = observed.data.value("connections");
            if (!observed.data.contains("connections") || (!field.isArray() && !field.isNull())) { finish(Result::fail("连接关闭后的回读格式错误")); return; }
            const auto values = field.toArray(); bool present = false; for (const auto& value : values) if (value.toObject().value("id").toString() == id) present = true;
            if (connectionsVisible_ && token == connectionsGeneration_) { acceptConnectionSnapshot(observed.data); emit connectionsChanged(); }
            finish(!present ? Result{} : !written.ok ? written : Result::fail("选中连接仍在内核快照中"));
        });
    });
}
void AppController::closeAllConnections() {
    const OperationScope operation(this, OperationId::ConnectionCloseAll);
    if (!connectionsCurrent() || !state_.ready || connections_.isEmpty()) { emit operationFinished(false,connectionsPaused_?"连接已暂停，请恢复并等待新快照后再关闭":"当前没有可关闭的活动连接"); return; }
    if (!begin("正在关闭全部活动连接")) return;
    const auto token=connectionsGeneration_; QSet<QString> before; for (const auto& value:connections_) before.insert(value.toObject().value("id").toString());
    api_.closeAllConnections([this,token,before](Result written) {
        api_.connections([this,token,before,written](Result observed) {
            if (!observed.ok || !observed.data.contains("connections") || (!observed.data.value("connections").isArray() && !observed.data.value("connections").isNull())) { finish(observed.ok?Result::fail("关闭全部后的回读格式错误"):observed); return; }
            bool remaining=false; for (const auto& value:observed.data.value("connections").toArray()) { const auto id=value.toObject().value("id").toString(); if (!isId(id)) { finish(Result::fail("关闭全部后的连接 ID 回读损坏")); return; } if (before.contains(id)) remaining=true; }
            if (connectionsVisible_ && token==connectionsGeneration_) { acceptConnectionSnapshot(observed.data); emit connectionsChanged(); }
            finish(!remaining?Result{}:!written.ok?written:Result::fail("部分原活动连接仍未关闭"));
        });
    });
}
void AppController::testNodes(const QStringList& names,const QString& group) {
    if (!state_.ready || names.isEmpty() || !begin("正在测速")) return;
    delayTestUrl_=groupTestUrl(group); delayQueue_ = names; delayQueue_.removeDuplicates(); delayQueue_ = delayQueue_.mid(0, 500);
    delayTotal_ = delayQueue_.size(); delayCompleted_ = delayInFlight_ = 0; ++delayGeneration_; emit delayProgress(0, delayTotal_); runDelayBatch();
}
void AppController::runDelayBatch() {
    const auto generation = delayGeneration_;
    constexpr int kConcurrentProbes = 12; // Two local HTTP pools; no per-node OS threads.
    while (delayInFlight_ < kConcurrentProbes && !delayQueue_.isEmpty()) {
        const auto name = delayQueue_.takeFirst(); ++delayInFlight_;
        api_.delay(name, [this, name, generation](Result r) {
            if (generation != delayGeneration_) return;
            --delayInFlight_; ++delayCompleted_;
            const int delay = r.data.value("delay").toInt(-1);
            const auto error=r.ok && delay>0 ? QString() : r.error.isEmpty()?QString("测速失败"):r.error;
            recordTrayDelay(name,r.ok && delay>0?delay:-1,error,delayTestUrl_);
            emit nodeDelay(name, r.ok && delay > 0 ? delay : -1, error);
            emit delayProgress(delayCompleted_, delayTotal_);
            if (delayQueue_.isEmpty() && delayInFlight_ == 0) { finish({}); return; }
            runDelayBatch();
        }, 5000, delayTestUrl_);
    }
}
void AppController::cancelTests() {
    if (delayInFlight_ == 0 && delayQueue_.isEmpty()) return;
    ++delayGeneration_; delayQueue_.clear(); delayInFlight_ = 0; api_.cancelAll(); updateDiagnosticStream(); finish({true,{},{{"cancelled",true}}});
}
void AppController::startHomeGroupTests(const QString& group) {
    if (!state_.ready || state_.busy || state_.exiting || state_.readOnly) return;
    const auto object=proxies_.value(group).toObject();
    if (!object.value("all").isArray()) return;
    QStringList members;
    for (const auto& value:object.value("all").toArray()) {
        const auto name=value.toString();
        const auto proxy=proxies_.value(name).toObject();
        if (name.isEmpty() || name.size()>512 || proxy.isEmpty()
            || QStringList{"Direct","Reject","RejectDrop","Pass"}.contains(proxy.value("type").toString())) continue;
        members.append(name);
    }
    if (members.isEmpty()) return;
    groupDelayTests_.start(group,members,groupTestUrl(group));
}
void AppController::cancelHomeGroupTests() { groupDelayTests_.cancel(); }
QJsonObject AppController::homeGroupTestStatus() const {
    return {{"group",groupDelayTests_.group()},{"running",groupDelayTests_.running()},
            {"completed",groupDelayTests_.completed()},{"total",groupDelayTests_.total()},
            {"cancelled",groupDelayTests_.cancelled()}};
}
QJsonObject AppController::trayNodeDelay(const QString& name,const QString& group) const {
    const auto url=groupTestUrl(group); const auto found=trayDelays_.constFind(name+QChar::Null+url);
    const bool cached=state_.ready && trayCacheGeneration_==state_.generation && found!=trayDelays_.cend(); const bool known=cached&&trayClock_.elapsed()-found->measured<60000;
    return {{"known",known},{"cached",cached},{"pending",trayTestUrl_==url&&(trayRequests_.contains(name) || trayQueue_.contains(name))},
        {"delay",cached?found->delay:-1},{"error",cached?found->error:QString()},{"measured_at",cached?found->time.toString(Qt::ISODateWithMs):QString()}};
}
void AppController::recordTrayDelay(const QString& name,int delay,const QString& error,const QString& url) {
    lastNodeTestUrl_=url.isEmpty()?settings_.testUrl:url; const auto key=name+QChar::Null+lastNodeTestUrl_;
    if (trayCacheGeneration_!=state_.generation) { trayDelays_.clear(); trayCacheGeneration_=state_.generation; }
    if (trayDelays_.size()>=500 && !trayDelays_.contains(key)) trayDelays_.clear();
    trayDelays_.insert(key,{delay,diagnostics_.sanitize(error),trayClock_.elapsed(),QDateTime::currentDateTimeUtc()}); emit trayDelaysChanged();
}
void AppController::cancelTrayTests() {
    const bool pending=!trayQueue_.isEmpty() || !trayRequests_.isEmpty();
    ++trayGeneration_; trayQueue_.clear(); const auto requests=trayRequests_; trayRequests_.clear();
    for (const auto& reply:requests) if (reply) reply->abort();
    if (pending) emit trayDelaysChanged();
}
void AppController::testTrayGroup(const QString& name,bool force) {
    cancelTrayTests();
    if (!state_.ready || state_.busy || state_.exiting || state_.readOnly) return;
    const auto group=proxies_.value(name).toObject(); if (!group.value("all").isArray()) return;
    trayTestUrl_=groupTestUrl(name);
    if (trayCacheGeneration_!=state_.generation) { trayDelays_.clear(); trayCacheGeneration_=state_.generation; }
    QSet<QString> seen;
    QStringList candidates;
    if (!force && group.value("all").toArray().contains(group.value("now"))) candidates.append(group.value("now").toString());
    for (const auto& member:group.value("all").toArray()) candidates.append(member.toString());
    for (const auto& node:candidates) {
        const auto proxy=proxies_.value(node).toObject();
        if (node.isEmpty() || node.size()>512 || proxy.isEmpty() || seen.contains(node) || QStringList{"Direct","Reject","RejectDrop","Pass"}.contains(proxy.value("type").toString())) continue;
        seen.insert(node); if (force) trayDelays_.remove(node+QChar::Null+trayTestUrl_);
        if (!trayNodeDelay(node,name).value("known").toBool()) trayQueue_.append(node);
        // Hover sampling is quick and bounded; the explicit full-group action
        // retains the existing 500-node ceiling and cancellation behavior.
        if (seen.size()>=(force?500:16)) break;
    }
    trayRunGeneration_=state_.generation; emit trayDelaysChanged(); runTrayDelayBatch();
}
void AppController::runTrayDelayBatch() {
    const auto token=trayGeneration_; const auto run=trayRunGeneration_;
    while (trayRequests_.size()<3 && !trayQueue_.isEmpty()) {
        const auto name=trayQueue_.takeFirst(); trayRequests_.insert(name,{});
        auto* reply=api_.delay(name,[this,name,token,run](Result result) {
            if (token!=trayGeneration_ || run!=state_.generation || !state_.ready || state_.exiting) return;
            trayRequests_.remove(name); const auto delay=result.data.value("delay").toInt(-1);
            const auto error=result.ok && delay>0?QString():result.error.isEmpty()?QString("测速失败"):result.error;
            recordTrayDelay(name,result.ok && delay>0?delay:-1,error,trayTestUrl_); emit nodeDelay(name,result.ok && delay>0?delay:-1,error);
            QTimer::singleShot(0,this,[this,token] { if (token==trayGeneration_) runTrayDelayBatch(); });
        },5000,trayTestUrl_);
        if (trayRequests_.contains(name)) trayRequests_[name]=reply;
    }
}
}
