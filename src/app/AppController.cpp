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
AppController::AppController(QString root, bool smoke, QObject* parent, std::shared_ptr<PlatformServices> platform, QList<WebsiteTarget> websiteTargets, std::unique_ptr<GlobalHotkeys> hotkeys,std::unique_ptr<WebDavClient> remoteClient,std::unique_ptr<TunSession> tun,std::unique_ptr<SubscriptionDownloader> sourceDownloader,std::unique_ptr<SubscriptionDownloader> officialDownloader)
    : QObject(parent), root_(std::move(root)), smoke_(smoke), platform_(platform ? std::move(platform) : createPlatformServices()), hotkeys_(hotkeys?std::move(hotkeys):createGlobalHotkeys()),remoteClient_(remoteClient?std::move(remoteClient):std::make_unique<WebDavClient>()),
      store_(std::make_shared<ProfileStore>(root_, platform_)), proxy_(std::make_shared<ProxyCoordinator>(root_, platform_)),
      diagnostics_(root_, this), api_(this), websiteTests_(&api_,this,std::move(websiteTargets)), core_(&api_, platform_, this,std::move(tun)), officialCore_(this,std::move(officialDownloader)), downloader_(sourceDownloader?std::move(sourceDownloader):std::make_unique<SubscriptionDownloader>(this)) {
    downloader_->setParent(this);
    trayClock_.start();
    localBackupTimer_.setParent(this); localBackupTimer_.setObjectName("localBackupTimer"); localBackupTimer_.setInterval(60000); connect(&localBackupTimer_,&QTimer::timeout,this,&AppController::runLocalMaintenance);
    remoteTimer_.setInterval(60000); connect(&remoteTimer_,&QTimer::timeout,this,&AppController::runRemoteMaintenance);
    connect(this,&AppController::proxiesChanged,this,&AppController::updateHomeProbeContext);
    connect(&homeProbe_,&HomeNodeProbe::measured,this,[this](const QString& node,int delay,const QString& error) { recordTrayDelay(node,delay,error); emit nodeDelay(node,delay,error); });
    connect(hotkeys_.get(),&GlobalHotkeys::activated,this,&AppController::dispatchHotkey);
    connect(&maintenance_, &ProfileMaintenance::workAvailable, this, &AppController::runMaintenance);
    providerTimer_.setInterval(60000); providerTimer_.setTimerType(Qt::VeryCoarseTimer); connect(&providerTimer_,&QTimer::timeout,this,&AppController::runProviderMaintenance);
    connect(&core_, &CoreManager::stateChanged, this, [this](CoreState s) {
        state_.core = s; if (s != CoreState::Running) { state_.ready = false; state_.tunObserved=false; }
        if (s != CoreState::Running) { outboundDiagnostics_.cancel(); websiteTests_.clear(); cancelTrayTests(); trayDelays_.clear(); cancelConnectionRead(); connectionSession_.reset(); connections_ = {}; connectionError_.clear(); activeRules_ = {}; emit rulesChanged(); emit connectionsChanged(); }
        if (s == CoreState::Running && !smoke_) health_.start(15000); else health_.stop(); publish();
    });
    connect(&core_, &CoreManager::output, this, [this](const QString& output) { for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) diagnostics_.append("core", line); });
    connect(&core_, &CoreManager::crashed, this, &AppController::handleCrash);
    connect(&api_, &MihomoClient::logReceived, this, [this](const QString& line) { diagnostics_.append("core-log", line); });
    connectionsTimer_.setInterval(2000); connect(&connectionsTimer_, &QTimer::timeout, this, &AppController::refreshConnections);
    connect(&health_, &QTimer::timeout, this, [this] {
        if (state_.busy || state_.exiting || core_.state() != CoreState::Running) return;
        const auto generation = state_.generation;
        api_.configInfo([this, generation](CoreObservation<CoreConfigInfo> observed) {
            if (generation != state_.generation || state_.busy || state_.exiting) return;
            if (!observed.ok()) { handleCrash("内核控制面健康检查失败"); return; }
            if (settings_.tunEnabled&&!observed.value.tunEnabled) { handleCrash("TUN 运行配置已改变，停止并恢复受控状态"); return; }
            state_.mode = observed.value.mode; state_.ready = true; publish();
            updateDiagnosticStream();
        });
        const auto proxy = proxy_;
        runTask(this, [proxy] { return proxy->inspect(); }, [this, generation](Result r) {
            if (generation == state_.generation && !state_.busy) { applyProxyObservation(r); publish(); }
        });
    });
}
void AppController::publish() { if (hotkeyCaptureToken_&&(state_.busy||state_.exiting||state_.readOnly)) endHotkeyCapture(hotkeyCaptureToken_); if ((state_.busy&&!preferencesOnly_)||state_.exiting||(!state_.ready&&outboundDiagnostics_.result().value("route").toString()=="core")) outboundDiagnostics_.cancel(); homeMetrics_.setContext(state_.ready&&(!state_.busy||preferencesOnly_)&&!state_.exiting,core_.processId(),state_.generation); updateHomeProbeContext(); emit stateChanged(); }
void AppController::setHomeVisible(bool visible) { homeMetrics_.setVisible(visible); homeProbe_.setVisible(visible); }
void AppController::setHomeProbeGroup(const QString& group) { if (homeProbeGroup_==group) return; homeProbeGroup_=group; updateHomeProbeContext(); }
void AppController::runConnectivityProbe() {
    // R3b：首页连通性行。与 startWebsiteTests 不同，这里不受页面可见性限制——
    // 它是首页自己的只读探测，也不写入任何节点选择（WebsiteTests 的跨页复用另立切片）。
    const auto node=websiteNode(homeNode());
    if (!state_.ready || state_.busy || state_.exiting || state_.readOnly || node.isEmpty()) {
        emit operationFinished(false, node.isEmpty() ? QString("当前没有可探测的叶子节点，请先在首页选择节点") : QString("内核未就绪，暂时无法测试连通性"));
        return;
    }
    connectivity_.setUrl(settings_.testUrl);
    connectivity_.run(node);
}
void AppController::cancelConnectivityProbe() { connectivity_.cancel(); }
QString AppController::homeNode() const {
    if (!state_.ready) return {}; QString node=state_.mode=="global"?QString("GLOBAL"):state_.mode=="direct"?QString("DIRECT"):homeProbeGroup_; QSet<QString> visited;
    for (int hop=0;hop<16&&!visited.contains(node);++hop) { visited.insert(node); const auto object=proxies_.value(node).toObject(); if (object.isEmpty()) return {}; if (!object.value("all").isArray()) return node; if (!QStringList{"Selector","URLTest","Fallback"}.contains(object.value("type").toString())) return {}; const auto next=object.value("now").toString(); if (next.isEmpty()||!object.value("all").toArray().contains(next)) return {}; node=next; }
    return {};
}
void AppController::updateHomeProbeContext() { homeProbe_.setContext(state_.ready&&(!state_.busy||preferencesOnly_)&&!state_.exiting&&!state_.readOnly,websiteNode(homeNode()),state_.generation,settings_.testUrl,settings_.homeProbeSeconds); }
void AppController::updateHomeProbe(int seconds) { if (!QList<int>{0,60,300}.contains(seconds)) { emit operationFinished(false,"首页探测周期无效"); return; } if (settings_.homeProbeSeconds==seconds) return; if (!beginPreferences("正在保存首页探测偏好")) return; auto next=settings_; next.homeProbeSeconds=seconds; saveSnapshot(next,profiles_,[this](Result result) { finish(result); }); }
void AppController::inspectUrlProtocol(Completion done) { if (protocolPreviewPending_||state_.exiting) { done(Result::fail("关联状态正在读取或即将退出")); return; } protocolPreviewPending_=true; const auto platform=platform_; const auto exe=QCoreApplication::applicationFilePath(); runTask(this,[platform,exe] { return platform->readUrlProtocol(exe); },[this,done=std::move(done)](Result result) mutable { protocolPreviewPending_=false; done(result); }); }
void AppController::setUrlProtocol(bool enabled) { if (!begin("正在更新Mlash链接关联")) return; const auto platform=platform_; const auto exe=QCoreApplication::applicationFilePath(); runTask(this,[platform,exe,enabled] { const auto written=platform->setUrlProtocol(enabled,exe); if (!written.ok) return written; const auto observed=platform->readUrlProtocol(exe); if (!observed.ok||observed.data.value("enabled").toBool()!=enabled||observed.data.value("conflict").toBool()) return Result::fail("链接关联回读不一致"); return observed; },[this](Result result) { finish(result); }); }
void AppController::applyProxyObservation(const Result& r) {
    const auto text = r.data.value("state").toString();
    state_.proxy = text == "owned" ? ProxyState::Owned : text == "disabled" ? ProxyState::Disabled : text == "external" ? ProxyState::External : ProxyState::Unknown;
}
void AppController::initialize() {
    const auto store = store_; const auto proxy = proxy_; const auto platform = platform_;
    const auto executable = QCoreApplication::applicationFilePath();
    runTask(this, [store, proxy, platform, executable] {
        auto snapshot = store->load();
        auto recovery = proxy->hasJournal() ? proxy->release() : proxy->inspect();
        snapshot.result.data["proxy_result"] = recovery.data;
        if (!recovery.ok) snapshot.result.data["recovery_error"] = recovery.error;
        const auto startup = platform->readAutoStart(executable);
        snapshot.result.data["auto_start_known"] = startup.ok;
        snapshot.result.data["auto_start_enabled"] = startup.data.value("enabled").toBool();
        snapshot.result.data["auto_start_conflict"] = startup.data.value("conflict").toBool();
        snapshot.settings.autoStart = startup.ok && startup.data.value("enabled").toBool();
        return snapshot;
    }, [this](StoreSnapshot snapshot) {
        readOnly_ = !snapshot.result.ok; state_.readOnly = readOnly_;
        readOnlyError_ = readOnly_ ? diagnostics_.sanitize(snapshot.result.error) : QString();
        settings_ = snapshot.settings; profiles_ = snapshot.profiles;
        rememberRemoteSecrets();
        websiteTests_.configureTargets(settings_.websiteTargets);
        for (const auto& profile : profiles_) rememberPrivateSource(profile);
        if (settings_.corePath.isEmpty()) {
            const auto directory = QCoreApplication::applicationDirPath();
            const auto packaged = directory + "/cores/mihomo.exe";
            const auto development = QDir(directory).absoluteFilePath("../../../cores/mihomo.exe");
            settings_.corePath = QFileInfo(packaged).exists() ? packaged : QDir::cleanPath(development);
        }
        applyProxyObservation({true, {}, snapshot.result.data.value("proxy_result").toObject()});
        state_.activeProfile = settings_.activeProfile; state_.busy = false;
        state_.autoStartKnown = snapshot.result.data.value("auto_start_known").toBool(); state_.autoStartEnabled = snapshot.result.data.value("auto_start_enabled").toBool(); state_.autoStartConflict = snapshot.result.data.value("auto_start_conflict").toBool();
        state_.lastError = snapshot.result.ok ? snapshot.result.data.value("recovery_error").toString() : snapshot.result.error;
        maintenance_.setProfiles(profiles_); maintenance_.setEnabled(!smoke_ && !readOnly_&&!startupSuppressed_); configureProviderMaintenance();
        diagnostics_.append("app", "Mlash " MLASH_VERSION " 初始化完成");
        if (!state_.lastError.isEmpty()) diagnostics_.append("error", state_.lastError);
        if (!smoke_&&!readOnly_&&!settings_.hotkeys.isEmpty()) { hotkeysLoading_=true; hotkeys_->configure(settings_.hotkeys,[this](Result result) { hotkeysLoading_=false; activeHotkeys_=result.data.value("bindings").toObject(); hotkeyError_=result.ok?QString():result.error; if (!result.ok) diagnostics_.append("error",result.error); publish(); }); }
        remoteInitialized_=true; diagnostics_.setSecrets({settings_.localBackup.value("password").toString()}); if (!smoke_&&!readOnly_&&!startupSuppressed_&&!settings_.localBackup.isEmpty()) localBackupTimer_.start(); if (!smoke_&&!readOnly_&&!startupSuppressed_&&settings_.remoteBackup.value("enabled").toBool()) remoteTimer_.start(); emit profilesChanged(); publish(); emit initialized();
        QTimer::singleShot(0,this,&AppController::runRemoteMaintenance);
        QTimer::singleShot(0,this,&AppController::drainConnectionView);
        if (!smoke_ && !startupSuppressed_ && !readOnly_ && settings_.startCore && !settings_.activeProfile.isEmpty()) QTimer::singleShot(0, this, [this] { startCore(settings_.restoreProxy); });
    });
}
bool AppController::begin(const QString& operation, bool background, bool allowReadOnly) {
    if ((state_.busy && !profileBatchDispatch_) || state_.exiting || (readOnly_ && !allowReadOnly)) {
        emit operationFinished(false, readOnly_ ? "状态文件不可用，已进入只读保护；请检查数据目录" : "当前操作尚未结束"); return false;
    }
    cancelTrayTests();
    cancelDnsQuery();
    websiteTests_.clear();
    changeBackupDone_=false;
    backgroundOperation_ = background;
    // 事务把当前操作标识带进 finish()：调用点只需在 begin 之后表态，不必在每条异步
    // 回调里重复传 id（P4.1 反馈路由）。
    transactionOperation_ = operationId_;
    state_.busy = true; ++state_.generation; state_.lastError.clear(); publish(); if (!background) emit operationStarted(operation, transactionOperation_); return true;
}
void AppController::finish(Result r) {
    if (!sourceUpdateId_.isEmpty()) {
        const auto id = std::exchange(sourceUpdateId_, {}); maintenance_.completed(id, r.ok);
        const int i = profileIndex(id);
        if (!r.ok && i >= 0) {
            auto next = profiles_; next[i].updateError = diagnostics_.sanitize(r.error).left(1024);
            next[i].lastUpdateAttempt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            saveSnapshot(settings_, next, [this, r](Result saved) { finish(saved.ok ? r : Result::fail(r.error + "；更新结果未能保存")); }); return;
        }
    }
    if (profileBatchActive_ && !profileBatchCurrent_.isEmpty()) {
        auto rows=profileBatch_.value("results").toArray(); const auto i=profileIndex(profileBatchCurrent_);
        rows.append(QJsonObject{{"id",profileBatchCurrent_},{"name",i>=0?profiles_[i].name:QString("已删除订阅")},{"ok",r.ok},{"message",r.ok?QString("候选已检查，运行版本保持不变"):diagnostics_.sanitize(r.error).left(1024)}});
        profileBatch_["results"]=rows; profileBatchCurrent_.clear(); emit profileBatchChanged();
        QTimer::singleShot(0,this,&AppController::runProfileBatch); return;
    }
    state_.busy = false; preferencesOnly_=false;
    state_.lastError = r.ok ? readOnlyError_ : diagnostics_.sanitize(r.error);
    if (!r.ok) diagnostics_.append("error", r.error);
    const auto operation = std::exchange(transactionOperation_, OperationId::Unspecified);
    publish(); if (!backgroundOperation_) emit operationFinished(r.ok, r.ok ? r.data.value("notice").toString(r.data.value("cancelled").toBool() ? "测速已取消" : "操作完成") : state_.lastError, operation); backgroundOperation_ = false;
    if (exitPending_) { exitPending_ = false; QTimer::singleShot(0, this, &AppController::requestExit); }
    else if (crashPending_) { crashPending_ = false; const auto error = crashError_; QTimer::singleShot(0, this, [this, error] { handleCrash(error); }); }
    else if (pendingConnectionView_) QTimer::singleShot(0,this,&AppController::drainConnectionView);
    else QTimer::singleShot(0, this, &AppController::runMaintenance);
}
int AppController::profileIndex(const QString& id) const { for (int i = 0; i < profiles_.size(); ++i) if (profiles_[i].id == id) return i; return -1; }
int AppController::chooseApiPort() const { QTcpServer reservation; reservation.setProxy(QNetworkProxy::NoProxy); return reservation.listen(QHostAddress::LocalHost, 0) ? reservation.serverPort() : 0; }
QString AppController::createSecret() const {
    QByteArray secret; for (int i = 0; i < 8; ++i) secret += QByteArray::number(QRandomGenerator::system()->generate(), 16).rightJustified(8, '0');
    return QString::fromLatin1(secret);
}
void AppController::saveSnapshot(Settings settings, QVector<Profile> profiles, Completion done) {
    const auto store = store_;
    runTask(this, [store, settings, profiles] { return store->save(settings, profiles); },
        [this, settings, profiles, done = std::move(done)](Result r) mutable {
            if (r.ok) { settings_ = settings; profiles_ = profiles; websiteTests_.configureTargets(settings_.websiteTargets); maintenance_.setProfiles(profiles_); configureProviderMaintenance(); state_.activeProfile = settings_.activeProfile; emit profilesChanged(); publish(); }
            done(r);
        });
}
}
