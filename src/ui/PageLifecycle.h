#pragma once
#include "Route.h"
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QHash>
#include <QPointer>

namespace mlash {

// 页面生命周期（界面分层设计 4「页面生命周期契约」）。
//
// 此前进入/离开页面的副作用散落在 MainWindow::navigate() 里的若干 `if (view == N)`：
// 启动轮询、开合连接采样、订阅诊断、刷新内核规则、滚动到节点锚点。行为是对的，
// 但每加一个视图或一项副作用都要回到那个函数里加分支，容易漏。
//
// 这里把「离开旧页」与「进入新页」收敛成两次调用；每个视图一个对象，
// 副作用写在它自己的 onEnter/onLeave 里。MainWindow 只负责顺序。
struct PageMemory {
    QVariantMap filters;   // 页内筛选，会话内有效，不持久化
    int tab = 0;           // 页内分段下标
};

class PageLifecycle : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    // 路由进入该视图。路由里的 category/anchor 由页面自己决定怎么用。
    virtual void onEnter(const Route& route) { Q_UNUSED(route) }
    // 路由离开该视图：停轮询、取消本页测试、关闭本页详情窗口。
    virtual void onLeave() {}
    // 轻量重建后恢复页内状态（不恢复轮询；轮询由随后的 onEnter 重新启动）。
    virtual void onRestore(const PageMemory& memory) { Q_UNUSED(memory) }
};

// 按视图 ID 保存一组生命周期对象，并保证「离开 → 进入」的顺序与幂等。
// 同一视图重复进入只触发一次 onEnter；离开未进入过的视图不触发 onLeave。
class PageRouter {
public:
    void registerPage(int view, PageLifecycle* page);
    void go(const Route& route);
    void restore(int view, const PageMemory& memory);
    int current() const { return current_; }
    PageLifecycle* page(int view) const;
private:
    // Qt parents own pages. A destroyed page must never remain callable during
    // lightweight window teardown or a later router transition.
    QHash<int, QPointer<PageLifecycle>> pages_;
    int current_ = -1;
};

} // namespace mlash
