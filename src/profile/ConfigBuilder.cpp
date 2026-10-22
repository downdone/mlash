#include "ConfigBuilder.h"
#include "RuleDraft.h"
#include "ProviderEdits.h"
#include "MrsSnapshot.h"
#include "ProxyEdits.h"
#include "core/TunPolicy.h"
#include "diagnostics/Diagnostics.h"
#include <yaml-cpp/yaml.h>
#include <QSet>
#include <QUrl>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QStringDecoder>
#include <QRegularExpression>

namespace mlash {
namespace {
struct CompatibilityError { QString code, path, reason, suggestion; };
[[noreturn]] void reject(const QString& code, const QString& path, const QString& reason, const QString& suggestion) {
    throw CompatibilityError{code, path, reason, suggestion};
}
QString fieldKey(const std::string& key) {
    const auto text = QString::fromStdString(key);
    static const QRegularExpression safe("^[a-z][a-z0-9-]{0,47}$");
    return safe.match(text).hasMatch() ? text : QString("<未识别字段>");
}
void checkProxy(const YAML::Node& proxy, const QString& path = "proxies[]") {
    if (!proxy.IsMap()) reject("PROXY_SHAPE", path, "节点必须是映射对象", "使用完整的 mihomo YAML 节点定义");
    const auto type = QString::fromStdString(proxy["type"].as<std::string>(""));
    static const QSet<QString> protocols{"ss","ssr","vmess","vless","trojan","http","socks5","snell","hysteria","hysteria2","tuic","wireguard","ssh","mieru","anytls","direct","reject","dns"};
    if (!protocols.contains(type)) reject("PROXY_PROTOCOL", path + ".type", "节点协议不在已验证范围内", "使用受支持协议，或移除此节点后重新导入");
    for (const auto& field : proxy) {
        const auto key = QString::fromStdString(field.first.as<std::string>());
        if (key.endsWith("-file") || key == "listeners" || key == "mapped-listeners") reject("PROXY_EXTERNAL_FILE", path + '.' + fieldKey(key.toStdString()), "节点引用了外部文件或监听器", "改为不依赖外部文件的节点配置");
        if ((key == "private-key" || key == "certificate" || key == "ca") && field.second.IsScalar()) {
            const auto value = QString::fromStdString(field.second.Scalar());
            if (value.startsWith('/') || value.startsWith("./") || value.startsWith("../") || (value.size() > 2 && value[1] == ':'))
                reject("PROXY_CERTIFICATE_PATH", path + '.' + key, "证书或密钥使用了外部路径", "使用受支持的内联证书/密钥");
        }
    }
}
void visit(const YAML::Node& node, int depth, int& count, QStringList& secrets) {
    if (depth > 48 || ++count > 30000) throw std::runtime_error("YAML depth/alias/size limit exceeded");
    const auto tag = node.Tag();
    if (!tag.empty() && tag != "?" && tag != "!" && !tag.starts_with("tag:yaml.org,2002:")) throw std::runtime_error("Custom YAML tags unsupported");
    if (node.IsMap()) {
        QSet<QString> keys;
        for (const auto& item : node) {
            if (!item.first.IsScalar()) throw std::runtime_error("YAML keys must be strings");
            const auto key = QString::fromStdString(item.first.Scalar());
            if (key == "<<") throw std::runtime_error("YAML merge keys unsupported; expand before import");
            if (keys.contains(key)) throw std::runtime_error("Duplicate YAML key");
            keys.insert(key);
            if (QStringList{"password","passwd","secret","token","uuid","private-key","api-key","authorization","private-key-passphrase","psk","pre-shared-key","auth-str"}.contains(key.toLower()) && item.second.IsScalar())
                secrets.append(QString::fromStdString(item.second.Scalar()));
            if (item.second.IsScalar() && (item.second.Scalar().starts_with("https://") || item.second.Scalar().starts_with("http://"))) secrets.append(QString::fromStdString(item.second.Scalar()));
            visit(item.second, depth + 1, count, secrets);
        }
    } else if (node.IsSequence()) for (const auto& item : node) visit(item, depth + 1, count, secrets);
}
}
BuiltConfig ConfigBuilder::build(const QByteArray& source, const Settings& settings, int apiPort, const QString& secret, const QStringList& customRules, const QStringList& appendRules, const QStringList& disabledRules) {
    Profile profile; profile.customRules=customRules; profile.appendRules=appendRules; profile.disabledRules=disabledRules;
    return buildProfile(source,settings,apiPort,secret,profile);
}
BuiltConfig ConfigBuilder::buildProfile(const QByteArray& source, const Settings& settings, int apiPort, const QString& secret, const Profile& profile) {
    const auto& customRules=profile.customRules; const auto& appendRules=profile.appendRules; const auto& disabledRules=profile.disabledRules;
    BuiltConfig result;
    if (source.isEmpty() || source.size() > 8 * 1024 * 1024 || source.contains('\0')) { result.result = Result::fail("YAML 为空、超过 8 MiB 或含 NUL"); return result; }
    QStringDecoder sourceDecoder(QStringDecoder::Utf8); (void)sourceDecoder(source);
    if (sourceDecoder.hasError()) { result.result = Result::fail("YAML 必须为 UTF-8 文本"); return result; }
    if (settings.mixedPort < 1024 || settings.mixedPort > 65535 || apiPort < 1024 || apiPort > 65535 || apiPort == settings.mixedPort || secret.size() < 32) {
        result.result = Result::fail("受控端口或 Secret 不合法"); return result;
    }
    try {
        auto documents = YAML::LoadAll(std::string(source.constData(), source.size()));
        if (documents.size() != 1 || !documents.front().IsMap()) throw std::runtime_error("Exactly one YAML mapping required");
        auto config = documents.front(); int count = 0; visit(config, 0, count, result.secrets);
        if (profile.dnsOverrideEnabled) {
            if (profile.dnsOverride.isEmpty() || profile.dnsOverride.toUtf8().size()>64*1024 || profile.dnsOverride.contains(QChar::Null)) reject("DNS_OVERRIDE_SIZE","dns_override","DNS覆盖为空或超过64KiB","填写需要覆盖的DNS字段，或关闭覆盖");
            const auto documents=YAML::LoadAll(profile.dnsOverride.toStdString()); if (documents.size()!=1 || !documents[0].IsMap()) reject("DNS_OVERRIDE_SHAPE","dns_override","DNS覆盖必须是一个字段映射","直接填写 nameserver 等字段，不要再包裹 dns 层");
            const auto overlay=documents[0]; visit(overlay,0,count,result.secrets);
            static const QSet<QString> keys{"enable","prefer-h3","ipv6","ipv6-timeout","use-hosts","use-system-hosts","respect-rules","nameserver","fallback","fallback-filter","fallback-lazy-query","enhanced-mode","fake-ip-range","fake-ip-range6","fake-ip-filter","fake-ip-filter-mode","fake-ip-ttl","default-nameserver","cache-algorithm","cache-max-size","nameserver-policy","proxy-server-nameserver","proxy-server-nameserver-policy","direct-nameserver","direct-nameserver-follow-policy"};
            if (config["dns"] && !config["dns"].IsMap()) reject("DNS_SHAPE","dns","来源DNS必须是字段映射","核对来源配置的dns字段");
            if (!config["dns"]) config["dns"]=YAML::Node(YAML::NodeType::Map);
            auto dns=config["dns"]; QJsonArray replaced;
            for (const auto& entry : overlay) {
                const auto key=QString::fromStdString(entry.first.as<std::string>());
                if (!keys.contains(key)) reject("DNS_OVERRIDE_FIELD","dns_override."+fieldKey(key.toStdString()),"DNS覆盖字段未开放","仅覆盖已支持的DNS行为/上游/策略；不增加监听或改变系统DNS");
                const auto name=key.toStdString();
                if ((key=="nameserver-policy" || key=="proxy-server-nameserver-policy") && entry.second.IsMap() && dns[name] && dns[name].IsMap()) { for (const auto& policy : entry.second) dns[name][policy.first.as<std::string>()]=YAML::Clone(policy.second); }
                else dns[name]=YAML::Clone(entry.second);
                replaced.append(key);
            }
            result.result.data["dns_override_fields"]=replaced;
        }
        const auto edited=applyProxyEdits(config,profile.proxyEdits); if (!edited.ok) { result.result=edited; return result; }
        const auto providerEdits=applyRuleProviderEdits(config,profile.ruleProviderEdits); if (!providerEdits.ok) { result.result=providerEdits; return result; }
        if (!profile.proxyEdits.isEmpty()||!profile.ruleProviderEdits.isEmpty()) { count=0; visit(config,0,count,result.secrets); }
        const auto nodes = config["proxies"];
        if (nodes && !nodes.IsSequence()) throw std::runtime_error("Proxies must be a sequence");
        if (nodes) { int index = 0; for (const auto& proxy : nodes) checkProxy(proxy, QString("proxies[%1]").arg(index++)); }
        if (customRules.size() + appendRules.size() > 256 || disabledRules.size() > 256) reject("RULE_LIMIT", "rules", "自定义规则或屏蔽列表超过限制", "前置和追加合计最多256条，屏蔽最多256条");
        if (config["rules"] && !config["rules"].IsSequence()) reject("RULE_SHAPE", "rules", "来源规则必须是列表", "使用每项一条规则的 YAML 列表");
        if (!customRules.isEmpty() || !appendRules.isEmpty() || !disabledRules.isEmpty()) {
            QSet<QString> targets{"DIRECT", "REJECT", "REJECT-DROP", "PASS", "COMPATIBLE", "GLOBAL"};
            for (const auto* key : {"proxies", "proxy-groups"}) if (config[key] && config[key].IsSequence()) for (const auto& entry : config[key]) if (entry["name"]) targets.insert(QString::fromStdString(entry["name"].as<std::string>()));
            QSet<QString> providers; if (config["rule-providers"]&&config["rule-providers"].IsMap()) for (const auto& entry:config["rule-providers"]) providers.insert(QString::fromStdString(entry.first.as<std::string>()));
            YAML::Node merged(YAML::NodeType::Sequence);
            const auto addRules=[&](const QStringList& rules,const QString& location) { const auto checked=RuleDraft::check(rules,targets,providers,true,location); if (!checked.ok) reject(checked.data.value("code").toString(),checked.data.value("path").toString(),checked.data.value("reason").toString(),checked.data.value("suggestion").toString()); for (const auto& rule:rules) merged.push_back(rule.trimmed().toStdString()); };
            addRules(customRules,"custom_rules");
            for (const auto& rule : disabledRules) if (rule.isEmpty() || rule.size() > 1024 || rule.contains('\n') || rule.contains('\r')) reject("DISABLED_RULE", "disabled_rules", "屏蔽规则格式无效", "使用来源列表中的完整规则，最多1024字符");
            if (config["rules"]) for (const auto& rule : config["rules"]) {
                if (!rule.IsScalar()) reject("RULE_SHAPE", "rules", "来源规则必须为字符串", "每项填写一条规则");
                if (!disabledRules.contains(QString::fromStdString(rule.as<std::string>()))) merged.push_back(rule);
            }
            addRules(appendRules,"append_rules");
            config["rules"] = merged;
        }
        const auto preferences = config["profile"];
        result.result.data["remember_selections"] = !preferences || !preferences["store-selected"] || preferences["store-selected"].as<bool>();
        static const QSet<QString> supported{
            "port","socks-port","redir-port","tproxy-port","mixed-port","allow-lan","bind-address","lan-allowed-ips","lan-disallowed-ips",
            "authentication","skip-auth-prefixes","mode","log-level","ipv6","external-controller","external-controller-tls",
            "external-controller-unix","external-controller-pipe","external-controller-cors","external-controller-routing-mark",
            "external-ui","external-ui-url","external-ui-name","external-doh-server","secret","tun","listeners","tunnels",
            "ss-config","vmess-config","tuic-server","iptables","dns","hosts","proxies","proxy-groups","proxy-providers","rule-providers",
            "rules","sub-rules","sniffer","profile","unified-delay","tcp-concurrent","find-process-mode","keep-alive-idle","keep-alive-interval",
            "disable-keep-alive","tcp-half-close-timeout","geodata-mode","geodata-loader","geo-auto-update","geo-update-interval","geox-url","geosite-matcher",
            "global-client-fingerprint","global-ua","etag-support","experimental","routing-mark","interface-name"};
        for (const auto& item : config) {
            if (!supported.contains(QString::fromStdString(item.first.Scalar()))) reject("UNSUPPORTED_FIELD", fieldKey(item.first.Scalar()), "顶层字段尚未经过兼容性验证", "核对字段拼写；移除该字段或使用不包含此能力的配置");
        }
        auto halfClose = config["tcp-half-close-timeout"];
        if (!halfClose) { config["tcp-half-close-timeout"] = YAML::Node(YAML::NodeType::Map); halfClose = config["tcp-half-close-timeout"]; }
        if (!halfClose.IsMap()) reject("HALFCLOSE_SHAPE", "tcp-half-close-timeout", "半关闭回收配置必须为映射", "使用client/server整秒参数；0关闭对应方向");
        static const QRegularExpression seconds("^(0|[1-9][0-9]{0,9})$");
        for (const auto& entry : halfClose) {
            const auto key=entry.first.Scalar();
            if (key!="client"&&key!="server") reject("HALFCLOSE_FIELD", "tcp-half-close-timeout", "半关闭回收字段不受支持", "仅支持client和server");
            if (!entry.second.IsScalar() || !seconds.match(QString::fromStdString(entry.second.Scalar())).hasMatch() || entry.second.as<qulonglong>()>9223372036ULL)
                reject("HALFCLOSE_VALUE", "tcp-half-close-timeout."+QString::fromStdString(key), "半关闭回收秒数无效", "使用0至9223372036的十进制整数；0关闭对应方向");
        }
        if (!halfClose["client"]) halfClose["client"] = 300;
        if (!halfClose["server"]) halfClose["server"] = 30;
        const char* blocked[] = {"listeners","tunnels","ss-config","vmess-config","tuic-server","iptables","external-ui","external-ui-url","external-doh-server"};
        for (const char* key : blocked) {
            const auto node = config[key];
            if (node && !node.IsNull() && ((node.IsScalar() && !node.Scalar().empty()) || (node.IsMap() && node.size()) || (node.IsSequence() && node.size())))
                reject("ADDITIONAL_LISTENER", key, "配置要求额外监听或服务", "删除此字段；Mlash 只管理本机 Mixed 代理及私有控制接口");
            config.remove(key);
        }
        QJsonArray managed;
        for (const auto* key : {"mixed-port", "port", "socks-port", "redir-port", "tproxy-port", "allow-lan", "bind-address", "external-controller", "secret", "tun", "log-level", "external-controller-tls", "external-controller-unix", "external-controller-pipe", "external-controller-cors", "external-controller-routing-mark", "external-ui-name", "authentication", "skip-auth-prefixes", "lan-allowed-ips", "lan-disallowed-ips"}) if (config[key]) managed.append(QString::fromLatin1(key));
        if (settings.ipv6Mode != "source" && config["ipv6"]) managed.append("ipv6");
        if (settings.unifiedDelayMode != "source" && config["unified-delay"]) managed.append("unified-delay");
        if (settings.tunEnabled&&config["geo-auto-update"]) managed.append("geo-auto-update");
        result.result.data["managed_fields"] = managed;
        for (const char* key : {"external-controller-tls","external-controller-unix","external-controller-pipe","external-controller-cors",
            "external-controller-routing-mark","external-ui-name","authentication","skip-auth-prefixes","lan-allowed-ips","lan-disallowed-ips"}) config.remove(key);
        for (const char* key : {"port","socks-port","redir-port","tproxy-port"}) config[key] = 0;
        config["mixed-port"] = settings.mixedPort; config["allow-lan"] = false; config["bind-address"] = "127.0.0.1";
        config["external-controller"] = QString("127.0.0.1:%1").arg(apiPort).toStdString();
        config["secret"] = secret.toStdString(); config["tun"] = YAML::Node(YAML::NodeType::Map); config["tun"]["enable"] = false;
        if (!validTunOptions(settings)) throw std::runtime_error("Invalid TUN stack");
        if (settings.tunEnabled) {
            config["geo-auto-update"]=false;
            auto tun=config["tun"]; tun["enable"]=true; tun["device"]="Mlash"; tun["stack"]=settings.tunStack.toStdString();
            tun["auto-route"]=true; tun["auto-detect-interface"]=true; tun["strict-route"]=settings.tunStrictRoute;
            tun["mtu"]=settings.tunMtu; tun["dns-hijack"]=YAML::Node(YAML::NodeType::Sequence); if (settings.tunDnsHijack) tun["dns-hijack"].push_back("any:53");
            if (!settings.tunExcludeRoutes.isEmpty()) { tun["route-exclude-address"]=YAML::Node(YAML::NodeType::Sequence); for (const auto& route:settings.tunExcludeRoutes) tun["route-exclude-address"].push_back(route.toStdString()); }
            if (!config["dns"]) config["dns"]=YAML::Node(YAML::NodeType::Map);
            config["dns"]["enable"]=true;
            if (!config["dns"]["enhanced-mode"]) config["dns"]["enhanced-mode"]="fake-ip";
            if (!config["dns"]["nameserver"]) { config["dns"]["nameserver"].push_back("https://1.1.1.1/dns-query"); config["dns"]["nameserver"].push_back("https://8.8.8.8/dns-query"); }
        }
        config["log-level"] = settings.logLevel.toStdString();
        if (settings.ipv6Mode!="source") config["ipv6"]=settings.ipv6Mode=="enabled";
        if (settings.tunEnabled&&config["ipv6"]&&config["ipv6"].as<bool>(false)) config["tun"]["inet6-address"].push_back("fdfe:dcba:9876::1/126");
        if (settings.unifiedDelayMode!="source") config["unified-delay"]=settings.unifiedDelayMode=="enabled";
        auto dns = config["dns"];
        if (dns && dns["listen"] && !dns["listen"].as<std::string>().empty()) reject("DNS_LISTENER", "dns.listen", "独立 DNS 监听未开放", "移除 listen；DNS 上游和策略仍可沿用订阅配置");
        QJsonArray downloads, summaries;
        for (const char* kind : {"proxy-providers","rule-providers"}) {
            auto providers = config[kind];
            if (providers && !providers.IsMap()) reject("PROVIDER_SHAPE", kind, "提供者必须是映射对象", "按提供者名称到对象的格式定义");
            if (!providers) continue;
            int providerIndex = 0;
            for (const auto& item : providers) {
                if (summaries.size() >= 256) reject("PROVIDER_LIMIT", "providers", "提供者总数超过 256", "拆分订阅后重新导入");
                const auto path = QString("%1[%2]").arg(kind).arg(providerIndex++);
                auto provider = item.second;
                if (!provider.IsMap()) reject("PROVIDER_SHAPE", path, "提供者必须是映射对象", "核对该提供者的 YAML 格式");
                const auto type = provider["type"].as<std::string>("");
                QJsonObject summary{{"kind", kind}, {"name", QString::fromStdString(item.first.as<std::string>())}, {"type", QString::fromStdString(type)}};
                if (QString::fromLatin1(kind)=="rule-providers") summary["behavior"]=QString::fromStdString(provider["behavior"].as<std::string>(""));
                if (type == "file") reject("PROVIDER_LOCAL_FILE", path + ".type", "不读取配置指定的外部提供者文件", "改用内联 payload 或 HTTPS 提供者，以便建立可回滚快照");
                if (type == "http") {
                    const QUrl url(QString::fromStdString(provider["url"].as<std::string>("")));
                    if (!url.isValid() || url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment()) reject("PROVIDER_URL", path + ".url", "提供者需要无用户凭据/片段的 HTTPS 地址", "使用有效 HTTPS 下载地址；认证参数不在报告中展示");
                    result.remoteUrls.append(url.toString());
                    summary["id"]=providerSnapshotId(QString::fromLatin1(kind),summary.value("name").toString()); summary["source_host"]=url.host();
                    provider["path"]=summary.value("id").toString().toStdString();
                    if (provider["proxy"] && !provider["proxy"].as<std::string>("").empty()) reject("PROVIDER_PROXY", path + ".proxy", "提供者指定代理下载尚未开放", "移除proxy字段，在提供者“下载与更新”选择直连或经本地代理；不支持指定任意单节点");
                    if (provider["header"] && !provider["header"].IsNull()) reject("PROVIDER_HEADER",path+".header","提供者自定义请求头尚未开放","移除此字段；UA可在提供者下载策略设置，其余认证可使用供应商签名HTTPS地址");
                    const auto format = QString::fromStdString(provider["format"].as<std::string>("yaml"));
                    if (!QStringList{"yaml","text","mrs"}.contains(format)) reject("PROVIDER_FORMAT", path + ".format", "提供者格式不受支持", "规则使用yaml/text/mrs，节点提供者使用yaml");
                    if (format=="mrs" && !QStringList{"domain","ipcidr"}.contains(summary.value("behavior").toString())) reject("MRS_BEHAVIOR",path+".behavior","MRS只支持domain/ipcidr行为","classical规则使用yaml或text");
                    if (QString::fromLatin1(kind) == "proxy-providers" && format != "yaml") reject("PROXY_PROVIDER_FORMAT", path + ".format", "节点提供者需要 YAML 格式", "使用包含 proxies 列表的 YAML 来源");
                    summary["format"] = format;
                    downloads.append(QJsonObject{{"kind", kind}, {"name", summary.value("name")}, {"url", url.toString(QUrl::FullyEncoded)}, {"format", format}, {"behavior",summary.value("behavior")}, {"path", summary.value("id")}});
                    provider["type"] = "file"; provider.remove("url"); provider.remove("interval"); provider.remove("proxy");
                } else if (type == "inline") {
                    summary["id"] = QString::fromLatin1(kind) + ":inline:" + QString::number(providerIndex - 1);
                    summary["count"] = int(provider["payload"] ? provider["payload"].size() : 0);
                    if (QString::fromLatin1(kind) == "proxy-providers") {
                        auto payload = provider["payload"];
                        if (!payload || !payload.IsSequence()) throw std::runtime_error("Inline proxy provider requires payload sequence");
                        for (const auto& proxy : payload) checkProxy(proxy);
                    }
                } else throw std::runtime_error("Unsupported provider type");
                summaries.append(summary);
            }
        }
        YAML::Emitter emitter; emitter << config;
        if (!emitter.good()) throw std::runtime_error("YAML serialization failed");
        result.yaml = QByteArray(emitter.c_str(), static_cast<int>(emitter.size())); result.secrets.append(secret);
        result.result.data["providers"] = downloads;
        result.result.data["provider_summary"] = summaries;
        result.result.data["rule_count"] = config["rules"] && config["rules"].IsSequence() ? int(config["rules"].size()) : -1;
        result.secrets.removeDuplicates();
    } catch (const CompatibilityError& error) {
        result.result = Result::fail(error.path + "：" + error.reason + "。" + error.suggestion);
        result.result.data = {{"code", error.code}, {"path", error.path}, {"reason", error.reason}, {"suggestion", error.suggestion}}; result.yaml.clear();
    } catch (const YAML::Exception& error) {
        const int line = error.mark.line >= 0 ? error.mark.line + 1 : 0;
        const int column = error.mark.column >= 0 ? error.mark.column + 1 : 0;
        result.result = Result::fail(QString("YAML 语法或字段类型错误%1；请核对缩进和类型，原始内容未写入日志").arg(line ? QString("（第 %1 行，第 %2 列）").arg(line).arg(column) : QString()));
        result.result.data = {{"code", "YAML_SYNTAX"}, {"line", line}, {"column", column}}; result.yaml.clear();
    } catch (const std::exception& error) {
        // Parser diagnostics can contain an entire secret-bearing input line.
        result.result = Result::fail(QString("配置不兼容或格式错误：%1").arg(QString::fromUtf8(error.what()).section('\n', 0, 0).left(180)));
        result.yaml.clear();
    }
    return result;
}
BuiltConfig ConfigBuilder::checkProvider(const QByteArray& payload, const QString& kind, const QString& format, const QString& behavior) {
    BuiltConfig result;
    if (format=="mrs") { result.result=kind=="rule-providers"?inspectMrs(payload,behavior):Result::fail("节点提供者不能使用MRS"); if (result.result.ok) result.yaml=payload; return result; }
    if (payload.isEmpty() || payload.size() > 8 * 1024 * 1024 || payload.contains('\0')) { result.result = Result::fail("Provider 大小/编码不受支持"); return result; }
    QStringDecoder decoder(QStringDecoder::Utf8); decoder(payload);
    if (decoder.hasError()) { result.result = Result::fail("Provider 必须为 UTF-8 文本"); return result; }
    try {
        if (kind == "proxy-providers" || format == "yaml") {
            auto docs = YAML::LoadAll(payload.toStdString()); if (docs.size() != 1 || !docs[0].IsMap()) throw std::runtime_error("Provider requires one mapping");
            int count = 0; visit(docs[0], 0, count, result.secrets);
            const auto entries = docs[0][kind == "proxy-providers" ? "proxies" : "payload"];
            if (!entries || !entries.IsSequence()) throw std::runtime_error("Provider payload sequence missing");
            for (const auto& entry : entries) {
                if (kind == "proxy-providers") checkProxy(entry);
                else if (!entry.IsScalar()) throw std::runtime_error("Rule provider entries must be strings");
            }
        }
        result.yaml = payload;
    } catch (const YAML::Exception&) { result.result = Result::fail("Provider YAML 语法或字段错误"); }
      catch (const CompatibilityError& error) { result.result = Result::fail(error.path + "：" + error.reason + "。" + error.suggestion); result.result.data = {{"code", error.code}, {"path", error.path}}; }
      catch (const std::exception& error) { result.result = Result::fail(QString::fromUtf8(error.what())); }
    return result;
}
}
