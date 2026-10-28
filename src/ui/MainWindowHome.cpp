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
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
    auto* headingRow=new QHBoxLayout; auto* headingTitle=new QLabel("首页"); headingTitle->setObjectName("pageTitle"); headingRow->addWidget(headingTitle); headingRow->addStretch();
    auto* modeControl=new QFrame; modeControl->setObjectName("modeControl"); modeControl->setProperty("homeMode",true); auto* modes=new QHBoxLayout(modeControl); modes->setContentsMargins(3,3,3,3); modes->setSpacing(1);
    for (const auto& mode:QList<QPair<QString,QString>>{{"规则","rule"},{"全局","global"},{"直连","direct"}}) {
        auto* control=button(mode.first,modes); control->setCheckable(true); control->setProperty("mode",mode.second); control->setAccessibleName("首页代理模式 · "+mode.first); proxyModes_.append(control);
        connect(control,&QPushButton::clicked,this,[this,mode] { controller_->setMode(mode.second); updateState(); });
    }
    headingRow->addWidget(modeControl); layout->addLayout(headingRow);
    auto* overview = panel(); overview->setObjectName("overview"); auto* summary = new QVBoxLayout(overview); summary->setContentsMargins(16, 14, 16, 14);
    auto* row = new QHBoxLayout; statusDot_ = new QLabel("●"); statusDot_->setObjectName("statusDot"); row->addWidget(statusDot_); overviewTitle_ = new QLabel("尚未连接"); overviewTitle_->setObjectName("overviewTitle"); row->addWidget(overviewTitle_, 1); auto* tunSettings=new QPushButton("TUN 设置…"); tunSettings->setObjectName("homeTunSettings"); tunSettings->setProperty("quiet",true); row->addWidget(tunSettings); connect(tunSettings,&QPushButton::clicked,this,[this] { setPage(6); findChild<QTabBar*>("settingsCategories")->setCurrentIndex(1); findChild<QPushButton*>("manageTunService")->click(); });
    proxyEnabled_ = new ProxySwitch("系统代理"); proxyEnabled_->setObjectName("systemProxy");
    tunEnabled_=new ProxySwitch("TUN"); tunEnabled_->setObjectName("homeTunSwitch");
    connect(tunEnabled_,&QCheckBox::clicked,this,[this](bool enabled) { controller_->setTunEnabled(enabled); updateState(); });
    coreAction_ = new QAction("启动内核", this); coreAction_->setObjectName("coreAction"); coreButton_ = actionButton(coreAction_, headingRow, "homeCoreControl");
    connect(coreAction_, &QAction::triggered, this, [this] { if (controller_->state().core == CoreState::Running) controller_->stopCore(); else if (controller_->state().activeProfile.isEmpty()) { if (controller_->profiles().isEmpty()) addUrl(); else setPage(1); } else controller_->startCore(); });
    connect(proxyEnabled_, &QCheckBox::clicked, this, [this](bool enabled) {
        bool allow = false;
        if (enabled && controller_->state().proxy == ProxyState::External) {
            allow = QMessageBox::question(this, "接管系统代理", "系统已有代理或 PAC。允许暂时接管，并在退出时恢复？") == QMessageBox::Yes;
            if (!allow) { updateState(); return; }
        }
        controller_->setSystemProxy(enabled, allow); updateState();
    });
    summary->addLayout(row); auto* observed = new QHBoxLayout; auto* observations=new QVBoxLayout; observations->setSpacing(2); coreLabel_ = new QLabel; coreLabel_->setObjectName("secondaryText"); proxyLabel_ = new QLabel; proxyLabel_->setObjectName("secondaryText"); observations->addWidget(coreLabel_); observations->addWidget(proxyLabel_); observed->addLayout(observations,1); observed->addWidget(proxyEnabled_); observed->addSpacing(8); observed->addWidget(tunEnabled_); summary->addLayout(observed); layout->addWidget(overview);
    auto* onboarding=panel(); onboarding->setObjectName("homeOnboarding"); auto* welcomeLayout=new QVBoxLayout(onboarding); welcomeLayout->setContentsMargins(24,28,24,28); auto* welcomeTitle=new QLabel("添加订阅，开始使用"); welcomeTitle->setObjectName("homeOnboardingTitle"); welcomeTitle->setProperty("role","headline"); auto* welcomeDetail=new QLabel; welcomeDetail->setObjectName("homeOnboardingDetail"); welcomeDetail->setWordWrap(true); welcomeDetail->setProperty("secondary",true); welcomeLayout->addWidget(welcomeTitle); welcomeLayout->addWidget(welcomeDetail); auto* next=new QPushButton("添加订阅…"); next->setObjectName("homeNextStep"); next->setProperty("primary",true); welcomeLayout->addWidget(next,0,Qt::AlignLeft); connect(next,&QPushButton::clicked,coreAction_,&QAction::trigger); layout->addWidget(onboarding);
    auto* dashboard = new QWidget; dashboard->setObjectName("dashboard"); auto* grid = new QGridLayout(dashboard); grid->setContentsMargins(0,0,0,0); grid->setSpacing(12); for (int column=0;column<2;++column) grid->setColumnStretch(column,1); homeSummaryRow_=new QWidget; homeSummaryRow_->setObjectName("homeSecondarySummary"); auto* summaryCards=new QHBoxLayout(homeSummaryRow_); summaryCards->setContentsMargins(0,0,0,0); summaryCards->setSpacing(12);
    const QStringList names{"订阅","节点","规则","连接","日志","设置"};
    for (int i=0;i<names.size();++i) {
        QWidget* card=i<2?static_cast<QWidget*>(new QFrame):static_cast<QWidget*>(new DashboardCardButton); card->setObjectName("dashboardCard"); card->setProperty("page",i+1); card->setProperty("tier",i<2?"primary":"secondary"); card->setAccessibleName(names[i]+"摘要"); card->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        auto* content=new QVBoxLayout(card); content->setContentsMargins(16,14,16,14); content->setSpacing(6); auto* top=new QHBoxLayout; top->setSpacing(7);
        auto* icon=new QLabel; icon->setObjectName("dashboardIcon"); icon->setFixedSize(24,24); icon->setAlignment(Qt::AlignCenter); icon->setPixmap(navigationIcon(i+1).pixmap(18,18,QIcon::Normal)); top->addWidget(icon);
        auto* title=new QLabel(i==0?"当前订阅":i==1?"策略组":names[i]); title->setObjectName(i==1?"homeMemberHeading":"secondaryText"); title->setProperty("secondary",true); top->addWidget(title,i==1?0:1); auto* arrow=new QLabel("›",card); arrow->setObjectName("secondaryText");
        if (i==1) { homeQuickGroup_=new CompactComboBox; homeQuickGroup_->setObjectName("homeQuickGroup"); homeQuickGroup_->setAccessibleName("首页浏览策略组"); homeQuickGroup_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); top->addWidget(homeQuickGroup_,1); }
        if (i<2) { auto* navigate=new QPushButton(i==0?"订阅 ›":"节点 ›"); navigate->setObjectName("summaryNavigate"); navigate->setProperty("page",i+1); navigate->setProperty("quiet",true); navigate->setAutoDefault(false); top->addWidget(navigate); connect(navigate,&QPushButton::clicked,this,[this,i] { if (i==1) profilesCombo_->setCurrentIndex(profilesCombo_->findData(controller_->state().activeProfile)); setPage(i+1); }); arrow->hide(); }
        else top->addWidget(arrow); content->addLayout(top);
        auto* value=new ElidedLabel; value->setObjectName("statValue"); value->setProperty("summaryPage",i+1); value->setProperty("tier",i<2?"primary":"secondary"); value->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); auto* detail=new ElidedLabel; detail->setObjectName("secondaryText"); detail->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        if (i==1) { auto* member=new QHBoxLayout; member->setSpacing(8); homeMemberIcon_=new QLabel; homeMemberIcon_->setObjectName("homeMemberIcon"); homeMemberIcon_->setFixedSize(24,22); homeMemberIcon_->setAttribute(Qt::WA_TransparentForMouseEvents); member->addWidget(homeMemberIcon_); member->addWidget(value,1); content->addLayout(member); } else content->addWidget(value);
        content->addWidget(detail); if (i==0) detail->hide();
        if (i==1) { detail->hide(); auto* quick=new QHBoxLayout; auto* label=new QLabel("节点"); label->setProperty("secondary",true); label->setObjectName("homeMemberLabel"); quick->addWidget(label); homeQuickMember_=new CompactComboBox; homeQuickMember_->setObjectName("homeQuickMember"); homeQuickMember_->setAccessibleName("选择待切换节点，点击切换后生效"); homeQuickMember_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); quick->addWidget(homeQuickMember_,1); auto* switchNode=new QPushButton("切换"); switchNode->setObjectName("homeSwitchNode"); switchNode->setProperty("quiet",true); quick->addWidget(switchNode); content->addLayout(quick); connect(switchNode,&QPushButton::clicked,this,[this] { if (homeQuickMember_->isEnabled()) controller_->selectMember(homeQuickGroup_->currentData().toString(),homeQuickMember_->currentData().toString()); }); connect(homeQuickGroup_,&QComboBox::activated,this,[this] { selectGroup(homeQuickGroup_->currentData().toString()); }); connect(homeQuickMember_,&QComboBox::activated,this,[this] { homeQuickMember_->setProperty("pendingGroup",homeQuickGroup_->currentData().toString()); homeQuickMember_->setProperty("pendingMember",homeQuickMember_->currentData().toString()); homeQuickMember_->setProperty("pendingGeneration",QString::number(controller_->state().generation)); updateHome(); }); }

        if (i<2) { auto* meta=new ElidedLabel; meta->setObjectName(i==0?"homeProfileMeta":"homeProxyCounts"); meta->setProperty("secondary",true); meta->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); meta->setAttribute(Qt::WA_TransparentForMouseEvents); meta->setTextFormat(Qt::PlainText); content->addWidget(meta); if (i==0) homeProfileMeta_=meta; else homeCounts_=meta; if (i==1) meta->hide(); }
        for (auto* label:QList<QLabel*>{title,value,detail,icon,arrow}) { label->setAttribute(Qt::WA_TransparentForMouseEvents); label->setTextFormat(Qt::PlainText); }
        dashboardValues_.append(value); dashboardDetails_.append(detail); homeSummaryCards_.append(card); if (i<2) grid->addWidget(card,0,i); else summaryCards->addWidget(card,1);
        if (auto* clickable=qobject_cast<QPushButton*>(card)) connect(clickable,&QPushButton::clicked,this,[this,i] { if (i==2 && !rulesDirty_) { rulesProfiles_->setCurrentIndex(rulesProfiles_->findData(controller_->state().activeProfile)); } setPage(i+1); });
    }
    profileSummary_=dashboardValues_[0]; layout->addWidget(dashboard);
    auto* more=new QToolButton; more->setObjectName("homeDetailsDisclosure"); more->setText("更多运行信息"); more->setCheckable(true); more->setArrowType(Qt::RightArrow); more->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto* extras=new QWidget; extras->setObjectName("homeAdditionalInfo"); auto* extrasLayout=new QVBoxLayout(extras); extrasLayout->setContentsMargins(0,0,0,0); extrasLayout->setSpacing(10); extrasLayout->addWidget(homeSummaryRow_);
    connect(more,&QToolButton::toggled,this,[this,more](bool checked) { more->setArrowType(checked?Qt::DownArrow:Qt::RightArrow); updateHome(); });
    auto* probeRow=new QHBoxLayout; probeRow->addWidget(new QLabel("当前节点探测")); homeProbeInterval_=new CompactComboBox; homeProbeInterval_->setObjectName("homeProbeInterval"); homeProbeInterval_->addItem("关闭",0); homeProbeInterval_->addItem("每60秒",60); homeProbeInterval_->addItem("每5分钟",300); homeProbeInterval_->setToolTip("仅首页可见时检测当前显示组的实际叶子；全局/直连模式使用其实际出口。不改变节点选择。"); probeRow->addWidget(homeProbeInterval_); homeProbeResult_=new ElidedLabel; homeProbeResult_->setObjectName("homeProbeResult"); homeProbeResult_->setTextFormat(Qt::PlainText); homeProbeResult_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); probeRow->addWidget(homeProbeResult_,1); extrasLayout->addLayout(probeRow);
    connect(homeProbeInterval_,&QComboBox::activated,this,[this] { controller_->updateHomeProbe(homeProbeInterval_->currentData().toInt()); }); connect(controller_->homeProbe(),&HomeNodeProbe::changed,this,&MainWindow::updateHome);
    auto* metrics=new HomeMetricsView(controller_->homeMetrics()); connect(metrics,&HomeMetricsView::inspectRequested,this,[this] { setPage(7); });
    homeOutboundSummary_=new ElidedLabel; homeOutboundSummary_->setObjectName("homeOutboundSummary"); homeOutboundSummary_->setProperty("secondary",true); homeOutboundSummary_->setTextFormat(Qt::PlainText); homeOutboundSummary_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); auto* metricsLayout=qobject_cast<QVBoxLayout*>(metrics->layout()); auto* graph=metrics->findChild<QWidget*>("homeTrafficGraph"); metricsLayout->removeWidget(graph); auto* observation=new QHBoxLayout; observation->addWidget(homeOutboundSummary_,2); observation->addWidget(graph,1); metricsLayout->insertLayout(1,observation); connect(controller_->outboundDiagnostics(),&OutboundDiagnostics::changed,this,[this] { updateHomeOutbound(true); }); connect(controller_,&AppController::stateChanged,this,[this] { updateHomeOutbound(); }); updateHomeOutbound(); layout->addWidget(metrics); layout->addWidget(more,0,Qt::AlignLeft); layout->addWidget(extras); layout->addStretch(1);
    homeSelection_=new ElidedLabel; homeSelection_->setObjectName("secondaryText"); homeSelection_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); extrasLayout->addWidget(homeSelection_);
    auto* scroll=new QScrollArea; scroll->setObjectName("homeScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(page); auto* surface=new QWidget; auto* outer=new QVBoxLayout(surface); outer->setContentsMargins(0,0,0,0); outer->addWidget(scroll); return surface;
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
    auto* more=findChild<QToolButton*>("homeDetailsDisclosure");
    const bool expanded=hasActive&&more->isChecked();
    const int mask=controller_->settings().homeSummaryMask; displayedHomeSummaryMask_=mask;
    for (int bit=0;bit<4;++bit) homeSummaryCards_[bit+2]->setVisible(mask&(1<<bit));
    homeSummaryRow_->setVisible(mask!=0);
    findChild<QWidget*>("homeAdditionalInfo")->setVisible(expanded);
    more->setVisible(hasActive);
    findChild<QWidget*>("dashboard")->setVisible(hasActive);
    findChild<QWidget*>("homeMetrics")->setVisible(hasActive&&current.ready);
    findChild<QWidget*>("homeOnboarding")->setVisible(!hasActive); findChild<QFrame*>("overview")->setVisible(hasActive);
    const bool empty=controller_->profiles().isEmpty();
    findChild<QLabel*>("homeOnboardingTitle")->setText(empty?"还没有订阅":"选择订阅");
    findChild<QLabel*>("homeOnboardingDetail")->setText(empty?"添加订阅链接、配置文件或节点链接。":"选择一份订阅，使用并启动内核。"); findChild<QPushButton*>("homeNextStep")->setText(empty?"添加订阅…":"选择订阅"); findChild<QPushButton*>("homeNextStep")->setEnabled(coreAction_->isEnabled()); coreButton_->setVisible(hasActive);
    coreLabel_->hide();
    proxyLabel_->setVisible(hasActive); proxyEnabled_->setVisible(hasActive); tunEnabled_->setVisible(hasActive);
    for (auto* frame:findChildren<QFrame*>("modeControl")) if (frame->property("homeMode").toBool()) frame->setVisible(hasActive);
    homeSelection_->hide();

    controller_->setHomeProbeGroup(profilesCombo_->currentData().toString()==controller_->state().activeProfile?selectedGroup_:QString());
    { const QSignalBlocker block(homeProbeInterval_); homeProbeInterval_->setCurrentIndex(homeProbeInterval_->findData(controller_->settings().homeProbeSeconds)); }
    homeProbeInterval_->setEnabled(!controller_->state().busy&&!controller_->state().readOnly&&!controller_->state().exiting);
    const auto probe=controller_->homeProbe()->result(); const auto probeText=!controller_->settings().homeProbeSeconds?QString():probe.isEmpty()?QString("等待可检测的当前叶子节点"):probe.value("node").toString()+" · "+(probe.value("pending").toBool()?QString("检测中…"):probe.value("delay").toInt(-1)>0?QString::number(probe.value("delay").toInt())+" ms":QString("本次失败")); homeProbeResult_->setText(probeText); homeProbeResult_->setToolTip(probeText+"\n"+probe.value("error").toString()+"\n"+probe.value("measured_utc").toString());
    const auto& state = controller_->state(); const auto& proxies = controller_->proxies();
    QStringList groups; int nodes = 0;
    if (state.ready) for (auto it = proxies.begin(); it != proxies.end(); ++it) {
        const auto item = it.value().toObject();
        if (item.value("all").isArray()) groups.append(it.key());
        else if (!QStringList{"Direct", "Reject", "RejectDrop", "Pass"}.contains(item.value("type").toString())) ++nodes;
    }
    const Profile* active=nullptr; int pending=0; for (const auto& profile:controller_->profiles()) { if (profile.id==state.activeProfile) active=&profile; if (!profile.active.isEmpty() && profile.candidate!=profile.active) ++pending; }
    dashboardValues_[0]->setText(active?active->name:"尚未使用订阅"); dashboardDetails_[0]->setText(QString("%1 个订阅").arg(controller_->profiles().size())+(pending?QString(" · %1 个有更新待使用").arg(pending):active?" · 当前版本已使用":" · 添加并使用后连接"));
    const auto updated=active?QDateTime::fromString(active->updated,Qt::ISODate).toLocalTime():QDateTime{};
    homeProfileMeta_->setText(!active?"URL 或本地配置文件":(active->sourceType=="url"?QString("URL 订阅"):QString("文件快照"))+(updated.isValid()?" · 最近获取 "+updated.toString("MM-dd HH:mm"):" · 尚无更新时间")); if (active&&active->candidate!=active->active) homeProfileMeta_->setText(homeProfileMeta_->text()+" · 有更新待应用"); homeProfileMeta_->setToolTip(homeProfileMeta_->text());
    homeCounts_->setText(state.ready?QString("%1 策略组 · %2 节点").arg(groups.size()).arg(nodes):"运行后显示配置规模"); homeCounts_->setToolTip(homeCounts_->text());
    {
        const QSignalBlocker a(homeQuickGroup_),b(homeQuickMember_);
        auto groupNames=groups;
        if (state.mode=="global") groupNames={"GLOBAL"};
        if (state.mode=="direct") groupNames.clear();
        QJsonArray groupChoices,memberChoices;
        for (const auto& name:groupNames) groupChoices.append(QJsonArray{name,QString()});
        const auto group=state.mode=="global"?QString("GLOBAL"):selectedGroup_;
        const auto object=proxies.value(group).toObject();
        for (const auto& member:object.value("all").toArray()) {
            const auto name=member.toString();
            memberChoices.append(QJsonArray{name,NodePresentation::iconKey(name,proxies.value(name).toObject().value("all").isArray())});
        }
        syncHomeChoices(homeQuickGroup_,groupChoices);
        syncHomeChoices(homeQuickMember_,memberChoices);
        const auto pending=homeQuickMember_->property("pendingMember").toString(); const bool keepPending=state.ready&&!state.busy&&homeQuickMember_->property("pendingGroup").toString()==group&&homeQuickMember_->property("pendingGeneration").toString()==QString::number(state.generation)&&homeQuickMember_->findData(pending)>=0;
        const int groupIndex=homeQuickGroup_->findData(group),memberIndex=homeQuickMember_->findData(keepPending?pending:object.value("now").toString());
        if (!keepPending) homeQuickMember_->setProperty("pendingMember",QVariant());
        if (homeQuickGroup_->currentIndex()!=groupIndex) homeQuickGroup_->setCurrentIndex(groupIndex);
        if (homeQuickMember_->currentIndex()!=memberIndex) homeQuickMember_->setCurrentIndex(memberIndex);
 homeQuickGroup_->setEnabled(state.ready&&!state.busy&&state.mode=="rule"); homeQuickMember_->setEnabled(state.ready&&!state.busy&&QStringList{"Selector","URLTest","Fallback"}.contains(object.value("type").toString()));
    }
    homeQuickGroup_->setVisible(state.ready&&state.mode!="direct"&&homeQuickGroup_->count()>0); homeQuickMember_->setVisible(state.ready&&state.mode!="direct"&&homeQuickMember_->count()>0);
    findChild<QLabel*>("homeMemberLabel")->setVisible(!homeQuickMember_->isHidden()); auto* switchNode=findChild<QPushButton*>("homeSwitchNode"); switchNode->setVisible(!homeQuickMember_->isHidden()); const bool automatic=QStringList{"URLTest","Fallback"}.contains(proxies.value(homeQuickGroup_->currentData().toString()).toObject().value("type").toString()); switchNode->setText(automatic?"固定":"切换"); switchNode->setToolTip(automatic?"固定自动策略组到选中节点；在节点页可恢复自动选择。":"切换到选中节点，成功回读后生效。"); switchNode->setEnabled(homeQuickMember_->isEnabled()&&homeQuickMember_->currentData().toString()!=proxies.value(homeQuickGroup_->currentData().toString()).toObject().value("now").toString());
    findChild<QLabel*>("homeMemberHeading")->setText(state.mode=="direct"?"当前出口":state.mode=="global"?"全局组":"策略组");
    const auto now=controller_->homeNode(); dashboardValues_[1]->setText(!state.ready?"内核未就绪":now.isEmpty()?"没有唯一当前节点":now);
    auto memberDetail=state.mode=="global"?QString("全局模式 · GLOBAL"):state.mode=="direct"?QString("直连模式"):selectedGroup_.isEmpty()?QString("选择策略组，再选择节点"):"所选策略组 · "+selectedGroup_; const auto memberRow=model_.names().indexOf(now); if (memberRow>=0 && profilesCombo_->currentData().toString()==state.activeProfile) { const auto delay=model_.data(model_.index(memberRow,2)).toString(); if (delay.endsWith(" ms")) memberDetail+=" · "+delay; } dashboardDetails_[1]->setText(memberDetail); dashboardDetails_[1]->setToolTip(memberDetail+"\n"+homeCounts_->text());
    homeMemberIcon_->setVisible(!now.isEmpty()); if (!now.isEmpty()) { homeMemberIcon_->setPixmap(NodePresentation::icon(now,proxies.value(now).toObject().value("all").isArray()).pixmap(24,18)); homeMemberIcon_->setToolTip("位置图标依据名称，不代表实际 IP 定位"); }
    dashboardValues_[2]->setText(!active?"尚未使用订阅":active->customRules.isEmpty()&&active->appendRules.isEmpty()&&active->disabledRules.isEmpty()?"沿用订阅规则":QString("前置 %1 · 追加 %2 · 屏蔽 %3").arg(active->customRules.size()).arg(active->appendRules.size()).arg(active->disabledRules.size())); dashboardDetails_[2]->setText(!state.ready?"等待内核就绪":state.mode=="rule"?"规则模式":"当前模式不匹配规则");
    const auto metrics=controller_->homeMetrics()->snapshot(); const auto count=metrics.value("active_count"); dashboardValues_[3]->setText(!state.ready?"内核未就绪":count.isDouble()?QString("%1 条活动连接").arg(count.toInt()):controller_->homeMetrics()->error().isEmpty()?"等待采样":"采样暂不可用"); dashboardDetails_[3]->setText("仅首页可见时刷新");
    dashboardValues_[4]->setText(QString("%1 行运行记录").arg(controller_->diagnostics()->lines().size())); dashboardDetails_[4]->setText(state.lastError.isEmpty()?"查看运行记录":"有待处理信息");
    dashboardValues_[5]->setText(controller_->settings().lightweight?"关闭窗口释放界面":"关闭窗口仅隐藏"); dashboardDetails_[5]->setText(QString("登录启动%1").arg(!state.autoStartKnown?"待确认":state.autoStartConflict?"路径冲突":state.autoStartEnabled?"开启":"关闭"));
    for (int i=0;i<dashboardValues_.size();++i) { dashboardValues_[i]->setToolTip(dashboardValues_[i]->text()); dashboardDetails_[i]->setToolTip(dashboardDetails_[i]->text()); }
    const auto text = !state.ready ? "添加并使用订阅，或启动内核后即可选择节点。" : state.tunObserved ? (state.proxy==ProxyState::Owned ? "TUN 与系统代理均已开启；流量按当前模式分流。" : "TUN 已接管流量；系统代理保持独立设置。") : state.proxy==ProxyState::Owned ? "系统代理已接管；遵循系统代理的应用会使用当前分流配置。" : state.proxy==ProxyState::External ? "系统代理由其他程序管理；开启上方开关可确认接管。" : state.proxy==ProxyState::Unknown ? "系统代理状态待确认，可在日志页查看原因。" : "系统代理未开启；使用本地地址的应用仍可连接内核。";
    homeSelection_->setToolTip(text); homeSelection_->setText(homeSelection_->fontMetrics().elidedText(text, Qt::ElideMiddle, qMax(240, homeSelection_->width())));
}

}
