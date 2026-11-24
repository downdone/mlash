#pragma once
#include <QMetaType>
#include <QString>
#include <array>
namespace mlash {

// category 的「未指定」取值：保留当前设置分类，或让调用方补。
constexpr int kNoCategory = -1;

// 在代理和隧道运行视图中区分运行控制和节点／Peer 页面。
enum class HomeAnchor { Control = 0, Nodes = 1 };
enum class WorkspaceArea { Global, Proxy, Tunnel };
enum class WorkspacePage { Run, Members, Traffic, Logs };

// 唯一的路由键。view 为视图 ID（见 ViewIds.h：首页/配置/规则/数据/设置），
// category 仅设置页使用（kNoCategory 表示未指定），anchor 仅首页使用。
// 数据页的四个页签用 tab 表达，与视图分开：同一个视图切换页签不改视图。
// 见 docs/ui-layered-design.md「路由模型」与 ADR 0046 / 0048。
struct Route {
    int view = 0;
    int category = kNoCategory;
    HomeAnchor anchor = HomeAnchor::Control;
    int tab = -1;   // 仅数据页使用：-1 表示沿用当前页签。
    WorkspaceArea area = WorkspaceArea::Global;

    bool operator==(const Route&) const = default;
};

Route workspaceRoute(WorkspaceArea area, WorkspacePage page);
WorkspacePage workspacePage(const Route& route);
QString workspaceTitle(const Route& route);

struct WorkspaceBookmark {
    WorkspaceArea area = WorkspaceArea::Proxy;
    std::array<Route,2> routes{
        workspaceRoute(WorkspaceArea::Proxy,WorkspacePage::Run),
        workspaceRoute(WorkspaceArea::Tunnel,WorkspacePage::Run)};
    bool collapsed = false;
    int proxyScroll = 0;
    int tunnelScroll = 0;
};

// 历史整数入口（启动页、托盘、GuiHost 恢复、设置搜索）到路由的转换。
// 旧编号 0–7 的映射集中在 ViewIds.cpp，见 fromLegacy()。
Route fromLegacyPage(int page);

// 检查视图、锚点、页签与工作区是否兼容。
bool isRoutableView(const Route& route);

// 从路由取回一个「入口 ID」，供只接受整数的旧入口回显与持久化使用：
// 首页 Control 回到 0，首页 Nodes 回到 2，其余原样。
int toLegacyPage(const Route& route);

// 窗口标题「Mlash · 视图名」，设置页追加「› 分类名」；未知视图只有「Mlash」。
QString routeTitle(const Route& route, const QString& categoryName = QString());

} // namespace mlash

Q_DECLARE_METATYPE(mlash::Route)
