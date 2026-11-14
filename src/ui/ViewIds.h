#pragma once
#include "Route.h"
#include <QString>

namespace mlash::ViewIds {
// 页面栈里的视图 ID（GPT 原型 K 版：数据页合并为四页签后重新编号）。
// 这里的数字只在这一个文件里赋予含义；其余代码一律用常量，禁止写字面量。
inline constexpr int Home = 0;
inline constexpr int Profiles = 1;
inline constexpr int Rules = 2;
inline constexpr int Data = 3;
inline constexpr int Settings = 4;
// 数据页的四个页签。顺序与原型一致：流量 / 连接 / 测试 / 日志。
enum class DataTab { Traffic = 0, Connections = 1, Tests = 2, Logs = 3 };
inline constexpr int DataTabCount = 4;

inline constexpr int Count = 5;

// 旧入口 0–7 → 新视图 + 数据页签 + 分类。旧编号只在这里出现一次，用于持久化与快捷键兼容。
struct LegacyTarget { int view; DataTab tab; int category; };
LegacyTarget fromLegacy(int legacy);

// 反向：当前路由 → 旧入口编号（供只接受整数的持久化与回显使用）。
int toLegacy(int view, DataTab tab, int category);

// 顶栏标题与副标题（原型 setPage）。
QString title(int view);
QString subtitle(int view);

// 数据页签名称。
QString dataTabName(DataTab tab);
DataTab dataTabFromIndex(int index);
} // namespace mlash::ViewIds
