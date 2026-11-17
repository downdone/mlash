#include "app/AppController.h"
#include "TaskDialog.h"
#include "TaskWorkspace.h"
#include "MainWindow.h"
#include "BackupDialog.h"
#include "CompactComboBox.h"
#include "CompactSpinBox.h"
#include "CoreUpdateDialog.h"
#include "AppUpdateDialog.h"
#include "GeoDataDialog.h"
#include "HotkeysDialog.h"
#include "LocalBackupDialog.h"
#include "ProtocolSettingsDialog.h"
#include "PrototypeUi.h"
#include "ProxyBypassDialog.h"
#include "ProxySwitch.h"
#include "RemoteBackupDialog.h"
#include "SettingRow.h"
#include "SettingsSearch.h"
#include "TunDialog.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "WindowUiFactories.h"
#include <QtWidgets>
#include "ViewIds.h"

namespace mlash {
using namespace WindowParts;
void MainWindow::selectSettingsCategory(int index) {
    // K8：设置页只有一个滚动面，分类不再是页签，只用来把视野滚到对应分组。
    if (index < 0 || index >= settingsSections_.size() || !settingsScroll_) return;
    settingsPage_->setProperty("settingsCategory", index);
    openSettingsTask(QStringList{"startup","network","core"}.value(index));
    updateWindowTitle();
}

void MainWindow::ensureSettingsGroupVisible(int index) {
    // ensureWidgetVisible() 会「向下多滚半个视口」：深链到最后一组时目标被推到视口外，
    // 测试与用户都只看到中间那一段。这里按控件的真实几何直接定位：
    // 卡片顶部滚到视口顶部；未超过底部的短卡片保持视口不动。
    if (index < 0 || index >= settingsSections_.size() || !settingsScroll_) return;
    auto* bar = settingsScroll_->verticalScrollBar();
    auto* viewport = settingsScroll_->viewport();
    auto* card = settingsSections_[index];
    const int top = card->mapTo(viewport, QPoint()).y() + bar->value();
    const int target = qBound(0, top, bar->maximum());
    bar->setValue(target);
}

// Rows that need a core restart keep an explicit action; they are the only reason the bar appears.
int MainWindow::networkSettingCount() const {
    const auto& s=controller_->settings(); int count=0;
    for (bool changed:{testUrl_->text().trimmed()!=s.testUrl,mixedPort_->value()!=s.mixedPort,logLevel_->currentData().toString()!=s.logLevel,corePath_->text().trimmed()!=s.corePath,ipv6Mode_->currentData().toString()!=s.ipv6Mode,unifiedDelayMode_->currentData().toString()!=s.unifiedDelayMode}) count+=changed;
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
    next.appearance=appearance_->currentData().toString(); next.textScale=textScale_->currentData().toInt(); next.windowChrome=windowChrome_->currentData().toString();
    const bool unchanged=next.autoStart==current.autoStart&&next.silentStart==current.silentStart&&next.startCore==current.startCore&&next.restoreProxy==current.restoreProxy&&next.lightweight==current.lightweight&&next.startupPage==current.startupPage&&next.trayClick==current.trayClick&&next.appearance==current.appearance&&next.textScale==current.textScale&&next.windowChrome==current.windowChrome&&next.testUrl==current.testUrl;
    if (unchanged) return;
    // Instant preferences must not carry an unrelated pending restart value into the controller.
    next.mixedPort=current.mixedPort; next.logLevel=current.logLevel; next.corePath=current.corePath; next.ipv6Mode=current.ipv6Mode; next.unifiedDelayMode=current.unifiedDelayMode;
    controller_->updateSettings(next);
}

void MainWindow::resetSettingsDraft() { settingsFeedback_.clear(); hasDisplayedSettings_=false; updateProfiles(); updateActions(); }

void MainWindow::applySettings() {
    settingsApplying_=true; settingsFeedback_="正在校验并应用…";
    // Carries whatever instant preferences are already saved plus this explicit network change.
    auto s = controller_->settings(); s.mixedPort = mixedPort_->value(); s.logLevel = logLevel_->currentData().toString(); s.corePath = corePath_->text().trimmed(); s.ipv6Mode=ipv6Mode_->currentData().toString(); s.unifiedDelayMode=unifiedDelayMode_->currentData().toString(); s.testUrl=testUrl_->text().trimmed(); controller_->updateSettings(s);
}
QWidget* MainWindow::settingsPage() {
    // U14 · 设置目录通向具体任务；任务内用紧凑的连续分组行。
    // 「应用网络设置 / 放弃修改」仍固定在组末（ADR 0044 的显式提交）。
    auto* page = new QWidget; page->setObjectName("settingsPage"); page->setAttribute(Qt::WA_StyledBackground, true);
    auto* column = new QVBoxLayout(page); column->setContentsMargins(0, 0, 0, 0); column->setSpacing(8);
    // group() 建一张卡片并把「行容器」交回调用方。
    const auto editorGroup=[&column](const QString& title, const QString& name, QVBoxLayout*& rows) {
        auto* card = new QFrame; card->setObjectName("settingsGroup"); card->setProperty("settingsGroup", name); card->setAccessibleName(title);
        auto* layout = new QVBoxLayout(card); layout->setContentsMargins(12, 10, 12, 5); layout->setSpacing(6);
        auto* heading = new QLabel(title); heading->setObjectName("sectionTitle"); layout->addWidget(heading);
        rows = new QVBoxLayout; rows->setContentsMargins(0, 0, 0, 0); rows->setSpacing(0); layout->addLayout(rows);
        column->addWidget(card); return card;
    };
    QVBoxLayout *generalRows = nullptr, *networkRows = nullptr, *coreRows = nullptr;
    auto* generalCard = editorGroup("通用", "general", generalRows);
    auto* networkCard = editorGroup("网络", "network", networkRows);
    auto* coreCard = editorGroup("内核与维护", "core", coreRows);

    // —— 通用：外观、启动与托盘、快捷键与导入 ——
    appearance_=new CompactComboBox; appearance_->setObjectName("appearancePreference"); appearance_->addItem("跟随系统","system"); appearance_->addItem("浅色","light"); appearance_->addItem("深色","dark"); appearance_->setFixedWidth(120); settingRow(generalRows,"外观","系统高对比度设置优先",appearance_,false,{},UiIcons::Glyph::Moon);
    textScale_=new CompactComboBox; textScale_->setObjectName("textScalePreference"); for (const int percent:{90,100,110,120}) textScale_->addItem(QString::number(percent)+"%",percent); textScale_->setFixedWidth(96); settingRow(generalRows,"文字大小","调整文字，不放大整张界面",textScale_,false,{},UiIcons::Glyph::Sliders);
    windowChrome_=new CompactComboBox; windowChrome_->setObjectName("windowChromePreference"); windowChrome_->addItem("系统标题栏","native"); windowChrome_->addItem("应用内标题栏","integrated"); windowChrome_->setFixedWidth(140); settingRow(generalRows,"窗口标题栏","可在两种样式之间切换",windowChrome_,false,{},UiIcons::Glyph::Layers);
    autoStart_ = new ProxySwitch; autoStart_->setObjectName("autoStart"); silentStart_=new ProxySwitch; silentStart_->setObjectName("silentStart"); startCore_=new ProxySwitch; startCore_->setObjectName("startCore"); restoreProxy_=new ProxySwitch; restoreProxy_->setObjectName("restoreProxy"); lightweight_=new ProxySwitch; lightweight_->setObjectName("lightweightMode");
    for (auto* toggle:{autoStart_,silentStart_,startCore_,restoreProxy_,lightweight_}) toggle->setFixedWidth(54);
    settingRow(generalRows,"登录启动","随 Windows 登录启动 Mlash",autoStart_,false,{},UiIcons::Glyph::Rocket);
    settingRow(generalRows,"启动时隐藏窗口","已有订阅时直接进入托盘",silentStart_,false,{},UiIcons::Glyph::Home);
    settingRow(generalRows,"启动内核","自动运行上次使用的订阅",startCore_,false,{},UiIcons::Glyph::Play);
    settingRow(generalRows,"启动时恢复代理开关","按上次的开关状态恢复，不接管其他程序的代理",restoreProxy_,false,{},UiIcons::Glyph::Refresh);
    settingRow(generalRows,"关闭窗口时释放界面","释放界面资源；内核与托盘继续运行",lightweight_,false,{},UiIcons::Glyph::Close); lightweight_->setToolTip("托盘打开时重新创建界面；未保存内容会提示确认");
    startupPage_=new CompactComboBox; startupPage_->setObjectName("startupPage"); for (const auto& pair:QList<QPair<QString,int>>{{"首页",0},{"订阅",1},{"节点",2},{"网站测试",7},{"规则",3},{"连接",4},{"日志",5},{"设置",6}}) startupPage_->addItem(pair.first,pair.second); startupPage_->setFixedWidth(120); settingRow(generalRows,"启动页面","首次打开主界面时显示",startupPage_,false,{},UiIcons::Glyph::Home);
    trayClick_=new CompactComboBox; trayClick_->setObjectName("trayClick"); trayClick_->addItem("显示主窗口","show"); trayClick_->addItem("打开节点页","proxy-page"); trayClick_->addItem("切换系统代理","toggle-proxy"); trayClick_->setFixedWidth(120); trayClick_->setToolTip("切换代理不会自动覆盖外部程序的代理"); settingRow(generalRows,"托盘单击","双击始终显示主界面",trayClick_,false,{},UiIcons::Glyph::List);
    auto* hotkeys=new QPushButton("配置…"); hotkeys->setObjectName("configureHotkeys"); settingRow(generalRows,"全局快捷键","从任意应用唤起Mlash",hotkeys,false,{},UiIcons::Glyph::Settings); connect(hotkeys,&QPushButton::clicked,this,[this] { showTool("hotkeys", {}, [this] { return new HotkeysDialog(controller_,this); }); });
    auto* protocol=new QPushButton("管理…"); protocol->setObjectName("configureUrlProtocol"); settingRow(generalRows,"链接导入","mlash协议与订阅确认",protocol,false,{},UiIcons::Glyph::Globe); connect(protocol,&QPushButton::clicked,this,[this] { showTool("protocol", {}, [this] { return new ProtocolSettingsDialog(controller_,this); }); });
    auto* locationHint=new QLabel; locationHint->setObjectName("startupLocationHint"); locationHint->setWordWrap(true); locationHint->setProperty("secondary",true); generalRows->addWidget(locationHint);
    auto* startupHelp=new QPushButton("查看启动应用"); startupHelp->setObjectName("openWindowsStartup"); generalRows->addWidget(startupHelp,0,Qt::AlignLeft); connect(startupHelp,&QPushButton::clicked,this,[] { QDesktopServices::openUrl(QUrl("ms-settings:startupapps")); });
    const auto observeLocation=[this,locationHint,startupHelp] { const auto& state=controller_->state(); locationHint->setText(state.autoStartConflict?"登录启动指向另一目录。请先停用旧启动项，再在此配置；不会自动覆盖旧项。":""); locationHint->setVisible(state.autoStartConflict); startupHelp->setVisible(state.autoStartConflict); startupHelp->setToolTip(QCoreApplication::applicationDirPath()); }; connect(controller_,&AppController::stateChanged,locationHint,observeLocation); observeLocation();

    // —— 网络：TUN、系统代理排除、本地端口、连通测试地址 ——
    auto* tun=new QPushButton("管理…"); tun->setObjectName("manageTunService"); settingRow(networkRows,"TUN 虚拟网卡","权限服务、协议栈与严格路由",tun,false,QStringLiteral("未启用"),UiIcons::Glyph::Layers); connect(tun,&QPushButton::clicked,this,[this] { showTool("tun", {}, [this] { return new TunDialog(controller_,this); }); });
    auto* bypass=new QPushButton("编辑…"); bypass->setObjectName("configureProxyBypass"); settingRow(networkRows,"系统代理排除","主机、IP与本地地址",bypass,false,{},UiIcons::Glyph::Globe); connect(bypass,&QPushButton::clicked,this,[this] { showTool("bypass",{},[this] { return new ProxyBypassDialog(controller_,this); }); });
    mixedPort_ = new CompactSpinBox; mixedPort_->setObjectName("mixedPort"); mixedPort_->setFixedWidth(88); mixedPort_->setButtonSymbols(QAbstractSpinBox::NoButtons); mixedPort_->setToolTip("1024–65535；修改后重新校验和安全重启"); mixedPort_->setRange(1024,65535); settingRow(networkRows,"本地端口","HTTP / SOCKS5 共用，仅本机",mixedPort_,false,{},UiIcons::Glyph::Route);
    testUrl_ = new QLineEdit; testUrl_->setObjectName("connectivityTestUrl"); testUrl_->setToolTip("手动 HTTP 连通测试，5 秒超时，不表示所有目的地址可用"); settingRow(networkRows,"节点延迟测试地址","仅用于手动延迟测试",testUrl_,true,{},UiIcons::Glyph::Gauge);

    // —— 内核与维护：内核路径/更新、高级开关、日志、工具与备份 ——
    auto* coreRow = new QWidget; auto* coreLayout = new QHBoxLayout(coreRow); coreLayout->setContentsMargins(0,0,0,0); coreLayout->setSpacing(6); corePath_ = new QLineEdit; corePath_->setAccessibleName("mihomo 文件路径"); coreLayout->addWidget(corePath_,1); auto* browse=button("选择…",coreLayout); settingRow(coreRows,"mihomo 文件","应用后校验并安全重启",coreRow,true,{},UiIcons::Glyph::Folder);
    connect(browse, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getOpenFileName(this, "选择可信 mihomo 文件", {}, "Executable (*.exe);;All files (*)"); if (!path.isEmpty()) corePath_->setText(path); });
    auto* coreUpdate=new QPushButton("管理…"); coreUpdate->setObjectName("manageCoreUpdate"); settingRow(coreRows,"内核更新","官方稳定版、本地预检与回退",coreUpdate,false,QStringLiteral("当前内核路径可在下方查看"),UiIcons::Glyph::Refresh); connect(coreUpdate,&QPushButton::clicked,this,[this] { showTool("core-update", {}, [this] { return new CoreUpdateDialog(controller_,this); }); });
    logLevel_ = new CompactComboBox; logLevel_->setFixedWidth(120); logLevel_->setObjectName("coreLogLevel"); for (const auto& option:QList<QPair<QString,QString>>{{"警告","warning"},{"信息","info"},{"错误","error"},{"调试","debug"},{"关闭日志","silent"}}) logLevel_->addItem(option.first,option.second); settingRow(coreRows,"日志等级","应用后校验并安全重启",logLevel_,false,{},UiIcons::Glyph::List);
    ipv6Mode_=new CompactComboBox; ipv6Mode_->setObjectName("ipv6Mode"); unifiedDelayMode_=new CompactComboBox; unifiedDelayMode_->setObjectName("unifiedDelayMode");
    for (auto* option:{ipv6Mode_,unifiedDelayMode_}) { option->addItem("跟随订阅","source"); option->addItem("开启","enabled"); option->addItem("关闭","disabled"); }
    ipv6Mode_->setFixedWidth(120); unifiedDelayMode_->setFixedWidth(120); settingRow(coreRows,"IPv6","默认跟随订阅",ipv6Mode_,false,{},UiIcons::Glyph::Globe); settingRow(coreRows,"统一延迟","默认跟随订阅",unifiedDelayMode_,false,{},UiIcons::Glyph::Gauge);
    auto* address=new QPushButton("复制地址"); settingRow(coreRows,"本地代理地址","复制已应用的 HTTP 代理地址",address,false,{},UiIcons::Glyph::Copy);
    auto* data=new QPushButton("打开目录"); settingRow(coreRows,"数据目录","订阅快照与加密状态",data,false,{},UiIcons::Glyph::Folder);
    connect(address, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(QString("http://127.0.0.1:%1").arg(controller_->settings().mixedPort)); });
    connect(data, &QPushButton::clicked, this, [this] { openDirectory(controller_->dataRoot()); });
    auto* toolsMenu=new QMenu(this); auto* environment=toolsMenu->addMenu("复制代理环境变量"); auto* environmentRow=new QWidget; auto* environmentLayout=new QHBoxLayout(environmentRow); environmentLayout->setContentsMargins(0,0,0,0); auto* format=new CompactComboBox; format->addItems({"PowerShell","CMD","sh"}); environmentLayout->addWidget(format); auto* copyEnvironment=button("复制",environmentLayout); environmentRow->setMinimumWidth(180); settingRow(coreRows,"代理环境变量","仅复制，不自动执行",environmentRow,false,{},UiIcons::Glyph::Copy); connect(copyEnvironment,&QPushButton::clicked,this,[this,format] { findChild<QAction*>("copyProxyEnvironment"+format->currentText())->trigger(); });
    for (const auto& format:QStringList{"PowerShell","CMD","sh"}) { auto* action=environment->addAction(format); action->setObjectName("copyProxyEnvironment"+format); connect(action,&QAction::triggered,this,[this,format] { const auto http=QString("http://127.0.0.1:%1").arg(controller_->settings().mixedPort), socks=QString("socks5://127.0.0.1:%1").arg(controller_->settings().mixedPort); QStringList lines; for (const auto& pair:QList<QPair<QString,QString>>{{"HTTP_PROXY",http},{"HTTPS_PROXY",http},{"ALL_PROXY",socks},{"NO_PROXY","localhost,127.0.0.1,::1"}}) lines.append(format=="PowerShell"?QString("$env:%1=\"%2\"").arg(pair.first,pair.second):format=="CMD"?QString("set \"%1=%2\"").arg(pair.first,pair.second):QString("export %1='%2'").arg(pair.first,pair.second)); QApplication::clipboard()->setText(lines.join('\n')); }); }
    auto* geo=toolsMenu->addAction("Geo 数据…"); geo->setObjectName("importGeoData"); geo->setToolTip("管理默认快照与运行文件，按需下载或本地导入"); connect(geo,&QAction::triggered,this,[this] { showTool("geo", {}, [this] { return new GeoDataDialog(controller_,this); }); });
    auto* preview=toolsMenu->addAction("当前激活配置（脱敏）…"); preview->setObjectName("previewActiveConfig"); connect(toolsMenu,&QMenu::aboutToShow,this,[this,preview] { preview->setEnabled(!controller_->settings().activeProfile.isEmpty()); });
    connect(preview,&QAction::triggered,this,[this] { const QPointer<MainWindow> window(this); controller_->previewActiveConfig([window](Result result,QString text) { if (!window || !window->isVisible()) return; if (window->configPreview_) window->configPreview_->close(); auto* dialog=new TaskDialog(window); window->configPreview_=dialog; dialog->setObjectName("configPreview"); dialog->setWindowTitle("当前激活配置（脱敏，只读快照）"); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(620,420); auto* layout=new QVBoxLayout(dialog); auto* content=new QPlainTextEdit(result.ok?text:result.error); content->setReadOnly(true); layout->addWidget(content); auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close); layout->addWidget(buttons); connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject); window->presentTask(dialog); }); });
    auto* coreDirectory=toolsMenu->addAction("内核目录"); connect(coreDirectory,&QAction::triggered,this,[this] { openDirectory(QFileInfo(controller_->settings().corePath).absolutePath()); });
    for (const auto& pair:QList<QPair<QAction*,QString>>{{geo,"IP 与域名分类库"},{preview,"当前运行配置"},{coreDirectory,"内核目录"}}) { auto* control=new QPushButton(pair.first==coreDirectory?"打开目录":pair.first==geo?"管理…":"查看…"); connect(control,&QPushButton::clicked,pair.first,&QAction::trigger); connect(pair.first,&QAction::changed,control,[control,action=pair.first] { control->setEnabled(action->isEnabled()); }); settingRow(coreRows,pair.second,pair.first==geo?"默认快照、来源及在线更新":pair.first==preview?"脱敏的只读快照":"当前配置的 mihomo 文件位置",control,false,{},pair.first==geo?UiIcons::Glyph::Globe:pair.first==preview?UiIcons::Glyph::Info:UiIcons::Glyph::Folder); }
    auto* logDirectory=new QPushButton("打开目录"); connect(logDirectory,&QPushButton::clicked,this,[this] { openDirectory(controller_->dataRoot()+"/logs"); }); settingRow(coreRows,"日志目录","有界运行记录",logDirectory,false,{},UiIcons::Glyph::Folder);
    auto* backup = new QPushButton("创建备份…"); backup->setObjectName("exportBackup"); connect(backup,&QPushButton::clicked,this,[this] { showTool("backup", "create", [this] { return new BackupDialog(controller_,false,this); }); }); settingRow(coreRows,"本地备份","加密保存订阅、规则和偏好",backup,false,{},UiIcons::Glyph::Save);
    auto* restore = new QPushButton("恢复备份…"); restore->setObjectName("restoreBackup"); connect(restore,&QPushButton::clicked,this,[this] { showTool("backup", "restore", [this] { return new BackupDialog(controller_,true,this); }); }); settingRow(coreRows,"恢复与迁移","先预览，保留现有订阅",restore,false,{},UiIcons::Glyph::Refresh);
    auto* localBackup=new QPushButton("管理…"); localBackup->setObjectName("configureLocalBackup"); settingRow(coreRows,"自动备份与保护","本地计划、变更前快照和保留策略",localBackup,false,{},UiIcons::Glyph::Clock); connect(localBackup,&QPushButton::clicked,this,[this] { showTool("local-backup", {}, [this] { return new LocalBackupDialog(controller_,this); }); });
    auto* remoteBackup=new QPushButton("管理…"); remoteBackup->setObjectName("configureRemoteBackup"); settingRow(coreRows,"WebDAV备份","加密远端版本与可选定时",remoteBackup,false,{},UiIcons::Glyph::Save); connect(remoteBackup,&QPushButton::clicked,this,[this] { showTool("remote-backup", {}, [this] { return new RemoteBackupDialog(controller_,this); }); });

    // 每张卡片的最后一行不画分隔线。
    for (auto* rows:{generalRows,networkRows,coreRows}) if (auto* last=rows->itemAt(rows->count()-1)->widget()) last->setProperty("last",true);
    settingsVersion_=new QLabel("Mlash " MLASH_VERSION " · Qt " QT_VERSION_STR); settingsVersion_->setObjectName("settingsVersion"); settingsVersion_->setProperty("secondary",true); column->addWidget(settingsVersion_); column->addStretch(1);

    // 「应用网络设置 / 放弃修改」：只有需要重启内核的改动才会出现，位置固定在组末。
    auto* actions=new QWidget; actions->setObjectName("settingsActions"); auto* actionsLayout=new QHBoxLayout(actions); actionsLayout->setContentsMargins(0,0,0,0); actionsLayout->setSpacing(10);
    settingsDraftHint_=new QLabel; settingsDraftHint_->setObjectName("settingsDraftHint"); settingsDraftHint_->setProperty("secondary",true); settingsDraftHint_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); actionsLayout->addWidget(settingsDraftHint_,1);
    resetSettings_=button("取消",actionsLayout); resetSettings_->setObjectName("discardNetworkSettings"); connect(resetSettings_,&QPushButton::clicked,this,[this] { resetSettingsDraft(); returnToSettingsDirectory(); });
    auto* apply=button("保存并重启",actionsLayout); applySettings_=apply; apply->setObjectName("applySettings"); apply->setProperty("primary",true); mutations_ += {apply}; connect(apply,&QPushButton::clicked,this,&MainWindow::applySettings);
    actions->hide();

    auto* scroll = new QScrollArea; scroll->setObjectName("settingsScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* canvas=new QWidget; canvas->setObjectName("settingsCanvas"); auto* centering=new QHBoxLayout(canvas); centering->setContentsMargins(0,0,0,0); centering->addWidget(page,1,Qt::AlignTop); scroll->setWidget(canvas);
    auto* surface=new QWidget; surface->setObjectName("settingsSurface"); auto* outer=new QVBoxLayout(surface); outer->setContentsMargins(0,0,0,0); outer->setSpacing(8);
    // 只有检索：分类不再是页签，命中的行直接滚进视野。
    auto* header=new QHBoxLayout; header->setSpacing(12); header->addStretch(1);
    auto* locator=new SettingsSearch(page,scroll,surface); locator->setObjectName("settingsLocator"); locator->setMaximumWidth(470); header->addWidget(locator,0,Qt::AlignRight);
    connect(locator,&SettingsSearch::pageRequested,this,&MainWindow::setPage);
    connect(locator,&SettingsSearch::routeRequested,this,&MainWindow::navigate);
    locator->addRoute("订阅 › 来源与更新","订阅 URL provider 来源 更新 用量 subscription",1);
    locator->addRoute("订阅 › DNS 与上游代理链","DNS DNS覆盖 上游 拨号 链 chain",1);
    locator->addRoute("节点 › 选择节点与策略组","代理 节点 策略组 proxy node",Route{0,-1,HomeAnchor::Nodes});
    // K1：规则是独立视图（视图 ID 2）；这里的 2 过去被当成旧编号的「首页节点」。
    locator->addRoute("规则 › 自定义规则与规则集","规则 域名 自定义 规则集 DOMAIN rules",Route{ViewIds::Rules});
    locator->addRoute("连接 › 实时连接与进程","连接 进程 规则 流量 connection process",Route{ViewIds::Data,-1,HomeAnchor::Control,int(ViewIds::DataTab::Connections)});
    locator->addRoute("网站测试 › 响应延迟","网站 测速 目标 YouTube ChatGPT Google",Route{ViewIds::Data,-1,HomeAnchor::Control,int(ViewIds::DataTab::Tests)});
    locator->addRoute("日志 › 筛选与诊断","日志 错误 诊断 脱敏 导出 log",Route{ViewIds::Data,-1,HomeAnchor::Control,int(ViewIds::DataTab::Logs)});
    outer->addLayout(header); outer->addWidget(scroll,1); outer->addWidget(actions);
    connect(locator,&SettingsSearch::anchorRequested,surface,[this,locator](QWidget* anchor) { if (settingsScroll_&&anchor&&settingsPage_->isAncestorOf(anchor)) { if (route_.view!=ViewIds::Settings) navigate(Route{ViewIds::Settings}); openSettingsTask("all"); locator->show(); settingsScroll_->ensureWidgetVisible(anchor,8,12); } });
    settingsSections_={generalCard,networkCard,coreCard}; settingsScroll_=scroll; settingsPage_=page;
    // Preference rows commit immediately; only core-restart rows wait for the explicit action bar.
    const QList<QComboBox*> instantCombos{startupPage_,trayClick_,appearance_,textScale_,windowChrome_};
    for (auto* toggle:{autoStart_,silentStart_,startCore_,restoreProxy_,lightweight_}) connect(toggle,&QCheckBox::toggled,this,&MainWindow::commitInstantSettings);
    for (auto* combo:instantCombos) connect(combo,&QComboBox::currentIndexChanged,this,&MainWindow::commitInstantSettings);

    for (auto* combo:{logLevel_,ipv6Mode_,unifiedDelayMode_}) connect(combo,&QComboBox::currentIndexChanged,this,&MainWindow::updateActions);
    connect(mixedPort_,&QSpinBox::valueChanged,this,&MainWindow::updateActions); connect(corePath_,&QLineEdit::textChanged,this,&MainWindow::updateActions); connect(testUrl_,&QLineEdit::textChanged,this,&MainWindow::updateActions);
    settingsStack_=new QStackedWidget; settingsStack_->setObjectName("settingsTaskStack");
    settingsDirectory_=new QWidget; settingsDirectory_->setObjectName("settingsDirectory"); auto* directory=new QVBoxLayout(settingsDirectory_); directory->setContentsMargins(0,0,0,10); directory->setSpacing(UiTheme::PanelGap);
    using namespace PrototypeUi; using G=UiIcons::Glyph;
    auto* appearanceRows=group(directory);
    const auto choice=[this](QVBoxLayout* rows,const QString& name,const QString& title,G glyph,QComboBox* combo) {
        auto* item=row(rows,name,title,glyph,combo->currentText());
        connect(combo,&QComboBox::currentIndexChanged,item,[item,combo] { item->setValue(combo->currentText()); });
        connect(item,&QPushButton::clicked,this,[item,combo] { auto* menu=new QMenu(item); menu->setAttribute(Qt::WA_DeleteOnClose); for (int i=0;i<combo->count();++i) { auto* action=menu->addAction(combo->itemText(i)); action->setCheckable(true); action->setChecked(i==combo->currentIndex()); QObject::connect(action,&QAction::triggered,combo,[combo,i] { combo->setCurrentIndex(i); }); } menu->popup(item->mapToGlobal(QPoint(0,item->height()))); }); return item;
    };
    choice(appearanceRows,"settingsAppearanceRow","外观",G::Moon,appearance_); choice(appearanceRows,"settingsTextSizeRow","文字大小",G::Sliders,textScale_); choice(appearanceRows,"settingsWindowChromeRow","窗口标题栏",G::Layers,windowChrome_);
    const auto taskRow=[this](QVBoxLayout* rows,const QString& name,const QString& title,G glyph,const QString& task,const QString& value=QString()) {
        auto* item=row(rows,name,title,glyph,value); connect(item,&QPushButton::clicked,this,[this,task] { openSettingsTask(task); }); return item;
    };
    auto* general=group(directory,"通用"); taskRow(general,"settingsStartupRow","启动与托盘",G::Home,"startup");
    auto* keys=row(general,"settingsHotkeysRow","快捷键",G::Settings); connect(keys,&QPushButton::clicked,hotkeys,&QPushButton::click);
    const auto toggleRow=[&](const QString& name,const QString& title,G glyph,QCheckBox* toggle) { auto* item=row(general,name,title,glyph); item->setFocusPolicy(Qt::NoFocus); item->setTrailingControl(toggle); connect(item,&QPushButton::clicked,toggle,[toggle] { toggle->click(); }); };
    toggleRow("settingsLightweightRow","关闭窗口时释放界面",G::Layers,lightweight_); toggleRow("settingsAutostartRow","登录启动",G::Rocket,autoStart_);
    auto* network=group(directory,"连接与网络"); auto* local=taskRow(network,"settingsNetworkRow","本地代理",G::Route,"network",QString("端口 %1").arg(controller_->settings().mixedPort));
    connect(controller_,&AppController::stateChanged,local,[this,local] { local->setValue(QString("端口 %1").arg(controller_->settings().mixedPort)); });
    auto* tunRow=row(network,"settingsTunRow","TUN 虚拟网卡",G::Layers); connect(tunRow,&QPushButton::clicked,tun,&QPushButton::click);
    connect(controller_,&AppController::stateChanged,tunRow,[this,tunRow] { tunRow->setValue(controller_->state().tunObserved?"已启用":"未启用"); }); tunRow->setValue(controller_->state().tunObserved?"已启用":"未启用");
    auto* bypassRow=row(network,"settingsBypassRow","系统代理排除",G::Globe); connect(bypassRow,&QPushButton::clicked,bypass,&QPushButton::click);
    auto* dnsRow=row(network,"settingsDnsRow","DNS 设置",G::Globe,{},"当前使用的配置"); connect(dnsRow,&QPushButton::clicked,this,[this] { const auto id=controller_->state().activeProfile; if (id.isEmpty()) { navigate(Route{ViewIds::Profiles}); return; } navigate(Route{ViewIds::Profiles}); showProfileDetails(id); openProfileTask(id,"dns"); });
    auto* maintain=group(directory,"维护与数据"); auto* coreTask=row(maintain,"settingsCoreRow","内核管理",G::Layers); connect(coreTask,&QPushButton::clicked,this,[this] { showTool("core-update",{},[this] { return new CoreUpdateDialog(controller_,this); }); });
    auto* appUpdateTask=row(maintain,"settingsAppUpdateRow","Mlash 更新",G::Refresh); connect(appUpdateTask,&QPushButton::clicked,this,[this] { showTool("app-update",{},[this] { return new AppUpdateDialog(controller_,this); }); });
    auto* backupTask=row(maintain,"settingsBackupRow","备份与恢复",G::Folder); connect(backupTask,&QPushButton::clicked,this,[this] { openBackupTask("create"); });
    auto* geoRow=row(maintain,"settingsGeoRow","Geo 数据",G::Globe); connect(geoRow,&QPushButton::clicked,geo,&QAction::trigger);
    auto* diagnostics=row(maintain,"settingsDiagnosticsRow","诊断",G::Gauge); connect(diagnostics,&QPushButton::clicked,this,&MainWindow::showOutboundDiagnostics);
    taskRow(maintain,"settingsToolsRow","文件与其他工具",G::Folder,"tools");
    auto* aboutRows=group(directory,"关于"); auto* about=row(aboutRows,"settingsAboutRow","Mlash",G::Info,MLASH_VERSION); connect(about,&QPushButton::clicked,this,[this] { auto* task=new TaskDialog(this); task->setWindowTitle("关于 Mlash"); task->setObjectName("aboutTask"); auto* box=new QVBoxLayout(task); auto* text=new QLabel("Mlash " MLASH_VERSION "\nQt " QT_VERSION_STR "\nMihomo 图形客户端"); text->setWordWrap(true); box->addWidget(text); box->addStretch(); presentTask(task,"about"); }); directory->addStretch(1);
    auto* directoryScroll=new QScrollArea; directoryScroll->setObjectName("settingsDirectoryScroll"); directoryScroll->setFrameShape(QFrame::NoFrame); directoryScroll->setWidgetResizable(true); directoryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); directoryScroll->setWidget(settingsDirectory_);
    settingsStack_->addWidget(directoryScroll); settingsStack_->addWidget(surface);
    // Reuse the original single edit session; only its presentation moves behind directory tasks.
    auto* taskHeader=new QHBoxLayout; auto* backButton=new QPushButton("‹ 返回设置"); backButton->setObjectName("settingsBack"); backButton->setProperty("backLink",true); taskHeader->addWidget(backButton); taskHeader->addStretch(); outer->insertLayout(0,taskHeader);
    settingsTaskTitle_=new QLabel; settingsTaskTitle_->setObjectName("dialogTitle"); outer->insertWidget(1,settingsTaskTitle_);
    connect(backButton,&QPushButton::clicked,this,&MainWindow::returnToSettingsDirectory);
    // Search is available from the directory without a permanent search bar in the page header.
    locator->hide(); auto* searchSettings=new QPushButton("搜索设置"); searchSettings->setToolTip("搜索设置 · Ctrl+F"); searchSettings->setObjectName("openSettingsSearch"); searchSettings->setProperty("backLink",true); directory->insertWidget(0,searchSettings,0,Qt::AlignRight);
    connect(searchSettings,&QPushButton::clicked,this,[this,locator] { openSettingsTask("all"); locator->show(); if (auto* input=locator->findChild<QLineEdit*>("settingsSearch")) input->setFocus(); });
    auto* directoryFind=new QShortcut(QKeySequence::Find,settingsDirectory_); directoryFind->setContext(Qt::WindowShortcut); connect(directoryFind,&QShortcut::activated,searchSettings,&QPushButton::click);
    const QHash<QString,QString> directoryAnchors{{"外观","settingsAppearanceRow"},{"文字大小","settingsTextSizeRow"},{"登录启动","settingsAutostartRow"},{"关闭窗口时释放界面","settingsLightweightRow"},{"全局快捷键","settingsHotkeysRow"},{"TUN 虚拟网卡","settingsTunRow"},{"系统代理排除","settingsBypassRow"}};
    for (auto it=directoryAnchors.begin();it!=directoryAnchors.end();++it) locator->remapAnchor(it.key(),settingsDirectory_->findChild<QWidget*>(it.value()));
    connect(locator,&SettingsSearch::anchorRequested,settingsDirectory_,[this,directoryScroll](QWidget* anchor) { if (anchor&&settingsDirectory_->isAncestorOf(anchor)) { navigate(Route{ViewIds::Settings}); settingsStack_->setCurrentIndex(0); directoryScroll->ensureWidgetVisible(anchor,8,12); anchor->setFocus(Qt::ShortcutFocusReason); } });
    settingsStack_->setCurrentIndex(0);
    return settingsStack_;
}
void MainWindow::openBackupTask(const QString& task) {
    auto* workspace=taskWorkspaceFor("backup");
    const QList<TaskWorkspace::Task> tasks{
        {"backup:create","创建备份",[this] { showTool("backup","create",[this] { return new BackupDialog(controller_,false,this); }); }},
        {"backup:restore","恢复备份",[this] { showTool("backup","restore",[this] { return new BackupDialog(controller_,true,this); }); }},
        {"backup:plan","自动备份",[this] { showTool("local-backup",{},[this] { return new LocalBackupDialog(controller_,this); }); }},
        {"backup:webdav","WebDAV",[this] { showTool("remote-backup",{},[this] { return new RemoteBackupDialog(controller_,this); }); }}
    };
    if (workspace->configure("settings","备份与恢复",tasks,[this] { returnFromWorkspace(); })) workspace->select("backup:"+task);
}
void MainWindow::openSettingsTask(const QString& task) {
    if (!settingsStack_) return;
    if (task=="core") { showTool("core-update",{},[this] { return new CoreUpdateDialog(controller_,this); }); return; }
    if (task=="backup") { openBackupTask("create"); return; }
    if (!settingsTask_.isEmpty()&&settingsTask_!=task&&task!="all"&&settingsDirty()) {
        if (QMessageBox::question(this,"未保存的修改","放弃尚未应用的网络设置，进入其他设置？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
        resetSettingsDraft();
    }
    const QHash<QString,QStringList> fields{
        {"startup",{"启动时隐藏窗口","启动内核","启动时恢复代理开关","启动页面","托盘单击","链接导入"}},
        {"network",{"本地端口","节点延迟测试地址","日志等级","IPv6","统一延迟"}},
        {"core",{"mihomo 文件","内核更新","当前运行配置"}},
        {"backup",{"本地备份","恢复与迁移","自动备份与保护","WebDAV备份"}},
        {"tools",{"本地代理地址","数据目录","代理环境变量","IP 与域名分类库","当前运行配置","内核目录","日志目录","链接导入"}}
    };
    const auto allowed=fields.value(task); const QSet<QString> directoryOnly{"外观","文字大小","登录启动","关闭窗口时释放界面","全局快捷键","TUN 虚拟网卡","系统代理排除"};
    for (auto* card:settingsSections_) {
        int count=0;
        for (auto* item:card->findChildren<QWidget*>("settingRow")) {
            const auto name=item->property("settingSearchName").toString(); const bool show=task=="all"?!QStringList{"登录启动","关闭窗口时释放界面"}.contains(name):allowed.contains(name);
            item->setVisible(show); if (show) ++count;
        }
        card->setVisible(count>0);
        if (auto* title=card->findChild<QLabel*>("sectionTitle")) title->hide();
    }
    if (task!="all"||!settingsDirty()) settingsTask_=task;
    const QHash<QString,QString> names{{"startup","启动与托盘"},{"network","本地代理"},{"core","内核管理"},{"backup","备份与恢复"},{"tools","文件与其他工具"},{"all","搜索设置"}};
    settingsTaskTitle_->setText(names.value(task,"设置")); settingsStack_->setCurrentIndex(1); settingsScroll_->verticalScrollBar()->setValue(0); updateActions();
}
void MainWindow::returnToSettingsDirectory() {
    if (settingsApplying_) return;
    if (settingsDirty()) {
        if (QMessageBox::question(this,"未保存的修改","放弃尚未应用的网络设置并返回？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
        resetSettingsDraft();
    }
    settingsTask_.clear(); settingsStack_->setCurrentIndex(0); route_.category=-1; updateWindowTitle();
}

}
