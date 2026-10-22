#include "TunSession.h"
#include "TunPolicy.h"
#include "app/Async.h"
#include "storage/Storage.h"
#include <QCryptographicHash>
#include <QJsonDocument>

namespace mlash {
TunSession::~TunSession() { socket_.disconnect(this); socket_.abort(); }
TunSession::TunSession(std::shared_ptr<PlatformServices> platform,QObject* parent,QString pipeName):QObject(parent),platform_(std::move(platform)),pipeName_(std::move(pipeName)) {
    deadline_.setSingleShot(true); heartbeat_.setInterval(1000);
    connect(&deadline_,&QTimer::timeout,this,[this] { fail("TUN 服务通信超时"); });
    connect(&heartbeat_,&QTimer::timeout,this,[this] { send({{"op","status"}}); if (!deadline_.isActive()) deadline_.start(4500); });
    connect(&socket_,&QLocalSocket::readyRead,this,[this] {
        input_+=socket_.readAll();
        if (input_.size()>16384) { fail("TUN 服务响应超过预算"); return; }
        while (input_.contains('\n')) {
            const auto row=input_.left(input_.indexOf('\n')); input_.remove(0,row.size()+1); QJsonParseError error;
            const auto doc=QJsonDocument::fromJson(row,&error); const auto r=doc.object();
            if (error.error!=QJsonParseError::NoError||!doc.isObject()||r.value("protocol").toInt()!=1||!r.value("ok").isBool()) { fail("TUN 服务协议不兼容"); return; }
            if (!r.value("ok").toBool()) { fail(r.value("error").toString("TUN 服务请求失败").left(500)); return; }
            if (stopping_) { if (r.value("op")!="stopped"&&r.value("op")!="idle") continue; if (!r.value("cleanup_confirmed").toBool()) { fail("TUN 路由清理未确认"); return; } deadline_.stop(); heartbeat_.stop(); leaseMayExist_=false; active_=false; pid_=0; observed_={}; socket_.abort(); auto done=std::move(stopDone_); stopping_=false; idleProbe_=false; if (done) done({true,{},r}); return; }
            deadline_.stop();
            const auto pid=qint64(r.value("pid").toDouble()); const auto apiPid=qint64(r.value("api_pid").toDouble()); if (pid<1||apiPid<1||!r.value("adapter_up").isBool()||!r.value("routes").isBool()) { fail("TUN 服务未返回有效内核或接口身份"); return; }
            const bool wasReady=ready();
            pid_=pid; observed_=r; active_=true; heartbeat_.start();
            if (wasReady&&!ready()) { fail("TUN 接口或自动路由已经离开就绪状态"); return; }
            auto done=std::move(startDone_); if (done) done({true,{},r});
        }
    });
    connect(&socket_,&QLocalSocket::disconnected,this,[this] { if (active_||startDone_) fail("TUN 服务连接已断开，等待确认会话清理"); });
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError) {
        if (stopping_&&idleProbe_&&stopDone_) { const auto generation=generation_; QTimer::singleShot(200,this,[this,generation] { if (generation==generation_&&stopping_&&idleProbe_) connectPipe(true); }); return; }
        if (active_||startDone_) fail("无法连接 TUN 服务，请检查服务安装和所属账户");
    });
}
void TunSession::send(const QJsonObject& request) { socket_.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n'); }
void TunSession::fail(const QString& error) {
    ++generation_; deadline_.stop(); heartbeat_.stop(); const bool had=active_; active_=false; pid_=0; observed_={};
    auto started=std::move(startDone_); auto stopped=std::move(stopDone_); socket_.abort(); if (started) started(Result::fail(error)); if (stopped) stopped(Result::fail(error)); if (had&&!stopping_) emit lost(error); stopping_=false;
}
void TunSession::start(const QString& executable,const QString& work,Completion done) {
    if (active_||startDone_||stopDone_||leaseMayExist_) { done(Result::fail("已有 TUN 服务会话或清理尚未确认")); return; }
    stopping_=false; input_.clear(); startDone_=std::move(done); const auto generation=++generation_; const auto platform=platform_;
    deadline_.start(12000);
    runTask(this,[platform,executable] {
        QByteArray bytes; auto r=readFile(executable,bytes,128*1024*1024);
        if (!r.ok||QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()!=TunCoreSha256) return Result::fail("TUN 服务仅支持随包锁定版本的 mihomo；请先恢复随包内核");
        return platform->tunServiceInfo(true);
    },[this,generation,work](Result r) {
        if (generation!=generation_) return;
        if (!r.ok||!r.data.value("installed").toBool()||!r.data.value("running").toBool()) { fail(r.ok?QString("请先在设置中安装 TUN 服务"):r.error); return; }
        socket_.setProperty("work",work); connectPipe(false);
    });
}
void TunSession::connectPipe(bool idleProbe) {
    idleProbe_=idleProbe; const auto generation=generation_;
    const auto platform=platform_; const auto name=pipeName_;
    runTask(this,[platform,name] { return platform->openTunPipe(name); },[this,generation,idleProbe](TunPipeOpen opened) {
    if (generation!=generation_) return;
    if (opened.native) {
        if (!opened.result.ok||!opened.pipe) {
            if (idleProbe&&stopDone_) { QTimer::singleShot(200,this,[this,generation] { if (generation==generation_&&stopping_&&idleProbe_) connectPipe(true); }); return; }
            fail(opened.result.ok?QString("TUN 服务管道未就绪"):opened.result.error); return;
        }
        const auto handle=opened.pipe->value;
        if (!socket_.setSocketDescriptor(handle,QLocalSocket::ConnectedState,QIODevice::ReadWrite)) { fail("无法接收 TUN 服务管道句柄"); return; }
        opened.pipe->take();
        if (idleProbe) send({{"op","idle"},{"protocol",1}});
        else { leaseMayExist_=true; send({{"op","start"},{"protocol",1},{"work",socket_.property("work").toString()}}); }
        return;
    }
    disconnect(&socket_,&QLocalSocket::connected,this,nullptr);
    auto connection=std::make_shared<QMetaObject::Connection>();
    *connection=connect(&socket_,&QLocalSocket::connected,this,[this,generation,idleProbe,connection] {
        disconnect(*connection); if (generation!=generation_) return;
        const auto verified=platform_->verifyTunServer(socket_.socketDescriptor()); if (!verified.ok) { fail(verified.error); return; }
        if (idleProbe) send({{"op","idle"},{"protocol",1}});
        else { leaseMayExist_=true; send({{"op","start"},{"protocol",1},{"work",socket_.property("work").toString()}}); }
    });
    socket_.connectToServer(pipeName_,QIODevice::ReadWrite);
    });
}
void TunSession::stop(Completion done) {
    ++generation_; auto started=std::move(startDone_); if (started) started(Result::fail("TUN 启动已取消"));
    if (stopDone_) { done(Result::fail("TUN 正在停止")); return; }
    deadline_.stop(); heartbeat_.stop(); stopping_=true;
    if (socket_.state()!=QLocalSocket::ConnectedState) {
        socket_.abort(); input_.clear(); active_=false; pid_=0; observed_={};
        if (leaseMayExist_) {
            stopDone_=std::move(done); deadline_.start(12000); const auto generation=generation_; const auto platform=platform_;
            runTask(this,[platform] { return platform->tunServiceInfo(true); },[this,generation](Result info) { if (generation!=generation_||!stopping_) return; if (!info.ok||!info.data.value("installed").toBool()) { fail(info.ok?QString("TUN 服务缺失，无法确认清理；请修复服务后重试"):info.error); return; } connectPipe(true); }); return;
        }
        stopping_=false; done({}); return;
    }
    stopDone_=std::move(done); send({{"op","stop"}}); deadline_.start(10000);
}
void TunSession::confirmCleanup(Completion done) {
    if (active_||startDone_||stopDone_) { done(Result::fail("请先结束当前 TUN 会话操作")); return; }
    leaseMayExist_=true; stop(std::move(done));
}
}
