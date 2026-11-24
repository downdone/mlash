#include "Route.h"
#include "ViewIds.h"

namespace mlash {
namespace {
// 视图名只用于窗口标题，不参与布局。数据页的页签不改变窗口标题。
const char* viewName(int view) {
    switch (view) {
    case ViewIds::Home: return "首页";
    case ViewIds::Profiles: return "订阅";
    case ViewIds::Rules: return "规则";
    case ViewIds::Data: return "数据";
    case ViewIds::Settings: return "设置";
    case ViewIds::Overview: return "总览";
    case ViewIds::Tunnel: return "隧道";
    default: return "";
    }
}
} // namespace

Route workspaceRoute(WorkspaceArea area, WorkspacePage page) {
    if (area==WorkspaceArea::Global) return Route{ViewIds::Overview};
    Route target{area==WorkspaceArea::Proxy?ViewIds::Home:ViewIds::Tunnel};
    target.area=area;
    if (page==WorkspacePage::Members) target.anchor=HomeAnchor::Nodes;
    if (page==WorkspacePage::Traffic || page==WorkspacePage::Logs) {
        target.view=ViewIds::Data;
        target.tab=int(page==WorkspacePage::Traffic?ViewIds::DataTab::Traffic:ViewIds::DataTab::Logs);
    }
    return target;
}

WorkspacePage workspacePage(const Route& route) {
    if (route.view==ViewIds::Data)
        return route.tab==int(ViewIds::DataTab::Logs)?WorkspacePage::Logs:WorkspacePage::Traffic;
    return route.anchor==HomeAnchor::Nodes?WorkspacePage::Members:WorkspacePage::Run;
}

QString workspaceTitle(const Route& route) {
    if (route.area==WorkspaceArea::Global) return {};
    const bool tunnel=route.area==WorkspaceArea::Tunnel;
    const QString name=tunnel?"隧道":"代理";
    const auto label=[&] {
        if (route.view==ViewIds::Data) return ViewIds::dataTabName(ViewIds::dataTabFromIndex(route.tab));
        if (route.anchor==HomeAnchor::Nodes) return tunnel?QString("Peer"):QString("节点");
        return QString("运行");
    }();
    return name+" / "+label;
}

Route fromLegacyPage(int page) {
    const auto target = ViewIds::fromLegacy(page);
    // 旧编号 2 是「首页节点列表」，落到节点锚点而不是控制区。
    const auto anchor = page == 2 ? HomeAnchor::Nodes : HomeAnchor::Control;
    // 页签只属于数据页：旧编号 0/1/2/3/6 落到别的视图时带上页签，
    // 会让首页、规则、设置的 currentRoute() 各字段都不同（同一条路由比较不相等），
    // 还可能经 toLegacyPage() 往返成数据页的编号。非数据页一律留 -1（沿用）。
    const int tab = target.view == ViewIds::Data ? int(target.tab) : -1;
    return Route{target.view, target.category, anchor, tab};
}

bool isRoutableView(const Route& route) {
    if (route.area!=WorkspaceArea::Global && route.area!=WorkspaceArea::Proxy && route.area!=WorkspaceArea::Tunnel) return false;
    if (route.area==WorkspaceArea::Proxy && route.view!=ViewIds::Home && route.view!=ViewIds::Data) return false;
    if (route.area==WorkspaceArea::Tunnel && route.view!=ViewIds::Tunnel && route.view!=ViewIds::Data) return false;
    if (route.area==WorkspaceArea::Tunnel && route.view==ViewIds::Data && route.tab!=int(ViewIds::DataTab::Traffic)
        && route.tab!=int(ViewIds::DataTab::Logs)) return false;
    if (route.view==ViewIds::Tunnel && route.area!=WorkspaceArea::Tunnel) return false;
    if (route.view == ViewIds::Home || route.view==ViewIds::Tunnel)
        return route.anchor == HomeAnchor::Control || route.anchor == HomeAnchor::Nodes;
    const bool known = route.view == ViewIds::Profiles || route.view == ViewIds::Rules
        || route.view == ViewIds::Data || route.view == ViewIds::Settings || route.view==ViewIds::Overview;
    if (!known) return false;
    if (route.tab >= 0 && route.tab >= ViewIds::DataTabCount) return false;
    return true;
}

int toLegacyPage(const Route& route) {
    const auto tab = route.tab < 0 ? ViewIds::DataTab::Traffic : ViewIds::dataTabFromIndex(route.tab);
    return ViewIds::toLegacy(route.view, tab, route.category);
}

QString routeTitle(const Route& route, const QString& categoryName) {
    const auto workspace=workspaceTitle(route);
    if (!workspace.isEmpty()) return "Mlash · "+workspace;
    const QString name = QString::fromUtf8(viewName(route.view));
    if (name.isEmpty()) return "Mlash";
    if (route.view == ViewIds::Settings && !categoryName.isEmpty()) return QString("Mlash · %1 › %2").arg(name, categoryName);
    return QString("Mlash · %1").arg(name);
}

} // namespace mlash
