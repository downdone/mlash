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
void AppController::startCore(bool allowProxyRestore) {
    const OperationScope operation(this, OperationId::CoreStart);
    if (core_.state() == CoreState::Running) return;
    const int i = profileIndex(settings_.activeProfile);
    if (i < 0) { emit operationFinished(false, "先导入并激活一个订阅"); return; }
    if (!begin("正在启动内核")) return;
    restartAttempts_ = 0;
    proxyRestoreAllowed_ = allowProxyRestore;
    prepareActivation(profiles_[i], profiles_[i].active.isEmpty() ? profiles_[i].candidate : profiles_[i].active, settings_);
}
void AppController::resumeDesiredProxy(Completion done) {
    if (!settings_.desiredProxy || !state_.ready || !proxyRestoreAllowed_ || wireGuard_.leasePending()) { done({}); return; }
    const auto proxy = proxy_; const int port = settings_.mixedPort; const auto bypass=settings_.proxyBypass;
    runTask(this, [proxy, port,bypass] { return proxy->acquire(port,false,bypass); }, [this, done = std::move(done)](Result r) mutable { applyProxyObservation(r); publish(); done(r); });
}
void AppController::verifyProfileOverrides(const Profile& profile,int expectedCount,Completion done) {
    verifyRuleOverlay(profile.customRules,profile.appendRules,expectedCount,[this,edits=profile.proxyEdits,done=std::move(done)](Result rules) mutable {
        if (!rules.ok||edits.isEmpty()) { done(rules); return; }
        api_.proxyGroups([edits,done=std::move(done)](CoreObservation<CoreProxyGroups> observed) mutable {
            if (!observed.ok()) { done(observed.result); return; } const auto proxies=observed.value.groups;
            for (const auto* key:{"remove_nodes","remove_groups"}) for (const auto& name:edits.value(key).toArray()) if (proxies.contains(name.toString())) { done(Result::fail("内核仍包含已移除的节点或组，恢复旧配置")); return; }
            for (const auto* key:{"nodes","groups"}) { const auto entries=edits.value(key).toObject(); for (auto entry=entries.begin();entry!=entries.end();++entry) if (!proxies.contains(entry.key())||(QString(key)=="groups"&&!proxies.value(entry.key()).toObject().value("all").isArray())) { done(Result::fail("内核节点或策略组回读缺失，恢复旧配置")); return; } }
            done({});
        });
    });
}
void AppController::verifyRuleOverlay(const QStringList& prepend, const QStringList& append, int expectedCount, Completion done) {
    if (prepend.isEmpty() && append.isEmpty() && expectedCount < 0) { done({}); return; }
    api_.rules( [prepend, append, expectedCount, done = std::move(done)](Result r) mutable {
        if (!r.ok) { done(r); return; }
        const auto observed = r.data.value("rules").toArray();
        if (observed.size() < prepend.size() + append.size() || (expectedCount >= 0 && observed.size() != expectedCount)) { done(Result::fail("内核规则回读数量不一致")); return; }
        const auto matches = [&](const QStringList& rules, int offset) {
            for (int i=0;i<rules.size();++i) { const auto parts=rules[i].split(','); auto expected=parts[0]; expected.remove('-'); const auto row=observed[i+offset].toObject(); auto type=row.value("type").toString(); type.remove('-'); if (expected.compare(type,Qt::CaseInsensitive)!=0 || row.value("proxy").toString()!=parts.value(parts[0]=="MATCH"?1:2).trimmed()) return false; }
            return true;
        };
        if (!matches(prepend,0) || !matches(append,observed.size()-append.size())) { done(Result::fail("内核前置/追加规则回读不一致")); return; }
        done({});
    });
}
void AppController::stopCore() {
    const OperationScope operation(this, OperationId::CoreStop);
    if (!begin("正在安全停止内核")) return;
    const auto proxy = proxy_;
    runTask(this, [proxy] { return proxy->release(); }, [this](Result released) {
        applyProxyObservation(released);
        if (!released.ok && !released.data.value("conflict").toBool()) { finish(released); return; }
        core_.stop([this, released](Result r) {
            proxies_ = {}; emit proxiesChanged();
            auto settings = settings_; settings.desiredProxy = false;
            saveSnapshot(settings, profiles_, [this, r, released](Result saved) { finish(!saved.ok ? saved : !r.ok ? r : released); });
        });
    });
}
void AppController::setTunEnabled(bool enabled) { updateTunOptions(enabled,settings_.tunStack,settings_.tunStrictRoute); }
void AppController::inspectTunService(Completion done) {
    const auto platform=platform_; const auto path=settings_.corePath;
    runTask(this,[platform,path] { auto result=platform->tunServiceInfo(false); if (result.ok) { QByteArray bytes; result.data["client_core_matches"]=readFile(path,bytes,128*1024*1024).ok&&QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()==TunCoreSha256; } return result; },[this,done=std::move(done)](Result r) mutable { if (r.ok) r.data["session"]=core_.tunObservation(); done(r); });
}
void AppController::inspectNetworkInterfaces(Completion done) { const auto platform=platform_; runTask(this,[platform] { return platform->networkInterfaces(); },[done=std::move(done)](Result result) mutable { done(result); }); }
void AppController::repairTunService() {
    if (wireGuard_.leasePending()) { emit operationFinished(false,"请先断开 WireGuard 并确认路由清理"); return; }
    if (smoke_||state_.tunObserved||settings_.tunEnabled||state_.core==CoreState::Failed) { emit operationFinished(false,"请先关闭 TUN 并确认清理，再修复服务；冒烟模式不执行安装"); return; }
    if (!begin("正在修复 TUN 服务，请确认 Windows 管理员授权")) return;
    const auto platform=platform_; const auto helper=QCoreApplication::applicationDirPath()+"/MlashTunService.exe";
    runTask(this,[platform,helper] { return platform->repairTunService(helper); },[this](Result result) { finish(result); });
}
void AppController::retryTunCleanup() {
    if (smoke_||state_.ready||state_.core==CoreState::Starting||state_.core==CoreState::Stopping) { emit operationFinished(false,"请先停止内核，再确认 TUN 清理；冒烟模式不操作真实服务"); return; }
    if (!begin("正在确认旧 TUN 会话和路由清理")) return;
    core_.confirmTunCleanup([this](Result stopped) { if (!stopped.ok) { finish(stopped); return; } const auto proxy=proxy_; runTask(this,[proxy] { return proxy->release(); },[this](Result released) { applyProxyObservation(released); if (!released.ok) { finish(released); return; } auto next=settings_; next.tunEnabled=false; next.desiredProxy=false; saveSnapshot(next,profiles_,[this](Result saved) { finish(saved); }); }); });
}
void AppController::manageTunService(bool install) {
    if (wireGuard_.leasePending()) { emit operationFinished(false,"请先断开 WireGuard 并确认路由清理"); return; }
    if (smoke_||state_.tunObserved||settings_.tunEnabled||state_.core==CoreState::Failed) { emit operationFinished(false,"请先关闭 TUN 并确认内核已停止，再安装或移除服务；隔离冒烟不执行系统安装"); return; }
    if (!begin(install?"正在安装 TUN 服务，请确认 Windows 管理员授权":"正在移除 TUN 服务，请确认 Windows 管理员授权")) return;
    const auto platform=platform_; const auto helper=QCoreApplication::applicationDirPath()+"/MlashTunService.exe";
    runTask(this,[platform,install,helper] { return platform->manageTunService(install,helper); },[this](Result r) { finish(r); });
}
void AppController::updateTunOptions(bool enabled,const QString& stack,bool strictRoute) {
    const OperationScope operation(this, OperationId::TunToggle);
    auto next=settings_; next.tunEnabled=enabled; next.tunStack=stack; next.tunStrictRoute=strictRoute; updateTunSettings(next);
}
void AppController::updateTunSettings(const Settings& options,const QJsonValue& expected) {
    if (options.tunEnabled&&wireGuard_.leasePending()) { emit operationFinished(false,"WireGuard 持有系统路由，请先安全断开"); return; }
    if (!expected.isUndefined()&&(!expected.isObject()||expected.toObject()!=tunOptionsJson(settings_))) { emit operationFinished(false,"TUN 设置已在其他窗口修改，请重新打开；当前草稿保留"); return; }
    auto next=settings_; next.tunEnabled=options.tunEnabled; next.tunStack=options.tunStack; next.tunStrictRoute=options.tunStrictRoute; next.tunExcludeRoutes=options.tunExcludeRoutes; next.tunMtu=options.tunMtu; next.tunDnsHijack=options.tunDnsHijack; const bool enabled=next.tunEnabled;
    if (!validTunOptions(next)) { emit operationFinished(false,"TUN 协议栈选项无效"); return; }
    if (!enabled&&!settings_.tunEnabled&&!state_.tunObserved&&core_.state()!=CoreState::Failed) {
        if (!beginPreferences("正在保存 TUN 选项")) return;
        saveSnapshot(next,profiles_,[this](Result saved) { finish(saved); }); return;
    }
    const int i=profileIndex(settings_.activeProfile);
    if (enabled&&i<0) { emit operationFinished(false,"请先激活一个订阅，再开启 TUN"); return; }
    if (!begin(enabled?"正在校验并启用 TUN":"正在关闭 TUN 并恢复普通代理")) return;
    const auto platform=platform_; const bool running=core_.state()==CoreState::Running;
    runTask(this,[platform,next] {
        if (!next.tunEnabled) return Result{};
        const auto info=platform->tunServiceInfo(false); if (!info.ok) return info; if (!info.data.value("installed").toBool()) return Result::fail("请先在设置的 TUN 服务中完成安装");
        if (info.data.contains("core_matches")&&!info.data.value("core_matches").toBool()) return Result::fail("已安装服务的内核不匹配，请关闭 TUN 后修复服务");
        if ((!next.tunExcludeRoutes.isEmpty()||next.tunMtu!=1500||!next.tunDnsHijack)&&!info.data.value("advanced_options").toBool()) return Result::fail("已安装服务版本尚不支持高级选项，请关闭 TUN 后修复服务");
        QByteArray bytes; if (!readFile(next.corePath,bytes,128*1024*1024).ok||QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()!=TunCoreSha256) return Result::fail("TUN 服务仅支持随包锁定版本的 mihomo，请恢复随包内核"); return Result{};
    },[this,next,i,running](Result r) {
        if (!r.ok) { finish(r); return; }
        if (exitPending_) { finish(Result::fail("TUN 修改已取消")); return; }
        if (!running&&!next.tunEnabled) { core_.stop([this,next](Result stopped) { if (!stopped.ok) { finish(stopped); return; } saveSnapshot(next,profiles_,[this](Result saved) { finish(saved); }); }); return; }
        if (i<0||i>=profiles_.size()) { finish(Result::fail("活动订阅不存在")); return; }
        prepareActivation(profiles_[i],profiles_[i].active.isEmpty()?profiles_[i].candidate:profiles_[i].active,next);
    });
}
void AppController::setSystemProxy(bool enabled, bool allowExternal) {
    if (enabled&&wireGuard_.leasePending()) { emit operationFinished(false,"WireGuard 并行期间请使用 Mihomo 本地代理"); return; }
    if (enabled && (!state_.ready || core_.state() != CoreState::Running)) { emit operationFinished(false, "内核尚未就绪，系统代理保持不变"); return; }
    if (!begin(enabled ? "正在接管系统代理" : "正在恢复系统代理")) return;
    proxyRestoreAllowed_ = true;
    const auto proxy = proxy_; const int port = settings_.mixedPort; const auto bypass=settings_.proxyBypass;
    runTask(this, [proxy, port, enabled, allowExternal,bypass] { return enabled ? proxy->acquire(port, allowExternal,bypass) : proxy->release(); }, [this, enabled](Result r) {
        applyProxyObservation(r);
        if (!r.ok) { finish(r); return; }
        auto settings = settings_; settings.desiredProxy = enabled;
        saveSnapshot(settings, profiles_, [this, r](Result saved) { finish(saved.ok ? r : saved); });
    });
}
void AppController::setMode(const QString& mode) {
    const OperationScope operation(this, OperationId::ModeSwitch);
    if (!QStringList{"rule","global","direct"}.contains(mode) || !state_.ready) { emit operationFinished(false, "内核未就绪或模式不合法"); return; }
    if (!begin("正在切换模式")) return;
    api_.setMode(mode, [this, mode](Result written) {
        api_.configInfo([this, mode, written](CoreObservation<CoreConfigInfo> observed) {
            if (observed.ok()) state_.mode = observed.value.mode;
            finish(observed.ok() && state_.mode == mode ? Result{} : !observed.ok() ? observed.result : !written.ok ? written : Result::fail("模式回读不一致"));
        });
    });
}
void AppController::selectMember(const QString& group, const QString& member) {
    const OperationScope operation(this, OperationId::MemberSelect);
    if (!state_.ready) { emit operationFinished(false, "内核未就绪"); return; }
    const auto selected = proxies_.value(group).toObject();
    if (!QStringList{"Selector","URLTest","Fallback"}.contains(selected.value("type").toString())) { emit operationFinished(false, "此策略组不支持手动选择"); return; }
    if (!selected.value("all").toArray().contains(member)) { emit operationFinished(false, "组成员不可用"); return; }
    if (!begin("正在切换策略组成员")) return;
    const auto apply=[this,group,member](QStringList affected,int skipped) {
    const auto complete=[this,affected,skipped](Result stored) { if (!stored.ok) { finish(stored); return; } closeAffectedConnections(affected,skipped,[this](Result result) { finish(result); }); };
    api_.select(group, member, [this, group, member,complete](Result written) {
        api_.proxy(group, [this, group, member, written,complete](Result observed) {
            if (observed.ok) { proxies_[group] = observed.data; emit proxiesChanged(); }
            if (!observed.ok || observed.data.value("now").toString() != member) { finish(!observed.ok ? observed : !written.ok ? written : Result::fail("节点选择回读不一致")); return; }
            const int i = profileIndex(settings_.activeProfile);
            if (i < 0 || !profiles_[i].rememberSelections) { complete({}); return; }
            auto next = profiles_; next[i].selections[group] = member;
            if (group.size() <= 512 && member.size() <= 512) {
                QJsonArray recent{QJsonObject{{"group",group},{"member",member}}};
                for (const auto& value : next[i].recentSelections) if (value.toObject().value("group").toString() != group || value.toObject().value("member").toString() != member) { if (recent.size() < 32) recent.append(value); }
                next[i].recentSelections = recent;
            }
            saveSnapshot(settings_, next, complete);
        });
    });
    };
    if (!nodePreferences().value("disconnect_affected").toBool() || selected.value("now").toString()==member) { apply({},0); return; }
    api_.connections([this,group,apply](Result result) {
        if (!result.ok||(!result.data.value("connections").isArray()&&!result.data.value("connections").isNull())) { finish(result.ok?Result::fail("未能确认切换前的连接，未切换节点"):result); return; }
        QStringList ids; int skipped=0; QSet<QString> seen;
        for (const auto& value:result.data.value("connections").toArray()) { const auto row=value.toObject(); const auto chains=row.value("chains").toArray(); const auto id=row.value("id").toString(); if (!chains.contains(group)||!isId(id)||seen.contains(id)) continue; seen.insert(id); if (ids.size()<32) ids.append(id); else ++skipped; }
        apply(ids,skipped);
    },1500);
}
void AppController::reconcileSelections(Profile profile, std::function<void(Result, Profile)> done) {
    if (!profile.rememberSelections || profile.selections.isEmpty()) { done({}, profile); return; }
    api_.proxyGroups([this, profile, done = std::move(done)](CoreObservation<CoreProxyGroups> observed) mutable {
        if (!observed.ok()) { done(observed.result, profile); return; }
        restoreSelectionList(profile, observed.value.groups, profile.selections.keys(), std::move(done));
    });
}
void AppController::closeAffectedConnections(const QStringList& ids,int skipped,Completion done) {
    if (ids.isEmpty()) { done(skipped?Result{true,{},{{"notice",QString("节点已切换；%1 条连接保持").arg(skipped)}}}:Result{}); return; }
    struct Batch { QStringList queue; int inFlight=0; }; auto state=std::make_shared<Batch>(); state->queue=ids; auto pump=std::make_shared<std::function<void()>>();
    const std::weak_ptr<std::function<void()>> weak=pump;
    *pump=[this,state,weak,ids,skipped,done] {
        const auto continuation=weak.lock(); if (!continuation) return;
        if (state->queue.isEmpty()&&state->inFlight==0) {
            const auto finalize=[this,ids,skipped,done] {
            api_.connections([this,ids,skipped,done](Result result) {
                if (!result.ok || (!result.data.value("connections").isArray()&&!result.data.value("connections").isNull())) { done({true,{},{{"notice","节点已切换；受影响连接的关闭结果未能确认"}}}); return; }
                int remaining=0; for (const auto& value:result.data.value("connections").toArray()) if (ids.contains(value.toObject().value("id").toString())) ++remaining;
                if (connectionsVisible_) { acceptConnectionSnapshot(result.data); emit connectionsChanged(); }
                done({true,{},{{"notice",QString("节点已切换；已断开 %1 条受影响连接，%2 条保持").arg(ids.size()-remaining).arg(remaining+skipped)}}});
            },1500); };
            finalize(); return;
        }
        while (state->inFlight<3&&!state->queue.isEmpty()) { const auto id=state->queue.takeFirst(); ++state->inFlight; api_.closeConnection(id,[state,continuation](Result) { --state->inFlight; (*continuation)(); },1500); }
    }; (*pump)();
}
void AppController::restoreSelectionList(Profile profile, QJsonObject proxies, QStringList groups, std::function<void(Result, Profile)> done) {
    if (groups.isEmpty()) { done({}, profile); return; }
    const auto group = groups.takeFirst(); const auto member = profile.selections.value(group).toString(); const auto current = proxies.value(group).toObject();
    if (!current.value("all").toArray().contains(member) || !QStringList{"Selector","URLTest","Fallback"}.contains(current.value("type").toString())) {
        profile.selections.remove(group); diagnostics_.append("app", "原选中成员在新配置中不可用，采用内核默认选择");
        restoreSelectionList(profile, proxies, groups, std::move(done)); return;
    }
    api_.select(group, member, [this, profile, proxies, groups, group, member, done = std::move(done)](Result written) mutable {
        api_.proxy(group, [this, profile, proxies, groups, member, written, done = std::move(done)](Result observed) mutable {
            if (!observed.ok || observed.data.value("now").toString() != member) { done(!observed.ok ? observed : !written.ok ? written : Result::fail("保存的节点选择回读不一致"), profile); return; }
            restoreSelectionList(profile, proxies, groups, std::move(done));
        });
    });
}
void AppController::clearAutomaticSelection(const QString& group) {
    const auto type = proxies_.value(group).toObject().value("type").toString();
    if (!state_.ready || !QStringList{"URLTest","Fallback"}.contains(type)) { emit operationFinished(false, "仅自动策略组可取消手动固定"); return; }
    if (!begin("正在恢复自动选择")) return;
    api_.releaseAutomaticGroup(group, [this, group](Result written) {
        api_.proxy(group, [this, group, written](Result observed) {
            if (!observed.ok || !observed.data.value("fixed").toString().isEmpty()) { finish(!observed.ok ? observed : !written.ok ? written : Result::fail("自动组仍处于固定状态")); return; }
            proxies_[group] = observed.data; emit proxiesChanged();
            const int i = profileIndex(settings_.activeProfile); if (i < 0) { finish({}); return; }
            auto next = profiles_; next[i].selections.remove(group); saveSnapshot(settings_, next, [this](Result r) { finish(r); });
        });
    });
}
void AppController::refresh() {
    if (!state_.ready || state_.exiting) return;
    const auto generation = state_.generation;
    api_.proxyGroups([this, generation](CoreObservation<CoreProxyGroups> observed) { if (generation == state_.generation && observed.ok()) { proxies_ = observed.value.groups; emit proxiesChanged(); } });
}
void AppController::updateSettings(const Settings& desired) {
    const OperationScope operation(this, OperationId::SettingsApply);
    if (!validAppearance(desired.appearance,desired.textScale) || !QStringList{"native","integrated"}.contains(desired.windowChrome)) { refuseOperation("外观、文字比例或窗口栏选项无效"); return; }
    auto withoutAppearance=desired; withoutAppearance.appearance=settings_.appearance; withoutAppearance.textScale=settings_.textScale; withoutAppearance.windowChrome=settings_.windowChrome;
    if (withoutAppearance.toJson()==settings_.toJson()&&(desired.appearance!=settings_.appearance||desired.textScale!=settings_.textScale||desired.windowChrome!=settings_.windowChrome)) { updateAppearance(desired.appearance,desired.textScale,desired.windowChrome); return; }
    if (desired.startupPage<0 || desired.startupPage>7 || !QStringList{"show","proxy-page","toggle-proxy"}.contains(desired.trayClick) ||
        !QStringList{"source","enabled","disabled"}.contains(desired.ipv6Mode) || !QStringList{"source","enabled","disabled"}.contains(desired.unifiedDelayMode)) { refuseOperation("设置选项不合法"); return; }
    if (!validTestUrl(desired.testUrl)) { refuseOperation("连通测试地址须为不含用户凭据/片段的 HTTP/HTTPS URL"); return; }
    if (desired.mixedPort < 1024 || desired.mixedPort > 65535 || !QStringList{"debug","info","warning","error","silent"}.contains(desired.logLevel)) {
        refuseOperation("端口或日志级别不合法"); return;
    }
    if (!begin("正在应用设置")) return;
    protectMajorChange([this,desired] {
    const bool reconfigure = core_.state() == CoreState::Running &&
        (desired.mixedPort != settings_.mixedPort || desired.logLevel != settings_.logLevel || desired.corePath != settings_.corePath || desired.ipv6Mode!=settings_.ipv6Mode || desired.unifiedDelayMode!=settings_.unifiedDelayMode);
    auto preferences = settings_; preferences.autoStart = desired.autoStart; preferences.silentStart = desired.silentStart;
    preferences.startCore = desired.startCore; preferences.restoreProxy = desired.restoreProxy; preferences.lightweight = desired.lightweight;
    preferences.testUrl = desired.testUrl;
    preferences.appearance=desired.appearance; preferences.textScale=desired.textScale; preferences.windowChrome=desired.windowChrome;
    preferences.startupPage=desired.startupPage; preferences.trayClick=desired.trayClick;
    const auto platform = platform_; const auto executable = QCoreApplication::applicationFilePath();
    const bool autoChanged = preferences.autoStart != settings_.autoStart;
    runTask(this, [platform, executable, autoChanged, preferences] {
        auto written = autoChanged ? platform->autoStart(preferences.autoStart, executable) : Result{};
        const auto observed = platform->readAutoStart(executable);
        if (autoChanged && written.ok && (!observed.ok || observed.data.value("enabled").toBool() != preferences.autoStart)) written = Result::fail("自启状态回读失败或不一致");
        written.data["auto_start_known"] = observed.ok; written.data["auto_start_enabled"] = observed.data.value("enabled").toBool(); written.data["auto_start_conflict"] = observed.data.value("conflict").toBool();
        return written;
    }, [this, desired, preferences, reconfigure](Result r) mutable {
            state_.autoStartKnown = r.data.value("auto_start_known").toBool(); state_.autoStartEnabled = r.data.value("auto_start_enabled").toBool(); state_.autoStartConflict = r.data.value("auto_start_conflict").toBool();
            if (state_.autoStartKnown) { settings_.autoStart = state_.autoStartEnabled; preferences.autoStart = state_.autoStartEnabled; }
            emit profilesChanged(); publish();
            if (!r.ok) { finish(r); return; }
            if (!reconfigure) { preferences.mixedPort = desired.mixedPort; preferences.logLevel = desired.logLevel; preferences.corePath = desired.corePath; preferences.ipv6Mode=desired.ipv6Mode; preferences.unifiedDelayMode=desired.unifiedDelayMode; }
            saveSnapshot(preferences, profiles_, [this, desired, reconfigure](Result saved) {
                if (!saved.ok || !reconfigure) { finish(saved); return; }
                auto runtime = settings_; runtime.mixedPort = desired.mixedPort; runtime.logLevel = desired.logLevel; runtime.corePath = desired.corePath; runtime.ipv6Mode=desired.ipv6Mode; runtime.unifiedDelayMode=desired.unifiedDelayMode;
                const int i = profileIndex(settings_.activeProfile);
                if (i < 0) { finish(Result::fail("活动订阅不存在")); return; }
                prepareActivation(profiles_[i], profiles_[i].active, runtime);
            });
        });
    });
}
}
