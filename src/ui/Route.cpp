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
    default: return "";
    }
}
} // namespace

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
    if (route.view == ViewIds::Home) return route.anchor == HomeAnchor::Control || route.anchor == HomeAnchor::Nodes;
    const bool known = route.view == ViewIds::Profiles || route.view == ViewIds::Rules
        || route.view == ViewIds::Data || route.view == ViewIds::Settings;
    if (!known) return false;
    if (route.tab >= 0 && route.tab >= ViewIds::DataTabCount) return false;
    return true;
}

int toLegacyPage(const Route& route) {
    const auto tab = route.tab < 0 ? ViewIds::DataTab::Traffic : ViewIds::dataTabFromIndex(route.tab);
    return ViewIds::toLegacy(route.view, tab, route.category);
}

QString routeTitle(const Route& route, const QString& categoryName) {
    const QString name = QString::fromUtf8(viewName(route.view));
    if (name.isEmpty()) return "Mlash";
    if (route.view == ViewIds::Settings && !categoryName.isEmpty()) return QString("Mlash · %1 › %2").arg(name, categoryName);
    return QString("Mlash · %1").arg(name);
}

} // namespace mlash
