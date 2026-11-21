#include "WireGuardConfig.h"
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QUrl>

namespace mlash {
namespace {
constexpr int maxPeers = 32;
constexpr int maxPeerPrefixes = 64;
constexpr int maxPrefixes = 256;

QString unComment(QString line) {
    const auto comment = line.indexOf('#');
    if (comment >= 0) line.truncate(comment);
    return line.trimmed();
}

Result invalid(qsizetype line, const QString& reason) {
    return Result::fail(QString("WireGuard 第 %1 行：%2").arg(line).arg(reason));
}

bool readKey(const QString& input, QByteArray& decoded) {
    if (input.size() != 44) return false;
    const auto encoded = input.toLatin1();
    auto key = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!key || key.decoded.size() != 32 || key.decoded == QByteArray(32, '\0') || key.decoded.toBase64() != encoded)
        return false;
    decoded = std::move(key.decoded);
    return true;
}

bool integer(const QString& text, quint16 min, quint16 max, quint16& value) {
    if (text.isEmpty() || text.size() > 5) return false;
    for (const auto c : text) if (c < '0' || c > '9') return false;
    bool ok = false;
    auto number = text.toUShort(&ok);
    if (!ok || number < min || number > max) return false;
    value = number;
    return true;
}

bool ip(const QString& text, QHostAddress& address) {
    if (!address.setAddress(text) || !address.scopeId().isEmpty()) return false;
    if (address.protocol() == QAbstractSocket::IPv4Protocol)
        return address.toString() == text;
    if (address.protocol() != QAbstractSocket::IPv6Protocol) return false;
    const auto bytes = address.toIPv6Address();
    const bool mapped = bytes[10] == 255 && bytes[11] == 255 &&
        std::all_of(bytes.c, bytes.c + 10, [](quint8 value) { return value == 0; });
    return !mapped;
}

template<class T, class Parse>
bool appendList(const QString& value, QVector<T>& entries, int limit, Parse parse) {
    for (auto part : value.split(',')) {
        if (entries.size() >= limit) return false;
        T entry;
        if (!parse(part.trimmed(), entry)) return false;
        entries.push_back(std::move(entry));
    }
    return true;
}

bool readPrefix(const QString& value, WireGuardPrefix& prefix, bool mask) {
    const auto slash = value.indexOf('/');
    if (slash < 0 || !ip(value.left(slash), prefix.address)) return false;
    const quint16 bits = prefix.address.protocol() == QAbstractSocket::IPv4Protocol ? 32 : 128;
    quint16 length = 0;
    if (!integer(value.mid(slash+1), 0, bits, length)) return false;
    prefix.length = length;
    if (mask) prefix.address = QHostAddress::parseSubnet(prefix.text()).first;
    return !prefix.address.isNull();
}

bool readEndpoint(const QString& value, WireGuardPeer& peer) {
    const auto split = value.lastIndexOf(':');
    if (split < 1 || !integer(value.mid(split+1), 1, 65535, peer.endpointPort)) return false;
    const auto host = value.left(split);
    QHostAddress address;
    if (host.startsWith('[') && host.endsWith(']')) {
        if (!ip(host.mid(1, host.size()-2), address) || address.protocol() != QAbstractSocket::IPv6Protocol)
            return false;
        peer.endpointHost = address.toString();
        return true;
    }
    if (ip(host, address) && address.protocol() == QAbstractSocket::IPv4Protocol) {
        peer.endpointHost = address.toString();
        return true;
    }
    if (host.contains(':')) return false;
    const QByteArray ace = QUrl::toAce(host);
    if (ace.isEmpty() || ace.size() > 253) return false;
    static const QRegularExpression label("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$");
    const auto normalized = QString::fromLatin1(ace);
    const auto unqualified = normalized.endsWith('.') ? normalized.chopped(1) : normalized;
    for (const auto& part : unqualified.split('.'))
        if (!label.match(part).hasMatch()) return false;
    peer.endpointHost = normalized;
    return true;
}

bool overlaps(const WireGuardPrefix& a, const WireGuardPrefix& b) {
    if (a.address.protocol() != b.address.protocol()) return false;
    return a.length <= b.length ? b.address.isInSubnet(a.address, a.length)
                                : a.address.isInSubnet(b.address, b.length);
}

class PrefixCoverage {
public:
    PrefixCoverage() { nodes_.resize(1); }

    void insert(const WireGuardPrefix& prefix) {
        const auto bytes = prefix.address.toIPv6Address();
        const auto v4 = prefix.address.protocol() == QAbstractSocket::IPv4Protocol;
        const auto ipv4 = v4 ? prefix.address.toIPv4Address() : 0;
        int current = 0;
        for (int bit = 0; bit < prefix.length; ++bit) {
            const int direction = v4 ? ((ipv4 >> (31-bit)) & 1)
                                     : ((bytes[bit / 8] >> (7-bit % 8)) & 1);
            int next = nodes_[current].children[direction];
            if (next < 0) {
                next = static_cast<int>(nodes_.size());
                nodes_[current].children[direction] = next;
                nodes_.append(Node{});
            }
            current = next;
        }
        nodes_[current].covered = true;
    }

    bool coversDefaultRoute(int node = 0) const {
        return node >= 0 && (nodes_[node].covered ||
            (coversDefaultRoute(nodes_[node].children[0]) && coversDefaultRoute(nodes_[node].children[1])));
    }

private:
    struct Node { int children[2] = {-1,-1}; bool covered = false; };
    QVector<Node> nodes_;
};
}

QString WireGuardPrefix::text() const { return address.toString() + '/' + QString::number(length); }

bool hasWireGuardHeader(QByteArrayView source) {
    if (source.size() > WireGuardConfig::sourceLimit) return false;
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString content = decoder(source);
    if (decoder.hasError()) return false;
    for (const auto& raw : content.split('\n')) {
        const auto line = unComment(raw).toLower();
        if (!line.isEmpty()) return line == "[interface]" || line == "[peer]";
    }
    return false;
}

Result parseWireGuard(QByteArrayView source, WireGuardConfig& config) {
    if (source.isEmpty() || source.size() > WireGuardConfig::sourceLimit)
        return Result::fail("WireGuard 配置必须为非空且不超过 128 KiB 的 UTF-8 文本");
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString content = decoder(source);
    if (decoder.hasError() || content.contains(QChar::Null))
        return Result::fail("WireGuard 配置须为有效的 UTF-8 文本");

    WireGuardConfig parsed;
    WireGuardPeer* peer = nullptr;
    bool seenInterface = false;
    QSet<QString> fields;
    qsizetype row = 0;
    for (const auto& raw : content.split('\n')) {
        ++row;
        const auto line = unComment(raw);
        if (line.isEmpty()) continue;
        const auto section = line.toLower();
        if (section == "[interface]") {
            if (seenInterface) return invalid(row, "只允许一个 Interface 段");
            seenInterface = true;
            continue;
        }
        if (!seenInterface) return invalid(row, "首段须为 Interface");
        if (section == "[peer]") {
            if (parsed.peers.size() >= maxPeers) return invalid(row, "Peer 超过 32 个");
            parsed.peers.append(WireGuardPeer{});
            peer = &parsed.peers.last();
            fields.clear();
            continue;
        }
        const auto delimiter = line.indexOf('=');
        if (delimiter < 1) return invalid(row, "此处需要字段和值");
        const auto field = line.left(delimiter).trimmed().toLower();
        const auto value = line.mid(delimiter+1).trimmed();
        const bool repeatedList = (!peer && (field == "address" || field == "dns")) || (peer && field == "allowedips");
        if (!repeatedList && fields.contains(field)) return invalid(row, "此字段重复");
        fields.insert(field);
        bool ok = false;
        if (!peer) {
            if (field == "privatekey") ok = readKey(value, parsed.privateKey);
            else if (field == "address") ok = appendList(value, parsed.addresses, 16, [](const auto& text, auto& prefix) { return readPrefix(text, prefix, false); });
            else if (field == "dns") ok = appendList(value, parsed.dns, 8, ip);
            else if (field == "listenport") ok = integer(value, 0, 65535, parsed.listenPort);
            else if (field == "mtu") ok = integer(value, 576, 65535, parsed.mtu);
            else return invalid(row, "不支持此 Interface 字段");
        } else {
            if (field == "publickey") ok = readKey(value, peer->publicKey);
            else if (field == "presharedkey") ok = readKey(value, peer->presharedKey);
            else if (field == "endpoint") ok = readEndpoint(value, *peer);
            else if (field == "allowedips") ok = appendList(value, peer->allowed, maxPeerPrefixes, [](const auto& text, auto& prefix) { return readPrefix(text, prefix, true); });
            else if (field == "persistentkeepalive") ok = integer(value, 0, 65535, peer->keepalive);
            else return invalid(row, "不支持此 Peer 字段");
        }
        if (!ok) return invalid(row, "字段值无效或超过限制");
    }
    if (!seenInterface || parsed.privateKey.isEmpty() || parsed.addresses.isEmpty() || parsed.peers.isEmpty())
        return Result::fail("WireGuard 配置缺少 Interface 地址、私钥或 Peer");

    QSet<QByteArray> keys;
    QVector<WireGuardPrefix> accepted;
    int total = 0;
    for (auto& item : parsed.peers) {
        if (item.publicKey.isEmpty() || item.endpointHost.isEmpty() || item.allowed.isEmpty())
            return Result::fail("每个 WireGuard Peer 都须包含 PublicKey、Endpoint 和 AllowedIPs");
        if (keys.contains(item.publicKey)) return Result::fail("WireGuard Peer 公钥重复");
        keys.insert(item.publicKey);
        total += static_cast<int>(item.allowed.size());
        if (total > maxPrefixes) return Result::fail("WireGuard 受管网段超过 256 个");
        QSet<QString> local;
        QVector<WireGuardPrefix> distinct;
        for (const auto& prefix : item.allowed) {
            if (local.contains(prefix.text())) continue;
            if (std::any_of(accepted.cbegin(), accepted.cend(), [&](const auto& old) { return overlaps(prefix, old); }))
                return Result::fail("不同 WireGuard Peer 的 AllowedIPs 重叠");
            local.insert(prefix.text());
            distinct.append(prefix);
        }
        item.allowed = std::move(distinct);
        accepted += item.allowed;
    }
    config = std::move(parsed);
    return {true, {}, config.summary()};
}

QJsonObject WireGuardConfig::summary() const {
    QJsonArray interfaceAddresses, routes;
    PrefixCoverage v4, v6;
    for (const auto& address : addresses) interfaceAddresses.append(address.text());
    for (const auto& peer : peers) {
        for (const auto& prefix : peer.allowed) {
            routes.append(prefix.text());
            (prefix.address.protocol() == QAbstractSocket::IPv4Protocol ? v4 : v6).insert(prefix);
        }
    }
    return {{"kind","wireguard"}, {"addresses", interfaceAddresses}, {"routes", routes},
            {"peer_count", static_cast<int>(peers.size())}, {"dns_count", static_cast<int>(dns.size())},
            {"full_v4", v4.coversDefaultRoute()}, {"full_v6", v6.coversDefaultRoute()}};
}
}
