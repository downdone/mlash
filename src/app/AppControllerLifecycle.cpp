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
void AppController::handleCrash(const QString& error) {
    if (state_.exiting) return;
    state_.ready = false; proxies_ = {}; emit proxiesChanged();
    if (state_.busy) { crashPending_ = true; crashError_ = error; return; }
    if (!begin("内核故障，正在恢复系统代理")) return;
    const auto proxy = proxy_;
    runTask(this, [proxy] { return proxy->release(); }, [this, error](Result restored) {
        applyProxyObservation(restored);
        core_.stop([this, error, restored](Result stopped) {
            finish(Result::fail(error + (restored.ok ? "；系统代理已检查/恢复" : "；代理恢复失败：" + restored.error)+(stopped.ok?QString():"；内核/TUN 停止未确认："+stopped.error)));
            if (stopped.ok&&restored.ok && restartAttempts_ < 2 && !settings_.activeProfile.isEmpty() && !exitPending_) {
                ++restartAttempts_;
                QTimer::singleShot(1000, this, [this] {
                    if (state_.busy || state_.exiting || core_.state() == CoreState::Running) return;
                    const int i = profileIndex(settings_.activeProfile);
                    if (i >= 0 && begin("正在有限重启内核")) prepareActivation(profiles_[i], profiles_[i].active, settings_);
                });
            }
        });
    });
}
QString AppController::websiteNode(const QString& groupOrNode) const {
    QString node=groupOrNode; QSet<QString> visited;
    for (int hop=0;hop<16 && !visited.contains(node);++hop) {
        visited.insert(node); const auto item=proxies_.value(node).toObject(); if (item.isEmpty()) return {};
        if (!item.value("all").isArray()) return QStringList{"Reject","RejectDrop","Pass"}.contains(item.value("type").toString())?QString():node;
        if (!QStringList{"Selector","URLTest","Fallback"}.contains(item.value("type").toString())) return {};
        node=item.value("now").toString(); if (node.isEmpty()) return {};
    }
    return {};
}
void AppController::setWebsiteTestsVisible(bool visible) { websiteTestsVisible_=visible; if (!visible) websiteTests_.cancel(); }
void AppController::startWebsiteTests(const QString& node,const QString& targetId) {
    if (!websiteTestsVisible_ || !state_.ready || state_.busy || state_.exiting || state_.readOnly || websiteNode(node)!=node || node.isEmpty()) { emit operationFinished(false,"请在测速页选择当前内核中的具体节点，等待内核就绪"); return; }
    websiteTests_.start(node,targetId);
}
void AppController::selectWebsiteTestNode(const QString& node) {
    if (!websiteTestsVisible_ || !state_.ready || state_.exiting || websiteNode(node)!=node || node.isEmpty()) return;
    websiteTests_.selectNode(node);
}
void AppController::cancelWebsiteTests() { websiteTests_.cancel(); }
void AppController::clearWebsiteTests() { websiteTests_.clear(); }
void AppController::updateWebsiteTargets(const QJsonArray& targets) {
    const auto valid = WebsiteTests::validateTargets(targets); if (!valid.ok) { emit operationFinished(false,valid.error); return; }
    if (!begin("正在保存测速目标")) return;
    auto next = settings_; next.websiteTargets = targets; saveSnapshot(next,profiles_,[this](Result result) { finish(result); });
}
void AppController::requestExit() {
    if (state_.exiting) return;
    groupDelayTests_.cancel();
    if (state_.busy) {
        cancelProfileBatch();
        exitPending_ = true; officialCore_.cancel(); downloader_->cancel(); core_.cancelValidation(); coreVersionProbe_.cancel(); cancelRemoteBackup();
        if (delayInFlight_ > 0) cancelTests();
        return;
    }
    cancelTrayTests(); websiteTests_.clear(); state_.exiting = true; state_.busy = true; ++state_.generation; health_.stop(); api_.cancelAll(); downloader_->cancel();
    maintenance_.setEnabled(false); providerTimer_.stop();
    publish(); emit operationStarted("正在恢复代理并退出");
    const auto proxy = proxy_;
    runTask(this, [proxy] { return proxy->release(); }, [this](Result r) {
        applyProxyObservation(r);
        if (!r.ok && !r.data.value("conflict").toBool()) {
            state_.exiting = false;
            if (core_.state() == CoreState::Running && !smoke_) health_.start(15000);
            maintenance_.setEnabled(!smoke_ && !readOnly_); configureProviderMaintenance(); updateDiagnosticStream();
            finish(Result::fail("退出前恢复失败，保留运行状态：" + r.error)); return;
        }
        if (!r.ok) diagnostics_.append("error", "退出恢复：" + r.error);
        core_.stop([this](Result r) {
            if (!r.ok) { state_.exiting=false; maintenance_.setEnabled(!smoke_&&!readOnly_); configureProviderMaintenance(); finish(Result::fail("退出前内核/TUN 清理未确认，请重试；恢复标记保留："+r.error)); return; }
            emit exitReady();
        });
    });
}
}
