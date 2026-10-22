#include "profile/YamlFields.h"
#include "profile/ProxyImpact.h"
#include "profile/VersionChanges.h"
#include "profile/RuleDraft.h"
#include "profile/ProviderEdits.h"
#include "profile/ProviderPolicy.h"
#include "profile/NodeLinks.h"
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
#include <QSet>
#include <yaml-cpp/yaml.h>
#include <QRegularExpression>
#include <QNetworkReply>


namespace mlash {
void AppController::stageSource(Profile profile, QByteArray source) {
    rememberPrivateSource(profile);
    const auto generation = state_.generation; const auto store = store_; const auto settings = settings_;
    const auto port = chooseApiPort(); const auto secret = createSecret();
    runTask(this, [store, profile, source, settings, port, secret]() mutable {
        profile.sourceHash = QString::fromLatin1(QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex());
        profile.lastUpdateAttempt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate); profile.updateError.clear();
        return store->prepare(profile, source, settings, port, secret);
    },
        [this, generation](PreparedProfile prepared) {
            if (generation != state_.generation) return;
            checkPrepared(prepared, [this](PreparedProfile checked) {
                if (!checked.result.ok) { finish(checked.result); return; }
                auto next = profiles_; const int i = profileIndex(checked.profile.id);
                if (i < 0) next.append(checked.profile); else next[i] = checked.profile;
                saveSnapshot(settings_, next, [this](Result r) { finish(r); });
            });
        });
}

void AppController::importUrl(const QString& name, const QUrl& url, const DownloadPolicy& policy) {
    if (!policy.valid()) { emit operationFinished(false, "下载策略无效：超时须为 5–120 秒，UA 须为单行 ASCII"); return; }
    if (!begin("正在下载订阅")) return;
    Profile profile; profile.id = newId(); profile.name = name.trimmed().isEmpty() ? url.host() : name.trimmed().left(120);
    profile.sourceType = "url"; profile.source = url.toString(QUrl::FullyEncoded);
    profile.download = policy;
    downloadProfileSource(profile, url, [this, profile](Result r, QByteArray bytes) mutable { if (!r.ok) finish(r); else { profile.subscriptionInfo = r.data.value("subscription_info").toObject(); stageSource(profile, bytes); } });
}

void AppController::importFile(const QString& name, const QString& path, const QString& replaceId) {
    if (!begin("正在导入本地 YAML")) return;
    if (!QStringList{"yaml","yml"}.contains(QFileInfo(path).suffix().toLower())) { finish(Result::fail("请选择 .yaml/.yml 文件")); return; }
    Profile profile; const int i = profileIndex(replaceId);
    if (!replaceId.isEmpty() && i < 0) { finish(Result::fail("订阅不存在")); return; }
    if (i >= 0) profile = profiles_[i]; else { profile.id = newId(); profile.name = name.trimmed().isEmpty() ? QFileInfo(path).completeBaseName() : name.trimmed().left(120); }
    profile.sourceType = "file"; profile.source = path;
    profile.subscriptionInfo = {}; sourceUpdateId_ = replaceId;
    struct Input { Result result; QByteArray bytes; };
    runTask(this, [path] { Input input; input.result = readFile(path, input.bytes); return input; },
        [this, profile](Input input) { if (!input.result.ok) finish(input.result); else stageSource(profile, input.bytes); });
}

void AppController::previewNodeLinks(const QString& text,const QStringList& reserved,Completion done) {
    if (nodeLinksPreviewPending_||state_.exiting) { done(Result::fail("节点链接正在检查或即将退出，请稍后重试")); return; }
    if (text.size()>64*1024||reserved.size()>20000) { done(Result::fail("节点链接或名称集合超过限制")); return; }
    nodeLinksPreviewPending_=true;
    runTask(this,[text,reserved] { return NodeLinks::parse(text,reserved); },[this,done=std::move(done)](Result result) mutable {
        nodeLinksPreviewPending_=false; done(state_.exiting?Result::fail("正在退出，未保留预览"):std::move(result));
    });
}

void AppController::importNodeLinks(const QString& name,const QString& text) {
    if (text.size()>64*1024) { emit operationFinished(false,"节点链接超过64KiB"); return; }
    if (!begin("正在校验节点链接")) return;
    Profile profile; profile.id=newId(); profile.name=name.trimmed().isEmpty()?QString("节点链接"):name.trimmed().left(120); profile.sourceType="file";
    runTask(this,[text] { return NodeLinks::parse(text); },[this,profile](Result result) {
        if (!result.ok) { finish(result); return; }
        stageSource(profile,result.data.value("yaml").toString().toUtf8());
    });
}

void AppController::downloadProfileSource(const Profile& profile, const QUrl& url, std::function<void(Result, QByteArray)> done) {
    if (profile.download.route == "direct") { downloader_->download(url, std::move(done), profile.download); return; }
    if (!state_.ready || core_.processId() <= 0) { done(Result::fail("本地内核未就绪，未切换为直连；请先启动内核或调整下载路径"), {}); return; }
    const auto generation = state_.generation; const auto platform = platform_; const auto pid = core_.processId(); const int port = settings_.mixedPort;
    runTask(this, [platform, pid, port] { return platform->ownsTcpPort(pid, port); }, [this, profile, url, port, generation, done = std::move(done)](Result observed) mutable {
        if (generation != state_.generation || !state_.ready || !observed.ok || !observed.data.value("owned").toBool()) { done(Result::fail("本地代理身份未能确认，未发送订阅请求"), {}); return; }
        downloader_->download(url, std::move(done), profile.download, port);
    });
}

void AppController::updateProfile(const QString& id) {
    refreshSource(id, false);
}

void AppController::updateProfilesBatch(const QStringList& ids) {
    QStringList unique;
    for (const auto& id:ids) { if (profileIndex(id)<0 || unique.size()>=256) { emit operationFinished(false,"批量更新包含无效订阅或超过 256 项"); return; } if (!unique.contains(id)) unique.append(id); }
    if (unique.isEmpty() || !begin("正在批量更新订阅候选")) return;
    profileBatchActive_=true; profileBatchCancelled_=false; profileBatchQueue_=unique;
    profileBatch_={{"active",true},{"cancel_requested",false},{"total",unique.size()},{"results",QJsonArray{}}}; emit profileBatchChanged();
    QTimer::singleShot(0,this,&AppController::runProfileBatch);
}

void AppController::cancelProfileBatch() {
    if (!profileBatchActive_ || profileBatchCancelled_) return;
    profileBatchCancelled_=true; profileBatch_["cancel_requested"]=true; emit profileBatchChanged();
}

void AppController::runProfileBatch() {
    if (!profileBatchActive_ || !profileBatchCurrent_.isEmpty()) return;
    if (profileBatchCancelled_ || exitPending_ || profileBatchQueue_.isEmpty()) {
        profileBatch_["active"]=false; profileBatch_["skipped"]=profileBatchQueue_.size(); profileBatch_["cancelled"]=profileBatchCancelled_||exitPending_; profileBatchQueue_.clear(); profileBatchActive_=false;
        const auto rows=profileBatch_.value("results").toArray(); int failed=0; for (const auto& row:rows) if (!row.toObject().value("ok").toBool()) ++failed;
        backgroundOperation_=false; emit profileBatchChanged(); finish({true,{},{{"notice",QString("订阅更新结束：已检查 %1 项，失败 %2 项，跳过 %3 项").arg(rows.size()).arg(failed).arg(profileBatch_.value("skipped").toInt())}}}); return;
    }
    profileBatchCurrent_=profileBatchQueue_.takeFirst(); profileBatch_["current"]=profileBatchCurrent_; emit profileBatchChanged();
    profileBatchDispatch_=true; refreshSource(profileBatchCurrent_,true); profileBatchDispatch_=false;
}

QJsonObject AppController::profileMaintenance(const QString& id) const {
    const auto i=profileIndex(id); if (i<0) return {}; const auto& p=profiles_[i];
    auto value=maintenance_.status(id); value["name"]=p.name; value["source"]=p.source; value["source_type"]=p.sourceType; value["automatic"]=p.autoUpdate; value["hours"]=p.updateIntervalHours; value["download"]=p.download.toJson(); return value;
}

void AppController::editProfileSource(const QString& id,const QString& name,const QString& source,bool automatic,int hours,const DownloadPolicy& policy,const QJsonObject& expected) {
    const auto i=profileIndex(id); if (i<0) { emit operationFinished(false,"订阅已不存在"); return; }
    const auto current=profileMaintenance(id);
    for (const auto& key:{"name","source","source_type","automatic","hours","download"}) if (expected.value(key)!=current.value(key)) { emit operationFinished(false,"订阅设置已变化，请重新打开后编辑"); return; }
    const auto input=source.trimmed();
    const bool localSnapshot=profiles_[i].sourceType=="file"&&profiles_[i].source.isEmpty()&&input.isEmpty()&&!automatic;
    if (name.trimmed().isEmpty() || name.size()>120 || (input.isEmpty()&&!localSnapshot) || input.size()>4096 || hours<1 || hours>168 || !policy.valid()) { emit operationFinished(false,"请检查名称、来源、更新周期和下载设置"); return; }
    if (profiles_[i].sourceType=="file" && !localSnapshot && !QStringList{"yml","yaml"}.contains(QFileInfo(input).suffix().toLower())) { emit operationFinished(false,"本地来源须为 YAML 文件"); return; }
    if (!begin("正在保存订阅来源与维护设置")) return;
    auto profile=profiles_[i]; profile.name=name.trimmed(); profile.source=input; profile.autoUpdate=automatic; profile.updateIntervalHours=hours; profile.download=policy;
    if (input==profiles_[i].source) { auto next=profiles_; next[i]=profile; saveSnapshot(settings_,next,[this](Result r) { finish(r); }); return; }
    rememberPrivateSource(profile); profile.subscriptionInfo={};
    if (profile.sourceType=="url") { downloadProfileSource(profile,QUrl(input),[this,profile](Result r,QByteArray bytes) mutable { if (!r.ok) finish(r); else { profile.subscriptionInfo=r.data.value("subscription_info").toObject(); stageSource(profile,bytes); } }); return; }
    struct Input { Result result; QByteArray bytes; };
    runTask(this,[input] { Input r; r.result=readFile(input,r.bytes); return r; },[this,profile](Input r) { if (!r.result.ok) finish(r.result); else stageSource(profile,r.bytes); });
}

void AppController::moveProfile(const QString& id,int direction) {
    const auto i=profileIndex(id), target=i+direction;
    if (i<0 || qAbs(direction)!=1 || target<0 || target>=profiles_.size()) return;
    if (!beginPreferences("正在调整订阅顺序")) return;
    auto next=profiles_; next.move(i,target); saveSnapshot(settings_,next,[this](Result r) { finish(r); });
}

void AppController::refreshSource(const QString& id, bool background) {
    const int i = profileIndex(id);
    if (i < 0) { emit operationFinished(false, "订阅不存在"); return; }
    if (!begin("正在更新候选订阅，运行版本保持不变", background)) return;
    sourceUpdateId_ = id;
    auto profile = profiles_[i];
    if (profile.source.isEmpty()) { finish(Result::fail("本地快照没有外部来源；请编辑节点或更换文件来源")); return; }
    if (profile.sourceType == "file") {
        struct Input { Result result; QByteArray bytes; bool unchanged = false; };
        runTask(this, [profile] {
            Input input; input.result = readFile(profile.source, input.bytes);
            input.unchanged = input.result.ok && QString::fromLatin1(QCryptographicHash::hash(input.bytes, QCryptographicHash::Sha256).toHex()) == profile.sourceHash;
            return input;
        }, [this, profile, background](Input input) {
            if (!input.result.ok) { finish(input.result); return; }
            if (!input.unchanged) { stageSource(profile, input.bytes); return; }
            if (background && !profileBatchActive_ && profile.updateError.isEmpty()) { finish({}); return; }
            auto next = profiles_; const int i = profileIndex(profile.id); next[i].updateError.clear(); next[i].lastUpdateAttempt=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            saveSnapshot(settings_, next, [this](Result r) { finish(r); });
        }); return;
    }
    downloadProfileSource(profile, QUrl(profile.source), [this, profile](Result r, QByteArray source) mutable { if (!r.ok) finish(r); else { profile.subscriptionInfo = r.data.value("subscription_info").toObject(); stageSource(profile, source); } });
}

void AppController::updateProfilePolicy(const QString& id, bool automatic, int intervalHours, std::optional<DownloadPolicy> policy) {
    const int i = profileIndex(id);
    if (i < 0 || intervalHours < 1 || intervalHours > 168) { emit operationFinished(false, "更新周期须为 1–168 小时"); return; }
    if (policy && !policy->valid()) { emit operationFinished(false, "下载策略无效：超时须为 5–120 秒，UA 须为单行 ASCII"); return; }
    if (automatic&&profiles_[i].source.isEmpty()) { emit operationFinished(false,"本地快照没有外部来源，不能启用自动更新"); return; }
    if (!begin("正在保存订阅更新策略")) return;
    auto next = profiles_; next[i].autoUpdate = automatic; next[i].updateIntervalHours = intervalHours;
    if (policy) next[i].download = *policy;
    saveSnapshot(settings_, next, [this](Result r) { finish(r); });
}

void AppController::runMaintenance() {
    if (smoke_ || readOnly_ || state_.busy || state_.exiting || exitPending_) return;
    const auto id = maintenance_.takeDue(); if (!id.isEmpty()) refreshSource(id, true); else runRemoteMaintenance();
}

void AppController::inspectProfileCompatibility(const QString& id, Completion done) {
    const int index = profileIndex(id);
    if (index < 0 || compatibilityPending_ || state_.exiting) { done(Result::fail("订阅不可用或报告正在生成")); return; }
    const auto profile = profiles_[index];
    const auto path = store_->versionRoot(id, profile.candidate.isEmpty() ? profile.active : profile.candidate);
    if (path.isEmpty()) { done(Result::fail("没有可检查的配置版本")); return; }
    compatibilityPending_ = true;
    const auto settings = settings_; const auto generation = state_.generation;
    runTask(this, [path, profile, settings] {
        QByteArray source; auto read = readFile(path + "/source.yaml", source); if (!read.ok) return read;
        return ConfigBuilder::buildProfile(source, settings, settings.mixedPort == 19090 ? 19091 : 19090, QString(64, 'x'), profile).result;
    }, [this, generation, done = std::move(done)](Result report) mutable {
        compatibilityPending_ = false;
        done(generation == state_.generation && !state_.exiting ? report : Result::fail("配置已变化，请重新检查"));
    });
}

void AppController::inspectProviders(const QString& id, Completion done, bool runningVersion) {
    const int index = profileIndex(id);
    if (index < 0 || providersPending_ || state_.exiting) { done(Result::fail("订阅不可用或提供者列表正在读取")); return; }
    providersPending_ = true; const auto profile = profiles_[index]; const auto settings = settings_; const auto store = store_; const auto generation = state_.generation;
    const auto version = runningVersion ? profile.active : profile.candidate.isEmpty() ? profile.active : profile.candidate;
    runTask(this, [store, profile, version, settings] { return store->inspectProviders(profile, version, settings); }, [this, id, generation, version, runningVersion, done = std::move(done)](Result result) mutable {
        providersPending_ = false;
        if (generation != state_.generation || state_.exiting) { done(Result::fail("配置已变化，请刷新列表")); return; }
        auto rows = result.data.value("providers").toArray();
        const int profileIndexValue=profileIndex(id); if (!runningVersion&&result.ok&&profileIndexValue>=0) { const auto policies=profiles_[profileIndexValue].providerPolicies; for (auto it=policies.begin();it!=policies.end();++it) { bool found=false; for (const auto& value:rows) found|=value.toObject().value("id")==it.key(); if (!found) rows.append(QJsonObject{{"id",it.key()},{"name","未引用策略 · "+it.key().mid(10,8)},{"type","orphan"},{"status","不在候选，可清除策略"}}); } }
        for (int i = 0; i < rows.size(); ++i) { auto row = rows[i].toObject(); const auto key = id + '/' + row.value("id").toString(); const auto error = runningVersion ? QString() : providerErrors_.value(key, providerErrors_.value(id + "/kind:" + row.value("kind").toString(), providerErrors_.value(id + "/*"))); if (!error.isEmpty()) row["error"] = error; const auto index=profileIndex(id); const auto policy=index>=0?profiles_[index].providerPolicies.value(row.value("id").toString()).toObject():QJsonObject{}; const auto attempt=providerAttempts_.value(key); row["policy"]=policy; row["maintenance"]=attempt; row["next_update"]=providerNextDue(policy,row.value("updated_utc").toString(),attempt).toString(Qt::ISODate); rows[i] = row; }
        result.data["providers"] = rows; result.data["version"]=version; result.data["scope"]=runningVersion?"active":"candidate"; done(result);
    });
}

void AppController::inspectProfileChanges(const QString& id, Completion done) {
    const int index=profileIndex(id); if (index<0||state_.exiting||profileChangesPending_) { done(Result::fail("订阅不可用或已有版本比较在进行，请稍后重试")); return; }
    profileChangesPending_=true; const auto profile=profiles_[index]; const auto store=store_; const auto generation=state_.generation;
    runTask(this,[store,profile] { return VersionChanges::inspect(*store,profile); },[this,id,profile,generation,done=std::move(done)](Result result) mutable { profileChangesPending_=false; const auto index=profileIndex(id); if (state_.exiting||generation!=state_.generation||index<0||profiles_[index].active!=profile.active||profiles_[index].candidate!=profile.candidate) done(Result::fail("版本已变化，请重新比较")); else done(result); });
}

void AppController::refreshProviders(const QString& id, const QString& providerId,bool background) {
    const int index = profileIndex(id); if (index < 0 || providerId.isEmpty()) { emit operationFinished(false, "请选择订阅和远程提供者"); return; }
    if (!begin("正在刷新提供者候选；当前运行快照保持不变",background)) return;
    if (providerAttempts_.size()>=1024) providerAttempts_.erase(providerAttempts_.begin()); auto attempt=providerAttempts_.value(id+'/'+providerId); attempt["last_attempt"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate); providerAttempts_[id+'/'+providerId]=attempt;
    const auto profile = profiles_[index]; const auto store = store_; const auto settings = settings_; const int port = chooseApiPort(); const auto secret = createSecret();
    runTask(this, [store, profile, settings, port, secret, providerId] { return store->prepareExisting(profile, profile.candidate, settings, port, secret, true, {providerId}); }, [this, id, providerId](PreparedProfile prepared) {
        QStringList requested; for (const auto& value:prepared.result.data.value("providers").toArray()) { const auto key=value.toObject().value("path").toString(); requested.append(key); if (providerAttempts_.size()>=1024&&!providerAttempts_.contains(id+'/'+key)) providerAttempts_.erase(providerAttempts_.begin()); auto attempt=providerAttempts_.value(id+'/'+key); attempt["last_attempt"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate); providerAttempts_[id+'/'+key]=attempt; }
        checkPrepared(prepared, [this, id, providerId,requested](PreparedProfile checked) {
            if (!checked.result.ok) {
                if (providerErrors_.size() >= 256) providerErrors_.clear();
                providerErrors_[id + '/' + providerId] = diagnostics_.sanitize(checked.result.error).left(512); if (requested.isEmpty()) completeProviderRefresh(id,providerId,checked.result); else for (const auto& key:requested) completeProviderRefresh(id,key,checked.result); finish(checked.result); return;
            }
            auto next = profiles_; const int index = profileIndex(id); if (index < 0) { finish(Result::fail("订阅已变化")); return; } next[index] = checked.profile;
            saveSnapshot(settings_, next, [this, id, providerId,requested](Result result) {
                if (requested.isEmpty()) completeProviderRefresh(id,providerId,result); else for (const auto& key:requested) completeProviderRefresh(id,key,result);
                if (result.ok) { for (const auto& key:requested) providerErrors_.remove(id+'/'+key); if (providerId == "*") { const auto keys = providerErrors_.keys(); for (const auto& key : keys) if (key.startsWith(id + '/')) providerErrors_.remove(key); } else providerErrors_.remove(id + '/' + providerId); }
                finish(result);
            });
        });
    });
}

void AppController::completeProviderRefresh(const QString& id,const QString& providerId,const Result& result) {
    auto attempt=providerAttempts_.value(id+'/'+providerId); const auto now=QDateTime::currentDateTimeUtc();
    if (result.ok) { attempt["failures"]=0; attempt.remove("retry_at"); attempt.remove("error"); }
    else { const int failures=qMin(5,attempt.value("failures").toInt()+1); attempt["failures"]=failures; attempt["retry_at"]=now.addSecs(qMin(3600,300*(1<<(failures-1)))).toString(Qt::ISODate); attempt["error"]=diagnostics_.sanitize(result.error).left(512); }
    providerAttempts_[id+'/'+providerId]=attempt;

}

void AppController::updateProviderPolicy(const QString& id,const QString& providerId,const QJsonObject& policy,const QJsonValue& expected) {
    const int index=profileIndex(id); if (index<0) { emit operationFinished(false,"订阅不存在"); return; } const auto previous=profiles_[index].providerPolicies.value(providerId).toObject();
    if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=previous)) { emit operationFinished(false,"提供者策略已变化，请重新打开"); return; }
    auto policies=profiles_[index].providerPolicies; if (policy.isEmpty()) policies.remove(providerId); else policies[providerId]=policy; auto checked=validateProviderPolicies({{providerId,policy}}); if (checked.ok) checked=validateProviderPolicies(policies); if (!checked.ok) { emit operationFinished(false,checked.error); return; }
    if (!beginPreferences("正在保存提供者下载与更新策略")) return; const auto profile=profiles_[index]; const auto store=store_; const auto settings=settings_;
    runTask(this,[store,profile,settings,providerId,policy] { if (policy.isEmpty()) return Result{}; const auto inspected=store->inspectProviders(profile,profile.candidate,settings); if (!inspected.ok) return inspected; for (const auto& value:inspected.data.value("providers").toArray()) { const auto row=value.toObject(); if (row.value("id")==providerId&&row.value("type")=="http") return Result{}; } return Result::fail("候选中没有此远程提供者；旧策略可明确清除"); },[this,id,policies,providerId](Result result) { if (!result.ok) { finish(result); return; } auto next=profiles_; const int index=profileIndex(id); if (index<0) { finish(Result::fail("订阅不存在")); return; } next[index].providerPolicies=policies; saveSnapshot(settings_,next,[this,id,providerId](Result saved) { if (saved.ok) providerAttempts_.remove(id+'/'+providerId); finish(saved); }); });
}

void AppController::configureProviderMaintenance() {
    bool automatic=false; for (const auto& profile:profiles_) for (auto it=profile.providerPolicies.begin();it!=profile.providerPolicies.end();++it) automatic|=it.value().toObject().value("automatic").toBool();
    if (!smoke_&&!readOnly_&&!startupSuppressed_&&!state_.exiting&&automatic) { if (!providerTimer_.isActive()) { providerTimer_.start(); QTimer::singleShot(0,this,&AppController::runProviderMaintenance); } } else providerTimer_.stop();
}

void AppController::runProviderMaintenance() {
    if (smoke_||readOnly_||startupSuppressed_||state_.busy||state_.exiting||exitPending_||providerSchedulePending_) return;
    providerSchedulePending_=true; const auto profiles=profiles_; const auto settings=settings_; const auto store=store_; const auto attempts=providerAttempts_; const auto cursor=providerScheduleCursor_; const auto generation=state_.generation;
    runTask(this,[store,profiles,settings,attempts,cursor] { return findDueProvider(*store,profiles,settings,attempts,cursor); },[this,generation](Result result) { providerSchedulePending_=false; if (!result.ok||result.data.isEmpty()||generation!=state_.generation||state_.busy||state_.exiting||exitPending_||!providerTimer_.isActive()) return; const auto id=result.data.value("profile").toString(),provider=result.data.value("provider").toString(); const int index=profileIndex(id); if (index<0||profiles_[index].candidate!=result.data.value("candidate").toString()||profiles_[index].providerPolicies.value(provider).toObject()!=result.data.value("policy").toObject()) return; providerScheduleCursor_=result.data.value("cursor").toInt(); refreshProviders(id,provider,true); });
}

void AppController::rememberPrivateSource(const Profile& profile) {
    QStringList urls; if (profile.sourceType=="url") urls.append(profile.source); for (auto it=profile.ruleProviderEdits.begin();it!=profile.ruleProviderEdits.end();++it) { const auto url=it.value().toObject().value("url").toString(); if (!url.isEmpty()) urls.append(url); }
    const auto groups=profile.nodePreferences.value("groups").toObject(); for (auto it=groups.begin();it!=groups.end();++it) { const auto url=it.value().toObject().value("test_url").toString(); if (!url.isEmpty()) urls.append(url); }
    QStringList values; for (const auto& source:urls) { const QUrl url(source); values.append(source); values.append(QUrl::fromPercentEncoding(source.toUtf8())); for (const auto& pair:QUrlQuery(url).queryItems(QUrl::FullyDecoded)) if (pair.second.size()>=4) values.append(pair.second); } diagnostics_.setSecrets(values);
}

void AppController::renameProfile(const QString& id, const QString& name) {
    const int i = profileIndex(id); if (i < 0 || name.trimmed().isEmpty()) return;
    if (!begin("正在重命名订阅")) return;
    auto next = profiles_; next[i].name = name.trimmed().left(120); saveSnapshot(settings_, next, [this](Result r) { finish(r); });
}

void AppController::deleteProfile(const QString& id) {
    const int i = profileIndex(id); if (i < 0) return;
    if (!begin("正在删除订阅")) return;
    protectMajorChange([this,id,i] {
    auto next = profiles_; next.removeAt(i);
    auto settings = settings_;
    const bool active = id == settings.activeProfile;
    if (active) { settings.activeProfile.clear(); settings.desiredProxy = false; }
    const auto commit = [this, id, settings, next] {
        saveSnapshot(settings, next, [this, id](Result r) {
            if (!r.ok) { finish(r); return; } const auto store = store_;
            runTask(this, [store, id] { return store->removeProfile(id); }, [this](Result result) { finish(result); });
        });
    };
    if (!active) { commit(); return; }
    const auto proxy = proxy_;
    runTask(this, [proxy] { return proxy->release(); }, [this, commit](Result r) {
        applyProxyObservation(r); if (!r.ok && !r.data.value("conflict").toBool()) { finish(r); return; }
        core_.stop([this, commit](Result r) { if (!r.ok) { finish(r); return; } proxies_ = {}; emit proxiesChanged(); commit(); });
    });
    });
}

}
