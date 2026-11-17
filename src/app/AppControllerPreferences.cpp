#include "profile/YamlFields.h"
#include "profile/ProxyImpact.h"
#include "profile/VersionChanges.h"
#include "profile/RuleDraft.h"
#include "profile/ProviderEdits.h"
#include "profile/ProviderPolicy.h"
#include "AppController.h"
#include "Async.h"
#include "storage/Storage.h"
#include "profile/BackupArchive.h"
#include "profile/GeoData.h"
#include "core/TunPolicy.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QUrlQuery>
#include <utility>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QNetworkProxy>
#include <QSet>
#include <yaml-cpp/yaml.h>
#include <QRegularExpression>
#include <QNetworkReply>

namespace mlash {
void AppController::importGeoData(const QString& sourceDirectory) {
    if (!begin("正在导入本地 Geo 数据；当前内核运行版本保持不变")) return;
    const auto store=store_; runTask(this,[store,sourceDirectory] { return store->importGeoData(sourceDirectory); },[this](Result r) { finish(r); });
}
quint64 AppController::beginHotkeyCapture() {
    if (state_.busy||state_.exiting||state_.readOnly||hotkeysLoading_) return 0;
    hotkeyCaptureToken_=++hotkeyCaptureSerial_; emit hotkeyCaptureChanged(hotkeyCaptureToken_); return hotkeyCaptureToken_;
}
void AppController::endHotkeyCapture(quint64 token) { if (!token||token!=hotkeyCaptureToken_) return; hotkeyCaptureToken_=0; hotkeyQuietUntil_=QDeadlineTimer(300); emit hotkeyCaptureChanged(0); }
void AppController::dispatchHotkey(const QString& action) {
    if (state_.exiting) return;
    if (hotkeyCaptureToken_) { const auto sequence=activeHotkeys_.value(action).toString(); if (!sequence.isEmpty()) emit hotkeyCaptured(hotkeyCaptureToken_,sequence); return; }
    if (!hotkeyQuietUntil_.hasExpired()) return;
    if (action=="show-window"||action=="show-proxy"||action=="show-connections") { emit openPageRequested(action=="show-proxy"?2:action=="show-connections"?4:-1); return; }
    // Background key events must not complete another window's in-flight task.
    if (!activeHotkeys_.contains(action)||state_.busy||hotkeysLoading_) return;
    if (state_.readOnly) { emit operationFinished(false,"当前为只读保护，快捷键没有修改网络。"); return; }
    if (action=="toggle-system-proxy") {
        if (state_.proxy==ProxyState::Owned) { setSystemProxy(false); return; }
        if (state_.proxy!=ProxyState::Disabled) { emit openPageRequested(0); emit operationFinished(false,"系统代理不由 Mlash 管理，请在主页确认接管；快捷键没有覆盖现有代理。"); return; }
        if (!state_.ready) { emit operationFinished(false,"内核未就绪，快捷键没有修改系统代理。"); return; } setSystemProxy(true,false); return;
    }
    if (action=="toggle-tun") { if (!state_.ready||settings_.tunEnabled!=state_.tunObserved||state_.activeProfile.isEmpty()) { emit openPageRequested(6); emit operationFinished(false,"请先启动运行订阅并在设置确认 TUN 状态；快捷键不安装或修复服务。"); return; } setTunEnabled(!state_.tunObserved); return; }
    if (!state_.ready) { emit operationFinished(false,"内核尚未就绪，网络快捷键没有执行。"); return; }
    if (action=="cycle-mode") { const QStringList modes{"rule","global","direct"}; const int index=modes.indexOf(state_.mode); if (index<0) { emit operationFinished(false,"当前代理模式未知，请先刷新运行状态。"); return; } setMode(modes[(index+1)%modes.size()]); }
    else if (action=="reload-active") reloadActive();
}
void AppController::reloadActive() {
    const int i=profileIndex(settings_.activeProfile); if (!state_.ready||core_.state()!=CoreState::Running||i<0||profiles_[i].active.isEmpty()) { emit operationFinished(false,"没有就绪的当前运行版，未重载或应用候选。"); return; }
    const bool restoreProxy=state_.proxy==ProxyState::Owned; if (!begin("正在重载当前运行版，独立候选保持不变")) return; proxyRestoreAllowed_=restoreProxy; prepareActivation(profiles_[i],profiles_[i].active,settings_);
}
void AppController::updateHotkeys(const QJsonObject& bindings,const QJsonValue& expected) {
    if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=settings_.hotkeys)) { emit operationFinished(false,"快捷键已在其他窗口修改，请重新打开；当前草稿保留。"); return; }

    QList<HotkeyBinding> parsed; const auto valid=parseHotkeys(bindings,parsed); if (!valid.ok) { emit operationFinished(false,valid.error); return; }
    if (hotkeysLoading_) { emit operationFinished(false,"启动快捷键注册尚未结束"); return; }
    if (!beginPreferences("正在注册全局快捷键")) return;
    const auto previous=activeHotkeys_; const auto next=valid.data.value("bindings").toObject();
    hotkeys_->configure(next,[this,previous,next](Result registered) {
        activeHotkeys_=registered.data.value("bindings").toObject(); hotkeyError_=registered.ok?QString():registered.error;
        if (!registered.ok) { finish(registered); return; }
        auto settings=settings_; settings.hotkeys=next;
        saveSnapshot(settings,profiles_,[this,previous](Result saved) { if (saved.ok) { finish(saved); return; } hotkeys_->configure(previous,[this,saved](Result restored) { activeHotkeys_=restored.data.value("bindings").toObject(); hotkeyError_=restored.ok?QString():restored.error; finish(restored.ok?saved:Result::fail(saved.error+"；"+restored.error)); }); });
    });
}
void AppController::updateConnectionView(const QJsonObject& preferences) {
    const OperationScope operation(this, OperationId::SettingsApply);
    const auto valid=validateConnectionView(preferences); if (!valid.ok) { emit operationFinished(false,valid.error); return; }
    if (state_.exiting||state_.readOnly) return;
    if (state_.busy) { pendingConnectionView_=preferences; return; }
    pendingConnectionView_.reset();
    if (preferences==settings_.connectionView) return;
    if (!beginPreferences("正在保存连接视图")) return;
    auto next=settings_; next.connectionView=preferences; saveSnapshot(next,profiles_,[this](Result result) { finish(result); });
}
void AppController::updateProxyBypass(const QString& text,const std::optional<QString>& expected) {
    const OperationScope operation(this, OperationId::SettingsApply);
    if (expected&&*expected!=settings_.proxyBypass) { emit operationFinished(false,"排除列表已在其他窗口修改，请重新打开；草稿保留"); return; }
    const auto checked=normalizeProxyBypass(text); if (!checked.ok) { emit operationFinished(false,checked.error); return; } const auto bypass=checked.data.value("bypass").toString(); if (bypass==settings_.proxyBypass) { emit operationFinished(true,"排除列表未变化"); return; } if (!begin("正在更新系统代理排除列表")) return;
    const auto proxy=proxy_; const auto previous=settings_.proxyBypass; const int port=settings_.mixedPort;
    runTask(this,[proxy,port,bypass,previous] { auto observed=proxy->inspect(); if (!observed.ok) return observed; const bool owned=observed.data.value("state").toString()=="owned"; if (owned) { observed=proxy->acquire(port,false,bypass); if (!observed.ok) { const auto rollback=proxy->acquire(port,false,previous); observed.error+=rollback.ok?"；旧排除目标已恢复":"；保留恢复日志："+rollback.error; observed.data=rollback.data; return observed; } } observed.data["updated_os"]=owned&&observed.ok; return observed; },[this,proxy,previous,port,bypass](Result applied) {
        applyProxyObservation(applied); if (!applied.ok) { finish(applied); return; } auto next=settings_; next.proxyBypass=bypass;
        saveSnapshot(next,profiles_,[this,proxy,previous,port,applied](Result saved) { if (saved.ok||!applied.data.value("updated_os").toBool()) { finish(saved); return; } runTask(this,[proxy,previous,port] { return proxy->acquire(port,false,previous); },[this,saved](Result rollback) { applyProxyObservation(rollback); finish(Result::fail(saved.error+(rollback.ok?"；旧代理排除列表已恢复":"；恢复失败，保留日志："+rollback.error))); }); });
    });
}
void AppController::updateAppearance(const QString& appearance,int scale,const QString& windowChrome) {
    const OperationScope operation(this, OperationId::SettingsApply);
    const auto chrome=windowChrome.isEmpty()?settings_.windowChrome:windowChrome;
    if (!validAppearance(appearance,scale) || !QStringList{"native","integrated"}.contains(chrome)) { emit operationFinished(false,"外观、文字比例或窗口栏选项无效"); return; }
    if (!beginPreferences("正在保存外观设置")) return;
    auto next=settings_; next.appearance=appearance; next.textScale=scale; next.windowChrome=chrome;
    saveSnapshot(next,profiles_,[this](Result result) { finish(result); });
}
void AppController::drainConnectionView() { if (state_.busy||state_.exiting||state_.readOnly||!pendingConnectionView_) return; const auto preferences=*pendingConnectionView_; pendingConnectionView_.reset(); updateConnectionView(preferences); if (!state_.busy) runMaintenance(); }
bool AppController::beginPreferences(const QString& operation) { if (state_.busy||state_.exiting||state_.readOnly) return false; preferencesOnly_=true; backgroundOperation_=false; transactionOperation_=operationId_; state_.busy=true; state_.lastError.clear(); publish(); emit operationStarted(operation, transactionOperation_); return true; }
void AppController::startIpProbe(bool coreRoute) {
    startOutboundProbe(coreRoute,false,{});
}
void AppController::startServiceProbe(bool coreRoute,const QString& id) {
    startOutboundProbe(coreRoute,true,id);
}
void AppController::startOutboundProbe(bool coreRoute,bool services,const QString& id) {
    if (state_.busy||state_.exiting||outboundDiagnostics_.busy()||(coreRoute&&!state_.ready)) return;
    const auto token=outboundDiagnostics_.prepare({{"kind",services?"services":"ip"},{"route",coreRoute?"core":"direct"},{"mode",coreRoute?state_.mode:QString()},{"mixed_port",coreRoute?settings_.mixedPort:0},{"core_pid",coreRoute?double(core_.processId()):0.0},{"chain","未观测；不能由当前选中节点推断"}});
    if (!coreRoute) { if (services) outboundDiagnostics_.runServices(token,0,id); else outboundDiagnostics_.runIp(token,0); return; }
    const auto platform=platform_; const auto pid=core_.processId(); const auto port=settings_.mixedPort; const auto generation=state_.generation;
    runTask(this,[platform,pid,port] { return platform->ownsTcpPort(pid,port); },[this,token,port,generation,services,id](Result result) {
        if (generation!=state_.generation||state_.busy||state_.exiting||!state_.ready) { outboundDiagnostics_.fail(token,"运行状态已变化，请重试"); return; }
        if (!result.ok||!result.data.value("owned").toBool()) { outboundDiagnostics_.fail(token,"本地代理端口不属于当前受控内核，未发送请求"); return; }
        if (services) outboundDiagnostics_.runServices(token,port,id); else outboundDiagnostics_.runIp(token,port);
    });
}
void AppController::inspectGeoData(Completion done) {
    if (geoPreviewPending_||state_.exiting) { done(Result::fail("Geo数据正在读取")); return; }
    geoPreviewPending_=true; QString work; const int index=profileIndex(state_.activeProfile);
    if (state_.ready&&index>=0) work=store_->versionRoot(state_.activeProfile,profiles_[index].active)+"/work";
    const auto generation=state_.generation;
    runTask(this,[root=root_,work] { return GeoData::inspect(root,work); },[this,generation,done=std::move(done)](Result result) mutable { geoPreviewPending_=false; done(generation==state_.generation&&!state_.exiting?result:Result::fail("运行版本已变化，请刷新Geo列表")); });
}
void AppController::updateGeoData(const QString& name) {
    const auto url=GeoData::source(name); if (url.isEmpty()) { emit operationFinished(false,"不支持的Geo在线数据类型"); return; }
    if (!begin("正在下载Geo默认快照；当前运行版本保持不变")) return;
    DownloadPolicy policy; policy.timeoutSeconds=120;
    downloader_->download(url,[this,name](Result downloaded,QByteArray bytes) {
        if (!downloaded.ok) { finish(downloaded); return; }
        const auto store=store_; runTask(this,[store,name,bytes=std::move(bytes)] { return GeoData::stage(*store,name,bytes); },[this,store](Result staged) {
            if (!staged.ok) { finish(staged); return; }
            const auto path=staged.data.value("stage").toString();
            if (state_.exiting) { runTask(this,[store,path] { GeoData::discard(store->root(),path); return Result::fail("Geo更新已取消"); },[this](Result result) { finish(result); }); return; }
            core_.validate(settings_.corePath,path+"/config.yaml",path,[this,store,staged,path](Result validation) {
                if (!validation.ok || state_.exiting) { runTask(this,[store,path,validation] { GeoData::discard(store->root(),path); return validation.ok?Result::fail("Geo更新已取消"):validation; },[this](Result result) { finish(result); }); return; }
                runTask(this,[store,staged] { return GeoData::commit(*store,staged); },[this](Result result) { finish(result); });
            },15000);
        });
    },policy,0,128*1024*1024);
}
void AppController::previewActiveConfig(std::function<void(Result,QString)> done) {
    if (previewPending_ || state_.busy || state_.exiting) { done(Result::fail("当前操作尚未结束"),{}); return; }
    const int i=profileIndex(settings_.activeProfile); if (i<0 || !isId(profiles_[i].active)) { done(Result::fail("没有当前激活配置"),{}); return; }
    const auto path=store_->versionRoot(profiles_[i].id,profiles_[i].active)+"/work/config.yaml"; const auto secrets=diagnostics_.snapshot().secrets; const auto generation=state_.generation;
    previewPending_=true;
    runTask(this,[path,secrets] { QByteArray bytes; auto result=readFile(path,bytes); if (!result.ok) return Result::fail("当前配置不可读取");
        try { auto yaml=YAML::Load(bytes.toStdString()); int visited=0; const QRegularExpression sensitive("password|passwd|passphrase|secret|token|uuid|username|private[-_]?key|preshared[-_]?key|authorization|authentication|cookie|headers|^psk$|^key$",QRegularExpression::CaseInsensitiveOption);
            std::function<void(YAML::Node,int)> redact=[&](YAML::Node node,int depth) { if (depth>32 || ++visited>200000) throw std::runtime_error("preview limit"); if (node.IsMap()) for (const auto& entry:node) { const auto key=QString::fromStdString(entry.first.as<std::string>()); if (sensitive.match(key).hasMatch()) node[entry.first]=std::string("[redacted]"); else redact(entry.second,depth+1); } else if (node.IsSequence()) for (const auto& child:node) redact(child,depth+1); };
            redact(yaml,0); const auto text=Diagnostics::sanitize(QString::fromStdString(YAML::Dump(yaml)),secrets); result.data["text"]=text.left(524288)+(text.size()>524288?"\n# 预览超过限制，后续内容省略":""); return result;
        } catch (const std::exception&) { return Result::fail("配置格式不可安全预览"); }
    },[this,generation,done=std::move(done)](Result result) mutable { previewPending_=false; if (generation!=state_.generation) { done(Result::fail("运行配置已变化，请重新查看"),{}); return; } done(result,result.data.value("text").toString()); });
}
}
