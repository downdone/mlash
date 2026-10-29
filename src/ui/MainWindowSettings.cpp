#include "MainWindowParts.h"

namespace mlash {
using namespace WindowParts;
void MainWindow::selectSettingsCategory(int index) {
    if (index<0||index>=settingsSections_.size()) return;
    for (int i=0;i<settingsSections_.size();++i) settingsSections_[i]->setVisible(i==index);
    settingsPage_->setProperty("settingsCategory",index); settingsPage_->layout()->invalidate(); settingsPage_->layout()->activate(); settingsPage_->updateGeometry();
    settingsScroll_->verticalScrollBar()->setValue(0);
    settingsCategories_->setAccessibleName("设置分类："+settingsCategories_->tabText(index));
}

// Rows that need a core restart keep an explicit action; they are the only reason the bar appears.
int MainWindow::networkSettingCount() const {
    const auto& s=controller_->settings(); int count=0;
    for (bool changed:{mixedPort_->value()!=s.mixedPort,logLevel_->currentData().toString()!=s.logLevel,corePath_->text().trimmed()!=s.corePath,ipv6Mode_->currentData().toString()!=s.ipv6Mode,unifiedDelayMode_->currentData().toString()!=s.unifiedDelayMode}) count+=changed;
    return count;
}

bool MainWindow::settingsDirty() const { return networkSettingCount()>0; }

void MainWindow::commitInstantSettings() {
    if (settingsSyncing_) return;
    const auto& current=controller_->settings();
    auto next=current;
    next.autoStart=autoStart_->isChecked(); next.silentStart=silentStart_->isChecked(); next.startCore=startCore_->isChecked();
    next.restoreProxy=restoreProxy_->isChecked(); next.lightweight=lightweight_->isChecked();
    next.startupPage=startupPage_->currentData().toInt(); next.trayClick=trayClick_->currentData().toString();
    next.appearance=appearance_->currentData().toString(); next.textScale=textScale_->currentData().toInt();
    // The address field also marks a restart-class change until its own explicit apply, so take it only on commit.
    if (auto* edit=qobject_cast<QLineEdit*>(sender())) if (edit==testUrl_) next.testUrl=testUrl_->text().trimmed();
    const bool unchanged=next.autoStart==current.autoStart&&next.silentStart==current.silentStart&&next.startCore==current.startCore&&next.restoreProxy==current.restoreProxy&&next.lightweight==current.lightweight&&next.startupPage==current.startupPage&&next.trayClick==current.trayClick&&next.appearance==current.appearance&&next.textScale==current.textScale&&next.testUrl==current.testUrl;
    if (unchanged) return;
    // Instant preferences must not carry an unrelated pending restart value into the controller.
    next.mixedPort=current.mixedPort; next.logLevel=current.logLevel; next.corePath=current.corePath; next.ipv6Mode=current.ipv6Mode; next.unifiedDelayMode=current.unifiedDelayMode;
    controller_->updateSettings(next);
}

void MainWindow::resetSettingsDraft() { settingsFeedback_.clear(); hasDisplayedSettings_=false; updateProfiles(); updateActions(); }

void MainWindow::applySettings() {
    settingsApplying_=true; settingsFeedback_="正在校验并应用…";
    // Carries whatever instant preferences are already saved plus this explicit network change.
    auto s = controller_->settings(); s.mixedPort = mixedPort_->value(); s.logLevel = logLevel_->currentData().toString(); s.corePath = corePath_->text().trimmed(); s.ipv6Mode=ipv6Mode_->currentData().toString(); s.unifiedDelayMode=unifiedDelayMode_->currentData().toString(); controller_->updateSettings(s);
}
QWidget* MainWindow::settingsPage() {
    auto* page = new QWidget; page->setObjectName("settingsSurface"); page->setAttribute(Qt::WA_StyledBackground, true); auto* layout = new QGridLayout(page); layout->setContentsMargins(0,0,0,0); layout->setSpacing(12); layout->setColumnStretch(0,1); layout->setColumnStretch(1,1);
    auto* startupPanel = panel(); startupPanel->setObjectName("settingsApplication"); startupPanel->setProperty("settingsColumn",0); auto* startupLayout = new QVBoxLayout(startupPanel); startupLayout->setContentsMargins(16,16,16,8); startupLayout->setSpacing(0);
    auto* startupTitle = new QLabel("常规"); startupTitle->setObjectName("sectionTitle"); startupLayout->addWidget(startupTitle); startupTitle->hide(); layout->addWidget(startupPanel,0,0,1,2,Qt::AlignTop);
    const auto groupTitle=[](QVBoxLayout* column,const QString& text) { column->addSpacing(16); auto* title=new QLabel(text); title->setObjectName("settingsSubheading"); title->setProperty("secondary",true); column->addWidget(title); };
    groupTitle(startupLayout,"启动与托盘");
    autoStart_ = new ProxySwitch; autoStart_->setObjectName("autoStart"); silentStart_=new ProxySwitch; silentStart_->setObjectName("silentStart"); startCore_=new ProxySwitch; startCore_->setObjectName("startCore"); restoreProxy_=new ProxySwitch; restoreProxy_->setObjectName("restoreProxy"); lightweight_=new ProxySwitch; lightweight_->setObjectName("lightweightMode");
    for (auto* toggle:{autoStart_,silentStart_,startCore_,restoreProxy_,lightweight_}) toggle->setFixedWidth(54);
    settingRow(startupLayout,"登录启动","随 Windows 登录启动 Mlash",autoStart_); settingRow(startupLayout,"启动时隐藏窗口","已有订阅时直接进入托盘",silentStart_); settingRow(startupLayout,"启动内核","自动运行上次使用的订阅",startCore_); settingRow(startupLayout,"启动时恢复代理开关","按上次的开关状态恢复，不接管其他程序的代理",restoreProxy_);
    settingRow(startupLayout,"关闭窗口时释放界面","释放界面资源；内核与托盘继续运行",lightweight_); lightweight_->setToolTip("托盘打开时重新创建界面；未保存内容会提示确认");
    startupPage_=new CompactComboBox; startupPage_->setObjectName("startupPage"); for (const auto& pair:QList<QPair<QString,int>>{{"首页",0},{"订阅",1},{"节点",2},{"网站测试",7},{"规则",3},{"连接",4},{"日志",5},{"设置",6}}) startupPage_->addItem(pair.first,pair.second); startupPage_->setFixedWidth(140); settingRow(startupLayout,"启动页面","首次打开主界面时显示",startupPage_);
    trayClick_=new CompactComboBox; trayClick_->setObjectName("trayClick"); trayClick_->addItem("显示主窗口","show"); trayClick_->addItem("打开节点页","proxy-page"); trayClick_->addItem("切换系统代理","toggle-proxy"); trayClick_->setFixedWidth(140); trayClick_->setToolTip("切换代理不会自动覆盖外部程序的代理"); settingRow(startupLayout,"托盘单击","双击始终显示主界面",trayClick_);
    groupTitle(startupLayout,"外观与首页");
    appearance_=new CompactComboBox; appearance_->setObjectName("appearancePreference"); appearance_->addItem("跟随系统","system"); appearance_->addItem("浅色","light"); appearance_->addItem("深色","dark"); appearance_->setFixedWidth(140); settingRow(startupLayout,"外观","系统高对比度设置优先",appearance_);
    textScale_=new CompactComboBox; textScale_->setObjectName("textScalePreference"); for (const int percent:{90,100,110,120}) textScale_->addItem(QString::number(percent)+"%",percent); textScale_->setFixedWidth(110); settingRow(startupLayout,"文字大小","调整文字，不放大整张界面",textScale_);
    auto* summaryDisplay=new QPushButton("自定…"); summaryDisplay->setObjectName("configureHomeSummary"); settingRow(startupLayout,"首页摘要","隐藏低频卡片，保留核心信息",summaryDisplay); connect(summaryDisplay,&QPushButton::clicked,this,[this] { (new HomeSummaryDialog(controller_,this))->show(); }); auto* locationHint=new QLabel; locationHint->setObjectName("startupLocationHint"); locationHint->setWordWrap(true); locationHint->setProperty("secondary",true); startupLayout->addWidget(locationHint);
    auto* startupHelp=new QPushButton("查看启动应用"); startupHelp->setObjectName("openWindowsStartup"); startupLayout->addWidget(startupHelp,0,Qt::AlignLeft); connect(startupHelp,&QPushButton::clicked,this,[] { QDesktopServices::openUrl(QUrl("ms-settings:startupapps")); });
    const auto observeLocation=[this,locationHint,startupHelp] { const auto& state=controller_->state(); locationHint->setText(state.autoStartConflict?"登录启动指向另一目录。请先停用旧启动项，再在此配置；不会自动覆盖旧项。":""); locationHint->setVisible(state.autoStartConflict); startupHelp->setVisible(state.autoStartConflict); startupHelp->setToolTip(QCoreApplication::applicationDirPath()); }; connect(controller_,&AppController::stateChanged,locationHint,observeLocation); observeLocation();
    auto* corePanel = panel(); corePanel->setObjectName("settingsCore"); corePanel->setProperty("settingsColumn",1); auto* coreSection = new QVBoxLayout(corePanel); coreSection->setContentsMargins(16,16,16,8); coreSection->setSpacing(0); layout->addWidget(corePanel,1,0,1,2,Qt::AlignTop);
    auto* coreTitle = new QLabel("内核与本地代理"); coreTitle->setObjectName("sectionTitle"); coreSection->addWidget(coreTitle); coreTitle->hide();
    groupTitle(coreSection,"网络与分流");
    auto* tun=new QPushButton("管理…"); tun->setObjectName("manageTunService"); settingRow(coreSection,"TUN 虚拟网卡","权限服务、协议栈与严格路由",tun); connect(tun,&QPushButton::clicked,this,[this] { if (auto* dialog=findChild<TunDialog*>("tunDialog")) { dialog->show(); dialog->raise(); } else (new TunDialog(controller_,this))->show(); });
    mixedPort_ = new CompactSpinBox; mixedPort_->setObjectName("mixedPort"); mixedPort_->setFixedWidth(96); mixedPort_->setButtonSymbols(QAbstractSpinBox::NoButtons); mixedPort_->setToolTip("1024–65535；修改后重新校验和安全重启"); mixedPort_->setRange(1024,65535); settingRow(coreSection,"本地端口","HTTP / SOCKS5 共用，仅本机",mixedPort_);
    auto* bypass=new QPushButton("编辑…"); bypass->setObjectName("configureProxyBypass"); settingRow(coreSection,"系统代理排除","主机、IP与本地地址",bypass); connect(bypass,&QPushButton::clicked,this,[this] { (new ProxyBypassDialog(controller_,this))->show(); });
    groupTitle(coreSection,"高级内核");
    logLevel_ = new CompactComboBox; logLevel_->setFixedWidth(140); logLevel_->setObjectName("coreLogLevel"); for (const auto& option:QList<QPair<QString,QString>>{{"警告","warning"},{"信息","info"},{"错误","error"},{"调试","debug"},{"关闭日志","silent"}}) logLevel_->addItem(option.first,option.second); settingRow(coreSection,"日志等级","应用后校验并安全重启",logLevel_);
    ipv6Mode_=new CompactComboBox; ipv6Mode_->setObjectName("ipv6Mode"); unifiedDelayMode_=new CompactComboBox; unifiedDelayMode_->setObjectName("unifiedDelayMode");
    for (auto* option:{ipv6Mode_,unifiedDelayMode_}) { option->addItem("跟随订阅","source"); option->addItem("开启","enabled"); option->addItem("关闭","disabled"); }
    ipv6Mode_->setFixedWidth(140); unifiedDelayMode_->setFixedWidth(140); settingRow(coreSection,"IPv6","默认跟随订阅",ipv6Mode_); settingRow(coreSection,"统一延迟","默认跟随订阅",unifiedDelayMode_);
    testUrl_ = new QLineEdit; testUrl_->setObjectName("connectivityTestUrl"); testUrl_->setToolTip("手动 HTTP 连通测试，5 秒超时，不表示所有目的地址可用"); settingRow(coreSection,"节点延迟测试地址","仅用于手动延迟测试",testUrl_,true);
    auto* coreRow = new QWidget; auto* coreLayout = new QHBoxLayout(coreRow); coreLayout->setContentsMargins(0,0,0,0); coreLayout->setSpacing(6); corePath_ = new QLineEdit; corePath_->setAccessibleName("mihomo 文件路径"); coreLayout->addWidget(corePath_,1); auto* browse=button("选择…",coreLayout); settingRow(coreSection,"mihomo 文件","应用后校验并安全重启",coreRow,true);
    auto* coreUpdate=new QPushButton("管理…"); coreUpdate->setObjectName("manageCoreUpdate"); settingRow(coreSection,"内核更新","官方稳定版、本地预检与回退",coreUpdate); connect(coreUpdate,&QPushButton::clicked,this,[this] { if (auto* dialog=findChild<CoreUpdateDialog*>("coreUpdateDialog")) { dialog->show(); dialog->raise(); } else (new CoreUpdateDialog(controller_,this))->show(); });
    connect(browse, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getOpenFileName(this, "选择可信 mihomo 文件", {}, "Executable (*.exe);;All files (*)"); if (!path.isEmpty()) corePath_->setText(path); });
    auto* utilityPanel=panel(); utilityPanel->setObjectName("settingsTools"); auto* utilityLayout=new QVBoxLayout(utilityPanel); utilityLayout->setContentsMargins(16,16,16,8); auto* utilityTitle=new QLabel("备份与工具"); utilityTitle->setObjectName("sectionTitle"); utilityLayout->addWidget(utilityTitle); utilityTitle->hide(); auto* utilityColumns=new QHBoxLayout; utilityColumns->setDirection(QBoxLayout::TopToBottom); utilityColumns->setObjectName("settingsToolsColumns"); utilityColumns->setSpacing(16); auto* utilities=new QVBoxLayout; utilities->setSpacing(0); auto* utilityRight=new QVBoxLayout; utilityRight->setSpacing(0); utilityColumns->addLayout(utilities,1); utilityColumns->addLayout(utilityRight,1); utilityLayout->addLayout(utilityColumns); layout->addWidget(utilityPanel,2,0,1,2,Qt::AlignTop);
    auto* address=new QPushButton("复制地址"); settingRow(utilities,"本地代理地址","复制已应用的 HTTP 代理地址",address); auto* data=new QPushButton("打开目录"); settingRow(utilities,"数据目录","订阅快照与加密状态",data);
    auto* actions = new QHBoxLayout; settingsDraftHint_=new QLabel; settingsDraftHint_->setObjectName("settingsDraftHint"); settingsDraftHint_->setProperty("secondary",true); settingsDraftHint_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); actions->addWidget(settingsDraftHint_,1); resetSettings_=button("放弃修改",actions); resetSettings_->setObjectName("discardNetworkSettings"); connect(resetSettings_,&QPushButton::clicked,this,&MainWindow::resetSettingsDraft); auto* apply = button("应用网络设置", actions); applySettings_=apply; apply->setObjectName("applySettings"); mutations_ += {apply};
    apply->setProperty("primary", true);
    connect(apply, &QPushButton::clicked, this, &MainWindow::applySettings); connect(address, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(QString("http://127.0.0.1:%1").arg(controller_->settings().mixedPort)); });
    connect(data, &QPushButton::clicked, this, [this] { openDirectory(controller_->dataRoot()); });
    auto* toolsMenu=new QMenu(this); auto* environment=toolsMenu->addMenu("复制代理环境变量"); auto* environmentRow=new QWidget; auto* environmentLayout=new QHBoxLayout(environmentRow); environmentLayout->setContentsMargins(0,0,0,0); auto* format=new CompactComboBox; format->addItems({"PowerShell","CMD","sh"}); environmentLayout->addWidget(format); auto* copyEnvironment=button("复制",environmentLayout); environmentRow->setMinimumWidth(180); settingRow(utilityRight,"代理环境变量","仅复制，不自动执行",environmentRow); connect(copyEnvironment,&QPushButton::clicked,this,[this,format] { findChild<QAction*>("copyProxyEnvironment"+format->currentText())->trigger(); });
    for (const auto& format:QStringList{"PowerShell","CMD","sh"}) { auto* action=environment->addAction(format); action->setObjectName("copyProxyEnvironment"+format); connect(action,&QAction::triggered,this,[this,format] { const auto http=QString("http://127.0.0.1:%1").arg(controller_->settings().mixedPort), socks=QString("socks5://127.0.0.1:%1").arg(controller_->settings().mixedPort); QStringList lines; for (const auto& pair:QList<QPair<QString,QString>>{{"HTTP_PROXY",http},{"HTTPS_PROXY",http},{"ALL_PROXY",socks},{"NO_PROXY","localhost,127.0.0.1,::1"}}) lines.append(format=="PowerShell"?QString("$env:%1=\"%2\"").arg(pair.first,pair.second):format=="CMD"?QString("set \"%1=%2\"").arg(pair.first,pair.second):QString("export %1='%2'").arg(pair.first,pair.second)); QApplication::clipboard()->setText(lines.join('\n')); }); }
    auto* geo=toolsMenu->addAction("Geo 数据…"); geo->setObjectName("importGeoData"); geo->setToolTip("管理默认快照与运行文件，按需下载或本地导入"); connect(geo,&QAction::triggered,this,[this] { (new GeoDataDialog(controller_,this))->show(); });
    auto* preview=toolsMenu->addAction("当前激活配置（脱敏）…"); preview->setObjectName("previewActiveConfig"); connect(toolsMenu,&QMenu::aboutToShow,this,[this,preview] { preview->setEnabled(!controller_->settings().activeProfile.isEmpty()); });
    connect(preview,&QAction::triggered,this,[this] { const QPointer<MainWindow> window(this); controller_->previewActiveConfig([window](Result result,QString text) { if (!window || !window->isVisible()) return; if (window->configPreview_) window->configPreview_->close(); auto* dialog=new QDialog(window); window->configPreview_=dialog; dialog->setObjectName("configPreview"); dialog->setWindowTitle("当前激活配置（脱敏，只读快照）"); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(620,420); auto* layout=new QVBoxLayout(dialog); auto* content=new QPlainTextEdit(result.ok?text:result.error); content->setReadOnly(true); layout->addWidget(content); auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close); layout->addWidget(buttons); connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject); dialog->show(); }); });
    auto* coreDirectory=toolsMenu->addAction("内核目录"); connect(coreDirectory,&QAction::triggered,this,[this] { openDirectory(QFileInfo(controller_->settings().corePath).absolutePath()); });
    for (const auto& pair:QList<QPair<QAction*,QString>>{{geo,"IP 与域名分类库"},{preview,"当前运行配置"},{coreDirectory,"内核目录"}}) { auto* control=new QPushButton(pair.first==coreDirectory?"打开目录":pair.first==geo?"管理…":"查看…"); connect(control,&QPushButton::clicked,pair.first,&QAction::trigger); connect(pair.first,&QAction::changed,control,[control,action=pair.first] { control->setEnabled(action->isEnabled()); }); settingRow(pair.first==preview?utilities:utilityRight,pair.second,pair.first==geo?"默认快照、来源及在线更新":pair.first==preview?"脱敏的只读快照":"当前配置的 mihomo 文件位置",control); }
    auto* logDirectory=new QPushButton("打开目录"); connect(logDirectory,&QPushButton::clicked,this,[this] { openDirectory(controller_->dataRoot()+"/logs"); }); settingRow(utilities,"日志目录","有界运行记录",logDirectory);
    auto* hotkeys=new QPushButton("配置…"); hotkeys->setObjectName("configureHotkeys"); settingRow(utilityRight,"全局快捷键","从任意应用唤起Mlash",hotkeys); connect(hotkeys,&QPushButton::clicked,this,[this] { (new HotkeysDialog(controller_,this))->show(); });
    auto* protocol=new QPushButton("管理…"); protocol->setObjectName("configureUrlProtocol"); settingRow(utilities,"链接导入","mlash协议与订阅确认",protocol); connect(protocol,&QPushButton::clicked,this,[this] { (new ProtocolSettingsDialog(controller_,this))->show(); });
    auto* backup = new QPushButton("创建备份…"); backup->setObjectName("exportBackup"); connect(backup,&QPushButton::clicked,this,[this] { (new BackupDialog(controller_,false,this))->show(); }); settingRow(utilities,"本地备份","加密保存订阅、规则和偏好",backup);
    auto* restore = new QPushButton("恢复备份…"); restore->setObjectName("restoreBackup"); connect(restore,&QPushButton::clicked,this,[this] { (new BackupDialog(controller_,true,this))->show(); }); settingRow(utilityRight,"恢复与迁移","先预览，保留现有订阅",restore);
    auto* localBackup=new QPushButton("管理…"); localBackup->setObjectName("configureLocalBackup"); settingRow(utilityRight,"自动备份与保护","本地计划、变更前快照和保留策略",localBackup); connect(localBackup,&QPushButton::clicked,this,[this] { if (auto* dialog=findChild<LocalBackupDialog*>("localBackupDialog")) { dialog->show(); dialog->raise(); } else (new LocalBackupDialog(controller_,this))->show(); });
    auto* remoteBackup=new QPushButton("管理…"); remoteBackup->setObjectName("configureRemoteBackup"); settingRow(utilities,"WebDAV备份","加密远端版本与可选定时",remoteBackup); connect(remoteBackup,&QPushButton::clicked,this,[this] { if (auto* dialog=findChild<RemoteBackupDialog*>("remoteBackupDialog")) { dialog->show(); dialog->raise(); } else (new RemoteBackupDialog(controller_,this))->show(); });
    QHash<QString,QWidget*> toolRows;
    for (auto* column:{utilities,utilityRight}) while (auto* item=column->takeAt(0)) { if (auto* row=item->widget()) toolRows.insert(row->property("settingSearchName").toString(),row); delete item; }
    groupTitle(coreSection,"连接与维护");
    for (const auto& name:QStringList{"本地代理地址","代理环境变量","IP 与域名分类库","当前运行配置","内核目录"}) if (auto* row=toolRows.take(name)) coreSection->addWidget(row);
    groupTitle(startupLayout,"快捷键与导入");
    for (const auto& name:QStringList{"全局快捷键","链接导入"}) if (auto* row=toolRows.take(name)) startupLayout->addWidget(row);
    startupLayout->addStretch(1);
    groupTitle(utilities,"备份与恢复"); groupTitle(utilityRight,"存储位置");
    for (const auto& name:QStringList{"本地备份","恢复与迁移","自动备份与保护","WebDAV备份"}) if (auto* row=toolRows.take(name)) utilities->addWidget(row);
    for (const auto& name:QStringList{"数据目录","日志目录"}) if (auto* row=toolRows.take(name)) utilityRight->addWidget(row);
    for (auto* row:toolRows) utilityRight->addWidget(row);
    utilities->addStretch(); utilityRight->addStretch(); settingsVersion_=new QLabel("Mlash " MLASH_VERSION " · Qt " QT_VERSION_STR); settingsVersion_->setObjectName("settingsVersion"); settingsVersion_->setProperty("secondary",true); layout->addWidget(settingsVersion_,3,0,1,2); layout->setRowStretch(4,1);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); auto* canvas=new QWidget; canvas->setObjectName("settingsCanvas"); auto* centering=new QHBoxLayout(canvas); centering->setContentsMargins(0,0,0,0); page->setMaximumWidth(760); page->setProperty("singleColumn",true); centering->addWidget(page,1,Qt::AlignTop); scroll->setWidget(canvas);
    scroll->setObjectName("settingsScroll"); auto* surface=new QWidget; auto* outer=new QVBoxLayout(surface); outer->setContentsMargins(0,0,0,0); outer->setSpacing(14);
    // One title row like every other page: category tabs occupy the tab slot, search right-aligned on the same baseline.
    auto* header=new QHBoxLayout; header->setSpacing(12); auto* categories=new QTabBar; categories->setObjectName("settingsCategories"); categories->setExpanding(false); categories->setDrawBase(false); categories->setAccessibleName("设置分类"); for (const auto& name:QStringList{"常规","网络与内核","备份与工具"}) categories->addTab(name);
    header->addWidget(categories,0,Qt::AlignLeft|Qt::AlignBottom); header->addStretch(1); auto* locator=new SettingsSearch(page,scroll,surface); locator->setObjectName("settingsLocator"); locator->setMaximumWidth(470); header->addWidget(locator,1,Qt::AlignBottom); outer->addLayout(header);
    connect(locator,&SettingsSearch::pageRequested,this,&MainWindow::setPage); locator->addRoute("订阅 › 来源与更新","订阅 URL provider 来源 更新 用量 subscription",1); locator->addRoute("订阅 › DNS 与上游代理链","DNS DNS覆盖 上游 拨号 链 chain",1); locator->addRoute("节点 › 选择节点与策略组","代理 节点 策略组 proxy node",2); locator->addRoute("规则 › 自定义规则与规则集","规则 域名 自定义 规则集 DOMAIN rules",3); locator->addRoute("连接 › 实时连接与进程","连接 进程 规则 流量 connection process",4); locator->addRoute("网站测试 › 响应延迟","网站 测速 目标 YouTube ChatGPT Google",7); locator->addRoute("日志 › 筛选与诊断","日志 错误 诊断 脱敏 导出 log",5);
    outer->addWidget(scroll,1); outer->addLayout(actions);
    const QList<QWidget*> sections{startupPanel,corePanel,utilityPanel};
    settingsSections_=sections; settingsScroll_=scroll; settingsPage_=page; settingsCategories_=categories;
    connect(categories,&QTabBar::currentChanged,surface,[this](int index) { selectSettingsCategory(index); }); selectSettingsCategory(0);
    connect(locator,&SettingsSearch::anchorRequested,surface,[this](QWidget* anchor) { for (int i=0;i<settingsSections_.size();++i) if (settingsSections_[i]->isAncestorOf(anchor)) { settingsCategories_->setCurrentIndex(i); break; } });
    // Preference rows commit immediately; only core-restart rows wait for the explicit action bar.
    const QList<QComboBox*> instantCombos{startupPage_,trayClick_,appearance_,textScale_};
    for (auto* toggle:{autoStart_,silentStart_,startCore_,restoreProxy_,lightweight_}) connect(toggle,&QCheckBox::toggled,this,&MainWindow::commitInstantSettings);
    for (auto* combo:instantCombos) connect(combo,&QComboBox::currentIndexChanged,this,&MainWindow::commitInstantSettings);
    connect(testUrl_,&QLineEdit::editingFinished,this,&MainWindow::commitInstantSettings);
    for (auto* combo:{logLevel_,ipv6Mode_,unifiedDelayMode_}) connect(combo,&QComboBox::currentIndexChanged,this,&MainWindow::updateActions);
    connect(mixedPort_,&QSpinBox::valueChanged,this,&MainWindow::updateActions); connect(corePath_,&QLineEdit::textChanged,this,&MainWindow::updateActions); connect(testUrl_,&QLineEdit::textChanged,this,&MainWindow::updateActions); return surface;
}

}
