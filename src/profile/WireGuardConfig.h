#pragma once

#include "domain/Types.h"
#include <QHostAddress>
#include <QByteArrayView>

namespace mlash {
struct WireGuardPrefix {
    QHostAddress address;
    int length = 0;
    QString text() const;
};

struct WireGuardPeer {
    QByteArray publicKey, presharedKey;
    QString endpointHost;
    quint16 endpointPort = 0, keepalive = 0;
    QVector<WireGuardPrefix> allowed;
};

struct WireGuardConfig {
    static constexpr qsizetype sourceLimit = 128 * 1024;
    QByteArray privateKey;
    QVector<WireGuardPrefix> addresses;
    QVector<QHostAddress> dns;
    QVector<WireGuardPeer> peers;
    quint16 listenPort = 0, mtu = 0;

    // Contains only addresses, prefix scopes and capabilities.
    QJsonObject summary() const;
};

bool hasWireGuardHeader(QByteArrayView source);
Result parseWireGuard(QByteArrayView source, WireGuardConfig& config);
}
