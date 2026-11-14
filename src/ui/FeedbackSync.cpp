#include "FeedbackSync.h"

namespace mlash {

// 归属表就是 ADR 0046 的「反馈路由」一行一条。没列出的（Unspecified 与跨页任务）
// 走状态栏「最近操作」段。
FeedbackTarget feedbackTarget(OperationId id) {
    switch (id) {
    case OperationId::ProfileImport:
    case OperationId::ProfileUpdate:
    case OperationId::ProfileActivate:
    case OperationId::ProfileBatch:
        return {FeedbackTarget::PageHint, 1};
    case OperationId::ModeSwitch:
    case OperationId::MemberSelect:
    case OperationId::ProxyToggle:
    case OperationId::TunToggle:
        // 首页控制区的三个开关/选择：结果回填到控制区提示行，不进状态栏。
        return {FeedbackTarget::PageHint, 0};
    case OperationId::ConnectionClose:
    case OperationId::ConnectionCloseAll:
        return {FeedbackTarget::PageHint, 4};
    case OperationId::SettingsApply:
        return {FeedbackTarget::PageHint, 6};
    case OperationId::RulesSave:
        return {FeedbackTarget::PageHint, 3};
    case OperationId::CoreStart:
    case OperationId::CoreStop:
    case OperationId::CoreUpdate:
    case OperationId::Backup:
    case OperationId::Restore:
    case OperationId::Unspecified:
        break;
    }
    return {FeedbackTarget::StatusNotice, -1};
}

QString statusNoticeText(bool ok, const QString& message) {
    return ok ? message : QStringLiteral("操作未完成 · ") + message;
}

} // namespace mlash
