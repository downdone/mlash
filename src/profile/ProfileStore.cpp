#include "ProviderEdits.h"
#include "ProviderPolicy.h"
#include "ProfileStore.h"
#include "core/TunPolicy.h"
#include "ProxyEdits.h"
#include "RemoteBackup.h"
#include "LocalBackup.h"
#include "SubscriptionInfo.h"
#include "app/WebsiteTests.h"
#include "platform/GlobalHotkeys.h"
#include "storage/Storage.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QFileInfo>
#include <QSet>
#include <QRegularExpression>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <QCryptographicHash>

namespace mlash {
namespace {
bool validSettingsObject(const QJsonObject& o) {
    for (const auto* key:{"tun_enabled","tun_strict_route","tun_dns_hijack"}) if (o.contains(key)&&!o.value(key).isBool()) return false;
    if (o.contains("tun_mtu")&&(!o.value("tun_mtu").isDouble()||o.value("tun_mtu").toDouble()!=o.value("tun_mtu").toInt())) return false;
    if (o.contains("tun_exclude_routes")) { if (!o.value("tun_exclude_routes").isArray()) return false; for (const auto& route:o.value("tun_exclude_routes").toArray()) if (!route.isString()) return false; }
    if (!validTunOptions(Settings::fromJson(o))) return false;
    if (o.contains("tun_stack")&&(!o.value("tun_stack").isString()||!QStringList{"mixed","system","gvisor"}.contains(o.value("tun_stack").toString()))) return false;
    if (o.contains("local_backup")&&(!o.value("local_backup").isObject()||!validateLocalBackup(o.value("local_backup").toObject()).ok)) return false;
    if (o.contains("remote_backup")&&(!o.value("remote_backup").isObject()||!validateRemoteBackup(o.value("remote_backup").toObject()).ok)) return false;
    if (o.contains("previous_core_path")&&(!o.value("previous_core_path").isString()||o.value("previous_core_path").toString().size()>4096||o.value("previous_core_path").toString().contains(QChar::Null))) return false;
    if (o.contains("core_history")) { if (!o.value("core_history").isArray()) return false; const auto history=o.value("core_history").toArray(); if (history.size()>8) return false; QSet<QString> digests; for (const auto& value:history) { const auto row=value.toObject(); const auto entry=CoreHistoryEntry::fromJson(row); if (!value.isObject()||row.size()!=6||digests.contains(entry.sha256)||!entry.valid(CoreHistoryEntry::sources())) return false; digests.insert(entry.sha256); } }
    if (o.contains("proxy_bypass")) { const auto value=o.value("proxy_bypass"); if (!value.isString()) return false; const auto checked=normalizeProxyBypass(value.toString()); if (!checked.ok||checked.data.value("bypass").toString()!=value.toString()) return false; }
    if (o.contains("home_summary_mask")&&(!o.value("home_summary_mask").isDouble()||o.value("home_summary_mask").toDouble()!=o.value("home_summary_mask").toInt()||o.value("home_summary_mask").toInt()<0||o.value("home_summary_mask").toInt()>15)) return false;
    if (o.contains("home_probe_seconds")&&(!o.value("home_probe_seconds").isDouble()||o.value("home_probe_seconds").toDouble()!=o.value("home_probe_seconds").toInt()||!QList<int>{0,60,300}.contains(o.value("home_probe_seconds").toInt()))) return false;
    if ((o.contains("appearance")&&!o.value("appearance").isString())||(o.contains("text_scale")&&(!o.value("text_scale").isDouble()||o.value("text_scale").toDouble()!=o.value("text_scale").toInt()))||!validAppearance(o.value("appearance").toString("system"),o.value("text_scale").toInt(100))) return false;
    if (o.contains("connection_view")&&(!o.value("connection_view").isObject()||!validateConnectionView(o.value("connection_view").toObject()).ok)) return false;
    QList<HotkeyBinding> hotkeys; if (o.contains("hotkeys")&&(!o.value("hotkeys").isObject()||!parseHotkeys(o.value("hotkeys").toObject(),hotkeys).ok)) return false;
    if (o.contains("website_targets") && (!o.value("website_targets").isArray() || !WebsiteTests::validateTargets(o.value("website_targets").toArray()).ok)) return false;
    for (const auto* key : {"auto_start", "silent_start", "start_core_on_launch", "restore_proxy_on_launch", "desired_proxy_enabled", "lightweight_mode"}) if (o.contains(key) && !o.value(key).isBool()) return false;
    for (const auto* key : {"log_level", "active_profile_id", "core_path", "test_url", "tray_click", "ipv6_mode", "unified_delay_mode"}) if (o.contains(key) && !o.value(key).isString()) return false;
    if (o.contains("startup_page") && (!o.value("startup_page").isDouble() || o.value("startup_page").toDouble()!=o.value("startup_page").toInt() || o.value("startup_page").toInt()<0 || o.value("startup_page").toInt()>7)) return false;
    if (o.contains("tray_click") && !QStringList{"show","proxy-page","toggle-proxy"}.contains(o.value("tray_click").toString())) return false;
    for (const auto* key:{"ipv6_mode","unified_delay_mode"}) if (o.contains(key) && !QStringList{"source","enabled","disabled"}.contains(o.value(key).toString())) return false;
    if (o.contains("schema_version") && (!o.value("schema_version").isDouble() || o.value("schema_version").toDouble() != 2)) return false;
    if (o.contains("mixed_port")) { const auto port = o.value("mixed_port").toDouble(-1); if (!o.value("mixed_port").isDouble() || port < 1024 || port > 65535 || port != int(port)) return false; }
    return !o.contains("log_level") || QStringList{"debug","info","warning","error","silent"}.contains(o.value("log_level").toString());
}
bool validMaintenance(const Profile& p) {
    if (p.dnsOverride.toUtf8().size()>64*1024 || p.dnsOverride.contains(QChar::Null)) return false;
    if (p.customRules.size() + p.appendRules.size() > 256 || p.disabledRules.size() > 256) return false;
    for (const auto& rules : {p.appendRules, p.disabledRules}) for (const auto& rule : rules) if (rule.isEmpty() || rule.size() > 1024 || rule.contains('\n') || rule.contains('\r')) return false;
    if (!p.download.valid()) return false;
    if (p.recentSelections.size() > 32) return false;
    for (const auto& value : p.recentSelections) {
        const auto pair = value.toObject();
        if (!value.isObject() || pair.size() != 2 || !pair.value("group").isString() || !pair.value("member").isString() || pair.value("group").toString().isEmpty() || pair.value("member").toString().isEmpty() || pair.value("group").toString().size() > 512 || pair.value("member").toString().size() > 512) return false;
    }
    static const QRegularExpression hash("^[a-f0-9]{64}$");
    return p.updateIntervalHours >= 1 && p.updateIntervalHours <= 168 && p.updateError.size() <= 1024 && !p.updateError.contains('\n') && !p.updateError.contains('\r') &&
        (p.lastUpdateAttempt.isEmpty() || QDateTime::fromString(p.lastUpdateAttempt, Qt::ISODate).isValid()) &&
        (p.sourceHash.isEmpty() || hash.match(p.sourceHash).hasMatch()) && SubscriptionInfo::valid(p.subscriptionInfo);
}
Result copyGeoSnapshot(const QString& sourceWork, const QString& targetWork, bool localDefault = false) {
    QByteArray previous, current;
    if (!localDefault && !readFile(sourceWork + "/config.yaml", previous).ok) return {};
    auto result = readFile(targetWork + "/config.yaml", current); if (!result.ok) return result;
    const auto b = YAML::Load(current.toStdString());
    if (localDefault) { if (b["geox-url"]) return {}; }
    else {
    const auto a = YAML::Load(previous.toStdString());
    // A different Geo source or format must fetch its own bytes.
    for (const auto* field : {"geox-url", "geodata-mode"})
        if (YAML::Dump(a[field]) != YAML::Dump(b[field])) return {};
    }
    for (const auto& entry : QDir(sourceWork).entryInfoList(QDir::Files | QDir::NoSymLinks)) {
        const auto name = entry.fileName().toLower();
        if (!QStringList{"country.mmdb", "geoip.db", "geoip.metadb", "geoip.dat", "geosite.dat", "asn.mmdb"}.contains(name)) continue;
        if (QFileInfo::exists(targetWork + '/' + entry.fileName())) continue;
        QByteArray bytes;
        result = readFile(entry.absoluteFilePath(), bytes, 128 * 1024 * 1024); if (!result.ok) return result;
        if (bytes.isEmpty()) continue;
        result = writeAtomic(targetWork + '/' + entry.fileName(), bytes); if (!result.ok) return result;
    }
    return {};
}
}
ProfileStore::ProfileStore(QString root, std::shared_ptr<PlatformServices> platform) : root_(std::move(root)), platform_(std::move(platform)) {}
QString ProfileStore::versionRoot(const QString& id, const QString& version) const {
    if (!isId(id) || !isId(version)) return {};
    return root_ + "/profiles/" + id + "/versions/" + version;
}
StoreSnapshot ProfileStore::load() {
    StoreSnapshot result;
    auto r = platform_->secureDirectory(root_); if (!r.ok) { result.result = r; return result; }
    if (!QFile::exists(root_ + "/state.dat")) return result;
    QByteArray encrypted, plain;
    r = readFile(root_ + "/state.dat", encrypted);
    if (r.ok) r = platform_->open(encrypted, plain);
    QJsonParseError error; auto doc = QJsonDocument::fromJson(plain, &error);
    if (!r.ok || error.error != QJsonParseError::NoError || !doc.isObject() || !doc.object().value("schema").isDouble() || doc.object().value("schema").toDouble() != 1 ||
        !doc.object().value("settings").isObject() || !doc.object().value("profiles").isArray() || !validSettingsObject(doc.object().value("settings").toObject())) {
        result.result = Result::fail("状态文件损坏或无法解密；保留 state.dat 和 state.last-good.dat，禁止覆盖"); return result;
    }
    const auto object = doc.object(); result.settings = Settings::fromJson(object.value("settings").toObject());
    const auto rawSettings = object.value("settings").toObject();
    if ((!rawSettings.value("test_url").isUndefined() && !rawSettings.value("test_url").isString()) || !validTestUrl(result.settings.testUrl)) { result.result = Result::fail("连通测试地址损坏，未加载"); return result; }
    if (object.value("profiles").toArray().size() > 256) { result.result = Result::fail("订阅数量超过 256，未加载"); return result; }
    QSet<QString> ids;
    for (const auto& value : object.value("profiles").toArray()) {
        const auto p = Profile::fromJson(value.toObject());
        const auto fields = value.toObject();
        if ((fields.contains("provider_policies")&&!fields.value("provider_policies").isObject())||!validateProviderPolicies(p.providerPolicies).ok) { result.result=Result::fail("提供者维护策略索引损坏，未加载"); return result; }
        if ((fields.contains("rule_provider_edits")&&!fields.value("rule_provider_edits").isObject())||!validateRuleProviderEdits(p.ruleProviderEdits).ok) { result.result=Result::fail("规则集声明索引损坏，未加载"); return result; }
        if ((fields.contains("node_preferences")&&!fields.value("node_preferences").isObject())||!validateNodePreferences(p.nodePreferences).ok) { result.result=Result::fail("节点偏好索引损坏，未加载"); return result; }
        if ((fields.contains("proxy_edits")&&!fields.value("proxy_edits").isObject())||!validateProxyEdits(p.proxyEdits).ok) { result.result=Result::fail("节点覆盖索引损坏，未加载"); return result; }
        if ((fields.contains("dns_override") && !fields.value("dns_override").isString()) || (fields.contains("dns_override_enabled") && !fields.value("dns_override_enabled").isBool())) { result.result=Result::fail("DNS覆盖索引损坏，未加载"); return result; }
        if (fields.contains("download_policy")) {
            const auto policy = fields.value("download_policy"); const auto object = policy.toObject();
            if (!policy.isObject() || (object.contains("route") && !object.value("route").isString()) || (object.contains("user_agent") && !object.value("user_agent").isString()) ||
                (object.contains("timeout_seconds") && (!object.value("timeout_seconds").isDouble() || object.value("timeout_seconds").toDouble() != p.download.timeoutSeconds))) {
                result.result = Result::fail("订阅下载设置损坏，未加载"); return result;
            }
        }
        if (!value.isObject()) { result.result = Result::fail("订阅索引条目不是对象，未加载"); return result; }
        for (const auto* key : {"id", "name", "source_type", "source", "candidate", "active", "last_good", "updated"}) if (fields.contains(key) && !fields.value(key).isString()) { result.result = Result::fail("订阅索引字段类型损坏"); return result; }
        if ((!fields.value("selections").isUndefined() && !fields.value("selections").isObject()) || (!fields.value("remember_selections").isUndefined() && !fields.value("remember_selections").isBool())) { result.result = Result::fail("策略组记忆索引损坏"); return result; }
        for (auto selection = p.selections.begin(); selection != p.selections.end(); ++selection) if (!selection.value().isString()) { result.result = Result::fail("已选成员索引损坏"); return result; }
        if (!fields.value("recent_selections").isUndefined() && !fields.value("recent_selections").isArray()) { result.result = Result::fail("最近成员索引损坏，未加载"); return result; }
        for (const auto* field : {"source_hash", "last_update_attempt", "update_error"}) if (!fields.value(field).isUndefined() && !fields.value(field).isString()) { result.result = Result::fail("订阅维护索引字段类型损坏"); return result; }
        if (!validMaintenance(p) || (!fields.value("auto_update").isUndefined() && !fields.value("auto_update").isBool()) ||
            (!fields.value("subscription_info").isUndefined() && !fields.value("subscription_info").isObject()) ||
            (!fields.value("update_interval_hours").isUndefined() && (!fields.value("update_interval_hours").isDouble() || fields.value("update_interval_hours").toDouble() != p.updateIntervalHours))) {
            result.result = Result::fail("订阅更新策略或用量索引损坏，未加载"); return result;
        }
        const auto rulesValue = value.toObject().value("custom_rules");
        for (const auto* key : {"append_rules", "disabled_rules"}) if (fields.contains(key)) { const auto array = fields.value(key).toArray(); if (!fields.value(key).isArray() || std::any_of(array.begin(), array.end(), [](const QJsonValue& value) { return !value.isString(); })) { result.result = Result::fail("规则覆盖索引损坏，未加载"); return result; } }
        if ((!rulesValue.isUndefined() && !rulesValue.isArray()) || std::any_of(p.customRules.begin(), p.customRules.end(), [](const QString& rule) { return rule.isEmpty() || rule.size() > 1024 || rule.contains('\n') || rule.contains('\r'); })) { result.result = Result::fail("自定义规则索引损坏，未加载"); return result; }
        if (!isId(p.id) || ids.contains(p.id) || !isId(p.candidate) || (!p.active.isEmpty() && !isId(p.active)) ||
            (!p.lastGood.isEmpty() && !isId(p.lastGood)) || p.selections.size() > 256 || p.customRules.size() > 256 || !QStringList{"url","file"}.contains(p.sourceType)) {
            result.result = Result::fail("订阅索引损坏，未使用非法路径"); return result;
        }
        ids.insert(p.id); result.profiles.append(p);
    }
    if (!result.settings.activeProfile.isEmpty() && !ids.contains(result.settings.activeProfile)) {
        result.result = Result::fail("活动订阅索引不一致"); return result;
    }
    return result;
}
Result ProfileStore::save(const Settings& settings, const QVector<Profile>& profiles) {
    for (const auto& profile:profiles) { const auto checked=validateProxyEdits(profile.proxyEdits); if (!checked.ok) return checked; const auto preferences=validateNodePreferences(profile.nodePreferences); if (!preferences.ok) return preferences; const auto policy=validateProviderPolicies(profile.providerPolicies); if (!policy.ok) return policy; const auto providers=validateRuleProviderEdits(profile.ruleProviderEdits); if (!providers.ok) return providers; }
    if (!validSettingsObject(settings.toJson())) return Result::fail("设置选项格式不合法");
    if (settings.mixedPort < 1024 || settings.mixedPort > 65535 || !QStringList{"debug","info","warning","error","silent"}.contains(settings.logLevel)) return Result::fail("设置越界，未保存");
    if (!validTestUrl(settings.testUrl)) return Result::fail("连通测试地址不合法");
    if (profiles.size() > 256) return Result::fail("订阅数量超过 256");
    for (const auto& profile : profiles) if (!validMaintenance(profile)) return Result::fail("订阅更新策略或用量格式不合法");
    for (const auto& profile : profiles) if (profile.customRules.size() > 256 || std::any_of(profile.customRules.begin(), profile.customRules.end(), [](const QString& rule) { return rule.isEmpty() || rule.size() > 1024 || rule.contains('\n') || rule.contains('\r'); })) return Result::fail("自定义规则超过限制或格式损坏");
    for (const auto& profile : profiles) if (profile.selections.size() > 256) return Result::fail("已选择成员，但记忆策略组数量超过 256，未保存新索引");
    QJsonArray array; for (const auto& p : profiles) array.append(p.toJson());
    QJsonObject object{{"schema", 1}, {"settings", settings.toJson()}, {"profiles", array}};
    QByteArray encrypted;
    const auto plain = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (plain.size() > 8*1024*1024) return Result::fail("索引超过 8 MiB，保留原状态");
    auto r = platform_->seal(plain, encrypted); if (!r.ok) return r;
    if (encrypted.size() > 8*1024*1024) return Result::fail("加密索引超过 8 MiB，保留原状态");
    const auto path = root_ + "/state.dat";
    QVector<Profile> previous;
    if (QFile::exists(path)) {
        QByteArray old; r = readFile(path, old); if (!r.ok) return r;
        QByteArray plain;
        if (platform_->open(old, plain).ok) {
            const auto oldObject = QJsonDocument::fromJson(plain).object();
            for (const auto& value : oldObject.value("profiles").toArray()) previous.append(Profile::fromJson(value.toObject()));
        }
        r = writeAtomic(root_ + "/state.last-good.dat", old); if (!r.ok) return r;
    }
    r = writeAtomic(path, encrypted); if (!r.ok) return r;
    for (const auto& profile : profiles) {
        QSet<QString> retained{profile.candidate, profile.active, profile.lastGood};
        for (const auto& old : previous) if (old.id == profile.id) { retained.insert(old.candidate); retained.insert(old.active); retained.insert(old.lastGood); }
        const auto versionsPath = root_ + "/profiles/" + profile.id + "/versions";
        const auto parent = QFileInfo(versionsPath).canonicalFilePath();
        if (!isId(profile.id) || parent.isEmpty() || QFileInfo(versionsPath).isSymLink()) continue;
        for (const auto& version : QDir(versionsPath).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            const auto target = version.canonicalFilePath();
            if (isId(version.fileName()) && !retained.contains(version.fileName()) && target.startsWith(parent + '/', Qt::CaseInsensitive)) QDir(target).removeRecursively();
        }
    }
    return {};
}
PreparedProfile ProfileStore::prepare(const Profile& profile, const QByteArray& source, const Settings& settings, int apiPort, const QString& secret, bool useLocalGeo) {
    PreparedProfile prepared; prepared.profile = profile; prepared.version = newId(); prepared.apiPort = apiPort; prepared.secret = secret;
    if (!isId(profile.id)) { prepared.result = Result::fail("非法 Profile ID"); return prepared; }
    auto built = ConfigBuilder::buildProfile(source, settings, apiPort, secret, profile);
    prepared.result = built.result; prepared.secrets = built.secrets; prepared.remoteUrls = built.remoteUrls;
    prepared.expectedRuleCount = built.result.data.value("rule_count").toInt(-1);
    if (!prepared.result.ok) return prepared;
    prepared.profile.rememberSelections = built.result.data.value("remember_selections").toBool(true);
    if (!prepared.profile.rememberSelections) { prepared.profile.selections = {}; prepared.profile.recentSelections = {}; }
    const auto directory = versionRoot(profile.id, prepared.version);
    prepared.workPath = directory + "/work"; prepared.configPath = prepared.workPath + "/config.yaml";
    if (!QDir().mkpath(prepared.workPath + "/providers")) { prepared.result = Result::fail("无法创建版本工作目录"); return prepared; }
    prepared.result = writeAtomic(directory + "/source.yaml", source);
    if (prepared.result.ok) prepared.result = writeAtomic(prepared.configPath, built.yaml);
    if (prepared.result.ok) prepared.result=writeAtomic(directory+"/overrides.json",QJsonDocument(QJsonObject{{"schema",1},{"overrides",overrideDocument(profile)},{"source_sha256",QString::fromLatin1(QCryptographicHash::hash(source,QCryptographicHash::Sha256).toHex())}}).toJson(QJsonDocument::Compact));
    if (prepared.result.ok && useLocalGeo) prepared.result = copyGeoSnapshot(root_ + "/geodata", prepared.workPath, true);
    if (prepared.result.ok && isId(profile.candidate))
        prepared.result = copyGeoSnapshot(versionRoot(profile.id, profile.candidate) + "/work", prepared.workPath);
    if (prepared.result.ok) prepared.result.data = built.result.data;
    prepared.profile.candidate = prepared.version;
    prepared.profile.updated = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return prepared;
}
PreparedProfile ProfileStore::prepareExisting(const Profile& profile, const QString& version, const Settings& settings, int apiPort, const QString& secret, bool useLocalGeo, const QStringList& refreshProviders,bool allowEmptyRefresh) {
    const auto directory = versionRoot(profile.id, version); QByteArray source;
    if (directory.isEmpty()) { PreparedProfile p; p.result = Result::fail("版本 ID 不合法"); return p; }
    const auto r = readFile(directory + "/source.yaml", source);
    if (!r.ok) { PreparedProfile p; p.result = r; return p; }
    auto sourceProfile=profile; sourceProfile.candidate=version;
    auto prepared = prepare(sourceProfile, source, settings, apiPort, secret,useLocalGeo);
    if (!prepared.result.ok) return prepared;
    const auto geoCopy = copyGeoSnapshot(directory + "/work", prepared.workPath);
    if (!geoCopy.ok) { prepared.result = geoCopy; return prepared; }
    QByteArray manifest; QJsonObject records; const auto manifestPath = directory + "/work/provider-snapshots.json";
    if (QFileInfo::exists(manifestPath)) {
        auto read = readFile(manifestPath, manifest, 1024 * 1024); const auto parsed = QJsonDocument::fromJson(manifest);
        if (!read.ok || !parsed.isObject()) { prepared.result = Result::fail("提供者快照清单不可读或损坏，未使用该版本"); return prepared; }
        records = parsed.object();
    }
    Profile frozen=profile; const auto snapshot=versionProfile(profile.id,version,frozen); if (!snapshot.ok) { prepared.result=snapshot; return prepared; }
    QJsonArray pending; QJsonObject selectedRecords; const auto downloads=prepared.result.data.value("providers").toArray();
    for (const auto& value:downloads) {
        const auto provider=value.toObject(); const auto relative=provider.value("path").toString();
        if (refreshProviders.contains("*")||refreshProviders.contains(relative)||refreshProviders.contains("kind:"+provider.value("kind").toString())) { pending.append(provider); continue; }
        const auto signature=providerSourceSignature(provider); const auto name=provider.value("name").toString(); const bool requireSignature=provider.value("kind")=="rule-providers"&&(profile.ruleProviderEdits.contains(name)||frozen.ruleProviderEdits.contains(name));
        auto record=records.value(relative).toObject(); QByteArray bytes; const auto originalRead=readFile(directory+"/work/"+relative,bytes);
        bool matching=originalRead.ok&&(record.contains("source_sha256")?record.value("source_sha256").toString()==signature:!requireSignature);
        if (!matching) {
            for (const auto& alternative:{profile.active,profile.lastGood}) {
                if (alternative.isEmpty()||alternative==version) continue; const auto root=versionRoot(profile.id,alternative); if (root.isEmpty()) continue; QByteArray metadata; if (!readFile(root+"/work/provider-snapshots.json",metadata,1024*1024).ok) continue;
                const auto parsed=QJsonDocument::fromJson(metadata); const auto other=parsed.object().value(relative).toObject(); if (!parsed.isObject()||other.value("source_sha256").toString()!=signature||other.value("sha256").toString().isEmpty()) continue;
                QByteArray cached; if (!readFile(root+"/work/"+relative,cached).ok) continue; record=other; bytes=std::move(cached); matching=true; break;
            }
        }
        if (!matching) { prepared.result=Result::fail("提供者没有与当前声明匹配的冻结快照；请先刷新候选，原版本保持不变"); return prepared; }
        const auto recordedHash=record.value("sha256").toString(); if (!recordedHash.isEmpty()&&recordedHash!=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())) { prepared.result=Result::fail("提供者快照哈希不一致，未使用该版本；请刷新提供者候选"); return prepared; }
        auto checked=ConfigBuilder::checkProvider(bytes,provider.value("kind").toString(),provider.value("format").toString(),provider.value("behavior").toString()); if (!checked.result.ok) { prepared.result=checked.result; return prepared; }
        prepared.secrets.append(checked.secrets); const auto copied=writeAtomic(prepared.workPath+"/"+relative,bytes); if (!copied.ok) { prepared.result=copied; return prepared; } if (!record.isEmpty()) selectedRecords[relative]=record;
    }
    if (!selectedRecords.isEmpty()) { const auto copied=writeAtomic(prepared.workPath+"/provider-snapshots.json",QJsonDocument(selectedRecords).toJson(QJsonDocument::Compact)); if (!copied.ok) { prepared.result=copied; return prepared; } }
    prepared.result.data["providers"] = pending;
    if (!allowEmptyRefresh && !refreshProviders.isEmpty() && pending.isEmpty()) { prepared.result = Result::fail("没有可刷新的远程提供者，运行版本保持不变"); return prepared; }
    prepared.profile.updated = profile.updated;
    // Rebuilding active/LKG must not discard an independently downloaded candidate.
    if (profile.candidate != profile.active && version != profile.candidate) prepared.profile.candidate = profile.candidate;
    return prepared;
}
Result ProfileStore::recordProviderSnapshot(const QString& workPath, const QJsonObject& provider, const QByteArray& bytes) {
    QByteArray previous; QJsonObject manifest;
    const auto path = workPath + "/provider-snapshots.json";
    if (QFileInfo::exists(path)) { const auto read = readFile(path, previous, 1024 * 1024); if (!read.ok) return read; const auto parsed = QJsonDocument::fromJson(previous); if (!parsed.isObject()) return Result::fail("提供者快照清单损坏"); manifest = parsed.object(); }
    const auto format = provider.value("format").toString(), kind = provider.value("kind").toString();
    int count = 0;
    if (format == "mrs") { const auto checked=ConfigBuilder::checkProvider(bytes,kind,format,provider.value("behavior").toString()); if (!checked.result.ok) return checked.result; count=checked.result.data.value("count").toInt(); }
    else if (format == "yaml") { const auto root = YAML::Load(bytes.toStdString()); const auto entries = root[kind == "proxy-providers" ? "proxies" : "payload"]; count = int(entries.size()); }
    else for (const auto& line : bytes.split('\n')) if (!line.trimmed().isEmpty() && !line.trimmed().startsWith('#')) ++count;
    manifest[provider.value("path").toString()] = QJsonObject{{"updated_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}, {"bytes", double(bytes.size())}, {"count", count},{"source_sha256",providerSourceSignature(provider)}};
    return writeAtomic(path, QJsonDocument(manifest).toJson(QJsonDocument::Compact));
}
QJsonObject ProfileStore::overrideDocument(const Profile& profile) {
    const auto all=profile.toJson(); QJsonObject result; for (const auto* key:{"custom_rules","append_rules","disabled_rules","dns_override","dns_override_enabled","proxy_edits","rule_provider_edits"}) result[key]=all.value(key); return result;
}
Result ProfileStore::applyOverrides(Profile& profile,const QJsonObject& object) {
    const auto bad=[] { return Result::fail("版本覆盖快照格式损坏"); }; if (object.size()!=7||QJsonDocument(object).toJson(QJsonDocument::Compact).size()>1024*1024) return bad();
    for (const auto* key:{"custom_rules","append_rules","disabled_rules"}) { if (!object.value(key).isArray()||object.value(key).toArray().size()>256) return bad(); for (const auto& value:object.value(key).toArray()) if (!value.isString()||value.toString().isEmpty()||value.toString().size()>1024||value.toString().contains('\n')||value.toString().contains('\r')) return bad(); }
    if (object.value("custom_rules").toArray().size()+object.value("append_rules").toArray().size()>256||!object.value("dns_override").isString()||object.value("dns_override").toString().toUtf8().size()>64*1024||!object.value("dns_override_enabled").isBool()||!object.value("proxy_edits").isObject()||!object.value("rule_provider_edits").isObject()) return bad();
    const auto decoded=Profile::fromJson(object); const auto nodes=validateProxyEdits(decoded.proxyEdits); if (!nodes.ok) return nodes; const auto providers=validateRuleProviderEdits(decoded.ruleProviderEdits); if (!providers.ok) return providers;
    profile.customRules=decoded.customRules; profile.appendRules=decoded.appendRules; profile.disabledRules=decoded.disabledRules; profile.dnsOverride=decoded.dnsOverride; profile.dnsOverrideEnabled=decoded.dnsOverrideEnabled; profile.proxyEdits=decoded.proxyEdits; profile.ruleProviderEdits=decoded.ruleProviderEdits; return {};
}
Result ProfileStore::versionProfile(const QString& id,const QString& version,Profile& profile) const {
    const auto root=versionRoot(id,version); if (root.isEmpty()) return Result::fail("覆盖快照版本不可用"); const auto path=root+"/overrides.json"; if (!QFileInfo::exists(path)) return {}; if (QFileInfo(path).isSymLink()) return Result::fail("覆盖快照不能为链接");
    QByteArray bytes; auto read=readFile(path,bytes,1024*1024+1024); if (!read.ok) return read; const auto parsed=QJsonDocument::fromJson(bytes); const auto value=parsed.object(); if (!parsed.isObject()||value.size()!=3||value.value("schema").toInt()!=1||!value.value("overrides").isObject()) return Result::fail("版本覆盖快照格式损坏");
    QByteArray source; read=readFile(root+"/source.yaml",source); if (!read.ok) return read; if (value.value("source_sha256").toString()!=QString::fromLatin1(QCryptographicHash::hash(source,QCryptographicHash::Sha256).toHex())) return Result::fail("版本来源与覆盖快照校验不一致"); return applyOverrides(profile,value.value("overrides").toObject());
}
Result ProfileStore::ensureVersionSnapshots(const Profile& profile) const {
    QSet<QString> seen; for (const auto& version:{profile.active,profile.candidate,profile.lastGood}) { if (version.isEmpty()||seen.contains(version)) continue; seen.insert(version); const auto root=versionRoot(profile.id,version); if (root.isEmpty()) return Result::fail("旧版本覆盖快照不可用"); const auto path=root+"/overrides.json"; if (QFileInfo::exists(path)) { auto verified=profile; const auto read=versionProfile(profile.id,version,verified); if (!read.ok) return read; continue; }
        QByteArray source; const auto read=readFile(root+"/source.yaml",source); if (!read.ok) return read; const auto saved=writeAtomic(path,QJsonDocument(QJsonObject{{"schema",1},{"overrides",overrideDocument(profile)},{"source_sha256",QString::fromLatin1(QCryptographicHash::hash(source,QCryptographicHash::Sha256).toHex())}}).toJson(QJsonDocument::Compact)); if (!saved.ok) return saved;
    } return {};
}
Result ProfileStore::inspectProviders(const Profile& profile, const QString& version, const Settings& settings) {
    const auto root = versionRoot(profile.id, version); if (root.isEmpty()) return Result::fail("提供者版本不可用");
    QByteArray source; const auto read = readFile(root + "/source.yaml", source); if (!read.ok) return read;
    auto frozen=profile; const auto snapshot=versionProfile(profile.id,version,frozen); if (!snapshot.ok) return snapshot;
    const auto built = ConfigBuilder::buildProfile(source, settings, settings.mixedPort == 19090 ? 19091 : 19090, QString(64, 'x'), frozen); if (!built.result.ok) return built.result;
    QJsonObject manifest; QByteArray metadata; if (readFile(root + "/work/provider-snapshots.json", metadata, 1024 * 1024).ok) manifest = QJsonDocument::fromJson(metadata).object();
    QHash<QString,QString> signatures; for (const auto& value:built.result.data.value("providers").toArray()) { const auto provider=value.toObject(); signatures[provider.value("path").toString()]=providerSourceSignature(provider); }
    QJsonArray rows;
    for (const auto& value : built.result.data.value("provider_summary").toArray()) {
        auto row = value.toObject(); row["status"] = "内联";
        if (row.value("type") == "http") {
            const auto path = root + "/work/" + row.value("id").toString(); QByteArray payload; const auto input = readFile(path, payload);
            row["status"] = input.ok ? "已冻结" : "缺少快照";
            if (input.ok) {
                const auto checked = ConfigBuilder::checkProvider(payload, row.value("kind").toString(), row.value("format").toString(), row.value("behavior").toString());
                if (!checked.result.ok) { row["status"] = "快照无效"; row["error"] = checked.result.error; }
                else {
                    auto record = manifest.value(row.value("id").toString()).toObject(); const auto hash = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
                    if ((record.contains("source_sha256")&&record.value("source_sha256").toString()!=signatures.value(row.value("id").toString()))||(!record.contains("source_sha256")&&row.value("kind")=="rule-providers"&&frozen.ruleProviderEdits.contains(row.value("name").toString()))) { row["status"]="声明不匹配"; row["error"]="快照来源/格式与该版本的声明不一致，请刷新候选"; }
                    else if (record.contains("sha256") && record.value("sha256").toString() != hash) { row["status"] = "哈希不一致"; row["error"] = "快照与清单不一致，请刷新候选"; }
                    row["sha256"] = hash; row["updated_utc"] = record.value("updated_utc").toString(QFileInfo(path).lastModified().toUTC().toString(Qt::ISODate));
                    if (row.value("format") == "mrs") row["count"]=checked.result.data.value("count");
                    else if (row.value("format") == "yaml") row["count"] = int(YAML::Load(payload.toStdString())[row.value("kind") == "proxy-providers" ? "proxies" : "payload"].size());
                    else { int count = 0; for (const auto& line : payload.split('\n')) if (!line.trimmed().isEmpty() && !line.trimmed().startsWith('#')) ++count; row["count"] = count; }
                }
            } else row["error"] = "快照文件不可读；刷新将生成新的候选";
        }
        rows.append(row);
    }
    return {true, {}, {{"providers", rows}, {"version", version}}};
}
Result ProfileStore::importGeoData(const QString& sourceDirectory,const QJsonObject& provenance) {
    if (!QFileInfo(sourceDirectory).isDir() || QFileInfo(sourceDirectory).isSymLink()) return Result::fail("Geo 来源目录不可用或为链接");
    const auto stage=root_+"/geo-import-"+newId(), current=root_+"/geodata", previous=root_+"/geodata.previous";
    const auto secure=platform_->secureDirectory(stage); if (!secure.ok) return secure;
    const auto inside=[this](const QString& path) { const auto info=QFileInfo(path); return !info.isSymLink() && info.canonicalFilePath().startsWith(QFileInfo(root_).canonicalFilePath()+'/',Qt::CaseInsensitive); };
    const auto failed=[&](const QString& error) { if (inside(stage)) QDir(stage).removeRecursively(); return Result::fail(error); };
    QJsonArray files;
    for (const auto* name:{"country.mmdb","geoip.db","geoip.metadb","geoip.dat","geosite.dat","asn.mmdb"}) {
        const auto source=sourceDirectory+'/'+name; if (!QFileInfo::exists(source)) continue;
        if (QFileInfo(source).isSymLink() || !QFileInfo(source).isFile()) return failed("Geo 数据不允许链接或目录");
        QByteArray bytes; auto result=readFile(source,bytes,128*1024*1024); if (!result.ok || bytes.isEmpty()) return failed("Geo 文件为空、过大或读取失败");
        result=writeAtomic(stage+'/'+name,bytes); if (!result.ok) return failed("Geo 数据快照写入失败");
        const auto record=provenance.value(name).toObject(); files.append(QJsonObject{{"name",name},{"bytes",double(bytes.size())},{"sha256",QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())},{"origin",record.value("origin").toString("user-provided-local")},{"updated_utc",record.value("updated_utc").toString(QDateTime::currentDateTimeUtc().toString(Qt::ISODate))}});
    }
    if (files.isEmpty()) return failed("目录中没有支持的 Geo 文件");
    auto result=writeAtomic(stage+"/manifest.json",QJsonDocument(QJsonObject{{"schema",1},{"origin","user-provided-local"},{"imported_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"files",files}}).toJson()); if (!result.ok) return failed("Geo 清单写入失败");
    if (QFileInfo::exists(previous) && (!inside(previous) || !QDir(previous).removeRecursively())) return failed("上一份 Geo 快照不可清理，未替换");
    if (QFileInfo::exists(current) && (!inside(current) || !QDir().rename(current,previous))) return failed("Geo 旧快照不可保存，未替换");
    if (!QDir().rename(stage,current)) { if (QFileInfo::exists(previous)) QDir().rename(previous,current); return failed("Geo 新快照提交失败"); }
    return {true,{},{{"file_count",files.size()}}};
}
Result ProfileStore::removeProfile(const QString& id) {
    if (!isId(id)) return Result::fail("Profile ID 不合法");
    const auto path = root_ + "/profiles/" + id;
    if (!QFileInfo::exists(path)) return {};
    const auto parent = QFileInfo(root_).canonicalFilePath(); const auto resolved = QFileInfo(path).canonicalFilePath();
    if (parent.isEmpty() || resolved.isEmpty() || !resolved.startsWith(parent + "/profiles/", Qt::CaseInsensitive) || QFileInfo(root_ + "/profiles").isSymLink()) return Result::fail("订阅目录超出数据边界，未执行删除");
    if (QFileInfo(path).isSymLink()) return Result::fail("订阅目录为符号链接，未执行删除");
    return !QDir(path).exists() || QDir(path).removeRecursively() ? Result{} : Result::fail("订阅目录删除失败");
}
}
