#include "TrayController.h"
#include "NodePresentation.h"
#include "WidgetStyle.h"
#include <QPainter>
#include <QPixmap>
#include <QJsonArray>
#include <QMessageBox>
#include <QHash>
#include <QElapsedTimer>
#include <QApplication>
#include <QPointer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>

static void initializeBrand() { static const bool initialized = [] { Q_INIT_RESOURCE(brand); return true; }(); Q_UNUSED(initialized); }

namespace mlash {
qint64 TrayController::measureMenuPreparation() { QElapsedTimer timer; timer.start(); rebuildMenu(); return timer.nsecsElapsed(); }
QIcon TrayController::icon() {
    initializeBrand(); return QIcon(":/brand/mlash.png");
}
QString TrayController::appearance(const AppState& state) {
    if (state.exiting || state.core == CoreState::Starting || state.core == CoreState::Stopping) return "working";
    if (state.core == CoreState::Failed || (state.proxy == ProxyState::Owned && (!state.ready || state.core != CoreState::Running))) return "attention";
    if (state.core != CoreState::Running || !state.ready) return "idle";
    if (state.proxy == ProxyState::Unknown || !QStringList{"rule", "global", "direct"}.contains(state.mode)) return "checking";
    if (state.proxy == ProxyState::Owned||state.tunObserved) return state.mode;
    if (state.proxy == ProxyState::External) return "external-" + state.mode;
    return "ready-" + state.mode;
}
QIcon TrayController::statusIcon(const AppState& state) {
    initializeBrand(); static QHash<QString, QIcon> icons;
    const auto key = appearance(state);
    if (!icons.contains(key)) icons.insert(key, QIcon(":/brand/tray/" + key + ".png"));
    return icons.value(key);
}
QString TrayController::statusTooltip(const AppState& state) {
    const auto key = appearance(state);
    const QString title = key == "idle" ? "小象打盹 · 内核未就绪" : key == "working" ? "小象忙碌 · 正在启停" : key == "attention" ? "小象需要照顾 · 内核或代理入口异常" : key == "checking" ? "小象观察中 · 状态待确认" : key.startsWith("ready-") ? "小象待命 · 系统代理未开启" : key.startsWith("external-") ? "小象旁观 · 系统代理由其他程序管理" : "小象上岗 · Mlash 已接管系统代理";
    const QString mode = !state.ready ? "未就绪" : state.mode == "rule" ? "规则" : state.mode == "global" ? "全局" : state.mode == "direct" ? "直连" : "未知";
    return "Mlash · " + (state.tunObserved?QString("小象上岗 · TUN 已接管"):title) + "\n内核：" + coreStateText(state.core) + " · 系统代理：" + proxyStateText(state.proxy) + "\n内核模式：" + mode;
}
TrayController::TrayController(AppController* controller, QObject* parent)
    : QObject(parent), controller_(controller), tray_(this), available_(QSystemTrayIcon::isSystemTrayAvailable()) {
    WidgetStyle::install();
    tray_.setContextMenu(&menu_);
    groupDwell_.setSingleShot(true); groupDwell_.setInterval(300);
    connect(&groupDwell_,&QTimer::timeout,this,[this] { if (autoTestGroups_ && activeGroupMenu_ && activeGroupMenu_->isVisible()) controller_->testTrayGroup(activeGroupMenu_->property("groupName").toString()); });
    connect(&menu_,&QMenu::aboutToHide,this,[this] { groupDwell_.stop(); activeGroupMenu_=nullptr; controller_->cancelTrayTests(); });
    connect(controller_,&AppController::trayDelaysChanged,this,&TrayController::updateGroupMenu);
    clickTimer_.setSingleShot(true); connect(&clickTimer_,&QTimer::timeout,this,[this] { controller_->setSystemProxy(controller_->state().proxy!=ProxyState::Owned); });
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason==QSystemTrayIcon::DoubleClick) { clickTimer_.stop(); emit showWindow(); return; } if (reason!=QSystemTrayIcon::Trigger) return;
        const auto action=controller_->settings().trayClick;
        if (action=="proxy-page") emit showPage(2);
        else if (action=="toggle-proxy") clickTimer_.start(QApplication::doubleClickInterval());
        else emit showWindow();
    });
    connect(&menu_, &QMenu::aboutToShow, this, &TrayController::rebuildMenu);
    connect(controller_, &AppController::stateChanged, this, &TrayController::updateState);
    connect(controller_, &AppController::operationFinished, this, [this](bool ok, const QString& text) {
        if (!ok && available_ && !controller_->state().exiting) tray_.showMessage("Mlash", text, QSystemTrayIcon::Warning, 5000);
    });
    updateState(); if (available_) tray_.show();
}
TrayController::~TrayController() { groupDwell_.stop(); controller_->cancelTrayTests(); }
bool TrayController::eventFilter(QObject* watched,QEvent* event) {
    auto* menu=qobject_cast<QMenu*>(watched); QAction* action=nullptr;
    if (menu && event->type()==QEvent::MouseButtonRelease) { auto* mouse=static_cast<QMouseEvent*>(event); if (mouse->button()==Qt::LeftButton) action=menu->actionAt(mouse->position().toPoint()); }
    if (menu && event->type()==QEvent::KeyPress) { const auto key=static_cast<QKeyEvent*>(event)->key(); if (key==Qt::Key_Return || key==Qt::Key_Enter || key==Qt::Key_Space) action=menu->activeAction(); }
    if (action && action->objectName()=="trayRetestGroup" && action->isEnabled()) { action->trigger(); return true; }
    return QObject::eventFilter(watched,event);
}
void TrayController::updateState() {
    const auto& state = controller_->state(); const auto key = appearance(state);
    if (key != visualKey_) { visualKey_ = key; tray_.setIcon(statusIcon(state)); }
    const auto tooltip = statusTooltip(state); if (tray_.toolTip() != tooltip) tray_.setToolTip(tooltip);
    if (!state.ready || state.busy || state.exiting) { groupDwell_.stop(); controller_->cancelTrayTests(); }
    updateGroupMenu();
}
void TrayController::updateGroupMenu() {
    if (!activeGroupMenu_) return;
    const auto group=controller_->proxies().value(activeGroupMenu_->property("groupName").toString()).toObject();
    const auto& state=controller_->state(); const bool idle=state.ready && !state.busy && !state.exiting && !state.readOnly;
    const bool selectable=QStringList{"Selector","URLTest","Fallback"}.contains(group.value("type").toString()); bool pending=false; QAction* retest=nullptr;
    for (auto* action:activeGroupMenu_->actions()) {
        const auto name=action->property("memberName").toString();
        if (name.isEmpty()) { if (action->objectName()=="trayRetestGroup") retest=action; continue; }
        const auto result=controller_->trayNodeDelay(name,activeGroupMenu_->property("groupName").toString());
        pending=pending || result.value("pending").toBool();
        const auto type=controller_->proxies().value(name).toObject().value("type").toString();
        const bool builtin=QStringList{"Direct","Reject","RejectDrop","Pass"}.contains(type);
        const auto delay=builtin?QString():result.value("pending").toBool()?QString("测速中…"):!result.value("known").toBool()?QString("—"):result.value("delay").toInt()>0?QString::number(result.value("delay").toInt())+" ms":QString("失败");
        action->setText(QString(name).replace('&',"&&")+(delay.isEmpty()?QString():"\t"+delay));
        action->setChecked(name==group.value("now").toString()); action->setEnabled(idle && selectable);
        action->setToolTip(name+(builtin?QString():"\n手动目标延迟，仅供本次选择参考")+(result.value("error").toString().isEmpty()?QString():"\n"+result.value("error").toString()));
    }
    if (retest) retest->setEnabled(idle && !pending);
}
void TrayController::rebuildMenu() {
    groupDwell_.stop(); activeGroupMenu_=nullptr; controller_->cancelTrayTests();
    // clear() removes actions, but child menus created by addMenu(title) remain owned.
    QList<QPointer<QMenu>> previousMenus;
    for (auto* child : menu_.findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly)) previousMenus.append(child);
    menu_.clear();
    for (const auto& child : previousMenus) if (child) delete child.data();
    menuIcons_.clear();
    const auto state = controller_->state(); const bool enabled = !state.busy && !state.exiting;
    auto* status=menu_.addAction("Mlash · "+(state.ready?QString("内核运行中"):"内核"+coreStateText(state.core)));
    status->setEnabled(false); status->setToolTip(statusTooltip(state)); menu_.setToolTipsVisible(true);
    for (const auto& p : controller_->profiles()) if (p.id == state.activeProfile) menu_.addAction("当前订阅 · " + p.name)->setEnabled(false);
    menu_.addSeparator();
    auto* proxy = menu_.addAction("系统代理"); proxy->setCheckable(true); proxy->setChecked(state.proxy == ProxyState::Owned); proxy->setEnabled(enabled && (state.ready || state.proxy == ProxyState::Owned));
    connect(proxy, &QAction::triggered, this, [this](bool checked) {
        bool allow = false;
        if (checked && controller_->state().proxy == ProxyState::External) {
            allow = QMessageBox::question(nullptr, "Mlash 接管系统代理", "系统已有代理/PAC/自动检测，允许暂时接管并在退出时恢复？") == QMessageBox::Yes;
            if (!allow) return;
        }
        controller_->setSystemProxy(checked, allow);
    });
    auto* tun=menu_.addAction("TUN 虚拟网卡"); tun->setObjectName("trayTunSwitch"); tun->setCheckable(true); tun->setChecked(state.tunObserved); tun->setEnabled(enabled&&!state.readOnly&&!state.activeProfile.isEmpty()); connect(tun,&QAction::triggered,controller_,&AppController::setTunEnabled);
    auto* mode = menu_.addMenu("代理模式"); mode->setEnabled(enabled && state.ready);
    for (const auto& entry : QList<QPair<QString, QString>>{{"规则","rule"},{"全局","global"},{"直连","direct"}}) {
        auto* action = mode->addAction(entry.first); action->setCheckable(true); action->setChecked(entry.second == state.mode);
        connect(action, &QAction::triggered, this, [this, entry] { controller_->setMode(entry.second); });
    }
    auto* groups = menu_.addMenu("策略组"); groups->setEnabled(enabled && state.ready);
    auto* automatic=groups->addAction("展开时自动测速"); automatic->setObjectName("trayAutoTestGroups"); automatic->setCheckable(true); automatic->setChecked(autoTestGroups_); automatic->setToolTip("本次启动有效；停留 300ms 后测速，复用 60 秒结果，关闭菜单停止");
    connect(automatic,&QAction::toggled,this,[this](bool checked) { autoTestGroups_=checked; if (!checked) { groupDwell_.stop(); controller_->cancelTrayTests(); } }); groups->addSeparator();
    const auto& proxies = controller_->proxies();
    auto names=proxies.keys(); const auto common=controller_->nodePreferences().value("common_groups").toArray(); std::stable_sort(names.begin(),names.end(),[common](const QString& a,const QString& b) { return common.contains(a)&&!common.contains(b); });
    for (const auto& name:names) {
        const auto group=proxies.value(name).toObject(); if (!group.value("all").isArray()) continue;
        auto* sub = groups->addMenu((common.contains(name)?QString("★ "):QString())+name); sub->setProperty("groupName",name); sub->setToolTipsVisible(true); sub->installEventFilter(this);
        connect(sub, &QMenu::aboutToShow, sub, [this, sub, name] {
            groupDwell_.stop(); controller_->cancelTrayTests(); activeGroupMenu_=sub;
            sub->clear(); const auto group = controller_->proxies().value(name).toObject();
            for (const auto& member : group.value("all").toArray()) {
                const auto value = member.toString(); auto* action = sub->addAction(value); action->setCheckable(true); action->setChecked(value == group.value("now").toString());
                action->setProperty("memberName",value); const bool nested=controller_->proxies().value(value).toObject().value("all").isArray();
                const auto iconKey=NodePresentation::iconKey(value,nested);
                if (!menuIcons_.contains(iconKey)) menuIcons_.insert(iconKey,NodePresentation::icon(value,nested)); action->setIcon(menuIcons_.value(iconKey));
                connect(action, &QAction::triggered, this, [this, name, value] { controller_->selectMember(name, value); });
            }
            sub->addSeparator(); auto* retest=sub->addAction("重新测速本组"); retest->setObjectName("trayRetestGroup"); connect(retest,&QAction::triggered,this,[this,sub,name] { groupDwell_.stop(); if (activeGroupMenu_==sub && sub->isVisible()) controller_->testTrayGroup(name,true); });
            updateGroupMenu(); if (autoTestGroups_) groupDwell_.start();
        });
        connect(sub,&QMenu::aboutToHide,this,[this,sub] { if (activeGroupMenu_!=sub) return; groupDwell_.stop(); activeGroupMenu_=nullptr; controller_->cancelTrayTests(); });
    }
    groups->addSeparator(); auto* browse=groups->addAction("打开代理页 / 筛选…"); connect(browse,&QAction::triggered,this,[this] { emit showPage(2); });
    auto* subscriptions = menu_.addMenu("订阅"); subscriptions->setEnabled(enabled);
    for (const auto& profile : controller_->profiles()) {
        auto* action = subscriptions->addAction(profile.name); action->setCheckable(true); action->setChecked(profile.id == state.activeProfile);
        connect(action, &QAction::triggered, this, [this, id = profile.id] { controller_->activateProfile(id); });
    }
    subscriptions->addSeparator(); auto* update = subscriptions->addAction("更新当前订阅");
    bool canUpdate = false; for (const auto& p : controller_->profiles()) if (p.id == state.activeProfile && p.sourceType == "url") canUpdate = true;
    update->setEnabled(canUpdate);
    connect(update, &QAction::triggered, this, [this] { controller_->updateProfile(controller_->settings().activeProfile); });
    auto* core = menu_.addAction(state.core == CoreState::Running ? "停止内核" : "启动内核"); core->setEnabled(enabled);
    connect(core, &QAction::triggered, this, [this] { if (controller_->state().core == CoreState::Running) controller_->stopCore(); else controller_->startCore(); });
    menu_.addSeparator(); connect(menu_.addAction("打开主窗口"), &QAction::triggered, this, &TrayController::showWindow);
    connect(menu_.addAction("代理 / 节点"), &QAction::triggered, this, [this] { emit showPage(2); });
    connect(menu_.addAction("连接"), &QAction::triggered, this, [this] { emit showPage(4); });
    auto* pages=menu_.addMenu("更多页面"); pages->setObjectName("trayMorePages");
    for (const auto& page:QList<QPair<QString,int>>{{"测速",7},{"订阅管理",1},{"规则",3},{"日志",5}}) connect(pages->addAction(page.first),&QAction::triggered,this,[this,page] { emit showPage(page.second); });
    connect(menu_.addAction("设置"), &QAction::triggered, this, [this] { emit showPage(6); });
    menu_.addSeparator();
    connect(menu_.addAction("退出 Mlash"), &QAction::triggered, this, &TrayController::exitRequested);
}
}
