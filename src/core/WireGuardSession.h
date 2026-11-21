#pragma once

#include "domain/Types.h"
#include "platform/PlatformServices.h"
#include <QLocalSocket>
#include <QTimer>

namespace mlash {
class WireGuardSession final : public QObject {
    Q_OBJECT
public:
    explicit WireGuardSession(std::shared_ptr<PlatformServices> platform, QObject* parent = nullptr,
                              QString pipeName = "Mlash.Tun.v1");
    ~WireGuardSession() override;
    using Done = std::function<void(Result)>;
    void start(QByteArray source,const QStringList& endpoints,Done done);
    void stop(Done done, bool verifyIdle = false);
    bool active() const { return active_; }
    bool leasePending() const { return lease_; }
    const QJsonObject& observation() const { return observation_; }

signals:
    void changed();
    void lost(const QString& reason);

private:
    void connectToService(bool cleanup);
    void openPipe(quint64 epoch,bool cleanup);
    void sendInitial(bool cleanup);
    void readReplies();
    void write(const QJsonObject& request);
    void finishStop(Result result);
    void fail(const QString& reason);
    void clearPayload();

    std::shared_ptr<PlatformServices> platform_;
    QString pipeName_;
    QLocalSocket socket_;
    QTimer deadline_,heartbeat_;
    QByteArray pending_,input_;
    Done startDone_,stopDone_;
    QJsonObject observation_;
    quint64 epoch_=0;
    bool active_=false,lease_=false;
};
}
