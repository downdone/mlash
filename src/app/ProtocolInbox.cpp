#include "ProtocolInbox.h"
#include <QLocalSocket>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>
#include <memory>
namespace mlash {
QString ProtocolInbox::name(const QString& root) { auto path=QFileInfo(root).absoluteFilePath();
#ifdef Q_OS_WIN
    path=path.toLower();
#endif
    return "Mlash-import-"+QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(),QCryptographicHash::Sha256).toHex());
}
ProtocolInbox::ProtocolInbox(QObject* parent):QObject(parent) {
    server_.setSocketOptions(QLocalServer::UserAccessOption); server_.setMaxPendingConnections(4);
    connect(&server_,&QLocalServer::newConnection,this,[this] { while (server_.hasPendingConnections()) { auto* socket=server_.nextPendingConnection(); if (clients_>=4) { socket->abort(); socket->deleteLater(); continue; } ++clients_; connect(socket,&QObject::destroyed,this,[this] { --clients_; }); socket->setReadBufferSize(20000); auto bytes=std::make_shared<QByteArray>(); auto delivered=std::make_shared<bool>(false); auto* timer=new QTimer(socket); timer->setSingleShot(true); connect(timer,&QTimer::timeout,socket,&QLocalSocket::abort); timer->start(2000); connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QLocalSocket::readyRead,this,[this,socket,bytes,delivered,timer] { if (*delivered) { socket->abort(); return; } *bytes+=socket->readAll(); if (bytes->size()>19000) { socket->abort(); return; } if (!bytes->endsWith('\n')) return; *delivered=true; timer->stop(); const auto document=QJsonDocument::fromJson(bytes->trimmed()); const auto object=document.object(); const auto link=object.value("import_url").toString(); const bool valid=document.isObject()&&object.size()==1&&object.value("import_url").isString()&&parseProtocolLink(link).ok; socket->write(valid?"1":"0"); socket->disconnectFromServer(); if (valid) emit received(link); });
    } });
}
Result ProtocolInbox::listen(const QString& root) { const auto serverName=name(root); QLocalServer::removeServer(serverName); return server_.listen(serverName)?Result{}:Result::fail("无法创建当前账户的链接确认通道"); }
void ProtocolInbox::forward(QObject* owner,const QString& root,const QString& link,std::function<void(Result)> done) {
    const auto valid=parseProtocolLink(link); if (!valid.ok) { done(valid); return; } auto* socket=new QLocalSocket(owner); auto completed=std::make_shared<bool>(false); auto* timer=new QTimer(socket); timer->setSingleShot(true);
    auto finish=[socket,completed,done=std::move(done)](Result result) mutable { if (*completed) return; *completed=true; socket->abort(); socket->deleteLater(); done(result); };
    connect(timer,&QTimer::timeout,socket,[finish]() mutable { finish(Result::fail("运行中的Mlash未接收导入链接，请在主窗口手动添加")); }); connect(socket,&QLocalSocket::errorOccurred,socket,[finish](QLocalSocket::LocalSocketError) mutable { finish(Result::fail("无法连接运行中的Mlash")); });
    connect(socket,&QLocalSocket::connected,socket,[socket,link] { socket->write(QJsonDocument(QJsonObject{{"import_url",link}}).toJson(QJsonDocument::Compact)+'\n'); }); connect(socket,&QLocalSocket::readyRead,socket,[socket,finish]() mutable { finish(socket->readAll()=="1"?Result{}:Result::fail("导入链接未被接受")); }); timer->start(1500); socket->connectToServer(name(root));
}
}
