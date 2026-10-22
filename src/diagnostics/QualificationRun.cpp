#include "QualificationRun.h"
#include "ProcessMetrics.h"
#include "app/Async.h"
#include "storage/Storage.h"
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QTableWidget>
#include <QListWidget>
#include <QTableView>
#include <QThread>
#include <QTcpServer>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>
#include <utility>
#include <QFileInfo>
namespace mlash {
namespace {
class MeasurementPlatform final : public PlatformServices {
    std::shared_ptr<PlatformServices> real_=createPlatformServices();
public:
    Result readProxy() override { return real_->readProxy(); }
    Result writeProxy(const QJsonObject&) override { return Result::fail("测量模式禁止系统代理写入"); }
    QJsonObject proxyTarget(int port) const override { return real_->proxyTarget(port); }
    bool proxyEnabled(const QJsonObject& o) const override { return real_->proxyEnabled(o); }
    Result autoStart(bool,const QString&) override { return Result::fail("测量模式禁止自启写入"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& p,QByteArray& e) override { return real_->seal(p,e); }
    Result open(const QByteArray& e,QByteArray& p) override { return real_->open(e,p); }
    Result secureDirectory(const QString& p) override { return real_->secureDirectory(p); }
    Result attachChild(qint64 pid) override { return real_->attachChild(pid); }
    Result ownsTcpPort(qint64 pid,int port) override { return real_->ownsTcpPort(pid,port); }
};
double p95(QVector<double> values) { if (values.isEmpty()) return 0; std::sort(values.begin(),values.end()); return values[qMin(values.size()-1,qsizetype(std::ceil(values.size()*0.95)-1))]; }
}
std::shared_ptr<PlatformServices> createMeasurementPlatform() { return std::make_shared<MeasurementPlatform>(); }
QualificationRun::QualificationRun(AppController* c,GuiHost* g,TrayController* t,QString path,int seconds,bool retainGui,QObject* parent)
    : QObject(parent),controller_(c),gui_(g),tray_(t),path_(std::move(path)),seconds_(seconds),retainGui_(retainGui) {
    connect(c,&AppController::operationFinished,this,[this](bool ok,const QString&) { if (phase_==1 || phase_==2 || phase_==6) prepared(ok); });
    connect(c,&AppController::stateChanged,this,[this] {
        if (phase_==2 && controller_->state().core==CoreState::Starting && !coreStart_.isValid()) coreStart_.start();
        if (phase_==4 && (controller_->state().core!=CoreState::Running || !controller_->state().ready || controller_->state().generation!=startGeneration_)) stop("测量期间内核/运行代次发生改变");
    });
    timer_.setInterval(15000); connect(&timer_,&QTimer::timeout,this,&QualificationRun::sample);
    watchdog_.setSingleShot(true); connect(&watchdog_,&QTimer::timeout,this,[this] { stop("测量准备超时"); });
    deadline_.setSingleShot(true); deadline_.setTimerType(Qt::PreciseTimer); connect(&deadline_,&QTimer::timeout,this,[this] {
        if (phase_!=4) return; const auto remaining=firstSampleMs_+qint64(seconds_)*1000-clock_.elapsed(); if (remaining>0) { deadline_.start(int(remaining)); return; } sample(); if (phase_==4) stop();
    });
}
void QualificationRun::start(qint64 initializationMs) {
    report_={{"format",1},{"status","preparing"},{"client_version",MLASH_VERSION},{"qt_version",QT_VERSION_STR},{"environment",measurementEnvironment()},
        {"requested_seconds",seconds_},{"fixture_nodes",500},{"scope",retainGui_?"GUI idle":"lightweight tray idle"},{"system_writes_blocked",true},
        {"application_initialization_ms",double(initializationMs)},{"startup_classification","uncontrolled-cache; not cold-start qualification"},{"tray_available",tray_->available()}};
    phase_=1; watchdog_.start(45000);
    const auto root=controller_->dataRoot(), exe=QCoreApplication::applicationFilePath(), core=controller_->settings().corePath;
    runTask(this,[root,exe,core] {
        QByteArray yaml="mode: rule\nproxies:\n"; for (int i=0;i<500;++i) yaml += QString("  - {name: node-%1, type: http, server: 127.0.0.1, port: 9}\n").arg(i,3,10,QChar('0')).toUtf8();
        yaml += "proxy-groups:\n  - name: Release fixture\n    type: select\n    proxies:\n"; for (int i=0;i<500;++i) yaml += QString("      - node-%1\n").arg(i,3,10,QChar('0')).toUtf8(); yaml += "rules: [\"MATCH,Release fixture\"]\n";
        auto result=writeAtomic(root+"/fixture.yaml",yaml); if (!result.ok) return result;
        auto hashFile=[](const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return QString(); QCryptographicHash hash(QCryptographicHash::Sha256); return hash.addData(&file)?QString::fromLatin1(hash.result().toHex()):QString(); };
        result.data={{"executable_sha256",hashFile(exe)},{"core_sha256",hashFile(core)},{"fixture_sha256",QString::fromLatin1(QCryptographicHash::hash(yaml,QCryptographicHash::Sha256).toHex())}};
        if (result.data.value("executable_sha256").toString().isEmpty() || result.data.value("core_sha256").toString().isEmpty()) return Result::fail("测量二进制哈希读取失败"); return result;
    },[this](Result r) { if (phase_!=1) return; if (!r.ok) { stop(r.error); return; } for (auto i=r.data.begin();i!=r.data.end();++i) report_[i.key()]=i.value();
        QTcpServer reservation; int port=0; for (int attempt=0;attempt<32;++attempt) { const int candidate=20000+QRandomGenerator::global()->bounded(25000); if (reservation.listen(QHostAddress::LocalHost,candidate)) { port=candidate; break; } }
        if (!port) { stop("没有可用测量端口"); return; } auto settings=controller_->settings(); settings.mixedPort=port; reservation.close(); phase_=6; controller_->updateSettings(settings);
    });
}
void QualificationRun::prepared(bool ok) {
    if (!ok) { stop("隔离配置准备失败："+controller_->state().lastError.left(1024)); return; }
    if (phase_==6) { phase_=1; controller_->importFile("Release fixture",controller_->dataRoot()+"/fixture.yaml"); return; }
    if (phase_==1) { phase_=2; activation_.start(); controller_->activateProfile(controller_->profiles().first().id); return; }
    if (!controller_->state().ready || !coreStart_.isValid()) { stop("测量内核未就绪"); return; }
    report_["core_version"]=controller_->state().coreVersion; report_["profile_activation_ms"]=double(activation_.elapsed()); report_["core_ready_ms"]=double(coreStart_.elapsed());
    phase_=3; watchdog_.stop(); warmGui(6);
}
void QualificationRun::warmGui(int remaining) {
    gui_->show(2);
    QTimer::singleShot(120,this,[this,remaining] {
        if (phase_!=3) return; auto* window=gui_->window(); if (!window) { stop("测量界面重建失败"); return; }
        auto* groups=window->findChild<QListWidget*>("proxyGroups"); for (int row=0;groups && row<groups->count();++row) if (groups->item(row)->text()=="Release fixture") groups->setCurrentRow(row);
        if (remaining>1) { window->close(); QTimer::singleShot(50,this,[this,remaining] { if (gui_->window()) { stop("轻量窗口未释放"); return; } warmGui(remaining-1); }); return; }
        auto* nodes=window->findChild<QTableView*>("nodesTable"); auto* sort=window->findChild<QComboBox*>("memberSort");
        if (!nodes || !sort || nodes->model()->rowCount()!=500) { stop("500 节点测量夹具不完整"); return; }
        QVector<double> sorting,menus; for (int i=0;i<60;++i) { QElapsedTimer t; t.start(); sort->setCurrentIndex(i%3); nodes->model()->rowCount(); sorting.append(t.nsecsElapsed()/1e6); menus.append(tray_->measureMenuPreparation()/1e6); }
        report_["node_sort_p95_ms"]=p95(sorting); report_["menu_preparation_p95_ms"]=p95(menus); report_["menu_measurement_scope"]="synchronous Qt preparation; excludes OS click/paint latency";
        if (!retainGui_) window->close(); QTimer::singleShot(150,this,&QualificationRun::beginIdle);
    });
}
void QualificationRun::beginIdle() {
    if (phase_!=3) return; if (!retainGui_ && gui_->window()) { stop("GUI 资源未释放"); return; }
    report_["gui_released"]=!gui_->window(); phase_=4; startGeneration_=controller_->state().generation; clock_.start(); startedUtc_=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    report_["started_utc"]=startedUtc_; sample(); if (phase_!=4) return; timer_.start(); deadline_.start(seconds_*1000);
}
void QualificationRun::sample() {
    if (phase_!=4) return; const auto client=processMetrics(QCoreApplication::applicationPid()), core=processMetrics(controller_->coreProcessId());
    if (!client.ok || !core.ok) { stop("资源采样失败"); return; }
    const auto sampledMs=clock_.elapsed(); if (firstSampleMs_<0) firstSampleMs_=sampledMs;
    const QJsonObject sample{{"elapsed_seconds",sampledMs/1000.0},{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"client",client.data},{"core",core.data}};
    ++sampleCount_; samples_.append(sample); if (samples_.size()>32) samples_.removeAt(0); pendingSamples_.append(sample);
    if (pendingSamples_.size()>64) { stop("采样写入积压，报告不完整"); return; } save();
}
void QualificationRun::save() {
    if (saving_) { savePending_=true; return; } saving_=true;
    auto result=report_; result["status"]=phase_==5 ? (failure_.isEmpty()?"completed":"failed") : "running"; result["failure"]=failure_; result["elapsed_seconds"]=clock_.isValid()?clock_.elapsed()/1000.0:0; result["samples"]=samples_;
    result["sample_count"]=sampleCount_; result["samples_file"]=QFileInfo(path_+".samples.jsonl").fileName();
    if (phase_==5) result["ended_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    // Stream all numeric samples; retain only 32 rows and at most 64 pending rows in RAM.
    const bool final=phase_==5;
    const auto batch=std::exchange(pendingSamples_,{}); const bool first=sampleCount_==batch.size();
    runTask(this,[path=path_,result,batch,first,final]() mutable {
        const auto streamPath=path+".samples.jsonl";
        if (!batch.isEmpty()) { QFile file(streamPath); if (!file.open(QIODevice::WriteOnly|(first?QIODevice::NewOnly:QIODevice::Append))) return Result::fail("采样流写入失败");
            for (const auto& value:batch) { const auto line=QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)+'\n'; if (file.write(line)!=line.size()) return Result::fail("采样流短写"); }
            if (!file.flush()) return Result::fail("采样流刷新失败"); }
        if (final && QFileInfo::exists(streamPath)) { QFile stream(streamPath); QCryptographicHash hash(QCryptographicHash::Sha256); if (!stream.open(QIODevice::ReadOnly) || !hash.addData(&stream)) return Result::fail("采样流哈希失败"); result["samples_sha256"]=QString::fromLatin1(hash.result().toHex()); }
        return writeAtomic(path,QJsonDocument(result).toJson(QJsonDocument::Compact));
    },[this,final](Result r) {
        saving_=false;
        if (!r.ok && phase_!=5) { stop("测量报告写入失败"); return; }
        if (savePending_) { savePending_=false; save(); return; }
        if (final) { if (!r.ok) failure_="测量报告写入失败"; controller_->requestExit(); }
    });
}
void QualificationRun::stop(const QString& failure) { if (phase_==5) return; failure_=failure; phase_=5; timer_.stop(); watchdog_.stop(); deadline_.stop(); save(); }
}
