#pragma once
// CONTRACT owner=ui layer=ui
// invariant: 每个界面容器有唯一稳定字符串标识；功能大纲只引用这里的值
// forbidden: 不得在功能大纲或检查脚本中写未登记的字面量容器名
// lifecycle: 纯常量注册表，无运行时状态；不持有界面对象
// tests: feature_outline
// spec: docs/requirements-traceability.md
//
// 容器标识是"功能 ↔ 界面"映射的唯一词汇表（原则 12）。
// 现有代码里视图/页签/工作区已有常量；设置分类此前只有数字索引、配置任务此前只有
// 局部字符串，这里补成登记值。新增容器必须在此登记后再写进功能大纲。

namespace mlash::ContainerIds {

// 一级视图（与 ViewIds 的视图常量一一对应）。
inline constexpr char Home[] = "view:home";
inline constexpr char Overview[] = "view:overview";
inline constexpr char Profiles[] = "view:profiles";
inline constexpr char Rules[] = "view:rules";
inline constexpr char Data[] = "view:data";
inline constexpr char Settings[] = "view:settings";
inline constexpr char Tunnel[] = "view:tunnel";

// 代理 / 隧道工作区页面（与 Route.h 的 WorkspaceArea + WorkspacePage 对齐）。
inline constexpr char ProxyRun[] = "proxy:run";
inline constexpr char ProxyMembers[] = "proxy:members";
inline constexpr char TunnelRun[] = "tunnel:run";
inline constexpr char TunnelMembers[] = "tunnel:members";

// 数据页签（与 ViewIds::DataTab 对齐）。
inline constexpr char DataTraffic[] = "data:traffic";
inline constexpr char DataConnections[] = "data:connections";
inline constexpr char DataTests[] = "data:tests";
inline constexpr char DataLogs[] = "data:logs";

// 设置任务（目录行进入的工作区）。
inline constexpr char SettingsStartup[] = "settings:startup";
inline constexpr char SettingsNetwork[] = "settings:network";
inline constexpr char SettingsEngine[] = "settings:engine";
inline constexpr char SettingsTools[] = "settings:tools";
inline constexpr char SettingsBackup[] = "settings:backup";
inline constexpr char SettingsTun[] = "settings:tun";
inline constexpr char SettingsAbout[] = "settings:about";
inline constexpr char SettingsHotkeys[] = "settings:hotkeys";

// 配置详情内的任务页。
inline constexpr char ProfileSource[] = "profile:source";
inline constexpr char ProfileVersions[] = "profile:versions";
inline constexpr char ProfileNodes[] = "profile:nodes";
inline constexpr char ProfileRules[] = "profile:rules";
inline constexpr char ProfileDns[] = "profile:dns";
inline constexpr char ProfileChain[] = "profile:chain";
inline constexpr char ProfileProviders[] = "profile:providers";
inline constexpr char ProfileCompatibility[] = "profile:compatibility";

// 托盘与无界面能力。
inline constexpr char TrayMenu[] = "tray:menu";
inline constexpr char Infra[] = "infra"; // 非界面能力：持久化、恢复、生命周期

} // namespace mlash::ContainerIds