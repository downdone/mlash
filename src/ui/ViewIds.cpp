#include "ViewIds.h"

#include <QHash>

namespace mlash::ViewIds {
namespace {
// 旧入口（0–7，R3 之前的历史编号）→ 新视图。
struct Legacy { int view; DataTab tab; int category; };
Legacy legacy(int value) {
    switch (value) {
    case 0:  return {Home, DataTab::Traffic, kNoCategory};
    case 2:  return {Home, DataTab::Traffic, kNoCategory};       // 节点视图 → 首页节点区
    case 1:  return {Profiles, DataTab::Traffic, kNoCategory};
    case 3:  return {Rules, DataTab::Traffic, kNoCategory};
    case 4:  return {Data, DataTab::Connections, kNoCategory};
    case 5:  return {Data, DataTab::Logs, kNoCategory};
    case 7:  return {Data, DataTab::Tests, kNoCategory};
    case 6:  return {Settings, DataTab::Traffic, kNoCategory};              // 设置固定落第 0 组「通用」
    default: return {Home, DataTab::Traffic, kNoCategory};
    }
}
} // namespace

LegacyTarget fromLegacy(int value) { const auto target = legacy(value); return {target.view, target.tab, target.category}; }

int toLegacy(int view, DataTab tab, int category) {
    switch (view) {
    case Home:     return 0;
    case Profiles: return 1;
    case Rules:    return 3;
    case Settings: return 6;
    case Data:
        switch (tab) {
        case DataTab::Traffic:     return 4;
        case DataTab::Connections: return 4;
        case DataTab::Tests:       return 7;
        case DataTab::Logs:        return 5;
        }
        return 4;
    default: return 0;
    }
}

QString title(int view) {
    switch (view) {
    case Home:     return QStringLiteral("首页");
    case Profiles: return QStringLiteral("配置");
    case Data:     return QStringLiteral("数据");
    case Settings: return QStringLiteral("设置");
    case Rules:    return QStringLiteral("规则");
    default:       return QStringLiteral("Mlash");
    }
}

QString subtitle(int view) {
    switch (view) {
    case Home:     return QStringLiteral("代理控制与节点选择");
    case Profiles: return QStringLiteral("订阅与本地配置");
    case Data:     return QStringLiteral("流量、连接与诊断");
    case Settings: return QStringLiteral("客户端偏好设置");
    case Rules:    return QStringLiteral("分流规则与匹配顺序");
    default:       return QString();
    }
}

QString dataTabName(DataTab tab) {
    switch (tab) {
    case DataTab::Traffic:     return QStringLiteral("流量");
    case DataTab::Connections: return QStringLiteral("连接");
    case DataTab::Tests:       return QStringLiteral("测试");
    case DataTab::Logs:        return QStringLiteral("日志");
    }
    return QString();
}

DataTab dataTabFromIndex(int index) {
    switch (index) {
    case 1: return DataTab::Connections;
    case 2: return DataTab::Tests;
    case 3: return DataTab::Logs;
    default: return DataTab::Traffic;
    }
}
} // namespace mlash::ViewIds
