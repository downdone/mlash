#include "app/AppController.h"
#include "TrayController.h"
#include "StatusSentence.h"
#include "FeedbackSync.h"
#include "NodePresentation.h"
#include "WidgetStyle.h"
#include "design/ShadowrocketStyle.h"
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
namespace {
QIcon makeTrayBadge(const QIcon& mark, const QString& key) {
    if (key == "idle") return mark;
    const auto& colors = design::ShadowrocketStyle::dark;
    const bool ready = key.startsWith("ready-");
    const bool external = key.startsWith("external-");
    const QString mode = key.section('-', -1);
    const bool pending = key == "working" || key == "checking";
    const QColor fill(key == "attention" ? colors.error : pending ? colors.warning :
                      ready ? colors.focus : external ? colors.secondary : colors.switchOn);
    const QColor ink(colors.sidebar);
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64}) {
        QPixmap tile(size, size);
        tile.fill(Qt::transparent);
        QPainter painter(&tile);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 64.0, size / 64.0);
        painter.drawPixmap(QRect(0, 0, 52, 52), mark.pixmap(size, size));
        painter.setBrush(fill);
        painter.setPen(QPen(ink, 3));
        painter.drawEllipse(QPointF(48, 48), 14, 14);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(ink, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (mode == "rule") {
            painter.drawPolyline(QPolygonF{{41,48},{46,53},{55,43}});
        } else if (mode == "global") {
            painter.drawEllipse(QPointF(48,48), 7, 7);
        } else if (mode == "direct") {
            painter.drawLine(QPointF(40,48),QPointF(53,48));
            painter.drawPolyline(QPolygonF{{49,43},{54,48},{49,53}});
        } else if (key == "working") {
            painter.drawLine(QPointF(41,48),QPointF(55,48));
        } else if (key == "checking") {
            painter.drawPolyline(QPolygonF{{43,43},{51,43},{53,46},{48,50}});
            painter.drawPoint(QPointF(48,55));
        } else {
            painter.drawLine(QPointF(48,41),QPointF(48,49));
            painter.drawPoint(QPointF(48,55));
        }
        painter.end();
        icon.addPixmap(tile);
    }
    return icon;
}
}
qint64 TrayController::measureMenuPreparation() { QElapsedTimer timer; timer.start(); rebuildMenu(); return timer.nsecsElapsed(); }
QIcon TrayController::icon() {
    initializeBrand(); return QIcon(":/brand/mlash-app-icon.png");
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
    if (!icons.contains(key)) icons.insert(key, makeTrayBadge(icon(), key));
    return icons.value(key);
}
QString TrayController::statusTooltip(const AppState& state, const Settings& settings, const QString& profileName) {
    const auto key = appearance(state);
    const QString title = key == "idle" ? "内核未就绪" : key == "working" ? "正在启停" :
        key == "attention" ? "内核或代理入口异常" : key == "checking" ? "状态待确认" :
        key.startsWith("ready-") ? "系统代理未开启" : key.startsWith("external-") ? "系统代理由其他程序管理" :
        state.tunObserved ? "TUN 已开启" : "系统代理已接管";
    const QString mode = !state.ready ? "未就绪" : state.mode == "rule" ? "规则" : state.mode == "global" ? "全局" : state.mode == "direct" ? "直连" : "未知";
    // 前两行与侧栏状态条逐字相同（分层设计 P1/P2），第三行是托盘独有的内核模式。
    const auto sentence = statusSentence(statusFacts(state, settings, profileName));
    return "Mlash · " + title + QChar(10) + sentence.line1 + QChar(10) + sentence.line2 + QChar(10) + "内核模式：" + mode;
}
TrayController::TrayController(AppController* controller, QObject* parent)
    : QObject(parent), controller_(controller), tray_(this), available_(QSystemTrayIcon::isSystemTrayAvailable()) {
    WidgetStyle::install();
    tray_.setContextMenu(&menu_);
    groupDwell_.setSingleShot(true); groupDwell_.setInterval(300);
    connect(&groupDwell_,&QTimer::timeout,this,[this] { if (autoTestGroups_ && activeGroupMenu_ && activeGroupMenu_->isVisible()) controller_->testTrayGroup(activeGroupMenu_->property("groupName").toString()); });
    connect(&menu_,&QMenu::aboutToHide,this,[this] { groupDwell_.stop(); activeGroupMenu_=nullptr; controller_->cancelTrayTests(); });
    connect(controller_,&AppController::trayDelaysChanged,this,&TrayController::updateGroupMenu);
    clickTimer_.setSingleShot(true); connect(&clickTimer_,&QTimer::timeout,this,[this] {
        // 与首页代理总开关同一命令与同一确认规则（5.9 约束）。
        const auto& state=controller_->state(); const bool on = state.proxy==ProxyState::Owned || state.tunObserved;
        if (!on && state.proxy==ProxyState::External) {
            if (QMessageBox::question(nullptr,QStringLiteral("Mlash 接管系统代理"),QStringLiteral("系统已有代理/PAC/自动检测，允许暂时接管并在退出时恢复？"))!=QMessageBox::Yes) return;
            controller_->setProxyEnabled(true,true); return;
        }
        controller_->setProxyEnabled(!on);
    });
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason==QSystemTrayIcon::DoubleClick) { clickTimer_.stop(); emit showWindow(); return; } if (reason!=QSystemTrayIcon::Trigger) return;
        const auto action=controller_->settings().trayClick;
        if (action=="proxy-page") emit showRoute(workspaceRoute(WorkspaceArea::Proxy,WorkspacePage::Members));
        else if (action=="toggle-proxy") clickTimer_.start(QApplication::doubleClickInterval());
        else emit showWindow();
    });
    connect(&menu_, &QMenu::aboutToShow, this, &TrayController::rebuildMenu);
    connect(controller_, &AppController::stateChanged, this, &TrayController::updateState);
    connect(controller_, &AppController::wireGuardChanged, this, &TrayController::updateState);
    connect(controller_, &AppController::operationFinished, this, [this](bool ok, const QString& text, OperationId id) {
        // 只用气泡承载「没有页面可以显示」的失败（P4.1）：页面级结果已经落在所属页面，
        // 窗口可见时再弹一次就是同一句话出现两遍。
        if (ok || !available_ || controller_->state().exiting) return;
        if (feedbackTarget(id).kind != FeedbackTarget::StatusNotice) return;
        tray_.showMessage("Mlash", text, QSystemTrayIcon::Warning, 5000);
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
QString TrayController::activeProfileName() const {
    const auto id = controller_->state().activeProfile;
    if (id.isEmpty()) return {};
    for (const auto& profile : controller_->profiles()) if (profile.id == id) return profile.name;
    return {};
}
void TrayController::updateState() {
    const auto& state = controller_->state(); const auto key = appearance(state);
    if (key != visualKey_) { visualKey_ = key; tray_.setIcon(statusIcon(state)); }
    auto tooltip = statusTooltip(state, controller_->settings(), activeProfileName());
    const auto tunnel=controller_->wireGuardObservation();
    if (tunnel.value("cleanup_pending").toBool()) tooltip+="\nWireGuard · 待清理";
    else if (tunnel.value("active").toBool()) tooltip+="\nWireGuard · 路由已生效";
    if (tray_.toolTip() != tooltip) tray_.setToolTip(tooltip);
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
    // 跨对象六组（设计 5.9）：窗口 / 代理 / 运行控制 / 订阅 / 页面 / 退出。
    // 托盘只承载动作与状态，不镜像完整页面列表；「节点」不再单独占入口（策略组子菜单已承担）。
    auto* status=menu_.addAction("Mlash · "+(state.ready?QString("内核运行中"):"内核"+coreStateText(state.core)));
    status->setEnabled(false); status->setObjectName("trayStatusHeader"); status->setToolTip(statusTooltip(state, controller_->settings(), activeProfileName())); menu_.setToolTipsVisible(true);
    connect(menu_.addAction("显示主窗口"), &QAction::triggered, this, &TrayController::showWindow);
    menu_.addSeparator();

    const auto tunnel=controller_->wireGuardObservation();
    const auto phase=tunnel.value("phase").toString();
    const bool wireGuardBusy=!phase.isEmpty();
    const bool ownsRoutes=tunnel.value("active").toBool()||tunnel.value("cleanup_pending").toBool()||wireGuardBusy;
    const bool takingOver = state.proxy == ProxyState::Owned || state.tunObserved;
    auto* master = menu_.addAction(takingOver ? "关闭代理" : "开启代理"); master->setObjectName("trayProxyMaster");
    master->setEnabled(enabled && !ownsRoutes && !state.activeProfile.isEmpty());
    connect(master, &QAction::triggered, this, [this] {
        const auto& s = controller_->state(); const bool on = s.proxy == ProxyState::Owned || s.tunObserved;
        if (!on && s.proxy == ProxyState::External) {
            if (QMessageBox::question(nullptr, QStringLiteral("Mlash 接管系统代理"), QStringLiteral("系统已有代理/PAC/自动检测，允许暂时接管并在退出时恢复？")) != QMessageBox::Yes) return;
            controller_->setProxyEnabled(true, true); return;
        }
        controller_->setProxyEnabled(!on);
    });
    auto* proxy = menu_.addAction("系统代理"); proxy->setCheckable(true); proxy->setChecked(state.proxy == ProxyState::Owned); proxy->setEnabled(enabled && !ownsRoutes && (state.ready || state.proxy == ProxyState::Owned));
    connect(proxy, &QAction::triggered, this, [this](bool checked) {
        bool allow = false;
        if (checked && controller_->state().proxy == ProxyState::External) {
            allow = QMessageBox::question(nullptr, QStringLiteral("Mlash 接管系统代理"), QStringLiteral("系统已有代理/PAC/自动检测，允许暂时接管并在退出时恢复？")) == QMessageBox::Yes;
            if (!allow) return;
        }
        controller_->setSystemProxy(checked, allow);
    });
    auto* tun=menu_.addAction("TUN 虚拟网卡"); tun->setObjectName("trayTunSwitch"); tun->setCheckable(true); tun->setChecked(state.tunObserved); tun->setEnabled(enabled&&!ownsRoutes&&!state.readOnly&&!state.activeProfile.isEmpty());
    connect(tun,&QAction::triggered,this,[this](bool checked) { controller_->setTunEnabled(checked); });
    if (wireGuardBusy) {
        menu_.addAction(phase=="connecting"?"WireGuard 正在连接":"WireGuard 正在清理")->setEnabled(false);
    } else if (ownsRoutes) {
        auto* disconnect=menu_.addAction(tunnel.value("active").toBool()?"断开 WireGuard":"重试 WireGuard 清理");
        disconnect->setObjectName("trayWireGuardAction");
        disconnect->setEnabled(enabled);
        connect(disconnect,&QAction::triggered,this,[this] { controller_->disconnectWireGuard(); });
    } else if (!controller_->wireGuardProfiles().isEmpty()) {
        const auto block=tunnel.value("connect_block").toString();
        auto* available=menu_.addMenu(block.isEmpty()?"连接 WireGuard":"WireGuard · "+block);
        available->setObjectName("trayWireGuardProfiles");
        available->setEnabled(enabled&&!state.readOnly&&block.isEmpty());
        for (const auto& item:controller_->wireGuardProfiles()) {
            auto* entry=available->addAction(item.name);
            entry->setEnabled(!item.summary.value("full_v4").toBool()&&!item.summary.value("full_v6").toBool());
            connect(entry,&QAction::triggered,this,[this,id=item.id,version=item.candidate] { controller_->connectWireGuard(id,version); });
        }
    }
    menu_.addSeparator();

    const auto routeLabel = state.mode == "global" ? QString("全局") : state.mode == "direct" ? QString("直连") : QString("规则");
    auto* mode = menu_.addMenu("路由模式 · " + routeLabel); mode->setEnabled(enabled && state.ready);
    for (const auto& entry : QList<QPair<QString, QString>>{{"规则","rule"},{"全局","global"},{"直连","direct"}}) {
        auto* action = mode->addAction(entry.first); action->setCheckable(true); action->setChecked(entry.second == state.mode);
        connect(action, &QAction::triggered, this, [this, entry] { controller_->setMode(entry.second); });
    }
    auto* groups = menu_.addMenu("策略组 · 快速选择"); groups->setEnabled(enabled && state.ready);
    auto* automatic=groups->addAction("展开时测延迟"); automatic->setObjectName("trayAutoTestGroups"); automatic->setCheckable(true); automatic->setChecked(autoTestGroups_); automatic->setToolTip("本次启动有效；复用 60 秒结果");
    connect(automatic,&QAction::toggled,this,[this](bool checked) { autoTestGroups_=checked; if (!checked) { groupDwell_.stop(); controller_->cancelTrayTests(); } }); groups->addSeparator();
    const auto& proxies = controller_->proxies();
    const auto preferences=controller_->nodePreferences();
    const auto common=preferences.value("common_groups").toArray();
    const auto allNames=proxies.keys();
    QStringList names;
    const auto appendGroup=[&](const QString& name) { if (!name.isEmpty() && !names.contains(name) && proxies.value(name).toObject().value("all").isArray()) names.append(name); };
    appendGroup(preferences.value("last_group").toString());
    for (const auto& value:common) { if (names.size()>=5) break; appendGroup(value.toString()); }
    for (const auto& name:allNames) { if (names.size()>=5) break; appendGroup(name); }
    for (const auto& name:names) {
        const auto group=proxies.value(name).toObject(); if (!group.value("all").isArray()) continue;
        auto* sub = groups->addMenu((common.contains(name)?QString("★ "):QString())+name); sub->setProperty("groupName",name); sub->setToolTipsVisible(true); sub->installEventFilter(this);
        connect(sub, &QMenu::aboutToShow, sub, [this, sub, name] {
            groupDwell_.stop(); controller_->cancelTrayTests(); activeGroupMenu_=sub;
            sub->clear(); const auto group = controller_->proxies().value(name).toObject();
            const auto members=group.value("all").toArray();
            const auto current=group.value("now").toString();
            const auto favorites=controller_->nodePreferences().value("favorites").toArray();
            QStringList quick;
            const auto appendMember=[&](const QString& value) { if (!value.isEmpty() && members.contains(value) && !quick.contains(value)) quick.append(value); };
            appendMember(current);
            for (const auto& value:favorites) { if (quick.size()>=6) break; appendMember(value.toString()); }
            for (const auto& member:members) { if (quick.size()>=6) break; appendMember(member.toString()); }
            for (const auto& value:quick) {
                auto* action = sub->addAction(value); action->setCheckable(true); action->setChecked(value == current);
                action->setProperty("memberName",value); const bool nested=controller_->proxies().value(value).toObject().value("all").isArray();
                const auto iconKey=NodePresentation::iconKey(value,nested);
                if (!menuIcons_.contains(iconKey)) menuIcons_.insert(iconKey,NodePresentation::icon(value,nested)); action->setIcon(menuIcons_.value(iconKey));
                connect(action, &QAction::triggered, this, [this, name, value] { controller_->selectMember(name, value); });
            }
            sub->addSeparator();
            if (members.size()>quick.size()) connect(sub->addAction(QString("在主窗口查看全部 %1 个节点…").arg(members.size())),&QAction::triggered,this,[this] { emit showRoute(workspaceRoute(WorkspaceArea::Proxy,WorkspacePage::Members)); });
            auto* retest=sub->addAction("重测本组延迟"); retest->setObjectName("trayRetestGroup"); connect(retest,&QAction::triggered,this,[this,sub,name] { groupDwell_.stop(); if (activeGroupMenu_==sub && sub->isVisible()) controller_->testTrayGroup(name,true); });
            updateGroupMenu(); if (autoTestGroups_) groupDwell_.start();
        });
        connect(sub,&QMenu::aboutToHide,this,[this,sub] { if (activeGroupMenu_!=sub) return; groupDwell_.stop(); activeGroupMenu_=nullptr; controller_->cancelTrayTests(); });
    }
    groups->addSeparator(); auto* browse=groups->addAction("打开完整节点页…"); connect(browse,&QAction::triggered,this,[this] { emit showRoute(workspaceRoute(WorkspaceArea::Proxy,WorkspacePage::Members)); });
    auto* core = menu_.addAction(state.core == CoreState::Running ? "停止内核" : "启动内核"); core->setEnabled(enabled);
    connect(core, &QAction::triggered, this, [this] { if (controller_->state().core == CoreState::Running) controller_->stopCore(); else controller_->startCore(); });
    menu_.addSeparator();

    auto* subscriptions = menu_.addMenu("订阅"); subscriptions->setEnabled(enabled);
    QList<QPair<QString,QString>> quickProfiles;
    const auto appendProfile=[&](const auto& profile) {
        if (quickProfiles.size()<5) quickProfiles.append({profile.id,profile.name});
    };
    for (const auto& profile:controller_->profiles()) if (profile.id==state.activeProfile) appendProfile(profile);
    for (const auto& profile:controller_->profiles()) if (profile.id!=state.activeProfile) appendProfile(profile);
    for (const auto& [id,name]:quickProfiles) {
        auto* action=subscriptions->addAction(name); action->setCheckable(true); action->setChecked(id==state.activeProfile);
        connect(action,&QAction::triggered,this,[this,id] { controller_->activateProfile(id); });
    }
    subscriptions->addSeparator(); auto* update = subscriptions->addAction("更新当前订阅");
    bool canUpdate = false; for (const auto& p : controller_->profiles()) if (p.id == state.activeProfile && p.sourceType == "url") canUpdate = true;
    update->setEnabled(canUpdate);
    connect(update, &QAction::triggered, this, [this] { controller_->updateProfile(controller_->settings().activeProfile); });
    connect(subscriptions->addAction("打开订阅页…"), &QAction::triggered, this, [this] { emit showPage(1); });
    menu_.addSeparator();

    // 页面只保留连接与设置；网站测试、规则、日志只走侧栏，不在托盘重复。
    connect(menu_.addAction("连接…"), &QAction::triggered, this, [this] { emit showPage(4); });
    connect(menu_.addAction("设置…"), &QAction::triggered, this, [this] { emit showPage(6); });
    menu_.addSeparator();
    connect(menu_.addAction("退出 Mlash"), &QAction::triggered, this, &TrayController::exitRequested);
}
}
