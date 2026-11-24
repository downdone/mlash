#pragma once
#include "ConnectionColumns.h"
#include "Types.h"
#include <QJsonObject>
#include <QVector>

namespace mlash {

struct ConnectionColumnPref {
    int id = -1;
    int width = 100;
    bool visible = true;
    bool operator==(const ConnectionColumnPref&) const = default;
};

// 连接页的搜索与列偏好。持久化字段 connection_view 的强类型形式。
// fromJson 只解析并记录结构性错误（未知字段、类型不符），valid() 做完整校验；
// 未知键在解析时记为错误而不是静默丢弃，避免旧索引被悄悄改写。
struct ConnectionViewPrefs {
    bool caseSensitive = false;
    bool wholeWord = false;
    bool regex = false;
    QVector<ConnectionColumnPref> columns;
    QString parseError; // fromJson 记录的结构性错误；不写入 toJson

    Result valid() const;
    QJsonObject toJson() const;
    static ConnectionViewPrefs fromJson(const QJsonObject& object);
};

} // namespace mlash