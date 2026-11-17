#include "TunOptions.h"
#include <QHostAddress>
#include <QRegularExpression>
#include <QSet>

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
}
