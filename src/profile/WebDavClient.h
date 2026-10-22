#pragma once
#include "RemoteBackup.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QHostAddress>
#include <QUrl>
#include <functional>
namespace mlash {
class WebDavClient final:public QObject {
    Q_OBJECT
public:
    using Done=std::function<void(Result,QByteArray)>;
    // Nonempty endpointFixture is only supplied by isolated transport tests.
    explicit WebDavClient(QObject* parent=nullptr,QUrl endpointFixture={});
    ~WebDavClient() override;
    void list(const QJsonObject& config,int corePort,Done done);
    void download(const QJsonObject& config,int corePort,const QString& file,Done done);
    void upload(const QJsonObject& config,int corePort,const QString& name,const QByteArray& encrypted,Done done);
    void cancel();
    bool busy() const { return busy_; }
    static Result parseListing(const QUrl& root,const QByteArray& xml);
private:
    void start(const QJsonObject& config,int port,Done done,std::function<void()> resolved);
    void send(const QByteArray& method,const QString& file,const QByteArray& body,const QString& destination,qint64 limit,Done done);
    void complete(Result result,QByteArray bytes={});
    void hashBytes(QByteArray bytes,std::function<void(QByteArray)> done);
    QNetworkAccessManager network_; QPointer<QNetworkReply> reply_; QPointer<QTimer> resolveTimer_; QTimer deadline_; int lookup_=-1;
    QUrl fixture_,root_,transport_; QHostAddress address_; QJsonObject config_; Done done_;
    quint64 generation_=0; bool busy_=false,hashPending_=false; QByteArray response_;
};
}
