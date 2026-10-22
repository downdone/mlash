#pragma once
#include "app/Types.h"
#include <QTcpServer>
#include <QSet>
#include <functional>

namespace mlash {
// The elevated REST endpoint is never handed to the GUI. Only this small API
// surface is forwarded; upgrade/restart/config upload/file/provider writes fail.
bool allowedTunApi(const QByteArray& method,const QByteArray& target,const QByteArray& body);
class TunApiGate final : public QObject {
public:
    explicit TunApiGate(QObject* parent=nullptr);
    ~TunApiGate() override;
    Result listen(int publicPort,int privatePort,const QString& publicSecret,const QString& privateSecret);
    void setBackendVerifier(std::function<bool(int,int)> verifier) { verifyBackend_=std::move(verifier); }
private:
    void accept();
    QTcpServer server_;
    QSet<QTcpSocket*> sockets_;
    std::function<bool(int,int)> verifyBackend_;
    int privatePort_=0;
    QByteArray publicSecret_,privateSecret_;
};
}
