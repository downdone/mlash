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
namespace {
YAML::Node redactDnsPreview(const YAML::Node& value) {
    if (value.IsMap()) { YAML::Node result(YAML::NodeType::Map); for (const auto& entry : value) result[entry.first.as<std::string>()]=redactDnsPreview(entry.second); return result; }
    if (value.IsSequence()) { YAML::Node result(YAML::NodeType::Sequence); for (const auto& entry : value) result.push_back(redactDnsPreview(entry)); return result; }
    if (value.IsScalar()) { const auto text=QString::fromStdString(value.Scalar()); if (text.contains("://")) { QUrl url(text); if (url.isValid()) { url.setUserInfo(QString()); url.setQuery(QString()); url.setFragment(QString()); return YAML::Node(url.toString().toStdString()); } } }
    return YAML::Clone(value);
}
}
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
        api_.config([this, generation](Result r) {
            if (generation != state_.generation || state_.busy || state_.exiting) return;
            if (!r.ok) { handleCrash("内核控制面健康检查失败"); return; }
            if (settings_.tunEnabled&&!r.data.value("tun").toObject().value("enable").toBool()) { handleCrash("TUN 运行配置已改变，停止并恢复受控状态"); return; }
            state_.mode = r.data.value("mode").toString("rule").toLower(); state_.ready = true; publish();
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
QString AppController::homeNode() const {
    if (!state_.ready) return {}; QString node=state_.mode=="global"?QString("GLOBAL"):state_.mode=="direct"?QString("DIRECT"):homeProbeGroup_; QSet<QString> visited;
    for (int hop=0;hop<16&&!visited.contains(node);++hop) { visited.insert(node); const auto object=proxies_.value(node).toObject(); if (object.isEmpty()) return {}; if (!object.value("all").isArray()) return node; if (!QStringList{"Selector","URLTest","Fallback"}.contains(object.value("type").toString())) return {}; const auto next=object.value("now").toString(); if (next.isEmpty()||!object.value("all").toArray().contains(next)) return {}; node=next; }
    return {};
}
void AppController::updateHomeProbeContext() { homeProbe_.setContext(state_.ready&&(!state_.busy||preferencesOnly_)&&!state_.exiting&&!state_.readOnly,websiteNode(homeNode()),state_.generation,settings_.testUrl,settings_.homeProbeSeconds); }
void AppController::updateHomeSummary(int mask,const QJsonValue& expected) {
    if (mask<0||mask>15) { emit operationFinished(false,"首页摘要显示选项无效"); return; }
    if (!expected.isUndefined()&&(!expected.isDouble()||expected.toDouble()!=settings_.homeSummaryMask)) { emit operationFinished(false,"首页显示已在其他窗口修改，当前草稿保留；请重新打开核对。"); return; }
    if (!beginPreferences("正在保存首页显示偏好")) return; auto next=settings_; next.homeSummaryMask=mask; saveSnapshot(next,profiles_,[this](Result result) { finish(result); });
}
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
    state_.busy = true; ++state_.generation; state_.lastError.clear(); publish(); if (!background) emit operationStarted(operation); return true;
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
    publish(); if (!backgroundOperation_) emit operationFinished(r.ok, r.ok ? r.data.value("notice").toString(r.data.value("cancelled").toBool() ? "测速已取消" : "操作完成") : state_.lastError); backgroundOperation_ = false;
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
void AppController::checkPrepared(PreparedProfile prepared, std::function<void(PreparedProfile)> done, const QString& executable) {
    if (!prepared.result.ok) { done(prepared); return; }
    diagnostics_.setSecrets(prepared.secrets);
    auto downloads = prepared.result.data.value("providers").toArray();
    if (!downloads.isEmpty()) {
        const auto provider = downloads.takeAt(0).toObject(); prepared.result.data["providers"] = downloads;
        auto downloadProfile=prepared.profile; const auto policy=downloadProfile.providerPolicies.value(provider.value("path").toString()).toObject(); if (policy.contains("download")) downloadProfile.download=DownloadPolicy::fromJson(policy.value("download").toObject());
        downloadProfileSource(downloadProfile, QUrl(provider.value("url").toString()), [this, prepared, provider, executable, done = std::move(done)](Result r, QByteArray bytes) mutable {
            if (!r.ok) { prepared.result = r; done(prepared); return; }
            const auto store = store_;
            runTask(this, [store, prepared, provider, bytes] {
                auto payload = ConfigBuilder::checkProvider(bytes, provider.value("kind").toString(), provider.value("format").toString(), provider.value("behavior").toString());
                if (payload.result.ok) payload.result = writeAtomic(prepared.workPath + "/" + provider.value("path").toString(), payload.yaml);
                if (payload.result.ok) payload.result = store->recordProviderSnapshot(prepared.workPath, provider, bytes);
                return payload;
            }, [this, prepared, executable, done = std::move(done)](BuiltConfig payload) mutable {
                if (!payload.result.ok) { prepared.result = payload.result; done(prepared); return; }
                diagnostics_.setSecrets(payload.secrets); checkPrepared(prepared, std::move(done), executable);
            });
        }); return;
    }
    if (!backgroundOperation_) emit operationStarted("正在校验配置；首次 Geo 数据下载可能需要数分钟");
    core_.validate(executable.isEmpty() ? settings_.corePath : executable, prepared.configPath, prepared.workPath,
        [prepared, done = std::move(done)](Result r) mutable { prepared.result = r; done(prepared); });
}
QJsonObject AppController::nodePreferences(const QString& id) const { const auto index=profileIndex(id.isEmpty()?state_.activeProfile:id); return index<0?QJsonObject{}:profiles_[index].nodePreferences; }
QString AppController::groupTestUrl(const QString& group) const { const auto value=nodePreferences().value("groups").toObject().value(group).toObject().value("test_url").toString(); return value.isEmpty()?settings_.testUrl:value; }
void AppController::updateNodePreferences(const QString& id,const QJsonObject& preferences,const QJsonObject& expected) {
    const auto i=profileIndex(id); if (i<0||profiles_[i].nodePreferences!=expected) { emit operationFinished(false,"节点偏好已变化，请重新打开后编辑"); return; }
    const auto checked=validateNodePreferences(preferences); if (!checked.ok) { emit operationFinished(false,checked.error); return; }
    if (!beginPreferences("正在保存节点偏好")) { emit operationFinished(false,"当前操作尚未结束或处于只读保护，请稍后重试"); return; }
    cancelTrayTests(); auto next=profiles_; next[i].nodePreferences=preferences; rememberPrivateSource(next[i]); saveSnapshot(settings_,next,[this](Result r) { finish(r); });
}
void AppController::prepareActivation(Profile profile, const QString& version, Settings settings) {
    const auto store = store_; const int port = chooseApiPort(); const auto secret = createSecret();
    runTask(this, [store, profile, version, settings, port, secret] { return store->prepareExisting(profile, version, settings, port, secret); },
        [this, settings](PreparedProfile prepared) {
            if (!prepared.result.ok) { finish(prepared.result); return; }
            diagnostics_.setSecrets(prepared.secrets);
            checkPrepared(prepared, [this, settings](PreparedProfile checked) {
                if (!checked.result.ok) { finish(checked.result); return; } transact(checked, settings);
            }, settings.corePath);
        });
}
void AppController::activateProfile(const QString& id, bool lastGood) {
    const int i = profileIndex(id);
    if (i < 0) { emit operationFinished(false, "订阅不存在"); return; }
    const auto version = lastGood ? profiles_[i].lastGood : profiles_[i].candidate;
    if (version.isEmpty()) { emit operationFinished(false, "没有可回滚版本"); return; }
    if (!begin(lastGood ? "正在回滚订阅" : "正在激活订阅")) return;
    proxyRestoreAllowed_ = true;
    prepareActivation(profiles_[i], version, settings_);
}
void AppController::updateCustomRules(const QString& id, const QStringList& rules,const QJsonValue& expected) {
    const int index = profileIndex(id); if (index < 0) { emit operationFinished(false,"订阅不存在"); return; }
    updateRuleOverlay(id, rules, profiles_[index].appendRules, profiles_[index].disabledRules,expected);
}
void AppController::inspectRuleSource(const QString& id, Completion done) {
    const int index = profileIndex(id); if (index < 0 || ruleSourcePending_>=3 || state_.exiting) { done(Result::fail("订阅不可用或来源正在读取")); return; }
    ++ruleSourcePending_; const auto profile = profiles_[index]; const auto settings = settings_; const auto version = state_.ready && state_.activeProfile == id ? profile.active : profile.candidate; const auto store = store_; const auto generation = state_.generation;
    runTask(this,[store,profile,settings,version] {
        QByteArray source; const auto root = store->versionRoot(profile.id,version); if (root.isEmpty()) return Result::fail("规则版本不可用"); const auto read = readFile(root+"/source.yaml",source); if (!read.ok) return read;
        auto referenceProfile=profile; referenceProfile.customRules.clear(); referenceProfile.appendRules.clear(); const auto built=ConfigBuilder::buildProfile(source,settings,settings.mixedPort==19090?19091:19090,QString(64,'x'),referenceProfile); if (!built.result.ok) return built.result;
        const auto config = YAML::Load(source.toStdString()); QJsonArray rules,providers; const auto original = config["rules"];
        if (original) for (size_t i=0;i<qMin(size_t(5000),original.size());++i) { if (!original[i].IsScalar()) return Result::fail("来源规则必须是字符串"); rules.append(QString::fromStdString(original[i].Scalar())); }
        QJsonObject definitions; if (config["rule-providers"]) for (const auto& entry:config["rule-providers"]) { const auto name=QString::fromStdString(entry.first.as<std::string>()); QJsonObject row; for (const auto* key:{"type","url","behavior","format"}) { const auto field=entry.second[key]; row[key]=field&&field.IsScalar()?QString::fromStdString(field.Scalar()):(QString::fromLatin1(key)=="format"?QString("yaml"):QString()); } definitions[name]=row; }
        const auto effective=YAML::Load(built.yaml.toStdString()); if (effective["rule-providers"]) for (const auto& entry:effective["rule-providers"]) providers.append(QString::fromStdString(entry.first.as<std::string>())); QJsonArray targets{"DIRECT","REJECT","REJECT-DROP","PASS","COMPATIBLE","GLOBAL"}; for (const auto* key:{"proxies","proxy-groups"}) if (effective[key]) for (const auto& entry:effective[key]) targets.append(QString::fromStdString(entry["name"].as<std::string>()));
        return Result{true,{},{{"source_provider_definitions",definitions},{"targets",targets},{"expected",RuleDraft::snapshot(profile)},{"rules",rules},{"total",int(original ? original.size() : 0)},{"rule_providers",providers},{"version",version}}};
    },[this,generation,done=std::move(done)](Result result) mutable { --ruleSourcePending_; done(generation == state_.generation && !state_.exiting ? result : Result::fail("规则版本已变化，请重新打开编辑器")); });
}
void AppController::updateRuleOverlay(const QString& id,const QStringList& rules,const QStringList& appended,const QStringList& disabled,const QJsonValue& expected) {
    const int i=profileIndex(id); if (i<0) { emit operationFinished(false,"订阅不存在"); return; } updateRuleDefinitions(id,rules,appended,disabled,profiles_[i].ruleProviderEdits,expected);
}
void AppController::updateRuleDefinitions(const QString& id,const QStringList& rules,const QStringList& appended,const QStringList& disabled,const QJsonObject& providers,const QJsonValue& expected) {
    const int i=profileIndex(id); if (i<0) { emit operationFinished(false,"订阅不存在"); return; }
    if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=RuleDraft::snapshot(profiles_[i]))) { emit operationFinished(false,"规则或来源版本已在其他窗口修改，请重新载入；当前草稿保留"); return; }
    if (rules.size()+appended.size()>256||disabled.size()>256) { emit operationFinished(false,"前置和追加合计最多256条，屏蔽最多256条"); return; }
    const auto valid=validateRuleProviderEdits(providers); if (!valid.ok) { emit operationFinished(false,valid.error); return; }
    QStringList refresh; const auto before=profiles_[i].ruleProviderEdits; QSet<QString> names; for (auto it=before.begin();it!=before.end();++it) names.insert(it.key()); for (auto it=providers.begin();it!=providers.end();++it) names.insert(it.key()); for (const auto& name:names) if (before.value(name)!=providers.value(name)) refresh.append(providerSnapshotId("rule-providers",name));
    auto profile=profiles_[i]; profile.customRules=rules; profile.appendRules=appended; profile.disabledRules=disabled; profile.ruleProviderEdits=providers; rememberPrivateSource(profile); updateProfileOverrides(profile,"正在校验规则与规则集声明",refresh);
}
void AppController::updateDnsOverride(const QString& id, const QString& yaml, bool enabled,const QJsonValue& expected) {
    const int index=profileIndex(id); if (index<0 || yaml.toUtf8().size()>64*1024 || yaml.contains(QChar::Null)) { emit operationFinished(false,"订阅不可用或DNS覆盖超过64KiB"); return; }
    const auto& current=profiles_[index]; const QJsonObject actual{{"yaml",current.dnsOverride},{"enabled",current.dnsOverrideEnabled},{"candidate",current.candidate},{"active",current.active}}; if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=actual)) { emit operationFinished(false,"DNS覆盖或来源版本已变化，请重新打开；本窗口草稿保留"); return; }
    auto profile=profiles_[index]; profile.dnsOverride=yaml; profile.dnsOverrideEnabled=enabled; updateProfileOverrides(profile,"正在校验订阅DNS覆盖");
}
void AppController::updateProxyEdits(const QString& id,const QJsonObject& edits,const QJsonValue& expected,const QJsonValue& sourceExpected) {
    const int index=profileIndex(id); if (index<0) { emit operationFinished(false,"订阅不存在"); return; }
    if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=profiles_[index].proxyEdits)) { emit operationFinished(false,"节点覆盖已在其他窗口修改，请重新打开；本窗口草稿保留"); return; }
    if (!sourceExpected.isUndefined()&&(!sourceExpected.isObject()||sourceExpected.toObject()!=QJsonObject{{"candidate",profiles_[index].candidate},{"active",profiles_[index].active}})) { emit operationFinished(false,"节点来源版本已变化，请重新打开；本窗口草稿保留"); return; }
    auto profile=profiles_[index]; profile.proxyEdits=edits; updateProfileOverrides(profile,"正在校验节点与策略组编辑");
}
void AppController::inspectProxyEntries(const QString& id,Completion done) {
    const int index=profileIndex(id); if (index<0||proxyPreviewPending_||state_.exiting) { done(Result::fail("订阅不可用或节点正在读取")); return; }
    proxyPreviewPending_=true; const auto profile=profiles_[index]; const auto settings=settings_; const auto root=store_->versionRoot(id,state_.ready&&state_.activeProfile==id?profile.active:profile.candidate); const auto generation=state_.generation;
    runTask(this,[profile,settings,root] {
        if (root.isEmpty()) return Result::fail("节点配置版本不可用"); QByteArray source; const auto read=readFile(root+"/source.yaml",source); if (!read.ok) return read;
        const auto built=ConfigBuilder::buildProfile(source,settings,settings.mixedPort==19090?19091:19090,QString(64,'x'),profile); if (!built.result.ok) return built.result;
        const auto config=YAML::Load(source.toStdString()); QJsonObject nodes,groups;
        const auto collect=[](const YAML::Node& sequence,QJsonObject& output) { if (sequence) for (const auto& entry:sequence) output[QString::fromStdString(entry["name"].as<std::string>())]=QString::fromStdString(YAML::Dump(entry)); };
        collect(config["proxies"],nodes); collect(config["proxy-groups"],groups);
        const auto effective=YAML::Load(built.yaml.toStdString()); QJsonArray nodeNames,targets{"DIRECT"}; QJsonObject dialers;
        if (effective["proxies"]) for (const auto& entry:effective["proxies"]) { const auto name=QString::fromStdString(entry["name"].as<std::string>()); nodeNames.append(name); targets.append(name); dialers[name]=QString::fromStdString(entry["dialer-proxy"].as<std::string>("")); }
        QJsonObject groupMembers; if (effective["proxy-groups"]) for (const auto& entry:effective["proxy-groups"]) { const auto name=QString::fromStdString(entry["name"].as<std::string>()); targets.append(name); QJsonArray members; const auto sequence=entry["proxies"]; if (sequence&&sequence.IsSequence()) for (const auto& member:sequence) if (member.IsScalar()) members.append(QString::fromStdString(member.Scalar())); groupMembers[name]=members; }
        QJsonObject sourceDialers; for (const auto& value:nodeNames) { const auto name=value.toString(); const auto definition=profile.proxyEdits.value("nodes").toObject().value(name).toString(nodes.value(name).toString()); const auto original=YAML::Load(definition.toStdString()); const auto dialer=static_cast<const YAML::Node&>(original)["dialer-proxy"]; sourceDialers[name]=dialer&&dialer.IsScalar()?QString::fromStdString(dialer.Scalar()):QString(); }
        QJsonArray providerNames; if (effective["proxy-providers"]) for (const auto& entry:effective["proxy-providers"]) if (entry.first.IsScalar()) providerNames.append(QString::fromStdString(entry.first.Scalar()));
        return Result{true,{},{{"nodes",nodes},{"groups",groups},{"node_names",nodeNames},{"targets",targets},{"dialers",dialers},{"source_dialers",sourceDialers},{"group_members",groupMembers},{"provider_names",providerNames}}};
    },[this,generation,done=std::move(done)](Result result) mutable { proxyPreviewPending_=false; done(generation==state_.generation&&!state_.exiting?result:Result::fail("节点版本已变化，请重新打开编辑器")); });
}
void AppController::inspectProxyImpact(const QString& id,const QJsonObject& draft,const QString& name,const QJsonObject& sourceExpected,Completion done) {
    const int index=profileIndex(id);
    if (index<0||proxyPreviewPending_||state_.busy||state_.exiting) { done(Result::fail("订阅不可用或正在读取，请稍后重试")); return; }
    const auto profile=profiles_[index];
    if (sourceExpected!=QJsonObject{{"candidate",profile.candidate},{"active",profile.active}}) { done(Result::fail("节点来源版本已变化，请重新打开；本窗口草稿保留")); return; }
    proxyPreviewPending_=true; const auto settings=settings_; const auto generation=state_.generation;
    const auto root=store_->versionRoot(id,state_.ready&&state_.activeProfile==id?profile.active:profile.candidate);
    runTask(this,[profile,settings,root,draft,name] {
        if (root.isEmpty()) return Result::fail("节点配置版本不可用");
        QByteArray source; const auto read=readFile(root+"/source.yaml",source); if (!read.ok) return read;
        const auto built=ConfigBuilder::buildProfile(source,settings,settings.mixedPort==19090?19091:19090,QString(64,'x'),profile);
        if (!built.result.ok) return built.result;
        try { return inspectProxyReferences(YAML::Load(source.toStdString()),YAML::Load(built.yaml.toStdString()),draft,name); }
        catch (const std::exception&) { return Result::fail("无法读取引用；草稿保留，请重新打开"); }
    },[this,profile,generation,done=std::move(done)](Result result) mutable {
        proxyPreviewPending_=false; const int current=profileIndex(profile.id);
        if (current<0||state_.exiting||generation!=state_.generation||profiles_[current].candidate!=profile.candidate||profiles_[current].active!=profile.active||profiles_[current].proxyEdits!=profile.proxyEdits)
            done(Result::fail("节点或来源版本已变化，请重新打开；当前草稿保留"));
        else done(std::move(result));
    });
}
void AppController::updateProxyDialers(const QString& id,const QJsonObject& dialers,const QJsonObject& expected) {
    const int index=profileIndex(id); if (index<0) { emit operationFinished(false,"订阅不存在"); return; }
    auto edits=profiles_[index].proxyEdits; if (edits.value("dialers").toObject()!=expected) { emit operationFinished(false,"拨号链已在其他窗口修改，请重新打开；草稿保留"); return; }
    if (dialers.isEmpty()) edits.remove("dialers"); else edits["dialers"]=dialers; updateProxyEdits(id,edits);
}
void AppController::updateProfileOverrides(Profile profile, const QString& operation,const QStringList& refreshProviders) {
    const int i=profileIndex(profile.id); if (i<0) { emit operationFinished(false,"订阅不存在"); return; }
    if (!begin(operation)) return;
    const auto previous=profiles_[i]; const auto id=profile.id;
    const bool running = state_.ready && state_.activeProfile == id;
    const auto version = running ? profile.active : profile.candidate;
    const auto store = store_; const auto settings = settings_; const int port = chooseApiPort(); const auto secret = createSecret();
    runTask(this, [store,profile,previous,version,settings,port,secret,refreshProviders] { if (profile.ruleProviderEdits!=previous.ruleProviderEdits) { const auto migrated=store->ensureVersionSnapshots(previous); if (!migrated.ok) { PreparedProfile failed; failed.result=migrated; return failed; } } return store->prepareExisting(profile,version,settings,port,secret,true,refreshProviders,true); },
        [this, previous, running](PreparedProfile prepared) {
            checkPrepared(prepared, [this, previous, running](PreparedProfile checked) {
                if (!checked.result.ok) { finish(checked.result); return; }
                if (running) {
                    // Editing running overrides must not silently activate a downloaded candidate.
                    if (previous.candidate != previous.active) checked.profile.candidate = previous.candidate;
                    transact(checked, settings_); return;
                }
                auto next = profiles_; const int i = profileIndex(previous.id); if (i < 0) { finish(Result::fail("订阅不存在")); return; }
                next[i] = checked.profile; saveSnapshot(settings_, next, [this](Result r) { finish(r); });
            });
        });
}
void AppController::inspectDns(const QString& id, Completion done) {
    const int index=profileIndex(id); if (index<0 || dnsPreviewPending_ || state_.exiting) { done(Result::fail("订阅不可用或DNS配置正在读取")); return; }
    dnsPreviewPending_=true; const auto profile=profiles_[index]; const auto settings=settings_; const auto root=store_->versionRoot(id,state_.ready&&state_.activeProfile==id?profile.active:profile.candidate); const auto generation=state_.generation;
    runTask(this,[root,profile,settings] {
        if (root.isEmpty()) return Result::fail("DNS配置版本不可用");
        QByteArray source; const auto read=readFile(root+"/source.yaml",source); if (!read.ok) return read;
        const auto built=ConfigBuilder::buildProfile(source,settings,settings.mixedPort==19090?19091:19090,QString(64,'x'),profile); if (!built.result.ok) return built.result;
        const auto original=YAML::Load(source.toStdString()), effective=YAML::Load(built.yaml.toStdString());
        const auto text=[](const YAML::Node& node) { return node ? QString::fromStdString(YAML::Dump(redactDnsPreview(node))) : QString("未配置DNS字段"); };
        QJsonArray sourceFields; if (original["dns"]&&original["dns"].IsMap()) for (const auto& field:yamlFields("dns")) if (static_cast<const YAML::Node&>(original)["dns"][field.key.toStdString()]) sourceFields.append(field.key);
        return Result{true,{},{{"source_fields",sourceFields},{"source_yaml",text(original["dns"])},{"effective_yaml",text(effective["dns"])},{"fields",built.result.data.value("dns_override_fields")},{"overridden",profile.dnsOverrideEnabled}}};
    },[this,generation,done=std::move(done)](Result result) mutable { dnsPreviewPending_=false; done(generation==state_.generation&&!state_.exiting?result:Result::fail("配置已变化，请重新读取DNS")); });
}
quint64 AppController::queryDns(const QString& name, const QString& type, Completion done) {
    QString normalized=QString::fromLatin1(QUrl::toAce(name.trimmed())); if (normalized.endsWith('.')) normalized.chop(1);
    static const QRegularExpression domain("^[a-zA-Z0-9](?:[a-zA-Z0-9.-]{0,251}[a-zA-Z0-9])?$");
    if (!state_.ready || state_.busy || state_.exiting || dnsQuery_ || normalized.size()>253 || !domain.match(normalized).hasMatch() || normalized.contains("..") || !QStringList{"A","AAAA"}.contains(type)) { done(Result::fail("内核未就绪、已有查询或域名/类型无效")); return 0; }
    for (const auto& label : normalized.split('.')) if (label.isEmpty() || label.size()>63 || label.startsWith('-') || label.endsWith('-')) { done(Result::fail("域名标签无效")); return 0; }
    const auto token=++dnsQueryGeneration_, generation=state_.generation; QUrlQuery query; query.addQueryItem("name",normalized); query.addQueryItem("type",type);
    dnsQuery_=api_.request("GET","/dns/query",{},[this,token,generation,done=std::move(done)](Result result) mutable {
        if (token==dnsQueryGeneration_) dnsQuery_=nullptr;
        if (!result.ok) result.error=diagnostics_.sanitize(result.error);
        done(token==dnsQueryGeneration_&&generation==state_.generation&&state_.ready?result:Result::fail("DNS查询已取消"));
    },4000,query);
    return token;
}
void AppController::cancelDnsQuery(quint64 token) {
    if (token && token!=dnsQueryGeneration_) return;
    ++dnsQueryGeneration_; const auto reply=dnsQuery_; dnsQuery_=nullptr; if (reply) reply->abort();
}
void AppController::setConnectionsVisible(bool visible) {
    if (connectionsVisible_ == visible) return;
    connectionsVisible_ = visible; connectionsPaused_=false; cancelConnectionRead();
    connectionSession_.reset(); connectionClock_.start(); connections_ = {}; connectionError_.clear(); emit connectionsChanged();
    if (visible) { connectionsTimer_.start(); refreshConnections(); } else connectionsTimer_.stop();
}
void AppController::setConnectionsPaused(bool paused) {
    if (!connectionsVisible_||connectionsPaused_==paused||state_.exiting) return;
    connectionsPaused_=paused; cancelConnectionRead();
    if (paused) { connectionsTimer_.stop(); connectionSession_.suspend(); connections_=connectionSession_.active(); }
    else { connectionsTimer_.start(); refreshConnections(); }
    emit connectionsChanged();
}
void AppController::refreshRules() {
    if (!rulesVisible_ || !state_.ready || state_.busy || state_.exiting) return;
    const auto generation = state_.generation; const auto token = rulesGeneration_;
    api_.request("GET", "/rules", {}, [this, generation, token](Result r) {
        if (!rulesVisible_ || token != rulesGeneration_ || generation != state_.generation || !state_.ready) return;
        activeRules_ = {}; if (r.ok) { const auto values = r.data.value("rules").toArray(); for (int i = 0; i < qMin(5000, int(values.size())); ++i) activeRules_.append(values[i]); }
        emit rulesChanged(); if (!r.ok) emit operationFinished(false, r.error);
    });
}
void AppController::setRulesVisible(bool visible) {
    if (rulesVisible_ == visible) return;
    rulesVisible_ = visible; ++rulesGeneration_;
    activeRules_ = {}; emit rulesChanged();
}
void AppController::cancelConnectionRead() {
    ++connectionsGeneration_; connectionsPending_=false; const auto pending=connectionRead_; connectionRead_=nullptr;
    if (pending) pending->abort();
}
void AppController::refreshConnections() {
    if (!connectionsVisible_ || connectionsPaused_ || connectionsPending_ || !state_.ready || state_.busy || state_.exiting) return;
    connectionsPending_ = true; const auto token = connectionsGeneration_; const auto generation = state_.generation;
    connectionRead_=api_.request("GET", "/connections", {}, [this, token, generation](Result r) {
        if (token != connectionsGeneration_) return;
        connectionsPending_ = false; connectionRead_=nullptr;
        if (!connectionsVisible_ || connectionsPaused_ || generation != state_.generation || !state_.ready || state_.exiting) return;
        if (r.ok) acceptConnectionSnapshot(r.data);
        else { connectionSession_.invalidate(); connections_={}; connectionError_=r.error; }
        emit connectionsChanged();
    }, 1500);
}
void AppController::acceptConnectionSnapshot(const QJsonObject& snapshot) {
    if (connectionsPaused_) return;
    const auto result=connectionSession_.update(snapshot,connectionClock_.isValid()?connectionClock_.elapsed():0);
    connectionError_=result.ok ? (connectionSession_.totals().value("truncated").toBool()?"仅显示前 5000 条连接，不推断遗漏条目的结束":"") : result.error;
    if (!result.ok) connectionSession_.invalidate(); connections_=connectionSession_.active();
}
void AppController::startCore(bool allowProxyRestore) {
    if (core_.state() == CoreState::Running) return;
    const int i = profileIndex(settings_.activeProfile);
    if (i < 0) { emit operationFinished(false, "先导入并激活一个订阅"); return; }
    if (!begin("正在启动内核")) return;
    restartAttempts_ = 0;
    proxyRestoreAllowed_ = allowProxyRestore;
    prepareActivation(profiles_[i], profiles_[i].active.isEmpty() ? profiles_[i].candidate : profiles_[i].active, settings_);
}
void AppController::resumeDesiredProxy(Completion done) {
    if (!settings_.desiredProxy || !state_.ready || !proxyRestoreAllowed_) { done({}); return; }
    const auto proxy = proxy_; const int port = settings_.mixedPort; const auto bypass=settings_.proxyBypass;
    runTask(this, [proxy, port,bypass] { return proxy->acquire(port,false,bypass); }, [this, done = std::move(done)](Result r) mutable { applyProxyObservation(r); publish(); done(r); });
}
void AppController::verifyProfileOverrides(const Profile& profile,int expectedCount,Completion done) {
    verifyRuleOverlay(profile.customRules,profile.appendRules,expectedCount,[this,edits=profile.proxyEdits,done=std::move(done)](Result rules) mutable {
        if (!rules.ok||edits.isEmpty()) { done(rules); return; }
        api_.proxies([edits,done=std::move(done)](Result observed) mutable {
            if (!observed.ok) { done(observed); return; } const auto proxies=observed.data.value("proxies").toObject();
            for (const auto* key:{"remove_nodes","remove_groups"}) for (const auto& name:edits.value(key).toArray()) if (proxies.contains(name.toString())) { done(Result::fail("内核仍包含已移除的节点或组，恢复旧配置")); return; }
            for (const auto* key:{"nodes","groups"}) { const auto entries=edits.value(key).toObject(); for (auto entry=entries.begin();entry!=entries.end();++entry) if (!proxies.contains(entry.key())||(QString(key)=="groups"&&!proxies.value(entry.key()).toObject().value("all").isArray())) { done(Result::fail("内核节点或策略组回读缺失，恢复旧配置")); return; } }
            done({});
        });
    });
}
void AppController::verifyRuleOverlay(const QStringList& prepend, const QStringList& append, int expectedCount, Completion done) {
    if (prepend.isEmpty() && append.isEmpty() && expectedCount < 0) { done({}); return; }
    api_.request("GET", "/rules", {}, [prepend, append, expectedCount, done = std::move(done)](Result r) mutable {
        if (!r.ok) { done(r); return; }
        const auto observed = r.data.value("rules").toArray();
        if (observed.size() < prepend.size() + append.size() || (expectedCount >= 0 && observed.size() != expectedCount)) { done(Result::fail("内核规则回读数量不一致")); return; }
        const auto matches = [&](const QStringList& rules, int offset) {
            for (int i=0;i<rules.size();++i) { const auto parts=rules[i].split(','); auto expected=parts[0]; expected.remove('-'); const auto row=observed[i+offset].toObject(); auto type=row.value("type").toString(); type.remove('-'); if (expected.compare(type,Qt::CaseInsensitive)!=0 || row.value("proxy").toString()!=parts.value(parts[0]=="MATCH"?1:2).trimmed()) return false; }
            return true;
        };
        if (!matches(prepend,0) || !matches(append,observed.size()-append.size())) { done(Result::fail("内核前置/追加规则回读不一致")); return; }
        done({});
    });
}
void AppController::stopCore() {
    if (!begin("正在安全停止内核")) return;
    const auto proxy = proxy_;
    runTask(this, [proxy] { return proxy->release(); }, [this](Result released) {
        applyProxyObservation(released);
        if (!released.ok && !released.data.value("conflict").toBool()) { finish(released); return; }
        core_.stop([this, released](Result r) {
            proxies_ = {}; emit proxiesChanged();
            auto settings = settings_; settings.desiredProxy = false;
            saveSnapshot(settings, profiles_, [this, r, released](Result saved) { finish(!saved.ok ? saved : !r.ok ? r : released); });
        });
    });
}
void AppController::setTunEnabled(bool enabled) { updateTunOptions(enabled,settings_.tunStack,settings_.tunStrictRoute); }
void AppController::inspectTunService(Completion done) {
    const auto platform=platform_; const auto path=settings_.corePath;
    runTask(this,[platform,path] { auto result=platform->tunServiceInfo(false); if (result.ok) { QByteArray bytes; result.data["client_core_matches"]=readFile(path,bytes,128*1024*1024).ok&&QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()==TunCoreSha256; } return result; },[this,done=std::move(done)](Result r) mutable { if (r.ok) r.data["session"]=core_.tunObservation(); done(r); });
}
void AppController::inspectNetworkInterfaces(Completion done) { const auto platform=platform_; runTask(this,[platform] { return platform->networkInterfaces(); },[done=std::move(done)](Result result) mutable { done(result); }); }
void AppController::repairTunService() {
    if (smoke_||state_.tunObserved||settings_.tunEnabled||state_.core==CoreState::Failed) { emit operationFinished(false,"请先关闭 TUN 并确认清理，再修复服务；冒烟模式不执行安装"); return; }
    if (!begin("正在修复 TUN 服务，请确认 Windows 管理员授权")) return;
    const auto platform=platform_; const auto helper=QCoreApplication::applicationDirPath()+"/MlashTunService.exe";
    runTask(this,[platform,helper] { return platform->repairTunService(helper); },[this](Result result) { finish(result); });
}
void AppController::retryTunCleanup() {
    if (smoke_||state_.ready||state_.core==CoreState::Starting||state_.core==CoreState::Stopping) { emit operationFinished(false,"请先停止内核，再确认 TUN 清理；冒烟模式不操作真实服务"); return; }
    if (!begin("正在确认旧 TUN 会话和路由清理")) return;
    core_.confirmTunCleanup([this](Result stopped) { if (!stopped.ok) { finish(stopped); return; } const auto proxy=proxy_; runTask(this,[proxy] { return proxy->release(); },[this](Result released) { applyProxyObservation(released); if (!released.ok) { finish(released); return; } auto next=settings_; next.tunEnabled=false; next.desiredProxy=false; saveSnapshot(next,profiles_,[this](Result saved) { finish(saved); }); }); });
}
void AppController::manageTunService(bool install) {
    if (smoke_||state_.tunObserved||settings_.tunEnabled||state_.core==CoreState::Failed) { emit operationFinished(false,"请先关闭 TUN 并确认内核已停止，再安装或移除服务；隔离冒烟不执行系统安装"); return; }
    if (!begin(install?"正在安装 TUN 服务，请确认 Windows 管理员授权":"正在移除 TUN 服务，请确认 Windows 管理员授权")) return;
    const auto platform=platform_; const auto helper=QCoreApplication::applicationDirPath()+"/MlashTunService.exe";
    runTask(this,[platform,install,helper] { return platform->manageTunService(install,helper); },[this](Result r) { finish(r); });
}
void AppController::updateTunOptions(bool enabled,const QString& stack,bool strictRoute) {
    auto next=settings_; next.tunEnabled=enabled; next.tunStack=stack; next.tunStrictRoute=strictRoute; updateTunSettings(next);
}
void AppController::updateTunSettings(const Settings& options,const QJsonValue& expected) {
    if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=tunOptionsJson(settings_))) { emit operationFinished(false,"TUN 设置已在其他窗口修改，请重新打开；当前草稿保留"); return; }
    auto next=settings_; next.tunEnabled=options.tunEnabled; next.tunStack=options.tunStack; next.tunStrictRoute=options.tunStrictRoute; next.tunExcludeRoutes=options.tunExcludeRoutes; next.tunMtu=options.tunMtu; next.tunDnsHijack=options.tunDnsHijack; const bool enabled=next.tunEnabled;
    if (!validTunOptions(next)) { emit operationFinished(false,"TUN 协议栈选项无效"); return; }
    if (!enabled&&!settings_.tunEnabled&&!state_.tunObserved&&core_.state()!=CoreState::Failed) {
        if (!beginPreferences("正在保存 TUN 选项")) return;
        saveSnapshot(next,profiles_,[this](Result saved) { finish(saved); }); return;
    }
    const int i=profileIndex(settings_.activeProfile);
    if (enabled&&i<0) { emit operationFinished(false,"请先激活一个订阅，再开启 TUN"); return; }
    if (!begin(enabled?"正在校验并启用 TUN":"正在关闭 TUN 并恢复普通代理")) return;
    const auto platform=platform_; const bool running=core_.state()==CoreState::Running;
    runTask(this,[platform,next] {
        if (!next.tunEnabled) return Result{};
        const auto info=platform->tunServiceInfo(false); if (!info.ok) return info; if (!info.data.value("installed").toBool()) return Result::fail("请先在设置的 TUN 服务中完成安装");
        if (info.data.contains("core_matches")&&!info.data.value("core_matches").toBool()) return Result::fail("已安装服务的内核不匹配，请关闭 TUN 后修复服务");
        if ((!next.tunExcludeRoutes.isEmpty()||next.tunMtu!=1500||!next.tunDnsHijack)&&!info.data.value("advanced_options").toBool()) return Result::fail("已安装服务版本尚不支持高级选项，请关闭 TUN 后修复服务");
        QByteArray bytes; if (!readFile(next.corePath,bytes,128*1024*1024).ok||QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()!=TunCoreSha256) return Result::fail("TUN 服务仅支持随包锁定版本的 mihomo，请恢复随包内核"); return Result{};
    },[this,next,i,running](Result r) {
        if (!r.ok) { finish(r); return; }
        if (exitPending_) { finish(Result::fail("TUN 修改已取消")); return; }
        if (!running&&!next.tunEnabled) { core_.stop([this,next](Result stopped) { if (!stopped.ok) { finish(stopped); return; } saveSnapshot(next,profiles_,[this](Result saved) { finish(saved); }); }); return; }
        if (i<0||i>=profiles_.size()) { finish(Result::fail("活动订阅不存在")); return; }
        prepareActivation(profiles_[i],profiles_[i].active.isEmpty()?profiles_[i].candidate:profiles_[i].active,next);
    });
}
void AppController::setSystemProxy(bool enabled, bool allowExternal) {
    if (enabled && (!state_.ready || core_.state() != CoreState::Running)) { emit operationFinished(false, "内核尚未就绪，系统代理保持不变"); return; }
    if (!begin(enabled ? "正在接管系统代理" : "正在恢复系统代理")) return;
    proxyRestoreAllowed_ = true;
    const auto proxy = proxy_; const int port = settings_.mixedPort; const auto bypass=settings_.proxyBypass;
    runTask(this, [proxy, port, enabled, allowExternal,bypass] { return enabled ? proxy->acquire(port, allowExternal,bypass) : proxy->release(); }, [this, enabled](Result r) {
        applyProxyObservation(r);
        if (!r.ok) { finish(r); return; }
        auto settings = settings_; settings.desiredProxy = enabled;
        saveSnapshot(settings, profiles_, [this, r](Result saved) { finish(saved.ok ? r : saved); });
    });
}
void AppController::setMode(const QString& mode) {
    if (!QStringList{"rule","global","direct"}.contains(mode) || !state_.ready) { emit operationFinished(false, "内核未就绪或模式不合法"); return; }
    if (!begin("正在切换模式")) return;
    api_.request("PATCH", "/configs", {{"mode", mode}}, [this, mode](Result written) {
        api_.config([this, mode, written](Result observed) {
            if (observed.ok) state_.mode = observed.data.value("mode").toString().toLower();
            finish(observed.ok && state_.mode == mode ? Result{} : !observed.ok ? observed : !written.ok ? written : Result::fail("模式回读不一致"));
        });
    });
}
void AppController::selectMember(const QString& group, const QString& member) {
    if (!state_.ready) { emit operationFinished(false, "内核未就绪"); return; }
    const auto selected = proxies_.value(group).toObject();
    if (!QStringList{"Selector","URLTest","Fallback"}.contains(selected.value("type").toString())) { emit operationFinished(false, "此策略组不支持手动选择"); return; }
    if (!selected.value("all").toArray().contains(member)) { emit operationFinished(false, "组成员不可用"); return; }
    if (!begin("正在切换策略组成员")) return;
    const auto apply=[this,group,member](QStringList affected,int skipped) {
    const auto complete=[this,affected,skipped](Result stored) { if (!stored.ok) { finish(stored); return; } closeAffectedConnections(affected,skipped,[this](Result result) { finish(result); }); };
    api_.select(group, member, [this, group, member,complete](Result written) {
        api_.request("GET", "/proxies/" + MihomoClient::segment(group), {}, [this, group, member, written,complete](Result observed) {
            if (observed.ok) { proxies_[group] = observed.data; emit proxiesChanged(); }
            if (!observed.ok || observed.data.value("now").toString() != member) { finish(!observed.ok ? observed : !written.ok ? written : Result::fail("节点选择回读不一致")); return; }
            const int i = profileIndex(settings_.activeProfile);
            if (i < 0 || !profiles_[i].rememberSelections) { complete({}); return; }
            auto next = profiles_; next[i].selections[group] = member;
            if (group.size() <= 512 && member.size() <= 512) {
                QJsonArray recent{QJsonObject{{"group",group},{"member",member}}};
                for (const auto& value : next[i].recentSelections) if (value.toObject().value("group").toString() != group || value.toObject().value("member").toString() != member) { if (recent.size() < 32) recent.append(value); }
                next[i].recentSelections = recent;
            }
            saveSnapshot(settings_, next, complete);
        });
    });
    };
    if (!nodePreferences().value("disconnect_affected").toBool() || selected.value("now").toString()==member) { apply({},0); return; }
    api_.request("GET","/connections",{},[this,group,apply](Result result) {
        if (!result.ok||(!result.data.value("connections").isArray()&&!result.data.value("connections").isNull())) { finish(result.ok?Result::fail("未能确认切换前的连接，未切换节点"):result); return; }
        QStringList ids; int skipped=0; QSet<QString> seen;
        for (const auto& value:result.data.value("connections").toArray()) { const auto row=value.toObject(); const auto chains=row.value("chains").toArray(); const auto id=row.value("id").toString(); if (!chains.contains(group)||!isId(id)||seen.contains(id)) continue; seen.insert(id); if (ids.size()<32) ids.append(id); else ++skipped; }
        apply(ids,skipped);
    },1500);
}
void AppController::reconcileSelections(Profile profile, std::function<void(Result, Profile)> done) {
    if (!profile.rememberSelections || profile.selections.isEmpty()) { done({}, profile); return; }
    api_.proxies([this, profile, done = std::move(done)](Result r) mutable {
        if (!r.ok) { done(r, profile); return; }
        restoreSelectionList(profile, r.data.value("proxies").toObject(), profile.selections.keys(), std::move(done));
    });
}
void AppController::closeAffectedConnections(const QStringList& ids,int skipped,Completion done) {
    if (ids.isEmpty()) { done(skipped?Result{true,{},{{"notice",QString("节点已切换；%1 条连接保持").arg(skipped)}}}:Result{}); return; }
    struct Batch { QStringList queue; int inFlight=0; }; auto state=std::make_shared<Batch>(); state->queue=ids; auto pump=std::make_shared<std::function<void()>>();
    const std::weak_ptr<std::function<void()>> weak=pump;
    *pump=[this,state,weak,ids,skipped,done] {
        const auto continuation=weak.lock(); if (!continuation) return;
        if (state->queue.isEmpty()&&state->inFlight==0) {
            const auto finalize=[this,ids,skipped,done] {
            api_.request("GET","/connections",{},[this,ids,skipped,done](Result result) {
                if (!result.ok || (!result.data.value("connections").isArray()&&!result.data.value("connections").isNull())) { done({true,{},{{"notice","节点已切换；受影响连接的关闭结果未能确认"}}}); return; }
                int remaining=0; for (const auto& value:result.data.value("connections").toArray()) if (ids.contains(value.toObject().value("id").toString())) ++remaining;
                if (connectionsVisible_) { acceptConnectionSnapshot(result.data); emit connectionsChanged(); }
                done({true,{},{{"notice",QString("节点已切换；已断开 %1 条受影响连接，%2 条保持").arg(ids.size()-remaining).arg(remaining+skipped)}}});
            },1500); };
            finalize(); return;
        }
        while (state->inFlight<3&&!state->queue.isEmpty()) { const auto id=state->queue.takeFirst(); ++state->inFlight; api_.request("DELETE","/connections/"+MihomoClient::segment(id),{},[state,continuation](Result) { --state->inFlight; (*continuation)(); },1500); }
    }; (*pump)();
}
void AppController::restoreSelectionList(Profile profile, QJsonObject proxies, QStringList groups, std::function<void(Result, Profile)> done) {
    if (groups.isEmpty()) { done({}, profile); return; }
    const auto group = groups.takeFirst(); const auto member = profile.selections.value(group).toString(); const auto current = proxies.value(group).toObject();
    if (!current.value("all").toArray().contains(member) || !QStringList{"Selector","URLTest","Fallback"}.contains(current.value("type").toString())) {
        profile.selections.remove(group); diagnostics_.append("app", "原选中成员在新配置中不可用，采用内核默认选择");
        restoreSelectionList(profile, proxies, groups, std::move(done)); return;
    }
    api_.select(group, member, [this, profile, proxies, groups, group, member, done = std::move(done)](Result written) mutable {
        api_.request("GET", "/proxies/" + MihomoClient::segment(group), {}, [this, profile, proxies, groups, member, written, done = std::move(done)](Result observed) mutable {
            if (!observed.ok || observed.data.value("now").toString() != member) { done(!observed.ok ? observed : !written.ok ? written : Result::fail("保存的节点选择回读不一致"), profile); return; }
            restoreSelectionList(profile, proxies, groups, std::move(done));
        });
    });
}
void AppController::clearAutomaticSelection(const QString& group) {
    const auto type = proxies_.value(group).toObject().value("type").toString();
    if (!state_.ready || !QStringList{"URLTest","Fallback"}.contains(type)) { emit operationFinished(false, "仅自动策略组可取消手动固定"); return; }
    if (!begin("正在恢复自动选择")) return;
    api_.request("DELETE", "/proxies/" + MihomoClient::segment(group), {}, [this, group](Result written) {
        api_.request("GET", "/proxies/" + MihomoClient::segment(group), {}, [this, group, written](Result observed) {
            if (!observed.ok || !observed.data.value("fixed").toString().isEmpty()) { finish(!observed.ok ? observed : !written.ok ? written : Result::fail("自动组仍处于固定状态")); return; }
            proxies_[group] = observed.data; emit proxiesChanged();
            const int i = profileIndex(settings_.activeProfile); if (i < 0) { finish({}); return; }
            auto next = profiles_; next[i].selections.remove(group); saveSnapshot(settings_, next, [this](Result r) { finish(r); });
        });
    });
}
void AppController::refresh() {
    if (!state_.ready || state_.exiting) return;
    const auto generation = state_.generation;
    api_.proxies([this, generation](Result r) { if (generation == state_.generation && r.ok) { proxies_ = r.data.value("proxies").toObject(); emit proxiesChanged(); } });
}
void AppController::updateSettings(const Settings& desired) {
    if (!validAppearance(desired.appearance,desired.textScale)) { emit operationFinished(false,"外观或文字比例无效"); return; }
    auto withoutAppearance=desired; withoutAppearance.appearance=settings_.appearance; withoutAppearance.textScale=settings_.textScale;
    if (withoutAppearance.toJson()==settings_.toJson()&&(desired.appearance!=settings_.appearance||desired.textScale!=settings_.textScale)) { updateAppearance(desired.appearance,desired.textScale); return; }
    if (desired.startupPage<0 || desired.startupPage>7 || !QStringList{"show","proxy-page","toggle-proxy"}.contains(desired.trayClick) ||
        !QStringList{"source","enabled","disabled"}.contains(desired.ipv6Mode) || !QStringList{"source","enabled","disabled"}.contains(desired.unifiedDelayMode)) { emit operationFinished(false,"设置选项不合法"); return; }
    if (!validTestUrl(desired.testUrl)) { emit operationFinished(false, "连通测试地址须为不含用户凭据/片段的 HTTP/HTTPS URL"); return; }
    if (desired.mixedPort < 1024 || desired.mixedPort > 65535 || !QStringList{"debug","info","warning","error","silent"}.contains(desired.logLevel)) {
        emit operationFinished(false, "端口或日志级别不合法"); return;
    }
    if (!begin("正在应用设置")) return;
    protectMajorChange([this,desired] {
    const bool reconfigure = core_.state() == CoreState::Running &&
        (desired.mixedPort != settings_.mixedPort || desired.logLevel != settings_.logLevel || desired.corePath != settings_.corePath || desired.ipv6Mode!=settings_.ipv6Mode || desired.unifiedDelayMode!=settings_.unifiedDelayMode);
    auto preferences = settings_; preferences.autoStart = desired.autoStart; preferences.silentStart = desired.silentStart;
    preferences.startCore = desired.startCore; preferences.restoreProxy = desired.restoreProxy; preferences.lightweight = desired.lightweight;
    preferences.testUrl = desired.testUrl;
    preferences.appearance=desired.appearance; preferences.textScale=desired.textScale;
    preferences.startupPage=desired.startupPage; preferences.trayClick=desired.trayClick;
    const auto platform = platform_; const auto executable = QCoreApplication::applicationFilePath();
    const bool autoChanged = preferences.autoStart != settings_.autoStart;
    runTask(this, [platform, executable, autoChanged, preferences] {
        auto written = autoChanged ? platform->autoStart(preferences.autoStart, executable) : Result{};
        const auto observed = platform->readAutoStart(executable);
        if (autoChanged && written.ok && (!observed.ok || observed.data.value("enabled").toBool() != preferences.autoStart)) written = Result::fail("自启状态回读失败或不一致");
        written.data["auto_start_known"] = observed.ok; written.data["auto_start_enabled"] = observed.data.value("enabled").toBool(); written.data["auto_start_conflict"] = observed.data.value("conflict").toBool();
        return written;
    }, [this, desired, preferences, reconfigure](Result r) mutable {
            state_.autoStartKnown = r.data.value("auto_start_known").toBool(); state_.autoStartEnabled = r.data.value("auto_start_enabled").toBool(); state_.autoStartConflict = r.data.value("auto_start_conflict").toBool();
            if (state_.autoStartKnown) { settings_.autoStart = state_.autoStartEnabled; preferences.autoStart = state_.autoStartEnabled; }
            emit profilesChanged(); publish();
            if (!r.ok) { finish(r); return; }
            if (!reconfigure) { preferences.mixedPort = desired.mixedPort; preferences.logLevel = desired.logLevel; preferences.corePath = desired.corePath; preferences.ipv6Mode=desired.ipv6Mode; preferences.unifiedDelayMode=desired.unifiedDelayMode; }
            saveSnapshot(preferences, profiles_, [this, desired, reconfigure](Result saved) {
                if (!saved.ok || !reconfigure) { finish(saved); return; }
                auto runtime = settings_; runtime.mixedPort = desired.mixedPort; runtime.logLevel = desired.logLevel; runtime.corePath = desired.corePath; runtime.ipv6Mode=desired.ipv6Mode; runtime.unifiedDelayMode=desired.unifiedDelayMode;
                const int i = profileIndex(settings_.activeProfile);
                if (i < 0) { finish(Result::fail("活动订阅不存在")); return; }
                prepareActivation(profiles_[i], profiles_[i].active, runtime);
            });
        });
    });
}
void AppController::importGeoData(const QString& sourceDirectory) {
    if (!begin("正在导入本地 Geo 数据；当前内核运行版本保持不变")) return;
    const auto store=store_; runTask(this,[store,sourceDirectory] { return store->importGeoData(sourceDirectory); },[this](Result r) { finish(r); });
}
quint64 AppController::beginHotkeyCapture() {
    if (state_.busy||state_.exiting||state_.readOnly||hotkeysLoading_) return 0;
    hotkeyCaptureToken_=++hotkeyCaptureSerial_; emit hotkeyCaptureChanged(hotkeyCaptureToken_); return hotkeyCaptureToken_;
}
void AppController::endHotkeyCapture(quint64 token) { if (!token||token!=hotkeyCaptureToken_) return; hotkeyCaptureToken_=0; hotkeyQuietUntil_=QDeadlineTimer(300); emit hotkeyCaptureChanged(0); }
void AppController::dispatchHotkey(const QString& action) {
    if (state_.exiting) return;
    if (hotkeyCaptureToken_) { const auto sequence=activeHotkeys_.value(action).toString(); if (!sequence.isEmpty()) emit hotkeyCaptured(hotkeyCaptureToken_,sequence); return; }
    if (!hotkeyQuietUntil_.hasExpired()) return;
    if (action=="show-window"||action=="show-proxy"||action=="show-connections") { emit openPageRequested(action=="show-proxy"?2:action=="show-connections"?4:-1); return; }
    // Background key events must not complete another window's in-flight task.
    if (!activeHotkeys_.contains(action)||state_.busy||hotkeysLoading_) return;
    if (state_.readOnly) { emit operationFinished(false,"当前为只读保护，快捷键没有修改网络。"); return; }
    if (action=="toggle-system-proxy") {
        if (state_.proxy==ProxyState::Owned) { setSystemProxy(false); return; }
        if (state_.proxy!=ProxyState::Disabled) { emit openPageRequested(0); emit operationFinished(false,"系统代理不由 Mlash 管理，请在主页确认接管；快捷键没有覆盖现有代理。"); return; }
        if (!state_.ready) { emit operationFinished(false,"内核未就绪，快捷键没有修改系统代理。"); return; } setSystemProxy(true,false); return;
    }
    if (action=="toggle-tun") { if (!state_.ready||settings_.tunEnabled!=state_.tunObserved||state_.activeProfile.isEmpty()) { emit openPageRequested(6); emit operationFinished(false,"请先启动运行订阅并在设置确认 TUN 状态；快捷键不安装或修复服务。"); return; } setTunEnabled(!state_.tunObserved); return; }
    if (!state_.ready) { emit operationFinished(false,"内核尚未就绪，网络快捷键没有执行。"); return; }
    if (action=="cycle-mode") { const QStringList modes{"rule","global","direct"}; const int index=modes.indexOf(state_.mode); if (index<0) { emit operationFinished(false,"当前代理模式未知，请先刷新运行状态。"); return; } setMode(modes[(index+1)%modes.size()]); }
    else if (action=="reload-active") reloadActive();
}
void AppController::reloadActive() {
    const int i=profileIndex(settings_.activeProfile); if (!state_.ready||core_.state()!=CoreState::Running||i<0||profiles_[i].active.isEmpty()) { emit operationFinished(false,"没有就绪的当前运行版，未重载或应用候选。"); return; }
    const bool restoreProxy=state_.proxy==ProxyState::Owned; if (!begin("正在重载当前运行版，独立候选保持不变")) return; proxyRestoreAllowed_=restoreProxy; prepareActivation(profiles_[i],profiles_[i].active,settings_);
}
void AppController::updateHotkeys(const QJsonObject& bindings,const QJsonValue& expected) {
    if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=settings_.hotkeys)) { emit operationFinished(false,"快捷键已在其他窗口修改，请重新打开；当前草稿保留。"); return; }

    QList<HotkeyBinding> parsed; const auto valid=parseHotkeys(bindings,parsed); if (!valid.ok) { emit operationFinished(false,valid.error); return; }
    if (hotkeysLoading_) { emit operationFinished(false,"启动快捷键注册尚未结束"); return; }
    if (!beginPreferences("正在注册全局快捷键")) return;
    const auto previous=activeHotkeys_; const auto next=valid.data.value("bindings").toObject();
    hotkeys_->configure(next,[this,previous,next](Result registered) {
        activeHotkeys_=registered.data.value("bindings").toObject(); hotkeyError_=registered.ok?QString():registered.error;
        if (!registered.ok) { finish(registered); return; }
        auto settings=settings_; settings.hotkeys=next;
        saveSnapshot(settings,profiles_,[this,previous](Result saved) { if (saved.ok) { finish(saved); return; } hotkeys_->configure(previous,[this,saved](Result restored) { activeHotkeys_=restored.data.value("bindings").toObject(); hotkeyError_=restored.ok?QString():restored.error; finish(restored.ok?saved:Result::fail(saved.error+"；"+restored.error)); }); });
    });
}
void AppController::updateConnectionView(const QJsonObject& preferences) {
    const auto valid=validateConnectionView(preferences); if (!valid.ok) { emit operationFinished(false,valid.error); return; }
    if (state_.exiting||state_.readOnly) return;
    if (state_.busy) { pendingConnectionView_=preferences; return; }
    pendingConnectionView_.reset();
    if (preferences==settings_.connectionView) return;
    if (!beginPreferences("正在保存连接视图")) return;
    auto next=settings_; next.connectionView=preferences; saveSnapshot(next,profiles_,[this](Result result) { finish(result); });
}
void AppController::updateProxyBypass(const QString& text,const std::optional<QString>& expected) {
    if (expected&&*expected!=settings_.proxyBypass) { emit operationFinished(false,"排除列表已在其他窗口修改，请重新打开；草稿保留"); return; }
    const auto checked=normalizeProxyBypass(text); if (!checked.ok) { emit operationFinished(false,checked.error); return; } const auto bypass=checked.data.value("bypass").toString(); if (bypass==settings_.proxyBypass) { emit operationFinished(true,"排除列表未变化"); return; } if (!begin("正在更新系统代理排除列表")) return;
    const auto proxy=proxy_; const auto previous=settings_.proxyBypass; const int port=settings_.mixedPort;
    runTask(this,[proxy,port,bypass,previous] { auto observed=proxy->inspect(); if (!observed.ok) return observed; const bool owned=observed.data.value("state").toString()=="owned"; if (owned) { observed=proxy->acquire(port,false,bypass); if (!observed.ok) { const auto rollback=proxy->acquire(port,false,previous); observed.error+=rollback.ok?"；旧排除目标已恢复":"；保留恢复日志："+rollback.error; observed.data=rollback.data; return observed; } } observed.data["updated_os"]=owned&&observed.ok; return observed; },[this,proxy,previous,port,bypass](Result applied) {
        applyProxyObservation(applied); if (!applied.ok) { finish(applied); return; } auto next=settings_; next.proxyBypass=bypass;
        saveSnapshot(next,profiles_,[this,proxy,previous,port,applied](Result saved) { if (saved.ok||!applied.data.value("updated_os").toBool()) { finish(saved); return; } runTask(this,[proxy,previous,port] { return proxy->acquire(port,false,previous); },[this,saved](Result rollback) { applyProxyObservation(rollback); finish(Result::fail(saved.error+(rollback.ok?"；旧代理排除列表已恢复":"；恢复失败，保留日志："+rollback.error))); }); });
    });
}
void AppController::updateAppearance(const QString& appearance,int scale) { if (!validAppearance(appearance,scale)) { emit operationFinished(false,"外观或文字比例无效"); return; } if (!beginPreferences("正在保存外观设置")) return; auto next=settings_; next.appearance=appearance; next.textScale=scale; saveSnapshot(next,profiles_,[this](Result result) { finish(result); }); }
void AppController::drainConnectionView() { if (state_.busy||state_.exiting||state_.readOnly||!pendingConnectionView_) return; const auto preferences=*pendingConnectionView_; pendingConnectionView_.reset(); updateConnectionView(preferences); if (!state_.busy) runMaintenance(); }
bool AppController::beginPreferences(const QString& operation) { if (state_.busy||state_.exiting||state_.readOnly) return false; preferencesOnly_=true; backgroundOperation_=false; state_.busy=true; state_.lastError.clear(); publish(); emit operationStarted(operation); return true; }
void AppController::startIpProbe(bool coreRoute) {
    startOutboundProbe(coreRoute,false,{});
}
void AppController::startServiceProbe(bool coreRoute,const QString& id) {
    startOutboundProbe(coreRoute,true,id);
}
void AppController::startOutboundProbe(bool coreRoute,bool services,const QString& id) {
    if (state_.busy||state_.exiting||outboundDiagnostics_.busy()||(coreRoute&&!state_.ready)) return;
    const auto token=outboundDiagnostics_.prepare({{"kind",services?"services":"ip"},{"route",coreRoute?"core":"direct"},{"mode",coreRoute?state_.mode:QString()},{"mixed_port",coreRoute?settings_.mixedPort:0},{"core_pid",coreRoute?double(core_.processId()):0.0},{"chain","未观测；不能由当前选中节点推断"}});
    if (!coreRoute) { if (services) outboundDiagnostics_.runServices(token,0,id); else outboundDiagnostics_.runIp(token,0); return; }
    const auto platform=platform_; const auto pid=core_.processId(); const auto port=settings_.mixedPort; const auto generation=state_.generation;
    runTask(this,[platform,pid,port] { return platform->ownsTcpPort(pid,port); },[this,token,port,generation,services,id](Result result) {
        if (generation!=state_.generation||state_.busy||state_.exiting||!state_.ready) { outboundDiagnostics_.fail(token,"运行状态已变化，请重试"); return; }
        if (!result.ok||!result.data.value("owned").toBool()) { outboundDiagnostics_.fail(token,"本地代理端口不属于当前受控内核，未发送请求"); return; }
        if (services) outboundDiagnostics_.runServices(token,port,id); else outboundDiagnostics_.runIp(token,port);
    });
}
void AppController::inspectGeoData(Completion done) {
    if (geoPreviewPending_||state_.exiting) { done(Result::fail("Geo数据正在读取")); return; }
    geoPreviewPending_=true; QString work; const int index=profileIndex(state_.activeProfile);
    if (state_.ready&&index>=0) work=store_->versionRoot(state_.activeProfile,profiles_[index].active)+"/work";
    const auto generation=state_.generation;
    runTask(this,[root=root_,work] { return GeoData::inspect(root,work); },[this,generation,done=std::move(done)](Result result) mutable { geoPreviewPending_=false; done(generation==state_.generation&&!state_.exiting?result:Result::fail("运行版本已变化，请刷新Geo列表")); });
}
void AppController::updateGeoData(const QString& name) {
    const auto url=GeoData::source(name); if (url.isEmpty()) { emit operationFinished(false,"不支持的Geo在线数据类型"); return; }
    if (!begin("正在下载Geo默认快照；当前运行版本保持不变")) return;
    DownloadPolicy policy; policy.timeoutSeconds=120;
    downloader_->download(url,[this,name](Result downloaded,QByteArray bytes) {
        if (!downloaded.ok) { finish(downloaded); return; }
        const auto store=store_; runTask(this,[store,name,bytes=std::move(bytes)] { return GeoData::stage(*store,name,bytes); },[this,store](Result staged) {
            if (!staged.ok) { finish(staged); return; }
            const auto path=staged.data.value("stage").toString();
            if (state_.exiting) { runTask(this,[store,path] { GeoData::discard(store->root(),path); return Result::fail("Geo更新已取消"); },[this](Result result) { finish(result); }); return; }
            core_.validate(settings_.corePath,path+"/config.yaml",path,[this,store,staged,path](Result validation) {
                if (!validation.ok || state_.exiting) { runTask(this,[store,path,validation] { GeoData::discard(store->root(),path); return validation.ok?Result::fail("Geo更新已取消"):validation; },[this](Result result) { finish(result); }); return; }
                runTask(this,[store,staged] { return GeoData::commit(*store,staged); },[this](Result result) { finish(result); });
            },15000);
        });
    },policy,0,128*1024*1024);
}
void AppController::previewActiveConfig(std::function<void(Result,QString)> done) {
    if (previewPending_ || state_.busy || state_.exiting) { done(Result::fail("当前操作尚未结束"),{}); return; }
    const int i=profileIndex(settings_.activeProfile); if (i<0 || !isId(profiles_[i].active)) { done(Result::fail("没有当前激活配置"),{}); return; }
    const auto path=store_->versionRoot(profiles_[i].id,profiles_[i].active)+"/work/config.yaml"; const auto secrets=diagnostics_.snapshot().secrets; const auto generation=state_.generation;
    previewPending_=true;
    runTask(this,[path,secrets] { QByteArray bytes; auto result=readFile(path,bytes); if (!result.ok) return Result::fail("当前配置不可读取");
        try { auto yaml=YAML::Load(bytes.toStdString()); int visited=0; const QRegularExpression sensitive("password|passwd|passphrase|secret|token|uuid|username|private[-_]?key|preshared[-_]?key|authorization|authentication|cookie|headers|^psk$|^key$",QRegularExpression::CaseInsensitiveOption);
            std::function<void(YAML::Node,int)> redact=[&](YAML::Node node,int depth) { if (depth>32 || ++visited>200000) throw std::runtime_error("preview limit"); if (node.IsMap()) for (const auto& entry:node) { const auto key=QString::fromStdString(entry.first.as<std::string>()); if (sensitive.match(key).hasMatch()) node[entry.first]=std::string("[redacted]"); else redact(entry.second,depth+1); } else if (node.IsSequence()) for (const auto& child:node) redact(child,depth+1); };
            redact(yaml,0); const auto text=Diagnostics::sanitize(QString::fromStdString(YAML::Dump(yaml)),secrets); result.data["text"]=text.left(524288)+(text.size()>524288?"\n# 预览超过限制，后续内容省略":""); return result;
        } catch (const std::exception&) { return Result::fail("配置格式不可安全预览"); }
    },[this,generation,done=std::move(done)](Result result) mutable { previewPending_=false; if (generation!=state_.generation) { done(Result::fail("运行配置已变化，请重新查看"),{}); return; } done(result,result.data.value("text").toString()); });
}
void AppController::updateDiagnosticStream() { api_.setLogsVisible(diagnosticsVisible_ && state_.ready && diagnosticsLevel_ != "app", diagnosticsLevel_ == "all" ? "debug" : diagnosticsLevel_); }
void AppController::setDiagnosticsVisible(bool visible) { if (diagnosticsVisible_ == visible) return; diagnosticsVisible_ = visible; updateDiagnosticStream(); }
void AppController::setDiagnosticsLevel(const QString& level) {
    if (!QStringList{"all", "debug", "info", "warning", "error", "app"}.contains(level) || level == diagnosticsLevel_) return;
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
    bool present = false; for (const auto& value : connections_) if (value.toObject().value("id").toString() == id) present = true;
    if (!connectionsCurrent() || !state_.ready || !isId(id) || !present) { emit operationFinished(false, connectionsPaused_?"连接已暂停，请恢复并等待新快照后再关闭":"连接已离开当前快照，请重新选择"); return; }
    if (!begin("正在关闭选中连接")) return;
    const auto token = connectionsGeneration_;
    api_.request("DELETE", "/connections/" + MihomoClient::segment(id), {}, [this, id, token](Result written) {
        api_.request("GET", "/connections", {}, [this, id, token, written](Result observed) {
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
    if (!connectionsCurrent() || !state_.ready || connections_.isEmpty()) { emit operationFinished(false,connectionsPaused_?"连接已暂停，请恢复并等待新快照后再关闭":"当前没有可关闭的活动连接"); return; }
    if (!begin("正在关闭全部活动连接")) return;
    const auto token=connectionsGeneration_; QSet<QString> before; for (const auto& value:connections_) before.insert(value.toObject().value("id").toString());
    api_.request("DELETE","/connections",{},[this,token,before](Result written) {
        api_.request("GET","/connections",{},[this,token,before,written](Result observed) {
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
    while (delayInFlight_ < 3 && !delayQueue_.isEmpty()) {
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
    for (const auto& member:group.value("all").toArray()) {
        const auto node=member.toString(); const auto proxy=proxies_.value(node).toObject();
        if (node.isEmpty() || node.size()>512 || proxy.isEmpty() || seen.contains(node) || QStringList{"Direct","Reject","RejectDrop","Pass"}.contains(proxy.value("type").toString())) continue;
        seen.insert(node); if (force) trayDelays_.remove(node+QChar::Null+trayTestUrl_);
        if (!trayNodeDelay(node,name).value("known").toBool()) trayQueue_.append(node);
        if (seen.size()>=500) break;
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
void AppController::cancelWebsiteTests() { websiteTests_.cancel(); }
void AppController::clearWebsiteTests() { websiteTests_.clear(); }
void AppController::updateWebsiteTargets(const QJsonArray& targets) {
    const auto valid = WebsiteTests::validateTargets(targets); if (!valid.ok) { emit operationFinished(false,valid.error); return; }
    if (!begin("正在保存测速目标")) return;
    auto next = settings_; next.websiteTargets = targets; saveSnapshot(next,profiles_,[this](Result result) { finish(result); });
}
void AppController::requestExit() {
    if (state_.exiting) return;
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
