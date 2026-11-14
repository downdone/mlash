#pragma once
#include <QAction>
#include <QMenu>
#include <QString>

namespace mlash {

// 菜单分组标题（原型 06 / 切片 V4）。
//
// 订阅管理菜单有五组，托盘是六个跨对象分组，但组与组之间只有一条分隔线：
// 用户数到第五组时已经不知道自己在哪一「组」里。这里给每组一个不可聚焦的小标题，
// 用 QAction 承载（QMenu 只能放 QAction），文字取次要色、禁用态且不进键盘焦点链。
inline QAction* menuSection(QMenu* menu, const QString& text) {
    auto* action = new QAction(text, menu);
    action->setObjectName("menuSection");
    action->setEnabled(false);
    // 裸 QMenu 里没有分组控件，只能靠一个禁用的 action 表达；它能被高亮但不能被触发，
    // 因此把它的菜单角色标成「不可选」，避免 Enter 落在分组标题上。
    action->setMenuRole(QAction::NoRole);
    menu->addAction(action);
    return action;
}

} // namespace mlash
