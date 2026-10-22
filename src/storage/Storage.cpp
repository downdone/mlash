#include "Storage.h"
#include "app/ConnectionColumns.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QUuid>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QSet>

namespace mlash {
Result normalizeProxyBypass(const QString& text) {
    if (text.size()>2048||text.contains(QChar::Null)) return Result::fail("代理排除列表最多2048字符"); QString input=text; input.replace('\r','\n'); input.replace('\n',';'); QStringList entries; QSet<QString> seen; static const QRegularExpression token("^[A-Za-z0-9.*_:\\[\\]-]{1,253}$");
    for (auto entry:input.split(';',Qt::SkipEmptyParts)) { entry=entry.trimmed(); if (entry.isEmpty()) continue; if (entry!="<local>"&&!token.match(entry).hasMatch()) return Result::fail("排除项只能为主机/IP模式、*或<local>；不填写URL、路径或凭据"); if (!seen.contains(entry.toLower())) { seen.insert(entry.toLower()); entries.append(entry); } }
    if (entries.size()>64) return Result::fail("代理排除列表最多64项"); return {true,{},{{"bypass",entries.join(';')}}};
}
bool validAppearance(const QString& appearance,int scale) { return QStringList{"system","light","dark"}.contains(appearance)&&QList<int>{90,100,110,120}.contains(scale); }
Result validateConnectionView(const QJsonObject& object) {
    for (auto it=object.begin();it!=object.end();++it) if (!QStringList{"case_sensitive","whole_word","regex","columns"}.contains(it.key())) return Result::fail("连接视图包含未知字段");
    for (const auto* key:{"case_sensitive","whole_word","regex"}) if (object.contains(key)&&!object.value(key).isBool()) return Result::fail("连接搜索选项类型无效");
    if (object.contains("columns")) {
        const auto count=object.value("columns").toArray().size();
        if (!object.value("columns").isArray()||(count!=ConnectionColumns::LegacyCount&&count!=ConnectionColumns::Count)) return Result::fail("连接列配置须包含完整的七列旧版或十五列"); QSet<int> ids;
        for (const auto& value:object.value("columns").toArray()) { const auto row=value.toObject(); const auto id=row.value("id").toInt(-1),width=row.value("width").toInt(-1); if (!value.isObject()||row.size()!=3||!row.value("id").isDouble()||row.value("id").toDouble()!=id||id<0||id>=count||ids.contains(id)||!row.value("width").isDouble()||row.value("width").toDouble()!=width||width<24||width>1000||!row.value("visible").isBool()||(id==0&&!row.value("visible").toBool())) return Result::fail("连接列宽、顺序或可见性无效"); ids.insert(id); }
    }
    return {};
}
bool DownloadPolicy::valid() const {
    if (!QStringList{"direct", "core"}.contains(route) || timeoutSeconds < 5 || timeoutSeconds > 120 || userAgent.size() > 256) return false;
    for (const auto ch : userAgent) if (ch.unicode() < 32 || ch.unicode() > 126) return false;
    return true;
}
QJsonObject DownloadPolicy::toJson() const { return {{"route", route}, {"user_agent", userAgent}, {"timeout_seconds", timeoutSeconds}}; }
DownloadPolicy DownloadPolicy::fromJson(const QJsonObject& object) {
    DownloadPolicy policy; policy.route = object.value("route").toString("direct"); policy.userAgent = object.value("user_agent").toString(); policy.timeoutSeconds = object.value("timeout_seconds").toInt(30); return policy;
}
bool validTestUrl(const QString& text) {
    const QUrl url(text, QUrl::StrictMode);
    return text.size() <= 2048 && url.isValid() && QStringList{"http", "https"}.contains(url.scheme()) && !url.host().isEmpty() && url.userInfo().isEmpty() && !url.hasFragment();
}
Result validateNodePreferences(const QJsonObject& object) {
    const auto bad=[] { return Result::fail("节点偏好格式无效或超过限制"); };
    if (QJsonDocument(object).toJson(QJsonDocument::Compact).size()>256*1024) return bad();
    const auto name=[](const QString& text) { return !text.isEmpty()&&text.size()<=512&&!text.contains(QChar::Null)&&!text.contains('\n')&&!text.contains('\r'); };
    for (auto it=object.begin();it!=object.end();++it) if (!QStringList{"favorites","common_groups","groups","last_group","disconnect_affected"}.contains(it.key())) return bad();
    for (const auto* key:{"favorites","common_groups"}) if (object.contains(key)) {
        const auto value=object.value(key); if (!value.isArray()||value.toArray().size()>512) return bad(); QSet<QString> seen;
        for (const auto& item:value.toArray()) if (!item.isString()||!name(item.toString())||seen.contains(item.toString())) return bad(); else seen.insert(item.toString());
    }
    if (object.contains("last_group")&&(!object.value("last_group").isString()||!name(object.value("last_group").toString()))) return bad();
    if (object.contains("disconnect_affected")&&!object.value("disconnect_affected").isBool()) return bad();
    if (object.contains("groups")) {
        if (!object.value("groups").isObject()||object.value("groups").toObject().size()>256) return bad(); const auto groups=object.value("groups").toObject();
        for (auto it=groups.begin();it!=groups.end();++it) {
            if (!name(it.key())||!it.value().isObject()) return bad(); const auto row=it.value().toObject();
            for (auto field=row.begin();field!=row.end();++field) {
                if (field.key()=="test_url") { if (!field.value().isString()||(!field.value().toString().isEmpty()&&!validTestUrl(field.value().toString()))) return bad(); }
                else if (field.key()=="view"||field.key()=="sort") { const auto n=field.value().toInt(-1); if (!field.value().isDouble()||field.value().toDouble()!=n||n<0||n>(field.key()=="view"?1:2)) return bad(); }
                else return bad();
            }
        }
    }
    return {};
}
Result writeAtomic(const QString& path, const QByteArray& data) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return Result::fail("无法创建数据目录");
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return Result::fail(file.errorString());
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(data) != data.size() || !file.commit()) return Result::fail(file.errorString());
    return {};
}
Result readFile(const QString& path, QByteArray& data, qint64 limit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return Result::fail(file.errorString());
    if (file.size() > limit) return Result::fail("文件超过大小限制");
    data = file.read(limit + 1);
    if (data.size() > limit || file.error() != QFileDevice::NoError) return Result::fail("文件读取失败或超过限制");
    return {};
}
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
bool isId(const QString& id) {
    static const QRegularExpression expression("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$");
    return expression.match(id).hasMatch();
}
QJsonObject Settings::toJson() const {
    return {{"schema_version", 2}, {"auto_start", autoStart}, {"silent_start", silentStart},
        {"start_core_on_launch", startCore}, {"restore_proxy_on_launch", restoreProxy},
        {"desired_proxy_enabled", desiredProxy}, {"lightweight_mode", lightweight}, {"tun_enabled",tunEnabled},{"tun_stack",tunStack},{"tun_strict_route",tunStrictRoute},{"tun_mtu",tunMtu},{"tun_dns_hijack",tunDnsHijack},{"tun_exclude_routes",QJsonArray::fromStringList(tunExcludeRoutes)}, {"mixed_port", mixedPort}, {"log_level", logLevel},
        {"active_profile_id", activeProfile}, {"core_path", corePath}, {"previous_core_path",previousCorePath}, {"test_url", testUrl}, {"startup_page",startupPage},
        {"tray_click",trayClick},{"ipv6_mode",ipv6Mode},{"unified_delay_mode",unifiedDelayMode},{"website_targets",websiteTargets},{"hotkeys",hotkeys},{"connection_view",connectionView},{"appearance",appearance},{"text_scale",textScale},{"home_probe_seconds",homeProbeSeconds},{"home_summary_mask",homeSummaryMask},{"proxy_bypass",proxyBypass},{"remote_backup",remoteBackup},{"local_backup",localBackup}};
}
Settings Settings::fromJson(const QJsonObject& o) {
    Settings s;
    s.autoStart = o.value("auto_start").toBool(); s.silentStart = o.value("silent_start").toBool(true);
    s.startCore = o.value("start_core_on_launch").toBool(true); s.restoreProxy = o.value("restore_proxy_on_launch").toBool(true);
    s.desiredProxy = o.value("desired_proxy_enabled").toBool(); s.mixedPort = o.value("mixed_port").toInt(17890);
    s.lightweight = o.value("lightweight_mode").toBool(true);
    s.tunEnabled=o.value("tun_enabled").toBool(false); s.tunStack=o.value("tun_stack").toString("mixed"); s.tunStrictRoute=o.value("tun_strict_route").toBool(false); s.tunMtu=o.value("tun_mtu").toInt(1500); s.tunDnsHijack=o.value("tun_dns_hijack").toBool(true); for (const auto& route:o.value("tun_exclude_routes").toArray()) s.tunExcludeRoutes.append(route.toString());
    if (s.mixedPort < 1024 || s.mixedPort > 65535) s.mixedPort = 17890;
    s.logLevel = o.value("log_level").toString("warning");
    if (!QStringList{"debug","info","warning","error","silent"}.contains(s.logLevel)) s.logLevel = "warning";
    s.activeProfile = o.value("active_profile_id").toString(); s.corePath = o.value("core_path").toString();
    s.previousCorePath=o.value("previous_core_path").toString();
    s.testUrl = o.value("test_url").toString(s.testUrl);
    s.startupPage=o.value("startup_page").toInt(0); s.trayClick=o.value("tray_click").toString("show");
    s.ipv6Mode=o.value("ipv6_mode").toString("source"); s.unifiedDelayMode=o.value("unified_delay_mode").toString("source");
    s.websiteTargets = o.value("website_targets").toArray();
    s.hotkeys = o.value("hotkeys").toObject();
    s.connectionView=o.value("connection_view").toObject();
    s.appearance=o.value("appearance").toString("system"); s.textScale=o.value("text_scale").toInt(100);
    s.homeProbeSeconds=o.value("home_probe_seconds").toInt(0);
    s.homeSummaryMask=o.value("home_summary_mask").toInt(15);
    s.proxyBypass=o.value("proxy_bypass").toString(s.proxyBypass);
    s.remoteBackup=o.value("remote_backup").toObject();
    s.localBackup=o.value("local_backup").toObject();
    return s;
}
QJsonObject Profile::toJson() const {
    return {{"id",id},{"name",name},{"source_type",sourceType},{"source",source},{"candidate",candidate},
        {"active",active},{"last_good",lastGood},{"updated",updated},{"selections",selections},{"remember_selections",rememberSelections},{"custom_rules",QJsonArray::fromStringList(customRules)},{"append_rules",QJsonArray::fromStringList(appendRules)},{"disabled_rules",QJsonArray::fromStringList(disabledRules)},
        {"auto_update",autoUpdate},{"update_interval_hours",updateIntervalHours},{"source_hash",sourceHash},{"dns_override",dnsOverride},{"dns_override_enabled",dnsOverrideEnabled},{"proxy_edits",proxyEdits},
        {"last_update_attempt",lastUpdateAttempt},{"update_error",updateError},{"subscription_info",subscriptionInfo},{"recent_selections",recentSelections},{"node_preferences",nodePreferences},{"rule_provider_edits",ruleProviderEdits},{"provider_policies",providerPolicies},{"download_policy",download.toJson()}};
}
Profile Profile::fromJson(const QJsonObject& o) {
    Profile p; p.id = o.value("id").toString(); p.name = o.value("name").toString(); p.sourceType = o.value("source_type").toString(); p.source = o.value("source").toString();
    p.candidate = o.value("candidate").toString(); p.active = o.value("active").toString(); p.lastGood = o.value("last_good").toString(); p.updated = o.value("updated").toString();
    p.selections = o.value("selections").toObject(); p.rememberSelections = o.value("remember_selections").toBool(true);
    for (const auto& value : o.value("custom_rules").toArray()) p.customRules.append(value.toString());
    for (const auto& value : o.value("append_rules").toArray()) p.appendRules.append(value.toString());
    for (const auto& value : o.value("disabled_rules").toArray()) p.disabledRules.append(value.toString());
    p.dnsOverride = o.value("dns_override").toString(); p.dnsOverrideEnabled = o.value("dns_override_enabled").toBool();
    p.proxyEdits=o.value("proxy_edits").toObject();
    p.autoUpdate = o.value("auto_update").toBool(); p.updateIntervalHours = o.value("update_interval_hours").toInt(12);
    p.sourceHash = o.value("source_hash").toString(); p.lastUpdateAttempt = o.value("last_update_attempt").toString();
    p.updateError = o.value("update_error").toString(); p.subscriptionInfo = o.value("subscription_info").toObject();
    p.recentSelections = o.value("recent_selections").toArray();
    p.nodePreferences=o.value("node_preferences").toObject(); p.ruleProviderEdits=o.value("rule_provider_edits").toObject(); p.providerPolicies=o.value("provider_policies").toObject();
    p.download = DownloadPolicy::fromJson(o.value("download_policy").toObject());
    return p;
}
QString coreStateText(CoreState s) {
    switch(s) { case CoreState::Stopped: return "已停止"; case CoreState::Starting:return "启动中";
    case CoreState::Running:return "运行中";case CoreState::Stopping:return "停止中";case CoreState::Failed:return "故障"; }
    return "未知";
}
QString proxyStateText(ProxyState s) {
    switch(s) {case ProxyState::Disabled:return "未启用";case ProxyState::Owned:return "Mlash 已接管";
    case ProxyState::External:return "外部代理";case ProxyState::Unknown:return "状态未知";} return "未知";
}
}
