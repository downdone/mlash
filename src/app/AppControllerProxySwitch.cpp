#include "AppController.h"
#include "Async.h"

namespace mlash {

// 代理总开关（ADR 0048）。存在的理由：UI 层无法在 begin() 的忙碌互斥下编排
// startCore → setSystemProxy → setTunEnabled，也无法在失败时回到「点击前快照」。
// 本命令在 Controller 内用一次事务包住整个流程，失败走既有回滚路径。
//
// 语义：
//  - 开启 = 让当前设置允许的接管方式生效。settings.tunEnabled 为真时 TUN 是接管手段，
//    系统代理保持独立设置不被强开；否则接管系统代理。
//  - 关闭 = 撤销接管；内核继续运行（停内核是技术动作，不是本开关的含义）。
//  - 失败 = 回到点击前快照：desiredProxy / tunEnabled 复原，系统代理由协调器
//    在「当前完整快照 == 我们最后写入的快照」时恢复（AGENTS.md 的恢复前提）。
void AppController::setProxyEnabled(bool enabled, bool allowExternal) {
    if (enabled&&wireGuard_.leasePending()) { emit operationFinished(false,"WireGuard 并行期间请使用 Mihomo 本地代理"); return; }
    const OperationScope operation(this, OperationId::ProxyToggle);
    if (state_.exiting) { refuseOperation("正在退出"); return; }
    const int index = profileIndex(settings_.activeProfile);
    const bool tunOn = state_.tunObserved || settings_.tunEnabled;
    const bool running = core_.state() == CoreState::Running;
    if (enabled && index < 0) { refuseOperation("请先导入并激活一个订阅，再开启代理"); return; }
    if (!enabled && !tunOn && state_.proxy != ProxyState::Owned) { reportOperation(true, "代理已关闭"); return; }

    if (!begin(enabled ? "正在开启代理" : "正在关闭代理")) return;
    proxyRestoreAllowed_ = true;

    // TUN 的开关只有一次完整 activation 能表达（core 启动参数与路由都在那里）。
    // 因此只要涉及 TUN，就复用 prepareActivation，让既有 release → stop → start → resume
    // 与 failActivation → restorePrevious 承担顺序和回滚。
    if (tunOn && index >= 0) {
        auto next = settings_;
        next.tunEnabled = enabled ? next.tunEnabled : false;
        next.desiredProxy = enabled;
        prepareActivation(profiles_[index], profiles_[index].active.isEmpty() ? profiles_[index].candidate : profiles_[index].active, next);
        return;
    }

    if (enabled) {
        auto next = settings_;
        next.desiredProxy = true;
        if (!running || !state_.ready) {
            prepareActivation(profiles_[index], profiles_[index].active.isEmpty() ? profiles_[index].candidate : profiles_[index].active, next);
            return;
        }
        const auto proxy = proxy_;
        const int port = settings_.mixedPort;
        const auto bypass = settings_.proxyBypass;
        const auto before = settings_.desiredProxy;
        runTask(this, [proxy, port, allowExternal, bypass] { return proxy->acquire(port, allowExternal, bypass); },
            [this, before, next](Result acquired) {
                applyProxyObservation(acquired);
                if (!acquired.ok) {
                    auto restored = settings_; restored.desiredProxy = before;
                    saveSnapshot(restored, profiles_, [this, acquired](Result saved) {
                        finish(saved.ok ? acquired : Result::fail(acquired.error + "；接管意图未能复原"));
                    });
                    return;
                }
                saveSnapshot(next, profiles_, [this, acquired](Result saved) { finish(saved.ok ? acquired : saved); });
            });
        return;
    }

    // 关闭且不涉及 TUN：只释放系统代理并清除接管意图，内核保持运行。
    const auto proxy = proxy_;
    runTask(this, [proxy] { return proxy->release(); }, [this](Result released) {
        applyProxyObservation(released);
        if (!released.ok) { finish(released); return; }
        auto next = settings_; next.desiredProxy = false;
        saveSnapshot(next, profiles_, [this, released](Result saved) { finish(saved.ok ? released : saved); });
    });
}

} // namespace mlash
