#include "ErrorCategory.h"

namespace mlash {
namespace {

// 26 个 ConfigBuilder 结构化码值 + YAML_SYNTAX + NODE_LINK 全部属于"输入被拒绝"。
// 运行时失败码值在此登记；新增码值必须同步回归断言。
const QList<ErrorCodeEntry> kCodes = {
    {"UNSUPPORTED_FIELD", ErrorCategory::Rejected},
    {"ADDITIONAL_LISTENER", ErrorCategory::Rejected},
    {"PROXY_SHAPE", ErrorCategory::Rejected},
    {"PROXY_PROTOCOL", ErrorCategory::Rejected},
    {"PROXY_EXTERNAL_FILE", ErrorCategory::Rejected},
    {"PROXY_CERTIFICATE_PATH", ErrorCategory::Rejected},
    {"PROXY_PROVIDER_FORMAT", ErrorCategory::Rejected},
    {"PROVIDER_SHAPE", ErrorCategory::Rejected},
    {"PROVIDER_FORMAT", ErrorCategory::Rejected},
    {"PROVIDER_URL", ErrorCategory::Rejected},
    {"PROVIDER_PROXY", ErrorCategory::Rejected},
    {"PROVIDER_LIMIT", ErrorCategory::Rejected},
    {"PROVIDER_LOCAL_FILE", ErrorCategory::Rejected},
    {"PROVIDER_HEADER", ErrorCategory::Rejected},
    {"RULE_SHAPE", ErrorCategory::Rejected},
    {"RULE_LIMIT", ErrorCategory::Rejected},
    {"CUSTOM_RULE", ErrorCategory::Rejected},
    {"RULE_OPTION", ErrorCategory::Rejected},
    {"RULE_SET_REFERENCE", ErrorCategory::Rejected},
    {"DISABLED_RULE", ErrorCategory::Rejected},
    {"DNS_SHAPE", ErrorCategory::Rejected},
    {"DNS_LISTENER", ErrorCategory::Rejected},
    {"DNS_OVERRIDE_SHAPE", ErrorCategory::Rejected},
    {"DNS_OVERRIDE_FIELD", ErrorCategory::Rejected},
    {"DNS_OVERRIDE_SIZE", ErrorCategory::Rejected},
    {"HALFCLOSE_SHAPE", ErrorCategory::Rejected},
    {"HALFCLOSE_FIELD", ErrorCategory::Rejected},
    {"HALFCLOSE_VALUE", ErrorCategory::Rejected},
    {"MRS_BEHAVIOR", ErrorCategory::Rejected},
    {"YAML_SYNTAX", ErrorCategory::Rejected},
    {"NODE_LINK", ErrorCategory::Rejected},
    {"DOWNLOAD_FAILED", ErrorCategory::Retryable},
    {"BUSY", ErrorCategory::Retryable},
    {"PORT_CONFLICT", ErrorCategory::UserAction},
    {"READ_ONLY", ErrorCategory::UserAction},
    {"CLEANUP_REQUIRED", ErrorCategory::UserAction},
    {"VERSION_CONFLICT", ErrorCategory::Reload},
};

} // namespace

const QList<ErrorCodeEntry>& knownErrorCodes() { return kCodes; }

ErrorCategory categorizeError(const QString& code) {
    for (const auto& entry : kCodes)
        if (code == QLatin1String(entry.code)) return entry.category;
    return ErrorCategory::Unspecified;
}

QString errorCategoryName(ErrorCategory category) {
    switch (category) {
    case ErrorCategory::Rejected: return QStringLiteral("rejected");
    case ErrorCategory::Retryable: return QStringLiteral("retryable");
    case ErrorCategory::UserAction: return QStringLiteral("user_action");
    case ErrorCategory::Reload: return QStringLiteral("reload");
    case ErrorCategory::Internal: return QStringLiteral("internal");
    case ErrorCategory::Unspecified: break;
    }
    return QStringLiteral("unspecified");
}

} // namespace mlash