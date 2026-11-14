#pragma once
#include "app/Types.h"
#include <QString>

namespace mlash {

// 常驻状态句的唯一实现：侧栏状态条与托盘 tooltip 都从这里取文案（分层设计 P1/P2）。
// 纯函数，只读 state 与 settings，不缓存、不判断可见性。
struct StatusSentence {
    QString line1;   // 内核 + 接管
    QString line2;   // TUN + 当前订阅
    QString accent;  // 状态点颜色角色名："success" / "warning" / "error" / "secondary"
};

// 拼装所需事实。profileName 为空表示未选择订阅。
struct StatusFacts {
    CoreState core = CoreState::Stopped;
    bool ready = false;
    ProxyState proxy = ProxyState::Disabled;
    bool tunObserved = false;
    bool tunRequested = false;
    QString profileName;
};

StatusSentence statusSentence(const StatusFacts& facts);
StatusFacts statusFacts(const AppState& state, const Settings& settings, const QString& profileName);

} // namespace mlash
