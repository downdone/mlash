#include "ConnectionViewPrefs.h"
#include <QJsonArray>
#include <QSet>

namespace mlash {
namespace {

constexpr const char* kUnknownField = "连接视图包含未知字段";
constexpr const char* kSearchType = "连接搜索选项类型无效";
constexpr const char* kColumnShape = "连接列配置须包含完整的七列旧版或十五列";
constexpr const char* kColumnValue = "连接列宽、顺序或可见性无效";

bool validColumnCount(int count) {
    return count == ConnectionColumns::LegacyCount || count == ConnectionColumns::Count;
}

// 单列校验：id 在范围内且唯一、宽度 24..1000、目标列必须可见。
bool validColumn(const ConnectionColumnPref& column, int count, QSet<int>& seen) {
    if (column.id < 0 || column.id >= count || seen.contains(column.id)) return false;
    if (column.width < 24 || column.width > 1000) return false;
    if (column.id == ConnectionColumns::Target && !column.visible) return false;
    seen.insert(column.id);
    return true;
}

} // namespace

ConnectionViewPrefs ConnectionViewPrefs::fromJson(const QJsonObject& object) {
    ConnectionViewPrefs preferences;
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!QStringList{"case_sensitive", "whole_word", "regex", "columns"}.contains(it.key())) {
            preferences.parseError = QString::fromUtf8(kUnknownField);
            return preferences;
        }
    }
    for (const auto* key : {"case_sensitive", "whole_word", "regex"})
        if (object.contains(key) && !object.value(key).isBool()) {
            preferences.parseError = QString::fromUtf8(kSearchType);
            return preferences;
        }
    preferences.caseSensitive = object.value("case_sensitive").toBool();
    preferences.wholeWord = object.value("whole_word").toBool();
    preferences.regex = object.value("regex").toBool();

    if (!object.contains("columns")) return preferences;
    const auto columns = object.value("columns");
    // 与旧校验等价：columns 出现时必须是完整的七列或十五列；空数组视为非法。
    // 未自定义列的合法表示是"没有这个键"，toJson 也据此省略。
    if (!columns.isArray() || !validColumnCount(columns.toArray().size())) {
        preferences.parseError = QString::fromUtf8(kColumnShape);
        return preferences;
    }
    QSet<int> seen;
    for (const auto& value : columns.toArray()) {
        const auto row = value.toObject();
        const auto id = row.value("id").toInt(-1);
        const auto width = row.value("width").toInt(-1);
        const bool integral = row.value("id").isDouble() && row.value("id").toDouble() == id &&
                              row.value("width").isDouble() && row.value("width").toDouble() == width;
        if (!value.isObject() || row.size() != 3 || !integral || !row.value("visible").isBool()) {
            preferences.parseError = QString::fromUtf8(kColumnValue);
            return preferences;
        }
        ConnectionColumnPref column{id, width, row.value("visible").toBool()};
        if (!validColumn(column, columns.toArray().size(), seen)) {
            preferences.parseError = QString::fromUtf8(kColumnValue);
            return preferences;
        }
        preferences.columns.append(column);
    }
    return preferences;
}

Result ConnectionViewPrefs::valid() const {
    if (!parseError.isEmpty()) return Result::fail(parseError);
    if (columns.isEmpty()) return {};
    if (!validColumnCount(columns.size())) return Result::fail(kColumnShape);
    QSet<int> seen;
    for (const auto& column : columns)
        if (!validColumn(column, columns.size(), seen)) return Result::fail(kColumnValue);
    return {};
}

QJsonObject ConnectionViewPrefs::toJson() const {
    QJsonArray rows;
    for (const auto& column : columns)
        rows.append(QJsonObject{{"id", column.id}, {"width", column.width}, {"visible", column.visible}});
    // 未自定义列时不写 columns：空数组与缺键等价，但缺键更贴近"未自定义"的本意。
    if (columns.isEmpty())
        return {{"case_sensitive", caseSensitive}, {"whole_word", wholeWord}, {"regex", regex}};
    return {{"case_sensitive", caseSensitive}, {"whole_word", wholeWord},
            {"regex", regex}, {"columns", rows}};
}

} // namespace mlash