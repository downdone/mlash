#include "AppController.h"
#include "profile/ProfileStore.h"
#include "Async.h"
#include <QHostInfo>
#include <QJsonArray>

namespace mlash {
namespace {
struct WireGuardInput {
    Result result;
    QByteArray source;
    QStringList hosts,endpoints;
    QList<int> lookups;
    int remaining=0;
    bool complete=false;
};
}

QString AppController::wireGuardConnectBlock() const {
    if (!wireGuardStoreError_.isEmpty()) return wireGuardStoreError_;
    if (settings_.tunEnabled||state_.tunObserved) return "请先关闭 Mihomo TUN";
    if (state_.proxy==ProxyState::Owned||settings_.desiredProxy) return "请先关闭 Mlash 系统代理";
    return {};
}

SessionGate AppController::sessionGate() const {
    return {wireGuard_.leasePending(), wireGuard_.active(), wireGuardPhase_==QStringLiteral("connecting")};
}

QJsonObject AppController::wireGuardObservation() const {
    auto seen=wireGuard_.observation();
    seen["active"]=wireGuard_.active();
    seen["cleanup_pending"]=sessionGate().cleanupPending();
    seen["profile_id"]=runningWireGuardProfile_;
    seen["phase"]=wireGuardPhase_;
    seen["error"]=wireGuardError_;
    seen["sampled_at"]=wireGuardSample_;
    seen["connect_block"]=wireGuardConnectBlock();
    return seen;
}

void AppController::compareWireGuardVersions(const QString& id, Completion done) {
    const auto entry=std::find_if(wireGuardProfiles_.cbegin(),wireGuardProfiles_.cend(),
        [&](const auto& profile) { return profile.id==id; });
    if (entry==wireGuardProfiles_.cend()) { done(Result::fail("配置已不存在")); return; }
    const auto store=wireGuardStore_;
    runTask(this,[store,profile=*entry] {
        const auto baseline=profile.candidate==profile.active?profile.lastGood:profile.active;
        if (baseline.isEmpty()) return Result{};
        WireGuardConfig before, after;
        const auto oldResult=store->readVersion(profile.id,baseline,before);
        if (!oldResult.ok) return oldResult;
        const auto newResult=store->readVersion(profile.id,profile.candidate,after);
        if (!newResult.ok) return newResult;
        bool keysChanged=before.privateKey!=after.privateKey;
        bool peerSettingsChanged=before.peers.size()!=after.peers.size();
        for (qsizetype i=0; i<qMin(before.peers.size(),after.peers.size()); ++i) {
            const auto& old=before.peers[i];
            const auto& next=after.peers[i];
            keysChanged|=old.publicKey!=next.publicKey||old.presharedKey!=next.presharedKey;
            peerSettingsChanged|=old.endpointHost!=next.endpointHost||old.endpointPort!=next.endpointPort||old.keepalive!=next.keepalive;
        }
        return Result{true,{},{{"before",before.summary()},{"after",after.summary()},
            {"keys_changed",keysChanged},{"endpoints_changed",peerSettingsChanged},
            {"dns_changed",before.dns!=after.dns},{"interface_options_changed",before.mtu!=after.mtu||before.listenPort!=after.listenPort}}};
    },[done=std::move(done)](Result result) { done(std::move(result)); });
}

void AppController::finishWireGuard(Result result) {
    wireGuardPhase_.clear();
    wireGuardError_=result.ok?QString{}:result.error;
    const auto message=result.ok?result.data.value("notice").toString():result.error;
    if (!message.isEmpty()) diagnostics_.append("wireguard",message);
    emit wireGuardChanged();
    finish(result);
}

void AppController::connectWireGuard(const QString& id,const QString& expected) {
    auto entry=std::find_if(wireGuardProfiles_.cbegin(),wireGuardProfiles_.cend(),[&](const auto& profile) { return profile.id==id; });
    if (entry==wireGuardProfiles_.cend() ||
        (!expected.isEmpty() && entry->candidate!=expected && entry->active!=expected && entry->lastGood!=expected)) {
        emit operationFinished(false,"WireGuard 配置已改变，请重新选择");
        return;
    }
    const auto block=wireGuardConnectBlock();
    const auto gate=sessionGate();
    if (gate.blocks(SessionIntent::TunnelConnect)||!block.isEmpty()) {
        emit operationFinished(false,gate.blocks(SessionIntent::TunnelConnect)?gate.reason(SessionIntent::TunnelConnect):block);
        return;
    }
    if (!begin("正在连接 WireGuard")) return;
    wireGuardPhase_="connecting";
    wireGuardError_.clear();
    emit wireGuardChanged();
    const QString version=expected.isEmpty()?entry->candidate:expected;
    const auto store=wireGuardStore_;
    runTask(this,[store,id,version] {
        auto input=std::make_shared<WireGuardInput>();
        WireGuardConfig config;
        input->result=store->readVersion(id,version,config,&input->source);
        if (!input->result.ok) return input;
        if (input->result.data.value("full_v4").toBool()||input->result.data.value("full_v6").toBool()) {
            input->source.fill('\0');
            input->result=Result::fail("当前仅支持 WireGuard 分流配置");
            return input;
        }
        for (const auto& peer:config.peers) input->hosts.append(peer.endpointHost);
        return input;
    },[this,store,id,version](std::shared_ptr<WireGuardInput> input) {
        if (!input->result.ok) { finishWireGuard(input->result); return; }
        const auto resolved=[this,store,id,version,input](Result result) {
            if (input->complete) return;
            input->complete=true;
            for (int lookup:input->lookups) QHostInfo::abortHostLookup(lookup);
            input->lookups.clear();
            if (!result.ok||exitPending_) {
                input->source.fill('\0');
                finishWireGuard(result.ok?Result::fail("WireGuard 连接已取消"):result);
                return;
            }
            wireGuard_.start(std::move(input->source),input->endpoints,[this,store,id,version](Result started) {
                if (!started.ok) { finishWireGuard(started); return; }
                runTask(this,[store,id,version] { return store->markStarted(id,version); },[this,id](WireGuardCatalog saved) {
                    if (!saved.result.ok) {
                        wireGuard_.stop([this,error=saved.result.error](Result stopped) {
                            finishWireGuard(stopped.ok?Result::fail(error):Result::fail(error+"；"+stopped.error));
                        });
                        return;
                    }
                    wireGuardProfiles_=std::move(saved.profiles);
                    runningWireGuardProfile_=wireGuard_.active()?id:QString{};
                    emit profilesChanged(); emit wireGuardChanged();
                    finishWireGuard(wireGuard_.active()?Result{true,{},{{"notice","WireGuard 分流路由已生效"}}}
                                              :Result::fail("WireGuard 会话已结束，请确认清理"));
                });
            });
        };
        for (qsizetype i=0;i<input->hosts.size();++i) {
            const auto& host=input->hosts[i];
            const QHostAddress literal(host);
            input->endpoints.append(literal.isNull()?QString{}:literal.toString());
            if (!literal.isNull()) continue;
            ++input->remaining;
            const auto lookup=QHostInfo::lookupHost(host,this,[input,i,resolved](const QHostInfo& entry) {
                if (input->complete) return;
                const auto list=entry.addresses();
                const auto found=std::find_if(list.begin(),list.end(),[](const auto& ip) {
                    return !ip.isNull()&&!ip.isLinkLocal()&&!ip.isMulticast()&&ip.scopeId().isEmpty();
                });
                if (entry.error()!=QHostInfo::NoError||found==list.end()) {
                    resolved(Result::fail("WireGuard Endpoint 解析失败"));
                    return;
                }
                input->endpoints[i]=found->toString();
                if (--input->remaining==0) resolved({});
            });
            input->lookups.append(lookup);
        }
        if (!input->remaining) resolved({});
        else QTimer::singleShot(8000,this,[resolved] { resolved(Result::fail("WireGuard Endpoint 解析超时")); });
    });
}

void AppController::disconnectWireGuard(bool verifyIdle) {
    const auto gate=sessionGate();
    if (verifyIdle ? gate.tunnelActive||settings_.tunEnabled||state_.tunObserved : !gate.leaseHeld) return;
    if (verifyIdle&&smoke_) return;
    if (!begin("正在断开 WireGuard",false,true)) return;
    wireGuardPhase_="disconnecting";
    wireGuardError_.clear();
    emit wireGuardChanged();
    wireGuard_.stop([this](Result stopped) {
        if (!stopped.ok) { finishWireGuard(stopped); return; }
        finishWireGuard({true,{},{{"notice","WireGuard 路由已清理"}}});
    },verifyIdle);
}
}
