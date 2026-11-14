#include "PageLifecycle.h"

namespace mlash {

void PageRouter::registerPage(int view, PageLifecycle* page) {
    pages_.insert(view, page);
}

PageLifecycle* PageRouter::page(int view) const {
    return pages_.value(view, nullptr);
}

void PageRouter::go(const Route& route) {
    // 这里收到的已经是新视图 ID（0=首页 … 4=设置），2 就是规则视图本身。
    // 旧编号的归一化只发生在整数入口（fromLegacyPage），不要在这里再猜一次。
    const int view = route.view;
    if (view == current_) {
        // 同一视图内换锚点/分类不算重新进入；页面自己处理页内定位。
        if (auto* same = page(view)) same->onEnter(route);
        return;
    }
    if (auto* previous = page(current_)) previous->onLeave();
    current_ = view;
    if (auto* next = page(view)) next->onEnter(route);
}

void PageRouter::restore(int view, const PageMemory& memory) {
    if (auto* target = page(view)) target->onRestore(memory);
}

} // namespace mlash
