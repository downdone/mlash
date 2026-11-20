#include "app/AppController.h"
#include "MainWindow.h"
#include "DashboardCardButton.h"
#include "ElidedLabel.h"
#include "HomeMetricsView.h"
#include "NodePresentation.h"
#include "ProxySwitch.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "ViewIds.h"
#include "WindowUiFactories.h"
#include "WidgetStyle.h"
#include <QtWidgets>
#include <algorithm>

namespace mlash {
using namespace WindowParts;
namespace {
void syncHomeChoices(QComboBox* combo, const QJsonArray& choices) {
    if (combo->property("choiceIdentities").toJsonArray()==choices) return;
    combo->setProperty("choiceIdentities",choices);
    combo->clear();
    for (const auto& entry:choices) {
        const auto pair=entry.toArray(); const auto name=pair[0].toString(),key=pair[1].toString();
        if (key.isEmpty()) combo->addItem(name,name);
        else combo->addItem(NodePresentation::icon(name,key=="group"),name,name);
    }
}
}

QWidget* MainWindow::homePage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(4);
    // 控制区（H1 / ADR 0048）：一张卡片、三行——当前代理 / 路由方式 / 连通性。
    // 行尾动作即主控件，标题行不再单独占位（原型 01）。
    auto* control = panel(); control->setObjectName("homeControlCard"); control->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    auto* summary = new QVBoxLayout(control); summary->setContentsMargins(12, 0, 12, 0); summary->setSpacing(0);

    // 每行只表达一个任务；固定同一行高，状态变化不推动后续内容。
    auto* currentRow = new QWidget; currentRow->setObjectName("homeControlRow"); currentRow->setProperty("first", true); currentRow->setFixedHeight(UiTheme::RowHeight);
    auto* currentLayout = new QHBoxLayout(currentRow); currentLayout->setContentsMargins(0, 3, 0, 3); currentLayout->setSpacing(10);
    statusDot_ = new QLabel; statusDot_->setObjectName("statusDot"); statusDot_->setFixedSize(0,0); statusDot_->hide(); auto* currentIcon=new QLabel; currentIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Rocket,UiTheme::Brand).pixmap(22,22)); currentIcon->setFixedSize(22,22); currentLayout->addWidget(currentIcon,0,Qt::AlignVCenter);
    overviewTitle_ = new ElidedLabel("尚未连接"); overviewTitle_->setObjectName("overviewTitle"); overviewTitle_->setTextFormat(Qt::PlainText);
    overviewTitle_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,currentIcon,[currentIcon] { currentIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Rocket,UiTheme::Brand).pixmap(22,22)); });
    currentLayout->addWidget(overviewTitle_, 1, Qt::AlignVCenter);
    // 行尾开关就是代理总开关（ADR 0048 的 setProxyEnabled）；原来的「开启代理」按钮移入
    // 「更多运行信息」，两处同源同命令，只是首屏只留一个入口（原型 01/02）。
    proxyEnabled_ = new ProxySwitch(""); proxyEnabled_->setObjectName("systemProxy"); proxyEnabled_->setAccessibleName("代理总开关 · 系统代理");
    currentLayout->addWidget(proxyEnabled_, 0, Qt::AlignVCenter);
    summary->addWidget(currentRow);
    connect(proxyEnabled_, &QCheckBox::clicked, this, [this](bool enabled) {
        const auto& state = controller_->state();
        // 这个开关是代理总开关：打开时若内核未运行，setProxyEnabled 内部会先启动内核（ADR 0048）。
        if (enabled && state.activeProfile.isEmpty()) { proxyEnabled_->setChecked(false); if (controller_->profiles().isEmpty()) addUrl(); else setPage(1); return; }
        bool allow = false;
        if (enabled && state.proxy == ProxyState::External) {
            allow = QMessageBox::question(this, "接管系统代理", "系统已有代理或 PAC。允许暂时接管，并在退出时恢复？") == QMessageBox::Yes;
            if (!allow) { updateState(); return; }
        }
        controller_->setProxyEnabled(enabled, allow); updateState();
    });

    // ② 路由方式：整行可点，行下方弹出浮层菜单（原型 03）；当前项带勾。
    homeRouteRow_ = new DashboardCardButton; homeRouteRow_->setObjectName("homeRouteRow"); homeRouteRow_->setProperty("controlRow", true); homeRouteRow_->setFixedHeight(UiTheme::RowHeight);
    auto* routeLayout = new QHBoxLayout(homeRouteRow_); routeLayout->setContentsMargins(0, 3, 0, 3); routeLayout->setSpacing(10);
    auto* routeIcon=new QLabel; routeIcon->setFixedSize(22,22); routeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Route,UiTheme::Brand).pixmap(22,22)); routeLayout->addWidget(routeIcon);
    auto* routeTitle = new QLabel("全局路由"); routeTitle->setObjectName("homeControlTitle"); routeTitle->setTextFormat(Qt::PlainText);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,routeIcon,[routeIcon] { routeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Route,UiTheme::Brand).pixmap(22,22)); });
    homeRouteRow_->setToolTip("控制代理请求的分流行为");
    routeLayout->addWidget(routeTitle, 1, Qt::AlignVCenter);
    homeRouteValue_ = new QLabel; homeRouteValue_->setObjectName("homeControlValue"); homeRouteValue_->setTextFormat(Qt::PlainText); routeLayout->addWidget(homeRouteValue_, 0, Qt::AlignVCenter);
    auto* routeChevron = new QLabel("›"); routeChevron->setObjectName("homeControlChevron"); routeChevron->setTextFormat(Qt::PlainText); routeLayout->addWidget(routeChevron, 0, Qt::AlignVCenter);
    // 每次打开从观察状态重建；菜单是临时对象，避免嵌套事件循环。
    connect(homeRouteRow_, &QPushButton::clicked, this, [this] {
        for (auto* old:homeRouteRow_->findChildren<QMenu*>("routeModeMenu")) { old->setObjectName({}); old->close(); }
        auto* menu=new QMenu(homeRouteRow_);
        menu->setObjectName("routeModeMenu");
        auto* choices=new QActionGroup(menu); choices->setExclusive(true);
        for (const auto& mode : QList<QPair<QString,QString>>{{"规则模式 · 按配置规则分流","rule"},{"全局模式 · 所有请求走代理","global"},{"直连模式 · 不经代理转发","direct"}}) {
            auto* action = menu->addAction(mode.first);
            action->setCheckable(true);
            action->setChecked(controller_->state().mode == mode.second);
            action->setProperty("mode", mode.second);
            choices->addAction(action);
            connect(action, &QAction::triggered, this, [this, mode] { controller_->setMode(mode.second); updateState(); });
        }
        connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
        WidgetStyle::popupMenu(menu,homeRouteRow_->mapToGlobal(QPoint(homeRouteRow_->width(),homeRouteRow_->height())),true);
    });
    auto* firstSeparator=new QFrame; firstSeparator->setObjectName("homeControlSeparator"); firstSeparator->setFixedHeight(1); summary->addWidget(firstSeparator);
    summary->addWidget(homeRouteRow_);
    auto* secondSeparator=new QFrame; secondSeparator->setObjectName("homeControlSeparator"); secondSeparator->setFixedHeight(1); summary->addWidget(secondSeparator);

    // ③ 连通性测试：只读测当前浏览策略组的所有可测成员。
    auto* connectivityRow = new QWidget; connectivityRow->setObjectName("homeControlRow"); connectivityRow->setFixedHeight(UiTheme::RowHeight);
    auto* connectivityLayout = new QHBoxLayout(connectivityRow); connectivityLayout->setContentsMargins(0, 3, 0, 3); connectivityLayout->setSpacing(10);
    auto* probeIcon=new QLabel; probeIcon->setFixedSize(22,22); probeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Gauge,UiTheme::Brand).pixmap(22,22)); connectivityLayout->addWidget(probeIcon);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,probeIcon,[probeIcon] { probeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Gauge,UiTheme::Brand).pixmap(22,22)); });
    auto* connectivityLabel = new QLabel("连通性测试"); connectivityLabel->setObjectName("homeControlTitle"); connectivityLabel->setTextFormat(Qt::PlainText);
    connectivityLayout->addWidget(connectivityLabel, 1, Qt::AlignVCenter);
    homeConnectivityButton_ = new QPushButton("开始测试", this); homeConnectivityButton_->setObjectName("homeConnectivityTest"); homeConnectivityButton_->setProperty("quiet", true); homeConnectivityButton_->setAutoDefault(false); connectivityLayout->addWidget(homeConnectivityButton_, 0, Qt::AlignVCenter);
    connect(homeConnectivityButton_, &QPushButton::clicked, this, [this] {
        if (controller_->homeGroupTestStatus().value("running").toBool()) controller_->cancelHomeGroupTests();
        else controller_->startHomeGroupTests(selectedGroup_);
        updateHome();
    });
    connect(controller_, &AppController::homeGroupTestsChanged, this, &MainWindow::updateHome);
    // RowHeight 是视觉下限；开关和图标按钮含内边距后可能更高。
    // 按三个实际控件的上界统一行高，避免 Qt 单独撑开首行。
    const auto syncControlRowHeight=[this,currentRow,connectivityRow] {
        const int height=std::max({UiTheme::RowHeight,proxyEnabled_->sizeHint().height()+6,
            homeConnectivityButton_->sizeHint().height()+6,overviewTitle_->sizeHint().height()+6});
        currentRow->setFixedHeight(height);
        homeRouteRow_->setFixedHeight(height);
        connectivityRow->setFixedHeight(height);
    };
    syncControlRowHeight();
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,currentRow,syncControlRowHeight);
    summary->addWidget(connectivityRow);
    layout->addWidget(control);
    // 对象内结果（P4.1）：挂到「更多运行信息」抽屉之前的位置无法保证可见，改挂在控制区之后——
    // 「请先激活订阅」这类结论恰恰出现在控制区被隐藏的时候，所以它按原设计留在面板外。
    homeControlHint_ = new ElidedLabel; homeControlHint_->setObjectName("homeControlHint");
    homeControlHint_->setAccessibleName("代理开关与切换结果"); homeControlHint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    homeControlHint_->hide(); layout->addWidget(homeControlHint_);

    // 节点区（原型 01）：标题行 + 策略组上下文，全部来自 nodeSection()。
    nodeSection_ = nodeSection(); layout->addWidget(nodeSection_,1);
    auto* empty=new QWidget; empty->setObjectName("prototypeHomeEmpty"); empty->setMaximumHeight(130); auto* emptyLayout=new QVBoxLayout(empty); emptyLayout->setContentsMargins(16,20,16,20);
    auto* emptyTitle=new QLabel; emptyTitle->setObjectName("homeSetupTitle"); emptyTitle->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyTitle);
    auto* emptyText=new QLabel; emptyText->setObjectName("homeSetupHint"); emptyText->setAlignment(Qt::AlignCenter); emptyText->setWordWrap(true); emptyText->setProperty("secondary",true); emptyLayout->addWidget(emptyText);
    auto* import=new QPushButton; import->setObjectName("homeSetupAction"); import->setProperty("primary",true); emptyLayout->addWidget(import,0,Qt::AlignHCenter);
    connect(import,&QPushButton::clicked,this,[this] { if (controller_->profiles().isEmpty()) addUrl(); else navigate(Route{ViewIds::Profiles}); });
    layout->addWidget(empty);
    // 列表底部一行轻量摘要（原型 01 的 inline-stat / 设计文档「只留一行」裁决）。
    auto* stat = new QWidget; stat->setObjectName("homeInlineStat");
    auto* statLayout = new QHBoxLayout(stat); statLayout->setContentsMargins(2, 8, 2, 0); statLayout->setSpacing(11);
    homeInlineStat_ = new QLabel; homeInlineStat_->setObjectName("homeInlineStatText"); homeInlineStat_->setProperty("secondary", true); homeInlineStat_->setTextFormat(Qt::PlainText); homeInlineStat_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); statLayout->addWidget(homeInlineStat_, 1);
    auto* statLink = new QToolButton(stat); statLink->setObjectName("homeStatDetails"); statLink->setText("数据详情 ›"); statLink->setProperty("quiet", true); statLayout->addWidget(statLink);
    connect(statLink, &QToolButton::clicked, this, [this] { navigate(Route{ViewIds::Data, kNoCategory, HomeAnchor::Control, int(ViewIds::DataTab::Connections)}); });
    layout->addWidget(stat);

    // K5（原型 01）：首页只有 控制卡 → 节点标题 → 节点列表 → inline-stat → 提示 五段。
    // 内核启停、代理总开关、指标曲线与出口摘要不再出现在首页：前两者在托盘与设置里已有，
    // 流量指标移到「数据 › 流量」。滚动提示保留为列表底部一行。
    homeSelection_=new ElidedLabel; homeSelection_->setObjectName("homeSelectionHint"); homeSelection_->setProperty("secondary",true); homeSelection_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); layout->addWidget(homeSelection_);
    auto* scroll=new QScrollArea; scroll->setObjectName("homeScroll"); homeScroll_=scroll; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(page); auto* surface=new QWidget;
    auto* outer=new QVBoxLayout(surface); outer->setContentsMargins(0,0,0,0); outer->addWidget(scroll); return surface;
}

void MainWindow::updateHomeOutbound(bool capture) {
    const auto& state=controller_->state(); const auto* diagnostics=controller_->outboundDiagnostics();
    if (!isVisible()||state.exiting||homeOutboundGeneration_!=state.generation||homeOutboundReady_!=state.ready||(homeOutboundSnapshot_.value("route")=="core"&&!state.ready)||diagnostics->busy()) homeOutboundSnapshot_={};
    homeOutboundGeneration_=state.generation; homeOutboundReady_=state.ready;
    const auto& result=diagnostics->result(); const auto route=result.value("route").toString();
    if (capture&&isVisible()&&!state.busy&&!state.exiting&&!diagnostics->busy()&&result.value("kind")!="services"&&result.contains("ip")&&(route=="direct"||(route=="core"&&state.ready))) {
        homeOutboundSnapshot_={}; for (const auto* key:{"ip","country","region","asn","route","mode","measured_utc"}) if (result.value(key).isString()&&!result.value(key).toString().isEmpty()) homeOutboundSnapshot_[key]=result.value(key).toString().left(128);
    }
    QString text,tooltip="不会自动查询公网；通过右侧出口诊断选择传输路径并显式检测。隐藏界面或运行上下文变化清空摘要。";
    if (!homeOutboundSnapshot_.isEmpty()) { const auto& value=homeOutboundSnapshot_; const auto time=QDateTime::fromString(value.value("measured_utc").toString(),Qt::ISODateWithMs).toLocalTime(); const auto path=value.value("route")=="core"?QString("经内核 · %1模式").arg(value.value("mode").toString("未知")):QString("直连"); text=QString("上次出口：%1 · %2 · %3 · %4 · %5").arg(value.value("ip").toString(),value.value("country").toString("地区未知"),value.value("asn").toString("ASN未知"),path,time.isValid()?time.toString("HH:mm:ss"):QString("时间未知")); tooltip=text+"\n地区："+value.value("region").toString("未知")+"\n采样："+(time.isValid()?time.toString("yyyy-MM-dd HH:mm:ss t"):QString("未知"))+"\n打开界面期间的上次检测快照；经内核按当时模式/规则分流，不代表指定叶子出口。"; }
    else if (diagnostics->busy()) text="诊断进行中 · 暂无出口快照";
    homeOutboundSummary_->setText(text); homeOutboundSummary_->setToolTip(tooltip); homeOutboundSummary_->setVisible(!text.isEmpty());
}

void MainWindow::updateHome() {
    const auto& current=controller_->state();
    const bool hasActive=!current.activeProfile.isEmpty();
    if (auto* empty=findChild<QWidget*>("prototypeHomeEmpty")) {
        const bool noProfiles=controller_->profiles().isEmpty();
        empty->findChild<QLabel*>("homeSetupTitle")->setText(noProfiles?"还没有配置":"还未使用配置");
        empty->findChild<QLabel*>("homeSetupHint")->setText(noProfiles?"导入订阅链接或本地配置文件即可开始。":"在配置卡片上点「使用」，然后回到首页开启代理。");
        auto* action=empty->findChild<QPushButton*>("homeSetupAction");
        action->setText(noProfiles?"导入配置":"前往配置");
        action->setEnabled(!current.busy && !current.exiting && (!noProfiles || !current.readOnly));
        empty->setVisible(!hasActive);
        nodeSection_->setVisible(hasActive);
    }
    // 控制卡在任何状态下都保持三行；技术状态和测试进度不额外占行。
    findChild<QWidget*>("homeControlCard")->setVisible(true);
    findChild<QWidget*>("homeInlineStat")->hide();
    // 名称只读；实现状态由 Controller 和全局状态栏维护。
    if (overviewTitle_) {
        const auto node = controller_->homeNode();
        const bool connected=current.proxy==ProxyState::Owned || current.tunObserved;
        const QString status=connected?"已开启代理":hasActive?"代理未开启":"未连接";
        overviewTitle_->setText(node.isEmpty() || !current.ready ? status : node);
    }
    // 控制区第 2 行：当前路由方式。
    if (homeRouteValue_) homeRouteValue_->setText(current.mode=="global"?"全局模式":current.mode=="direct"?"直连模式":"规则模式");
    homeRouteRow_->setEnabled(hasActive && current.ready && !current.busy && !current.exiting && !current.readOnly);
    // 测试结果和进度留在下方成员列表，控制行只提供启动/停止动作。
    if (homeConnectivityButton_) {
        const auto test=controller_->homeGroupTestStatus();
        const bool running=test.value("running").toBool();
        const bool sameProfile=profilesCombo_ && profilesCombo_->currentData().toString()==current.activeProfile;
        const auto group=controller_->proxies().value(selectedGroup_).toObject();
        const bool ready=hasActive && sameProfile && group.value("all").isArray()
            && current.ready && !current.busy && !current.exiting && !current.readOnly;
        homeConnectivityButton_->setText(QString());
        homeConnectivityButton_->setIcon(UiIcons::icon(running?UiIcons::Glyph::Close:UiIcons::Glyph::Gauge,UiTheme::Brand));
        homeConnectivityButton_->setAccessibleName(running?"停止本组测速":"测试当前策略组全部节点");
        homeConnectivityButton_->setEnabled(running || ready);
        homeConnectivityButton_->setToolTip(running?"停止本组测速":ready?"测试当前策略组所有可测成员":"连接并浏览当前配置后可测速");
    }
    if (homeInlineStat_) {
        int activeCount = 0; const auto metrics = controller_->homeMetrics()->snapshot();
        if (metrics.value("active_count").isDouble()) activeCount = metrics.value("active_count").toInt();
        const auto amount = [&metrics](const QString& key) { const auto value = metrics.value(key); return value.isDouble() ? ActivityTableModel::formatBytes(value.toDouble()) + "/s" : QString("—"); };
        const auto mode = current.mode == "global" ? QString("全局") : current.mode == "direct" ? QString("直连") : QString("规则");
        QStringList parts{ current.ready ? QString("内核运行中") : QString("内核未就绪"), mode + "模式" };
        if (current.ready) parts << QString("↓%1").arg(amount("downloadTotal_rate")) << QString("↑%1").arg(amount("uploadTotal_rate")) << QString("%1 条连接").arg(activeCount);
        else parts << QString("%1 个订阅").arg(controller_->profiles().size());
        homeInlineStat_->setText(parts.join(" · "));
        homeInlineStat_->setToolTip(homeInlineStat_->text());
    }
    // 列表底部提示行：只在内核未就绪时给一句下一步，就绪后为空。
    const auto& state = controller_->state();
    const auto text = controller_->profiles().isEmpty() || state.ready ? QString()
        : state.activeProfile.isEmpty() ? QString("使用配置后即可浏览节点。") : QString("连接后即可浏览节点。");
    homeSelection_->setToolTip(state.ready ? QString() : text); homeSelection_->setText(text);
}

}
