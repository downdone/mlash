#pragma once
#include <QString>

namespace mlash {

// 系统网络资源（TUN 接口、路由、DNS 租约）的清理状态。
// 会话端口与控制器只据此决定能否开始新的接管，不再各自解释布尔值。
// 来源：TunSession::observation() 的 cleanup_pending，以及 stop/confirmCleanup 的结果。
enum class CleanupState : quint8 {
    None = 0,       // 没有租约，可直接开始
    Pending = 1,    // 存在租约但未确认清理，必须先确认
    Confirmed = 2,  // 清理已确认，可开始
    Failed = 3,     // 清理失败，需人工处理
    Active = 4,     // 租约由正在运行的隧道持有，不能开始第二个会话
};

constexpr bool canStartSession(CleanupState state) noexcept {
    return state == CleanupState::None || state == CleanupState::Confirmed;
}

// 稳定标识：用于日志、诊断导出与测试断言，不随界面文案变化。
inline QString cleanupStateName(CleanupState state) {
    switch (state) {
    case CleanupState::None: return QStringLiteral("none");
    case CleanupState::Pending: return QStringLiteral("pending");
    case CleanupState::Confirmed: return QStringLiteral("confirmed");
    case CleanupState::Failed: return QStringLiteral("failed");
    case CleanupState::Active: return QStringLiteral("active");
    }
    return QStringLiteral("unknown");
}

} // namespace mlash