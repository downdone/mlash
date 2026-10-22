#pragma once
#include "MihomoClient.h"
#include "platform/PlatformServices.h"
#include <QLocalSocket>
#include <QTimer>

namespace mlash {
class TunSession : public QObject {
    Q_OBJECT
public:
    explicit TunSession(std::shared_ptr<PlatformServices> platform, QObject* parent=nullptr,QString pipeName="Mlash.Tun.v1");
    ~TunSession() override;
    virtual void start(const QString& executable,const QString& work,Completion done);
    virtual void stop(Completion done);
    virtual qint64 processId() const { return pid_; }
    virtual qint64 apiProcessId() const { return qint64(observed_.value("api_pid").toDouble()); }
    virtual bool ready() const { return observed_.value("adapter_up").toBool()&&observed_.value("routes").toBool()&&(!observed_.contains("dns_configured")||observed_.value("dns_configured").toBool())&&(!observed_.value("ipv6_required").toBool()||observed_.value("routes_v6").toBool()); }
    virtual bool active() const { return active_; }
    virtual QJsonObject observation() const { auto value=observed_; value["active"]=active_; value["cleanup_pending"]=leaseMayExist_&&!active_; return value; }
    virtual void confirmCleanup(Completion done);
signals:
    void lost(const QString& error);
private:
    void send(const QJsonObject& request);
    void connectPipe(bool idleProbe);
    void fail(const QString& error);
    std::shared_ptr<PlatformServices> platform_;
    QLocalSocket socket_;
    QTimer deadline_,heartbeat_;
    QByteArray input_;
    QJsonObject observed_;
    qint64 pid_=0;
    quint64 generation_=0;
    Completion startDone_,stopDone_;
    bool active_=false,stopping_=false;
    bool leaseMayExist_=false,idleProbe_=false;
    QString pipeName_;
};
}
