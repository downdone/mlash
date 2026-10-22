#pragma once
#include "app/AppController.h"
#include "ui/GuiHost.h"
#include "ui/TrayController.h"
#include <QElapsedTimer>
namespace mlash {
std::shared_ptr<PlatformServices> createMeasurementPlatform();
// Explicit isolated QA only. No OS proxy or autostart writes; ordinary startup never instantiates this.
class QualificationRun final : public QObject {
public:
    QualificationRun(AppController*,GuiHost*,TrayController*,QString report,int seconds,bool retainGui,QObject* parent=nullptr);
    void start(qint64 initializationMs);
    bool succeeded() const { return phase_==5 && failure_.isEmpty(); }
private:
    void prepared(bool ok);
    void warmGui(int remaining);
    void beginIdle();
    void sample();
    void save();
    void stop(const QString& failure={});
    AppController* controller_;
    GuiHost* gui_;
    TrayController* tray_;
    QString path_, failure_, startedUtc_;
    int seconds_, phase_=0;
    bool retainGui_;
    bool saving_=false, savePending_=false;
    QTimer timer_, watchdog_, deadline_;
    QElapsedTimer clock_, activation_, coreStart_;
    QJsonObject report_;
    QJsonArray samples_;
    QJsonArray pendingSamples_;
    int sampleCount_=0;
    qint64 firstSampleMs_=-1;
    quint64 startGeneration_=0;
};
}
