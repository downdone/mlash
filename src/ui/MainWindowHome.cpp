#include "MainWindowParts.h"

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

    // ① 当前代理：当前成员名 + 「订阅 · 选择方式」 + 状态句，行尾为系统代理开关。
    auto* currentRow = new QWidget; currentRow->setObjectName("homeControlRow"); currentRow->setProperty("first", true); currentRow->setMinimumHeight(UiTheme::RowHeight);
    auto* currentLayout = new QHBoxLayout(currentRow); currentLayout->setContentsMargins(0, 3, 0, 3); currentLayout->setSpacing(10);
    statusDot_ = new QLabel; statusDot_->setObjectName("statusDot"); statusDot_->setFixedSize(0,0); statusDot_->hide(); auto* currentIcon=new QLabel; currentIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Rocket,UiTheme::Brand).pixmap(22,22)); currentIcon->setFixedSize(22,22); currentLayout->addWidget(currentIcon,0,Qt::AlignVCenter);
    auto* currentText = new QVBoxLayout; currentText->setSpacing(2);
    overviewTitle_ = new QLabel("尚未连接"); overviewTitle_->setObjectName("overviewTitle"); overviewTitle_->setTextFormat(Qt::PlainText);
    homeCurrentMeta_ = new QLabel; homeCurrentMeta_->setObjectName("homeControlMeta"); homeCurrentMeta_->setProperty("secondary", true); homeCurrentMeta_->setTextFormat(Qt::PlainText); homeCurrentMeta_->hide();
    homeStateLine_ = new QLabel; homeStateLine_->setObjectName("homeStateLine"); homeStateLine_->setProperty("secondary", true); homeStateLine_->setTextFormat(Qt::PlainText); homeStateLine_->setWordWrap(true);
    currentText->addWidget(overviewTitle_); currentText->addWidget(homeCurrentMeta_); currentText->addWidget(homeStateLine_);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,currentIcon,[currentIcon] { currentIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Rocket,UiTheme::Brand).pixmap(22,22)); });
    currentLayout->addLayout(currentText, 1);
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
    homeRouteRow_ = new DashboardCardButton; homeRouteRow_->setObjectName("homeRouteRow"); homeRouteRow_->setProperty("controlRow", true); homeRouteRow_->setMinimumHeight(UiTheme::RowHeight);
    auto* routeLayout = new QHBoxLayout(homeRouteRow_); routeLayout->setContentsMargins(0, 3, 0, 3); routeLayout->setSpacing(10);
    auto* routeIcon=new QLabel; routeIcon->setFixedSize(22,22); routeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Route,UiTheme::Brand).pixmap(22,22)); routeLayout->addWidget(routeIcon);
    auto* routeText = new QVBoxLayout; routeText->setSpacing(2);
    auto* routeTitle = new QLabel("全局路由"); routeTitle->setObjectName("homeControlTitle"); routeTitle->setTextFormat(Qt::PlainText);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,routeIcon,[routeIcon] { routeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Route,UiTheme::Brand).pixmap(22,22)); });
    routeText->addWidget(routeTitle); homeRouteRow_->setToolTip("控制代理请求的分流行为");
    routeLayout->addLayout(routeText, 1);
    homeRouteValue_ = new QLabel; homeRouteValue_->setObjectName("homeControlValue"); homeRouteValue_->setTextFormat(Qt::PlainText); routeLayout->addWidget(homeRouteValue_, 0, Qt::AlignVCenter);
    auto* routeChevron = new QLabel("›"); routeChevron->setObjectName("homeControlChevron"); routeChevron->setTextFormat(Qt::PlainText); routeLayout->addWidget(routeChevron, 0, Qt::AlignVCenter);
    // 浮层菜单（原型 03）：每次点击按当前 state.mode 重建勾选状态，菜单项即三个模式。
    connect(homeRouteRow_, &QPushButton::clicked, this, [this] {
        QMenu menu(homeRouteRow_);
        menu.setObjectName("routeModeMenu");
        for (const auto& mode : QList<QPair<QString,QString>>{{"规则模式","rule"},{"全局模式","global"},{"直连模式","direct"}}) {
            auto* action = menu.addAction(mode.first);
            action->setCheckable(true);
            action->setChecked(controller_->state().mode == mode.second);
            action->setProperty("mode", mode.second);
            connect(action, &QAction::triggered, this, [this, mode] { controller_->setMode(mode.second); updateState(); });
        }
        menu.exec(homeRouteRow_->mapToGlobal(QPoint(0, homeRouteRow_->height())));
    });
    auto* firstSeparator=new QFrame; firstSeparator->setObjectName("homeControlSeparator"); firstSeparator->setFixedHeight(1); summary->addWidget(firstSeparator);
    summary->addWidget(homeRouteRow_);
    auto* secondSeparator=new QFrame; secondSeparator->setObjectName("homeControlSeparator"); secondSeparator->setFixedHeight(1); summary->addWidget(secondSeparator);

    // ③ 连通性测试：结果/测试目标 + 「开始测试」，行尾箭头进网站测试页。
    auto* connectivityRow = new QWidget; connectivityRow->setObjectName("homeControlRow"); connectivityRow->setMinimumHeight(UiTheme::RowHeight);
    auto* connectivityLayout = new QHBoxLayout(connectivityRow); connectivityLayout->setContentsMargins(0, 3, 0, 3); connectivityLayout->setSpacing(10);
    auto* probeIcon=new QLabel; probeIcon->setFixedSize(22,22); probeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Gauge,UiTheme::Brand).pixmap(22,22)); connectivityLayout->addWidget(probeIcon);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,probeIcon,[probeIcon] { probeIcon->setPixmap(UiIcons::icon(UiIcons::Glyph::Gauge,UiTheme::Brand).pixmap(22,22)); });
    auto* connectivityText = new QVBoxLayout; connectivityText->setSpacing(2);
    auto* connectivityLabel = new QLabel("连通性测试"); connectivityLabel->setObjectName("homeControlTitle"); connectivityLabel->setTextFormat(Qt::PlainText);
    homeConnectivityCaption_ = new ElidedLabel("尚未测试", this); homeConnectivityCaption_->setObjectName("homeControlMeta"); homeConnectivityCaption_->setProperty("secondary", true); homeConnectivityCaption_->setTextFormat(Qt::PlainText); homeConnectivityCaption_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connectivityText->addWidget(connectivityLabel); connectivityText->addWidget(homeConnectivityCaption_);
    connectivityLayout->addLayout(connectivityText, 1);
    homeConnectivityResult_ = new ElidedLabel(this); homeConnectivityResult_->setObjectName("homeConnectivity"); homeConnectivityResult_->hide();
    homeConnectivityButton_ = new QPushButton("开始测试", this); homeConnectivityButton_->setObjectName("homeConnectivityTest"); homeConnectivityButton_->setProperty("quiet", true); homeConnectivityButton_->setAutoDefault(false); connectivityLayout->addWidget(homeConnectivityButton_, 0, Qt::AlignVCenter);
    connect(homeConnectivityButton_, &QPushButton::clicked, this, [this] { if (controller_->connectivity()->running()) controller_->cancelConnectivityProbe(); else controller_->runConnectivityProbe(); updateHome(); });
    homeConnectivityLink_ = new QToolButton(this); homeConnectivityLink_->setObjectName("homeConnectivityOpen"); homeConnectivityLink_->setText("›"); homeConnectivityLink_->setProperty("quiet", true); homeConnectivityLink_->setToolTip("打开网站测试页查看各目标结果"); connectivityLayout->addWidget(homeConnectivityLink_, 0, Qt::AlignVCenter);
    connect(homeConnectivityLink_, &QToolButton::clicked, this, [this] { navigate(Route{ViewIds::Data, kNoCategory, HomeAnchor::Control, int(ViewIds::DataTab::Tests)}); });
    connect(const_cast<ConnectivityProbe*>(controller_->connectivity()), &ConnectivityProbe::changed, this, &MainWindow::updateHome);
    summary->addWidget(connectivityRow);
    // 内核与接管观测：只读一行，随当前代理行一起显示（不是两行、也不是开关）。
    proxyLabel_ = new QLabel; proxyLabel_->setObjectName("secondaryText"); proxyLabel_->setTextFormat(Qt::PlainText); proxyLabel_->setWordWrap(true); proxyLabel_->hide();
    summary->addWidget(proxyLabel_);
    layout->addWidget(control);
    // 对象内结果（P4.1）：挂到「更多运行信息」抽屉之前的位置无法保证可见，改挂在控制区之后——
    // 「请先激活订阅」这类结论恰恰出现在控制区被隐藏的时候，所以它按原设计留在面板外。
    homeControlHint_ = new ElidedLabel; homeControlHint_->setObjectName("homeControlHint");
    homeControlHint_->setAccessibleName("代理开关与切换结果"); homeControlHint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    homeControlHint_->hide(); layout->addWidget(homeControlHint_);

    // 节点区（原型 01）：标题行 + 策略组上下文，全部来自 nodeSection()。
    nodeSection_ = nodeSection(); layout->addWidget(nodeSection_,1);
    auto* empty=new QWidget; empty->setObjectName("prototypeHomeEmpty"); empty->setMaximumHeight(130); auto* emptyLayout=new QVBoxLayout(empty); emptyLayout->setContentsMargins(16,20,16,20);
    auto* emptyTitle=new QLabel("还没有配置"); emptyTitle->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyTitle);
    auto* emptyText=new QLabel("导入订阅或本地配置后，选择节点并连接。"); emptyText->setAlignment(Qt::AlignCenter); emptyText->setProperty("secondary",true); emptyLayout->addWidget(emptyText);
    auto* import=new QPushButton("导入配置"); import->setProperty("primary",true); emptyLayout->addWidget(import,0,Qt::AlignHCenter); connect(import,&QPushButton::clicked,this,&MainWindow::addUrl); layout->addWidget(empty);
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
    if (auto* empty=findChild<QWidget*>("prototypeHomeEmpty")) { const bool noProfiles=controller_->profiles().isEmpty(); empty->setVisible(noProfiles); nodeSection_->setVisible(!noProfiles); }
    // K5（原型 01）：首页固定五段，控制卡在任何状态下都可见——没有订阅时三行
    // 分别显示「尚未连接 / 规则模式 / 尚未测试」，不用整卡空态替换。
    findChild<QWidget*>("homeControlCard")->setVisible(true);
    findChild<QWidget*>("homeInlineStat")->hide();
    // 控制区第 1 行：当前成员名 + 「订阅 · 选择方式」 + 状态句。
    if (homeCurrentMeta_ && homeStateLine_) {
        const Profile* active=nullptr; for (const auto& profile:controller_->profiles()) if (profile.id==current.activeProfile) active=&profile;
        const auto node = controller_->homeNode();
        overviewTitle_->setText(hasActive&&current.ready ? (node.isEmpty() ? QString("没有唯一当前节点") : node) : QString("未连接"));
        const auto mode = current.mode=="global" ? QString("全局") : current.mode=="direct" ? QString("直连") : QString("规则");
        homeCurrentMeta_->setText(hasActive ? (active ? active->name : QString("订阅未加载")) + " · " + mode : QString("未订阅"));
        homeCurrentMeta_->setVisible(false);
        // 状态句：内核 / 接管 / 可达 三段，只报告观测（P3）。
        const auto core = current.core==CoreState::Running ? (current.ready?QString("内核运行中"):QString("正在确认内核")) : current.core==CoreState::Starting ? QString("内核启动中") : current.core==CoreState::Stopping ? QString("正在停止内核") : current.core==CoreState::Failed ? QString("内核失败") : QString("内核已停止");
        const auto takeover = current.tunObserved ? QString("TUN 已接管") : current.proxy==ProxyState::Owned ? QString("系统代理已接管") : current.proxy==ProxyState::External ? QString("系统代理由其他程序管理") : QString("未接管流量");
        const auto probe = controller_->connectivity()->result().value("status").toString();
        const auto reach = probe=="ok" ? QString("目标可达 · ") + QString::number(controller_->connectivity()->result().value("delay").toInt()) + " ms"
            : probe=="failed" ? QString("目标不可达")
            : probe=="running" ? QString("目标测试中…") : QString();
        const auto reason = current.core==CoreState::Failed && !current.lastError.isEmpty() ? QString("：") + current.lastError : QString();
        homeStateLine_->setText(core + reason + " · " + takeover + (reach.isEmpty() ? QString() : " · " + reach));
        homeStateLine_->setToolTip(homeStateLine_->text());
        const auto detail=homeStateLine_->text();
        homeStateLine_->setText(current.ready?(current.tunObserved?QString("TUN 已接管"):current.proxy==ProxyState::Owned?QString("系统代理已接管"):current.proxy==ProxyState::External?QString("外部系统代理"):QString("系统代理未开启"))+(active?" · "+active->name:QString()):QString("未连接"));
        homeStateLine_->setToolTip(detail);
        homeStateLine_->setVisible(current.core==CoreState::Failed||current.proxy==ProxyState::External||current.readOnly);
        if (current.core==CoreState::Failed) homeStateLine_->setText("连接失败 · "+current.lastError);
    }
    // 控制区第 2 行：当前路由方式。
    if (homeRouteValue_) homeRouteValue_->setText(current.mode=="global"?"全局模式":current.mode=="direct"?"直连模式":"规则模式");
    homeRouteRow_->setEnabled(hasActive && current.ready && !current.busy && !current.exiting && !current.readOnly);
    // 控制区第 3 行：连通性结果与测试按钮。
    if (homeConnectivityButton_ && homeConnectivityLink_ && homeConnectivityCaption_) {
        const auto* probe = controller_->connectivity(); const auto value = probe->result();
        const auto status = value.value("status").toString();
        QString text;
        if (status == "running") text = QString("测试中…");
        else if (status == "ok") {
            const auto measured = QDateTime::fromString(value.value("measured_utc").toString(), Qt::ISODate).toLocalTime();
            text = QString("%1 ms · %2").arg(value.value("delay").toInt()).arg(measured.isValid() ? measured.toString("HH:mm:ss") : QString("时间未知"));
            if (probe->stale()) text += " · 已过期";
        }
        else if (status == "failed") text = QString("失败 · ") + (value.value("error").toString().isEmpty() ? QString("原因未知") : value.value("error").toString());
        else text = QString("尚未测试");
        homeConnectivityCaption_->setText(text); homeConnectivityCaption_->setVisible(!value.isEmpty());
        homeConnectivityCaption_->setToolTip(value.isEmpty() ? QString("测试所选节点到 %1 的响应延迟；只读，不改变节点选择").arg(controller_->settings().testUrl) : value.value("error").toString().isEmpty() ? value.value("url").toString() : value.value("error").toString());
        const bool ready = hasActive && current.ready && !current.busy && !current.exiting && !current.readOnly;
        homeConnectivityButton_->setText(QString()); homeConnectivityButton_->setIcon(UiIcons::icon(status=="running"?UiIcons::Glyph::Close:UiIcons::Glyph::Gauge,UiTheme::Brand)); homeConnectivityButton_->setAccessibleName(status=="running"?"取消连通测试":"测试当前节点连通性");
        homeConnectivityButton_->setEnabled(status == "running" || ready);
        homeConnectivityButton_->setToolTip(ready ? "就地测试当前叶子节点到测试地址的响应延迟" : "内核就绪后可测试连通性");
        homeConnectivityLink_->setVisible(false);
        homeConnectivityLink_->setEnabled(true);
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
    const auto text = !state.ready ? "添加并使用订阅，或启动内核后即可选择节点。" : QString();
    homeSelection_->setToolTip(state.ready ? QString() : text); homeSelection_->setText(text);
}

}
