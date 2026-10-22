#include "MockHttp.h"
#include <QPointer>
#include <QTimer>
#include <QRegularExpression>

MockHttp::MockHttp(QObject* parent) : QTcpServer(parent) {
    connect(this, &QTcpServer::newConnection, this, [this] {
        while (hasPendingConnections()) {
            auto* socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] { buffers_.remove(socket); socket->deleteLater(); });
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                auto& buffer = buffers_[socket]; buffer += socket->readAll(); const int split = buffer.indexOf("\r\n\r\n"); if (split < 0) return;
                const auto match = QRegularExpression("Content-Length: ([0-9]+)", QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(buffer.left(split)));
                const int size = match.hasMatch() ? match.captured(1).toInt() : 0; if (buffer.size() < split + 4 + size) return;
                const auto request = buffer.left(split + 4 + size); buffer.remove(0, split + 4 + size); requests.append(request);
                const auto response = handler ? handler(request) : HttpResponse{}; const QPointer<QTcpSocket> guarded(socket);
                QTimer::singleShot(response.delayMs, this, [guarded, response] {
                    if (!guarded) return;
                    if (response.tunnel) { guarded->write("HTTP/1.1 200 Connection established\r\n\r\n"); return; }
                    const auto framing = response.stream ? QByteArray("Transfer-Encoding: chunked\r\nConnection: keep-alive\r\n")
                        : "Content-Length: " + QByteArray::number(response.body.size()) + "\r\nConnection: close\r\n";
                    const auto header = "HTTP/1.1 " + QByteArray::number(response.status) + " Test\r\nContent-Type: application/json\r\n" + framing + response.headers + "\r\n";
                    guarded->write(header + (response.stream ? QByteArray::number(response.body.size(), 16) + "\r\n" + response.body + "\r\n" : response.body));
                    if (!response.stream) guarded->disconnectFromHost();
                });
            });
        }
    });
}
