#include "profile/YamlFields.h"
#include "profile/ProxyImpact.h"
#include "profile/VersionChanges.h"
#include "profile/RuleDraft.h"
#include "profile/ProviderEdits.h"
#include "profile/ProviderPolicy.h"
#include "AppController.h"
#include "profile/ProfileStore.h"
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
void AppController::activateProfile(const QString& id, bool lastGood, const QString& expectedVersion) {
    const OperationScope operation(this, OperationId::ProfileActivate);
    const int i = profileIndex(id);
    if (i < 0) { reportOperation(false, "订阅不存在"); return; }
    const auto version = lastGood ? profiles_[i].lastGood : profiles_[i].candidate;
    if (!expectedVersion.isEmpty() && (version != expectedVersion || version == profiles_[i].active)) {
        reportOperation(false, "目标版本已变化或已经使用，请重新确认");
        return;
    }
    if (version.isEmpty()) { reportOperation(false, lastGood ? "没有可回滚版本" : "没有可使用版本"); return; }
    if (!begin(lastGood ? "正在回滚订阅" : "正在激活订阅")) return;
    proxyRestoreAllowed_ = true;
    prepareActivation(profiles_[i], version, settings_);
}
void AppController::updateCustomRules(const QString& id, const QStringList& rules,const QJsonValue& expected) {
    const OperationScope operation(this, OperationId::RulesSave);
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
    const auto token=++dnsQueryGeneration_, generation=state_.generation;
    dnsQuery_=api_.dnsQuery(normalized,type,[this,token,generation,done=std::move(done)](Result result) mutable {
        if (token==dnsQueryGeneration_) dnsQuery_=nullptr;
        if (!result.ok) result.error=diagnostics_.sanitize(result.error);
        done(token==dnsQueryGeneration_&&generation==state_.generation&&state_.ready?result:Result::fail("DNS查询已取消"));
    });
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
    api_.rules( [this, generation, token](Result r) {
        if (!rulesVisible_ || token != rulesGeneration_ || generation != state_.generation || !state_.ready) return;
        if (r.ok) { const auto values = r.data.value("rules").toArray(); QJsonArray next; for (int i = 0; i < qMin(5000, int(values.size())); ++i) next.append(values[i]); activeRules_ = next; rulesError_.clear(); rulesLoaded_ = true; }
        else { rulesError_ = r.error.isEmpty() ? QString("读取内核规则失败") : r.error.left(256); }
        // 读取失败已由页面空态承担（R5）；不再另发一次全局告警，同一事实只留一个承载位置。
        emit rulesChanged();
    });
}
void AppController::setRulesVisible(bool visible) {
    if (rulesVisible_ == visible) return;
    rulesVisible_ = visible; ++rulesGeneration_;
    // 只在真正需要重读时清空：读取失败后保留旧表并显示失败原因，不清成空白（状态之间可共存）。
    if (!visible) { activeRules_ = {}; rulesLoaded_ = false; rulesError_.clear(); }
    emit rulesChanged();
}
void AppController::cancelConnectionRead() {
    ++connectionsGeneration_; connectionsPending_=false; const auto pending=connectionRead_; connectionRead_=nullptr;
    if (pending) pending->abort();
}
void AppController::refreshConnections() {
    if (!connectionsVisible_ || connectionsPaused_ || connectionsPending_ || !state_.ready || state_.busy || state_.exiting) return;
    connectionsPending_ = true; const auto token = connectionsGeneration_; const auto generation = state_.generation;
    connectionRead_=api_.connections( [this, token, generation](Result r) {
        if (token != connectionsGeneration_) return;
        connectionsPending_ = false; connectionRead_=nullptr;
        if (!connectionsVisible_ || connectionsPaused_ || generation != state_.generation || !state_.ready || state_.exiting) return;
        if (r.ok) acceptConnectionSnapshot(r.data);
        else {
            // 状态之间可共存：刷新失败时保留上一份快照并标注过期，
            // 而不是清空列表让用户以为「没有连接」（状态呈现规格「通用约束」）。
            connectionError_=r.error;
            connectionStale_=true;
        }
        emit connectionsChanged();
    }, 1500);
}
void AppController::acceptConnectionSnapshot(const QJsonObject& snapshot) {
    if (connectionsPaused_) return;
    const auto result=connectionSession_.update(snapshot,connectionClock_.isValid()?connectionClock_.elapsed():0);
    connectionError_=result.ok ? (connectionSession_.totals().value("truncated").toBool()?"仅显示前 5000 条连接，不推断遗漏条目的结束":"") : result.error;
        if (result.ok) connectionStale_=false;
    if (!result.ok) connectionSession_.invalidate(); connections_=connectionSession_.active();
}
}
