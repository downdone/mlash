#include "WireGuardSession.h"
#include "app/Async.h"
#include <QJsonDocument>

namespace mlash {
WireGuardSession::WireGuardSession(std::shared_ptr<PlatformServices> platform,QObject* parent,QString pipeName)
    : QObject(parent),platform_(std::move(platform)),pipeName_(std::move(pipeName)) {
    deadline_.setSingleShot(true);
    heartbeat_.setInterval(1200);
    connect(&deadline_,&QTimer::timeout,this,[this] { fail("WireGuard 服务通信超时"); });
    connect(&heartbeat_,&QTimer::timeout,this,[this] {
        if (deadline_.isActive()) return;
        write({{"op","status"}});
        deadline_.start(6500);
    });
    connect(&socket_,&QLocalSocket::readyRead,this,&WireGuardSession::readReplies);
    connect(&socket_,&QLocalSocket::disconnected,this,[this] {
        if (active_ || startDone_ || stopDone_) fail("WireGuard 服务连接已断开");
    });
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError) {
        if (active_ || startDone_ || stopDone_) fail("无法连接 WireGuard 服务");
    });
}

WireGuardSession::~WireGuardSession() {
    socket_.disconnect(this);
    socket_.abort();
    clearPayload();
}

void WireGuardSession::start(QByteArray source,const QStringList& endpoints,Done done) {
    if (active_ || lease_ || startDone_ || stopDone_) {
        source.fill('\0');
        done(Result::fail("已有 WireGuard 会话或清理尚未确认"));
        return;
    }
    if (source.isEmpty() || source.size()>128*1024 || endpoints.isEmpty() || endpoints.size()>32) {
        source.fill('\0');
        done(Result::fail("WireGuard 请求超出配置预算"));
        return;
    }
    pending_=QJsonDocument(QJsonObject{{"source",QString::fromLatin1(source.toBase64())},
                       {"endpoints",QJsonArray::fromStringList(endpoints)}}).toJson(QJsonDocument::Compact);
    source.fill('\0');
    startDone_=std::move(done);
    input_.clear();
    deadline_.start(24000);
    connectToService(false);
}

void WireGuardSession::stop(Done done, bool verifyIdle) {
    if (startDone_ || stopDone_) { done(Result::fail("WireGuard 正在变更")); return; }
    if (!active_ && !lease_ && !verifyIdle) { done({}); return; }
    heartbeat_.stop();
    stopDone_=std::move(done);
    deadline_.start(15000);
    if (socket_.state()==QLocalSocket::ConnectedState) write({{"op","stop"}});
    else connectToService(true);
}

void WireGuardSession::connectToService(bool cleanup) {
    const auto platform=platform_;
    const auto epoch=++epoch_;
    runTask(this,[platform] { return platform->tunServiceInfo(true); },[this,epoch,cleanup](Result info) {
        if (epoch!=epoch_) return;
        if (!info.ok) { fail(info.error); return; }
        if (!info.data.value("installed").toBool() || !info.data.value("running").toBool() ||
            !info.data.value("wireguard").toBool()) {
            fail("请在设置中安装或修复随包 TUN 服务，启用 WireGuard 支持");
            return;
        }
        openPipe(epoch,cleanup);
    });
}

void WireGuardSession::openPipe(quint64 epoch,bool cleanup) {
    const auto platform=platform_;
    const auto name=pipeName_;
    runTask(this,[platform,name] { return platform->openTunPipe(name); },[this,epoch,cleanup](TunPipeOpen pipe) {
        if (epoch!=epoch_) return;
        if (pipe.native) {
            if (!pipe.result.ok || !pipe.pipe) {
                QTimer::singleShot(200,this,[this,epoch,cleanup] { if (epoch==epoch_) openPipe(epoch,cleanup); });
                return;
            }
            if (!socket_.setSocketDescriptor(pipe.pipe->value,QLocalSocket::ConnectedState,QIODevice::ReadWrite)) {
                fail("WireGuard 服务句柄不可用");
                return;
            }
            pipe.pipe->take();
            sendInitial(cleanup);
            return;
        }
        disconnect(&socket_,&QLocalSocket::connected,this,nullptr);
        connect(&socket_,&QLocalSocket::connected,this,[this,epoch,cleanup] {
            if (epoch!=epoch_) return;
            const auto verified=platform_->verifyTunServer(socket_.socketDescriptor());
            if (!verified.ok) { fail(verified.error); return; }
            sendInitial(cleanup);
        });
        socket_.connectToServer(pipeName_,QIODevice::ReadWrite);
    });
}

void WireGuardSession::sendInitial(bool cleanup) {
    if (cleanup) { write({{"op","wireguard-idle"},{"protocol",2}}); return; }
    lease_=true; // A disconnect after submitting the request requires a readback.
    write({{"op","wireguard-start"},{"protocol",2},{"size",pending_.size()}});
    const auto written=socket_.write(pending_);
    const auto expected=pending_.size();
    clearPayload();
    if (written!=expected) fail("WireGuard 配置请求未完整提交");
}

void WireGuardSession::readReplies() {
    input_+=socket_.readAll();
    if (input_.size()>32768) { fail("WireGuard 服务响应超过预算"); return; }
    while (input_.contains('\n')) {
        const auto split=input_.indexOf('\n');
        const auto row=QJsonDocument::fromJson(input_.left(split)).object();
        input_.remove(0,split+1);
        if (row.value("protocol").toInt()!=2 || !row.value("ok").isBool()) { fail("WireGuard 服务协议不兼容"); return; }
        if (!row.value("ok").toBool()) {
            if (row.value("cleanup_confirmed").toBool()) lease_=false;
            fail(row.value("error").toString("WireGuard 请求失败").left(400));
            return;
        }
        const auto op=row.value("op").toString();
        if (stopDone_) {
            if (op=="wireguard-status") continue;
            if ((op!="wireguard-stopped"&&op!="wireguard-idle") || !row.value("cleanup_confirmed").toBool()) {
                if (op=="wireguard-idle"&&row.value("cleanup_confirmed").isBool()) lease_=true;
                fail("WireGuard 系统路由清理未确认");
                return;
            }
            finishStop({true,{},row});
            return;
        }
        const auto observed=row.value("observation").toObject();
        if ((op!="wireguard-started"&&op!="wireguard-status") || !row.value("routes").isArray() ||
            !observed.value("peers").isArray() || observed.value("adapter_luid").toString().isEmpty()) {
            fail("WireGuard 服务未返回可核验的接口状态");
            return;
        }
        observation_=row;
        active_=true;
        deadline_.stop();
        heartbeat_.start();
        auto done=std::exchange(startDone_,{});
        emit changed();
        if (done) done({true,{},row});
    }
}

void WireGuardSession::write(const QJsonObject& request) {
    socket_.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');
}

void WireGuardSession::finishStop(Result result) {
    ++epoch_;
    deadline_.stop(); heartbeat_.stop();
    active_=false; observation_={}; input_.clear(); clearPayload();
    auto done=std::exchange(stopDone_,{});
    if (result.ok) lease_=false;
    socket_.abort();
    emit changed();
    if (done) done(std::move(result));
}

void WireGuardSession::fail(const QString& reason) {
    const bool lostActive=active_ && !stopDone_;
    auto done=std::exchange(startDone_,{});
    finishStop(Result::fail(reason));
    if (done) done(Result::fail(reason));
    if (lostActive) emit lost(reason);
}

void WireGuardSession::clearPayload() { pending_.fill('\0'); pending_.clear(); }
}
