#pragma once
#include "profile/SubscriptionDownloader.h"
#include "ServiceDiagnostics.h"
#include <QMap>
namespace mlash {
class OutboundDiagnostics final : public QObject {
    Q_OBJECT
public:
    explicit OutboundDiagnostics(QObject* parent=nullptr,bool allowPrivateForTests=false,QUrl ipTarget=QUrl("https://api.ip.sb/geoip"),QMap<QString,QUrl> serviceUrls={});
    quint64 prepare(const QJsonObject& route);
    void runIp(quint64 token,int corePort);
    void runServices(quint64 token,int corePort,const QString& id={});
    void fail(quint64 token,const QString& error);
    void cancel();
    bool busy() const { return busy_; }
    const QJsonObject& result() const { return result_; }
    static Result parseIp(const QByteArray& bytes);
signals:
    void changed();
private:
    QPointer<SubscriptionDownloader> downloader_;
    QUrl ipTarget_;
    bool allowPrivateForTests_=false,busy_=false;
    quint64 token_=0;
    QJsonObject route_,result_;
    QList<ServiceTarget> serviceQueue_;
    QMap<QString,QUrl> serviceUrls_;
    void runNextService(quint64 token,int corePort);
};
}
