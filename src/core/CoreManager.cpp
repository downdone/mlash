#include "CoreManager.h"
#include "diagnostics/Diagnostics.h"
#include <QFileInfo>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <memory>
#include <QProcessEnvironment>
#include <QTcpServer>

namespace mlash {
void CoreManager::confirmTunCleanup(Completion done) {
    if (process_.state()!=QProcess::NotRunning||state_==CoreState::Starting||state_==CoreState::Stopping) { done(Result::fail("请先停止内核，再确认旧 TUN 会话清理")); return; }
    tun_->confirmCleanup([this,done=std::move(done)](Result result) mutable { if (result.ok) { usingTun_=false; setState(CoreState::Stopped); } done(result); });
}
namespace {
QProcessEnvironment cleanEnvironment() {
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto& key : env.keys()) if (key.startsWith("CLASH_") || key == "SAFE_PATHS" || key == "SKIP_SAFE_PATH_CHECK") env.remove(key);
    env.insert("SKIP_SAFE_PATH_CHECK", "false"); return env;
}
}
CoreManager::CoreManager(MihomoClient* api, std::shared_ptr<PlatformServices> platform, QObject* parent,std::unique_ptr<TunSession> tun)
    : QObject(parent), api_(api), platform_(std::move(platform)),tun_(tun?std::move(tun):std::make_unique<TunSession>(platform_)) {
    connect(tun_.get(),&TunSession::lost,this,[this](const QString& error) {
        if (!usingTun_||state_==CoreState::Stopped||state_==CoreState::Stopping) return;
        deadline_.stop(); api_->cancelAll(); ++generation_; setState(CoreState::Failed);
        auto done=std::move(startDone_); if (done) done(Result::fail(error)); else emit crashed(error);
    });
    deadline_.setSingleShot(true); stopDeadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] { failStart("内核就绪超时：" + probeError_); });
    connect(&stopDeadline_, &QTimer::timeout, this, [this] { process_.kill(); });
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] { emit output(QString::fromUtf8(process_.readAllStandardOutput())); });
    connect(&process_, &QProcess::readyReadStandardError, this, [this] { emit output(QString::fromUtf8(process_.readAllStandardError())); });
    connect(&process_, &QProcess::started, this, [this] {
        auto r = platform_->attachChild(process_.processId());
        if (!r.ok) { failStart(r.error); return; }
        probe(generation_);
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setState(CoreState::Failed); deadline_.stop();
            auto done = std::move(startDone_); if (done) done(Result::fail("无法启动内核：" + process_.errorString()));
        }
    });
    connect(&process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        deadline_.stop(); stopDeadline_.stop(); api_->cancelAll();
        const bool expected = state_ == CoreState::Stopping;
        setState(expected ? CoreState::Stopped : CoreState::Failed);
        auto started = std::move(startDone_); if (started) started(Result::fail(QString("内核在就绪前退出 (%1)").arg(code)));
        auto stopped = std::move(stopDone_); if (stopped) stopped({});
        if (!expected) emit crashed(QString("内核异常退出 (%1, %2)").arg(code).arg(status == QProcess::CrashExit ? "crash" : "exit"));
    });
}
void CoreManager::setState(CoreState state) { state_ = state; emit stateChanged(state); }
CoreManager::~CoreManager() {
    process_.disconnect(this); tun_->disconnect(this);
    if (validation_) { validation_->disconnect(this); validation_->kill(); validation_->waitForFinished(1500); }
    if (process_.state()!=QProcess::NotRunning) { process_.kill(); process_.waitForFinished(2000); }
}
void CoreManager::start(const QString& executable, const QString& config, const QString& work, int mixedPort, int apiPort, const QString& secret, Completion done,bool tun) {
    if (process_.state() != QProcess::NotRunning||tun_->active()||state_==CoreState::Starting||state_==CoreState::Stopping) { done(Result::fail("内核仍在运行或停止中")); return; }
    if (!QFileInfo(executable).isFile()) { done(Result::fail("未找到 mihomo 内核，请在设置中指定文件")); return; }
    QTcpServer mixedReservation, apiReservation;
    mixedReservation.setProxy(QNetworkProxy::NoProxy); apiReservation.setProxy(QNetworkProxy::NoProxy);
    if (!mixedReservation.listen(QHostAddress::LocalHost, mixedPort)) { done(Result::fail(QString("Mixed 端口 %1 已被占用或无法监听").arg(mixedPort))); return; }
    if (!apiReservation.listen(QHostAddress::LocalHost, apiPort)) { done(Result::fail("管理端口已被占用，请重新激活以分配新端口")); return; }
    mixedReservation.close(); apiReservation.close();
    mixedPort_ = mixedPort; apiPort_ = apiPort; startDone_ = std::move(done); ++generation_; usingTun_=tun;
    probeError_.clear();
    api_->configure(apiPort, secret); setState(CoreState::Starting);
    if (usingTun_) {
        const auto generation=generation_; deadline_.start(20000);
        tun_->start(executable,work,[this,generation](Result result) { if (generation!=generation_||state_!=CoreState::Starting) return; if (!result.ok) { failStart(result.error); return; } probe(generation); }); return;
    }
    process_.setProgram(executable); process_.setArguments({"-d", work, "-f", config});
    process_.setWorkingDirectory(work); process_.setProcessEnvironment(cleanEnvironment());
    deadline_.start(12000); process_.start();
}
void CoreManager::probe(quint64 generation) {
    if (generation != generation_ || state_ != CoreState::Starting) return;
    const auto ownership = platform_->ownsTcpPort(usingTun_?tun_->apiProcessId():processId(), apiPort_);
    const auto proxyOwnership = platform_->ownsTcpPort(processId(), mixedPort_);
    if (!ownership.ok || !proxyOwnership.ok) { failStart(ownership.ok ? proxyOwnership.error : ownership.error); return; }
    if (!ownership.data.value("owned").toBool() || !proxyOwnership.data.value("owned").toBool()) {
        probeError_ = QString("监听端口身份尚未确认 (API %1 / Mixed %2)").arg(apiPort_).arg(mixedPort_);
        QTimer::singleShot(150, this, [this, generation] { probe(generation); }); return;
    }
    api_->version([this, generation](Result version) {
        if (generation != generation_ || state_ != CoreState::Starting) return;
        if (!version.ok || version.data.value("version").toString().isEmpty()) {
            probeError_ = version.ok ? "版本字段缺失" : version.error;
            QTimer::singleShot(150, this, [this, generation] { probe(generation); }); return;
        }
        api_->config([this, generation, version](Result config) {
            if (generation != generation_ || state_ != CoreState::Starting) return;
            if (!config.ok || config.data.value("mixed-port").toInt() != mixedPort_) {
                probeError_ = config.ok ? "Mixed 端口回读不一致" : config.error;
                QTimer::singleShot(150, this, [this, generation] { probe(generation); }); return;
            }
            if (usingTun_&&(!config.data.value("tun").toObject().value("enable").toBool()||!tun_->ready())) {
                probeError_="TUN 配置、虚拟接口或自动路由尚未确认";
                QTimer::singleShot(150,this,[this,generation] { probe(generation); }); return;
            }
            auto* socket = new QTcpSocket(this); socket->setProxy(QNetworkProxy::NoProxy);
            auto* timer = new QTimer(socket); timer->setSingleShot(true);
            const auto settled=std::make_shared<bool>(false);
            const auto complete=[this,socket,timer,generation,version,config,settled](bool connected,const QString& error) {
                if (*settled) return;
                *settled=true; timer->stop(); socket->disconnect(this); socket->abort(); socket->deleteLater();
                if (generation!=generation_||state_!=CoreState::Starting) return;
                if (!connected) { probeError_=error; QTimer::singleShot(150,this,[this,generation] { probe(generation); }); return; }
                deadline_.stop(); setState(CoreState::Running);
                Result result{true,{},config.data}; result.data["version"]=version.data.value("version");
                auto done=std::move(startDone_); if (done) done(result);
            };
            connect(timer,&QTimer::timeout,this,[complete] { complete(false,"代理入口连接超时"); });
            connect(socket,&QTcpSocket::connected,this,[complete] { complete(true,{}); });
            connect(socket,&QTcpSocket::errorOccurred,this,[complete,socket](QAbstractSocket::SocketError) { complete(false,"代理入口连接失败："+socket->errorString()); });
            timer->start(700); socket->connectToHost(QHostAddress::LocalHost,mixedPort_);
        });
    });
}
void CoreManager::failStart(const QString& error) {
    if (state_ != CoreState::Starting) return;
    deadline_.stop(); ++generation_;
    auto done = std::move(startDone_); setState(CoreState::Stopping);
    if (usingTun_) { tun_->stop([this,done=std::move(done),error](Result stopped) mutable { setState(stopped.ok?CoreState::Stopped:CoreState::Failed); if (done) done(Result::fail(error+(stopped.ok?QString():"；TUN 停止未确认："+stopped.error))); }); return; }
    process_.kill();
    if (done) done(Result::fail(error));
}
void CoreManager::stop(Completion done) {
    ++generation_; deadline_.stop(); api_->cancelAll();
    auto started = std::move(startDone_); if (started) started(Result::fail("启动已取消"));
    if (usingTun_) { setState(CoreState::Stopping); tun_->stop([this,done=std::move(done)](Result stopped) mutable { setState(stopped.ok?CoreState::Stopped:CoreState::Failed); if (stopped.ok) usingTun_=false; done(stopped); }); return; }
    if (process_.state() == QProcess::NotRunning) { setState(CoreState::Stopped); done({}); return; }
    if (stopDone_) { done(Result::fail("停止操作仍在进行")); return; }
    stopDone_ = std::move(done); setState(CoreState::Stopping); process_.terminate(); stopDeadline_.start(2000);
}
void CoreManager::cancelValidation() { if (validation_) { validation_->setProperty("cancelled", true); validation_->kill(); } }
void CoreManager::validate(const QString& executable, const QString& config, const QString& work, Completion done, int timeoutMs) {
    if (validation_) { done(Result::fail("已有配置校验任务")); return; }
    if (!QFileInfo(executable).isFile()) { done(Result::fail("未找到 mihomo，无法完成真实配置校验")); return; }
    auto* process = new QProcess(this); validation_ = process;
    process->setProcessEnvironment(cleanEnvironment()); process->setWorkingDirectory(work);
    process->setProgram(executable); process->setArguments({"-t", "-d", work, "-f", config});
    auto* timer = new QTimer(process); timer->setSingleShot(true);
    auto completion = std::make_shared<Completion>(std::move(done));
    connect(timer, &QTimer::timeout, process, [process] { process->setProperty("timedOut", true); process->kill(); });
    connect(process, &QProcess::errorOccurred, this, [this, process, completion](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        validation_ = nullptr; auto callback = std::move(*completion); if (callback) callback(Result::fail("配置校验程序无法启动")); process->deleteLater();
    });
    connect(process, &QProcess::finished, this, [this, process, completion](int code, QProcess::ExitStatus status) {
        validation_ = nullptr;
        const auto text = Diagnostics::redact(QString::fromUtf8(process->property("output").toByteArray() + process->readAllStandardOutput() + process->readAllStandardError()));
        auto callback = std::move(*completion);
        QString error = "内核配置校验失败：" + text;
        if (process->property("cancelled").toBool()) error = "配置校验已取消";
        else if (process->property("timedOut").toBool()) error = "配置校验超时；首次 Geo 数据下载可能未完成，请检查数据源连接后重试";
        if (callback) callback(code == 0 && status == QProcess::NormalExit ? Result{} : Result::fail(error));
        process->deleteLater();
    });
    const auto collect = [process] {
        auto bytes = process->property("output").toByteArray(); bytes += process->readAllStandardOutput(); bytes += process->readAllStandardError();
        if (bytes.size() > 64 * 1024) { bytes.truncate(64 * 1024); process->kill(); }
        process->setProperty("output", bytes);
    };
    connect(process, &QProcess::readyReadStandardOutput, process, collect);
    connect(process, &QProcess::readyReadStandardError, process, collect);
    timer->start(qMax(1, timeoutMs)); process->start();
}
}
