#pragma once
#include <QTcpServer>
#include <QNetworkProxy>
#include <QRandomGenerator>

// Reserve fixture listeners below Windows' default ephemeral client range.
// Closing a port-0 reservation releases a client port which an API connection
// can consume before the child process binds it. Production ownership/readiness
// checks and their deadlines remain unchanged.
inline int isolatedListenerPort() {
    for (int attempt=0;attempt<100;++attempt) {
        QTcpServer reservation; reservation.setProxy(QNetworkProxy::NoProxy);
        const int candidate=QRandomGenerator::global()->bounded(20000,40000);
        if (reservation.listen(QHostAddress::LocalHost,candidate)) return candidate;
    }
    return 0;
}
