#pragma once
#include "MihomoClient.h"
#include "platform/PlatformServices.h"
#include <QProcess>
#include <QTimer>
#include "TunSession.h"

namespace mlash {
class CoreManager : public QObject {
    Q_OBJECT
public:
    CoreManager(MihomoClient* api, std::shared_ptr<PlatformServices> platform, QObject* parent = nullptr,std::unique_ptr<TunSession> tun = {});
    ~CoreManager() override;
    void start(const QString& executable, const QString& config, const QString& work, int mixedPort, int apiPort, const QString& secret, Completion done,bool tun = false);
    void stop(Completion done);
    void validate(const QString& executable, const QString& config, const QString& work, Completion done, int timeoutMs = 360000);
    void cancelValidation();
    CoreState state() const { return state_; }
    qint64 processId() const { return usingTun_ ? tun_->processId() : process_.processId(); }
    QJsonObject tunObservation() const { return tun_->observation(); }
    void confirmTunCleanup(Completion done);
signals:
    void stateChanged(mlash::CoreState state);
    void output(const QString& text);
    void crashed(const QString& error);
private:
    void setState(CoreState state);
    void probe(quint64 generation);
    void failStart(const QString& error);
    MihomoClient* api_;
    std::shared_ptr<PlatformServices> platform_;
    QProcess process_;
    std::unique_ptr<TunSession> tun_;
    bool usingTun_=false;
    QPointer<QProcess> validation_;
    QTimer deadline_, stopDeadline_;
    CoreState state_ = CoreState::Stopped;
    Completion startDone_, stopDone_;
    int mixedPort_ = 0;
    int apiPort_ = 0;
    QString probeError_;
    quint64 generation_ = 0;
};
}
