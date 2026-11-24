#pragma once
#include <QList>
#include <QMetaType>
#include <QString>

namespace mlash {

// 错误的恢复语义。码值说明"哪里错了"，类别说明"调用方该怎么做"。
// 新增码值必须同时登记类别与回归断言，否则按未分类处理。
enum class ErrorCategory : quint8 {
    Unspecified = 0, // 未登记：调用方只能展示文本
    Rejected,        // 输入或配置被拒绝：用户需要修改内容
    Retryable,       // 暂时性失败：可原样重试
    UserAction,      // 需要用户处理环境或确认
    Reload,          // 上下文已变化：需重新读取后再操作
    Internal,        // 程序缺陷或不可恢复状态
};

struct ErrorCodeEntry {
    const char* code;
    ErrorCategory category;
};

// 全部已登记的稳定码值。顺序为登记顺序，不保证字母序。
const QList<ErrorCodeEntry>& knownErrorCodes();

// 未登记的码值返回 Unspecified，不猜测类别。
ErrorCategory categorizeError(const QString& code);

// 稳定标识：用于日志与测试，不随界面文案变化。
QString errorCategoryName(ErrorCategory category);

} // namespace mlash

// 信号参数需要可排队传递；与 OperationId 的处理方式一致。
Q_DECLARE_METATYPE(mlash::ErrorCategory)