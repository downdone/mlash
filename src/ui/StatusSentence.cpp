#include "StatusSentence.h"

namespace mlash {

namespace {
QString coreText(CoreState core, bool ready) {
    switch (core) {
    case CoreState::Stopped: return QStringLiteral("内核已停止");
    case CoreState::Starting: return QStringLiteral("内核启动中");
    case CoreState::Stopping: return QStringLiteral("内核停止中");
    case CoreState::Failed: return QStringLiteral("内核启动或运行失败");
    case CoreState::Running: return ready ? QStringLiteral("内核运行中") : QStringLiteral("正在确认内核");
    }
    return QStringLiteral("内核状态未知");
}

// 接管只报观测：系统代理优先，其次 TUN。未接管与内核停止必须分开表达（ADR 0048 六态）。
QString takeoverText(const StatusFacts& facts) {
    if (facts.proxy == ProxyState::Owned) return QStringLiteral("系统代理已接管");
    if (facts.tunObserved) return QStringLiteral("TUN 已接管");
    if (facts.proxy == ProxyState::External) return QStringLiteral("系统代理由其他程序管理");
    if (facts.proxy == ProxyState::Unknown) return QStringLiteral("系统代理状态未知");
    return QStringLiteral("未接管流量");
}

// tunObserved 目前是意图的影子，因此「请求开启但未观测到」只能说「未确认」，不能声称已开启。
QString tunText(const StatusFacts& facts) {
    if (facts.tunObserved) return QStringLiteral("TUN 已开启");
    if (facts.tunRequested) return QStringLiteral("TUN 未确认");
    return QStringLiteral("TUN 未开启");
}

QString accentName(const StatusFacts& facts) {
    if (facts.core == CoreState::Failed) return QStringLiteral("error");
    if (facts.core == CoreState::Starting || facts.core == CoreState::Stopping || facts.proxy == ProxyState::Unknown) return QStringLiteral("warning");
    if (facts.proxy == ProxyState::External || (facts.tunRequested && !facts.tunObserved)) return QStringLiteral("warning");
    if (facts.ready && (facts.proxy == ProxyState::Owned || facts.tunObserved)) return QStringLiteral("success");
    return QStringLiteral("secondary");
}
} // namespace

StatusFacts statusFacts(const AppState& state, const Settings& settings, const QString& profileName) {
    StatusFacts facts;
    facts.core = state.core;
    facts.ready = state.ready;
    facts.proxy = state.proxy;
    facts.tunObserved = state.tunObserved;
    facts.tunRequested = settings.tunEnabled;
    facts.profileName = profileName;
    return facts;
}

StatusSentence statusSentence(const StatusFacts& facts) {
    StatusSentence sentence;
    sentence.line1 = coreText(facts.core, facts.ready) + QStringLiteral(" · ") + takeoverText(facts);
    sentence.line2 = tunText(facts) + QStringLiteral(" · ")
        + (facts.profileName.isEmpty() ? QStringLiteral("未选择订阅") : facts.profileName);
    sentence.accent = accentName(facts);
    return sentence;
}

} // namespace mlash
