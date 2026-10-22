#include "TunApiGate.h"
#include <QTcpSocket>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QUrl>

namespace mlash {
bool allowedTunApi(const QByteArray& method,const QByteArray& target,const QByteArray& body) {
    if (target.size()>8192||body.size()>16384||!target.startsWith('/')||target.startsWith("//")||target.contains('#')||target.contains('\\')) return false;
    const auto url=QUrl::fromEncoded(target,QUrl::StrictMode); if (!url.isValid()||!url.scheme().isEmpty()||!url.host().isEmpty()) return false;
    const auto path=url.path(); if (path.contains('\\')||path.contains(QChar::Null)) return false;
    for (const auto character:path) if (character.unicode()<32||character.unicode()==127) return false;
    for (const auto& part:path.split('/')) if (part=="."||part=="..") return false;
    // Split before decoding so a slash in a valid node name remains a single
    // parameter, exactly as MihomoClient::segment encoded it.
    const auto rawPath=target.split('?').first(); auto encoded=rawPath.split('/');
    if (encoded.isEmpty()||!encoded.takeFirst().isEmpty()||encoded.contains(QByteArray{})) return false;
    QStringList segments; for (const auto& part:encoded) segments.append(QString::fromLatin1(part));
    if (method=="GET"&&body.isEmpty()) {
        if (QList<QByteArray>{"/version","/configs","/proxies","/connections","/rules","/dns/query","/logs"}.contains(rawPath)) return true;
        if (segments.size()==2&&segments[0]=="proxies") return true;
        if (segments.size()==3&&segments[0]=="proxies"&&segments[2]=="delay") return true;
        return false;
    }
    if (method=="DELETE"&&body.isEmpty()) return rawPath=="/connections"||(segments.size()==2&&QStringList{"connections","proxies"}.contains(segments[0]));
    QJsonParseError error; const auto document=QJsonDocument::fromJson(body,&error); if (error.error!=QJsonParseError::NoError||!document.isObject()) return false;
    const auto object=document.object(); if (object.size()!=1) return false;
    if (method=="PATCH"&&rawPath=="/configs") return object.value("mode").isString()&&QStringList{"rule","global","direct"}.contains(object.value("mode").toString());
    return method=="PUT"&&segments.size()==2&&segments[0]=="proxies"&&object.value("name").isString()&&!object.value("name").toString().isEmpty()&&object.value("name").toString().size()<=512;
}
TunApiGate::TunApiGate(QObject* parent):QObject(parent),server_(this) { connect(&server_,&QTcpServer::newConnection,this,&TunApiGate::accept); }
TunApiGate::~TunApiGate() { server_.close(); const auto sockets=sockets_; for (auto* socket:sockets) { QObject::disconnect(socket,nullptr,nullptr,nullptr); socket->abort(); delete socket; } sockets_.clear(); }
Result TunApiGate::listen(int publicPort,int privatePort,const QString& publicSecret,const QString& privateSecret) {
    privatePort_=privatePort; publicSecret_=publicSecret.toUtf8(); privateSecret_=privateSecret.toUtf8();
    return server_.listen(QHostAddress::LocalHost,quint16(publicPort))?Result{}:Result::fail("TUN 控制入口端口被占用");
}
void TunApiGate::accept() {
    while (auto* socket=server_.nextPendingConnection()) {
        socket->setParent(this); if (sockets_.size()>=24) { socket->abort(); socket->deleteLater(); continue; } sockets_.insert(socket); socket->setReadBufferSize(32768);
        auto bytes=std::make_shared<QByteArray>(); auto finished=std::make_shared<bool>(false);
        auto* timer=new QTimer(socket); timer->setSingleShot(true); timer->start(5000); connect(timer,&QTimer::timeout,socket,[socket] { socket->abort(); });
        connect(socket,&QTcpSocket::disconnected,this,[this,socket] { sockets_.remove(socket); socket->deleteLater(); });
        connect(socket,&QTcpSocket::readyRead,this,[this,socket,bytes,finished,timer] {
            if (*finished) { socket->readAll(); return; }
            *bytes+=socket->readAll(); const auto end=bytes->indexOf("\r\n\r\n");
            const auto reject=[socket,finished](int code) { *finished=true; socket->write("HTTP/1.1 "+QByteArray::number(code)+" Rejected\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); socket->disconnectFromHost(); };
            if (bytes->size()>32768||(end<0&&bytes->size()>16384)) { reject(413); return; } if (end<0) return;
            const auto lines=bytes->left(end).split('\n'); const auto first=lines.value(0).trimmed().split(' '); if (first.size()!=3||first[2]!="HTTP/1.1") { reject(400); return; }
            QByteArray auth; qint64 size=0; bool haveSize=false,haveAuth=false;
            for (int i=1;i<lines.size();++i) { const auto colon=lines[i].indexOf(':'); if (colon<1) { reject(400); return; } const auto key=lines[i].left(colon).trimmed().toLower(); const auto value=lines[i].mid(colon+1).trimmed();
                if (key=="transfer-encoding"||key=="upgrade"||lines[i].startsWith(' ')||lines[i].startsWith('\t')) { reject(400); return; }
                if (key=="authorization") { if (haveAuth) { reject(400); return; } haveAuth=true; auth=value; }
                if (key=="content-length") { bool ok=false; size=value.toLongLong(&ok); if (haveSize||!ok||size<0||size>16384) { reject(413); return; } haveSize=true; }
            }
            if (auth!="Bearer "+publicSecret_) { reject(403); return; }
            const auto body=bytes->mid(end+4); if (body.size()<size) return; if (body.size()!=size||!allowedTunApi(first[0],first[1],body)) { reject(403); return; }
            *finished=true; timer->stop(); const bool logs=QUrl::fromEncoded(first[1]).path()=="/logs";
            auto* backend=new QTcpSocket(socket); backend->setProxy(QNetworkProxy::NoProxy); backend->setReadBufferSize(256*1024); auto total=std::make_shared<qint64>(0); timer->start(5000);
            connect(timer,&QTimer::timeout,backend,&QTcpSocket::abort);
            connect(socket,&QTcpSocket::disconnected,backend,&QTcpSocket::abort);
            connect(backend,&QTcpSocket::connected,socket,[this,socket,backend,first,body,timer,logs,total] {
                // Check the established server tuple before releasing the private
                // credential. A listener-only check has a port-reuse race.
                if (!verifyBackend_||!verifyBackend_(backend->localPort(),backend->peerPort())) { *total=-1; socket->write("HTTP/1.1 503 Identity\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); backend->abort(); socket->disconnectFromHost(); return; }
                if (logs) timer->stop(); else timer->start(15000);
                backend->write(first[0]+' '+first[1]+" HTTP/1.1\r\nHost: 127.0.0.1:"+QByteArray::number(privatePort_)+"\r\nAuthorization: Bearer "+privateSecret_+"\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
            });
            connect(backend,&QTcpSocket::readyRead,socket,[socket,backend,total,logs] {
                const auto data=backend->readAll(); *total+=data.size(); if ((!logs&&*total>16*1024*1024)||socket->bytesToWrite()+data.size()>512*1024) { backend->abort(); socket->abort(); return; } socket->write(data);
            });
            connect(backend,&QTcpSocket::disconnected,socket,[socket,timer] { timer->stop(); socket->disconnectFromHost(); });
            connect(backend,&QTcpSocket::errorOccurred,socket,[socket,total,timer](QAbstractSocket::SocketError error) { if (error==QAbstractSocket::RemoteHostClosedError) return; timer->stop(); if (*total==0) socket->write("HTTP/1.1 502 Failed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); socket->disconnectFromHost(); });
            backend->connectToHost(QHostAddress::LocalHost,quint16(privatePort_));
        });
    }
}
}
