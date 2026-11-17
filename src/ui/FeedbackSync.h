#pragma once
#include "domain/Types.h"
#include <QObject>
#include <QStringList>

namespace mlash {

// 反馈路由（界面分层设计 P4.1 / ADR 0046）。
//
// 一次操作的结果要么属于某个页面/对象（页面级），要么属于整次会话（跨页级）。
// 之前所有结果都写进全局状态栏，页面内没有归属，`state.lastError` 还会把刚发生的
// 操作结果顶掉。这里把「归哪里」抽成可单测的纯逻辑，MainWindow 只负责渲染。
struct FeedbackTarget {
    enum Kind { StatusNotice, PageHint };
    Kind kind = StatusNotice;
    int page = -1;          // 页面级结果落到的视图 ID；-1 表示跨页
};

// 页面级：结果就是该页面的答案，写全局状态栏只会让它在别的页面看起来像是全局故障。
FeedbackTarget feedbackTarget(OperationId id);

// 跨页操作结果的状态栏文案。失败保留前缀，便于与「当前告警」段区分。
QString statusNoticeText(bool ok, const QString& message);

} // namespace mlash
