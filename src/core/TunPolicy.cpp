#include "TunPolicy.h"
#include "profile/ConfigBuilder.h"
#include <yaml-cpp/yaml.h>
#include <QRegularExpression>
#include <QSet>
#include <QHostAddress>
#include <functional>

namespace mlash {
QJsonObject tunOptionsJson(const Settings& s) { return {{"enabled",s.tunEnabled},{"stack",s.tunStack},{"strict_route",s.tunStrictRoute},{"mtu",s.tunMtu},{"dns_hijack",s.tunDnsHijack},{"exclude_routes",QJsonArray::fromStringList(s.tunExcludeRoutes)}}; }
Result normalizeTunRoutes(const QString& text) {
    if (text.size()>4096) return Result::fail("TUN 排除列表最多4096字符、64个网段");
    QStringList routes; QSet<QString> seen;
    for (const auto& part:text.split(QRegularExpression("[\\s,;]+"),Qt::SkipEmptyParts)) {
        if (part.count('/')!=1 || !QRegularExpression("^[0-9a-fA-F:.]+/[0-9]{1,3}$").match(part).hasMatch()) return Result::fail("请使用 IPv4/IPv6 CIDR 网段，例如 192.168.0.0/16");
        const auto network=QHostAddress::parseSubnet(part); const int maximum=network.first.protocol()==QAbstractSocket::IPv4Protocol?32:128;
        if (network.first.isNull()||network.second<1||network.second>maximum) return Result::fail("TUN 排除网段无效，不能排除整个默认路由 /0");
        const auto canonical=network.first.toString()+'/'+QString::number(network.second); if (!seen.contains(canonical)) { seen.insert(canonical); routes.append(canonical); } if (routes.size()>64) return Result::fail("TUN 排除列表最多64个网段");
    }
    return {true,{},{{"routes",QJsonArray::fromStringList(routes)}}};
}
bool validTunOptions(const Settings& s) {
    const auto checked=normalizeTunRoutes(s.tunExcludeRoutes.join('\n'));
    return QStringList{"mixed","system","gvisor"}.contains(s.tunStack)&&s.tunMtu>=1280&&s.tunMtu<=9000&&checked.ok&&checked.data.value("routes").toArray()==QJsonArray::fromStringList(s.tunExcludeRoutes);
}
bool validTunFile(const QString& path) {
    static const QRegularExpression provider("^providers/[a-f0-9]{24}\\.cache$");
    return QStringList{"config.yaml","country.mmdb","geoip.db","geoip.metadb","geoip.dat","geosite.dat","ASN.mmdb"}.contains(path)||provider.match(path).hasMatch();
}
Result checkTunProvider(const QByteArray& config,const QString& relative,const QByteArray& bytes) {
    if (!relative.startsWith("providers/")||!validTunFile(relative)) return Result::fail("TUN Provider 文件名无效");
    try {
        const auto node=YAML::Load(config.toStdString()); bool found=false;
        for (const auto* kind:{"proxy-providers","rule-providers"}) if (node[kind]) for (const auto& entry:node[kind]) {
            const auto p=entry.second; if (p["type"].as<std::string>("")!="file"||QString::fromStdString(p["path"].as<std::string>(""))!=relative) continue; found=true;
            const auto checked=ConfigBuilder::checkProvider(bytes,kind,QString::fromStdString(p["format"].as<std::string>("yaml")),QString::fromStdString(p["behavior"].as<std::string>("")));
            if (!checked.result.ok) return Result::fail("TUN Provider 未通过受控快照校验");
        }
        return found?Result{}:Result::fail("TUN Provider 不属于配置中的冻结快照");
    } catch (...) { return Result::fail("TUN Provider 配置无效"); }
}
Result prepareTunRuntime(const QByteArray& input, QByteArray& output, QStringList& files) {
    output.clear(); files.clear();
    if (input.isEmpty()||input.size()>8*1024*1024||input.contains('\0')) return Result::fail("TUN 配置大小或格式无效");
    try {
        // Build once first so malformed aliases/tags/duplicates hit the usual budgets.
        // Frozen file providers need a type adapter before the second normalisation.
        auto docs=YAML::LoadAll(input.toStdString());
        if (docs.size()!=1||!docs[0].IsMap()) return Result::fail("TUN 配置必须是单个映射");
        auto node=docs[0];
        // Bound aliases before traversing provider collections (including cyclic ones).
        int count=0; QSet<QString> keys;
        std::function<bool(const YAML::Node&,int)> bounded=[&](const YAML::Node& n,int depth) {
            if (++count>30000||depth>48) return false;
            if (n.IsMap()) { QSet<QString> seen; for (auto v:n) { if (!v.first.IsScalar()) return false; const auto key=QString::fromStdString(v.first.Scalar()); if (seen.contains(key)) return false; seen.insert(key); if (!bounded(v.second,depth+1)) return false; } }
            else if (n.IsSequence()) for (auto v:n) if (!bounded(v,depth+1)) return false;
            return true;
        };
        if (!bounded(node,0)) return Result::fail("TUN 配置超过结构预算或含重复字段");
        const auto tun=node["tun"];
        if (!tun.IsMap()||!tun["enable"].as<bool>(false)) return Result::fail("服务只接受显式启用的 TUN 配置");
        Settings settings; settings.tunEnabled=true; settings.tunStack=QString::fromStdString(tun["stack"].as<std::string>("mixed")); settings.tunStrictRoute=tun["strict-route"].as<bool>(false);
        settings.tunMtu=tun["mtu"].as<int>(1500);
        const auto routes=tun["route-exclude-address"]; if (routes) { if (!routes.IsSequence()||routes.size()>64) return Result::fail("TUN 排除列表结构无效"); for (const auto& route:routes) settings.tunExcludeRoutes.append(QString::fromStdString(route.as<std::string>())); }
        const auto dns=tun["dns-hijack"]; if (dns) { if (!dns.IsSequence()||dns.size()>1||(dns.size()==1&&dns[0].as<std::string>()!="any:53")) return Result::fail("TUN DNS 劫持仅支持 any:53 或关闭"); settings.tunDnsHijack=dns.size()==1; }
        if (!validTunOptions(settings)) return Result::fail("TUN 协议栈无效");
        settings.mixedPort=node["mixed-port"].as<int>(0); settings.logLevel=QString::fromStdString(node["log-level"].as<std::string>("warning"));
        const auto endpoint=QString::fromStdString(node["external-controller"].as<std::string>(""));
        static const QRegularExpression loopback("^127\\.0\\.0\\.1:([0-9]{4,5})$"); const auto match=loopback.match(endpoint);
        if (!match.hasMatch()) return Result::fail("TUN 控制接口必须仅监听本机");
        const auto secret=QString::fromStdString(node["secret"].as<std::string>(""));
        if (secret.size()!=64||!QRegularExpression("^[a-f0-9]{64}$").match(secret).hasMatch()) return Result::fail("TUN 控制凭据格式无效");
        YAML::Node saved(YAML::NodeType::Map);
        for (auto kind:{"proxy-providers","rule-providers"}) if (node[kind]) {
            if (!node[kind].IsMap()||node[kind].size()>256) return Result::fail("TUN Provider 结构无效");
            saved[kind]=YAML::Clone(node[kind]);
            for (auto entry:node[kind]) {
                auto p=entry.second; const auto type=p["type"].as<std::string>("");
                if (type=="file") {
                    const auto path=QString::fromStdString(p["path"].as<std::string>(""));
                    if (!path.startsWith("providers/")||!validTunFile(path)||p["url"]) return Result::fail("TUN Provider 路径不属于冻结快照");
                    files.append(path); p["type"]="inline"; p.remove("path"); p["payload"]=YAML::Node(YAML::NodeType::Sequence);
                } else if (type!="inline") return Result::fail("TUN Provider 必须先建立离线快照");
            }
        }
        const auto built=ConfigBuilder::build(QByteArray::fromStdString(YAML::Dump(node)),settings,match.captured(1).toInt(),secret);
        if (!built.result.ok) return Result::fail("TUN 快照不满足受控配置契约");
        auto safe=YAML::Load(built.yaml.toStdString());
        // Retain only whitelisted provider fields, never arbitrary file URLs/paths.
        for (auto kind:{"proxy-providers","rule-providers"}) if (saved[kind]) for (auto entry:saved[kind]) {
            auto p=entry.second; const auto name=entry.first.as<std::string>();
            if (p["type"].as<std::string>("")!="file") continue;
            auto target=safe[kind][name]; target["type"]="file"; target["path"]=p["path"].as<std::string>(); target.remove("payload");
        }
        safe["geo-auto-update"]=false;
        output=QByteArray::fromStdString(YAML::Dump(safe)); files.removeDuplicates(); return {};
    } catch (...) { return Result::fail("TUN 配置解析失败，原文未记录"); }
}
}
