#pragma once
// CONTRACT owner=app layer=app
// invariant: 租约闸门只回答「是否拒绝用户动作」，不代替真实清理确认
// forbidden: 不得在此判断内核就绪、代理归属或节点健康；不得从此处触发清理
// lifecycle: 纯值对象，由控制器按当前观测构造；不持有连接或定时器
// tests: session_gate_tests
// spec: docs/development/feature-contracts.md
#include "domain/CleanupState.h"
#include <QString>

namespace mlash {

// 被租约影响的操作。枚举只收「用户可发起、可能被租约拒绝」的动作。
enum class SessionIntent {
    ProxyTakeover,          // 接管系统代理
    TunEnable,              // 开启 TUN
    TunServiceMaintenance,  // 安装、修复或移除 TUN 服务
    ProfileDelete,          // 删除配置
    ProfileActivation,      // 激活配置（仅当目标要求 TUN）
    TunnelConnect,          // 连接隧道
};

// 三类语义必须分开，不能压成一个 isBlocked()：
//   blocks()            → 拒绝用户动作，需要就近提示原因
//   skipsProxyRestore() → 静默跳过（恢复代理意图在租约期间不执行）
//   cleanupPending()    → 派生观测，供界面显示「待清理」
struct SessionGate {
    bool leaseHeld = false;     // 系统路由租约仍可能被占用
    bool tunnelActive = false;  // 隧道正在运行
    bool connecting = false;    // 正在建立隧道

    // 租约存在但隧道已停：清理结果尚未确认。
    // Confirmed/Failed 由服务端清理回执决定，这两个布尔观测无法推导，故此处只可能得到
    // None（无租约或隧道仍在跑）或 Pending（已停但未确认）。
    CleanupState cleanupState() const {
        if (!leaseHeld) return CleanupState::None;
        if (tunnelActive || connecting) return CleanupState::None;
        return CleanupState::Pending;
    }

    bool cleanupPending() const { return cleanupState() == CleanupState::Pending; }

    // 租约期间不恢复系统代理；这不是失败，调用方应静默继续。
    bool skipsProxyRestore() const { return leaseHeld; }

    bool blocks(SessionIntent intent, bool tunRequested = false) const;

    // 只在 blocks() 为真时非空；与判定同源，避免出现「没有拦截却有提示」。
    QString reason(SessionIntent intent, bool tunRequested = false) const;
};

inline bool SessionGate::blocks(SessionIntent intent, bool tunRequested) const {
    if (!leaseHeld) return false;
    if (intent == SessionIntent::ProfileActivation) return tunRequested;
    return true;
}

inline QString SessionGate::reason(SessionIntent intent, bool tunRequested) const {
    if (!blocks(intent, tunRequested)) return {};
    switch (intent) {
    case SessionIntent::ProxyTakeover:
        return QStringLiteral("WireGuard 并行期间请使用 Mihomo 本地代理");
    case SessionIntent::TunEnable:
        return QStringLiteral("WireGuard 持有系统路由，请先安全断开");
    case SessionIntent::TunServiceMaintenance:
        return QStringLiteral("请先断开 WireGuard 并确认路由清理");
    case SessionIntent::ProfileDelete:
        return QStringLiteral("先断开 WireGuard 并确认路由清理，再删除配置");
    case SessionIntent::ProfileActivation:
        return QStringLiteral("WireGuard 持有系统路由，请先安全断开");
    case SessionIntent::TunnelConnect:
        return QStringLiteral("请先断开当前隧道并确认清理");
    }
    return {};
}

} // namespace mlash