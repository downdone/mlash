#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <functional>

struct HttpResponse { int status = 200; QByteArray body = "{}"; int delayMs = 0; QByteArray headers; bool stream = false; bool tunnel = false; };
class MockHttp : public QTcpServer {
public:
    explicit MockHttp(QObject* parent = nullptr);
    std::function<HttpResponse(const QByteArray&)> handler;
    QList<QByteArray> requests;
    int activeConnections() const { return buffers_.size(); }
private:
    QHash<QTcpSocket*, QByteArray> buffers_;
};
