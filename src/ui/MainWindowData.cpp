#include "app/AppController.h"
#include "WindowUiFactories.h"
#include "MainWindow.h"
#include "ViewIds.h"
#include "PrototypeUi.h"
#include "DetailsDrawer.h"
#include "MenuPushButton.h"
#include "WrapLayout.h"
#include "RuleOverlayDialog.h"
#include "ConnectionDetailsDialog.h"
#include "HomeMetricsView.h"
#include "ConnectionOptions.h"
#include "UiTheme.h"
#include "WebsiteTestsPage.h"
#include "CompactComboBox.h"
#include "UiIcons.h"
#include "WidgetStyle.h"
#include "profile/RuleDraft.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QStyle>
#include <QShortcut>
#include <QMenu>
#include <QScrollArea>
#include <QAction>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QGridLayout>
#include <QTabWidget>
#include <QToolButton>
#include <QTabBar>
#include <QStyledItemDelegate>
#include <QStyleHints>
#include <QStyleOptionButton>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QKeyEvent>
#include <functional>
#include <algorithm>

namespace mlash {
using namespace WindowParts;
QWidget* MainWindow::dataPage() {
    // K6（原型 · 数据页）：一条页签行 + 一个面板栈。四个页签各自持有自己的面板，
    // 可见性由 updateDataPanelVisibility 按「当前页签」合成——不可见的面板不再接收采样。
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0,0,0,0); layout->setSpacing(UiTheme::PanelGap);
    auto* heading = new QHBoxLayout; heading->setSpacing(10);
    dataTabs_ = new QTabBar; dataTabs_->setObjectName("dataTabs"); dataTabs_->setExpanding(false); dataTabs_->setDrawBase(false);
    for (int i = 0; i < ViewIds::DataTabCount; ++i) dataTabs_->addTab(ViewIds::dataTabName(ViewIds::dataTabFromIndex(i)));
    heading->addWidget(dataTabs_,0,Qt::AlignLeft); heading->addStretch();
    auto* copy = new QPushButton("复制诊断摘要"); copy->setObjectName("copyDiagnosticsSummary"); copy->setToolTip("复制当前流量、连接与日志的脱敏摘要，便于反馈问题");
    copy->hide(); layout->addLayout(heading);
    dataPanels_ = new QStackedWidget; dataPanels_->setObjectName("dataPanels");
    dataPanels_->addWidget(trafficPanel());       // DataTab::Traffic
    dataPanels_->addWidget(connectionsPanel());   // DataTab::Connections
    dataPanels_->addWidget(new QWidget);          // DataTab::Tests：由构造函数装入 WebsiteTestsPage
    dataPanels_->addWidget(diagnosticsPanel());   // DataTab::Logs
    layout->addWidget(dataPanels_, 1);
    buildDataTabs(page);
    connect(copy, &QPushButton::clicked, this, [this] { copyDiagnosticsSummary(); });
    return page;
}
void MainWindow::buildDataTabs(QWidget*) {
    connect(dataTabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (!dataPanels_ || index < 0) return;
        if (route_.view == ViewIds::Data && route_.tab == int(ViewIds::DataTab::Connections)
            && index != int(ViewIds::DataTab::Connections)) rememberConnectionPosition();
        dataPanels_->setCurrentIndex(index);
        route_.tab = index;
        updateDataPanelVisibility();
        // 同一视图内换页签不走 PageRouter 的离开/进入（视图没变），
        // 所以这里显式再跑一次 enterView(Data) 让副作用按新页签重算。
        pageRouter_.go(route_);
    });
}
int MainWindow::currentDataTab() const {
    if (route_.view != ViewIds::Data) return -1;
    return route_.tab >= 0 ? route_.tab : (dataTabs_ ? dataTabs_->currentIndex() : int(ViewIds::DataTab::Traffic));
}
void MainWindow::selectDataTab(int tab) {
    if (!dataTabs_ || tab < 0 || tab >= dataTabs_->count()) return;
    dataPanels_->setCurrentIndex(tab);
    if (dataTabs_->currentIndex() != tab) dataTabs_->setCurrentIndex(tab);   // 交给 currentChanged 统一收尾
}
void MainWindow::updateDataPanelVisibility() {
    // 四个面板的可见性互相独立，且每个 setter 都有破坏性副作用
    // （连接会清快照、日志会停流、测试会取消、首页会停采样），所以一律用
    // 「视图可见 && 当前页签」合成，不能直接传 false 再传 true。
    const bool showing = isVisible() && route_.view == ViewIds::Data && (!contentStack_||contentStack_->currentIndex()==0);
    const int tab = currentDataTab();
    if (tab >= 0 && dataPanels_) dataPanels_->setCurrentIndex(tab);
    controller_->setHomeVisible(showing && tab == int(ViewIds::DataTab::Traffic));
    controller_->setConnectionsVisible(showing && tab == int(ViewIds::DataTab::Connections));
    controller_->setDiagnosticsVisible(showing && tab == int(ViewIds::DataTab::Logs) && !logsPaused_);
    websitePage_->setActive(showing && tab == int(ViewIds::DataTab::Tests), selectedGroup_);
}
QWidget* MainWindow::trafficPanel() {
    // This page is already the detail surface. Show its one metric source directly.
    auto* panel = new QWidget; panel->setObjectName("trafficPanel"); auto* layout = new QVBoxLayout(panel); layout->setContentsMargins(0,0,0,0); layout->setSpacing(12);
    auto* metrics = new HomeMetricsView(controller_->homeMetrics()); connect(metrics,&HomeMetricsView::inspectRequested,this,&MainWindow::showOutboundDiagnostics); layout->addWidget(metrics);
    auto* missing=new QLabel; missing->setObjectName("trafficNextReason"); missing->setWordWrap(true);
    auto* next=new QPushButton; next->setObjectName("trafficNextStep"); next->setProperty("primary",true);
    auto* empty=new QFrame; empty->setObjectName("panel"); auto* emptyLayout=new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(16,20,16,20);
    missing->setAlignment(Qt::AlignHCenter);
    emptyLayout->addWidget(missing); emptyLayout->addWidget(next,0,Qt::AlignHCenter);
    layout->addWidget(empty);
    connect(next,&QPushButton::clicked,this,[this] {
        if (controller_->state().activeProfile.isEmpty()) openProfileSetup();
        else navigate(Route{ViewIds::Home});
    });
    const auto refresh=[this,metrics,empty,missing,next] {
        const auto& state=controller_->state();
        const bool hasProfile=!state.activeProfile.isEmpty();
        const bool waiting=state.core==CoreState::Starting || state.core==CoreState::Stopping || state.busy;
        const bool sampled=!controller_->homeMetrics()->snapshot().isEmpty();
        metrics->setVisible(sampled);
        empty->setVisible(!sampled);
        const auto copy=profileSetupCopy(!controller_->profiles().isEmpty());
        missing->setText(!hasProfile?copy.title:waiting?"正在等待连接…":
            !state.ready?"前往首页开启代理后查看流量。":
            controller_->homeMetrics()->error().isEmpty()?"正在采集流量…":"暂时无法读取流量，正在等待下次采集。");
        next->setText(hasProfile?"前往首页":copy.action);
        next->setVisible(!sampled && (!hasProfile || (!waiting && !state.ready)));
    };
    connect(controller_,&AppController::stateChanged,panel,refresh);
    connect(controller_->homeMetrics(),&HomeMetrics::changed,panel,refresh);
    refresh();
    homeOutboundSummary_ = new QLabel; homeOutboundSummary_->setObjectName("outboundSummary"); homeOutboundSummary_->setProperty("secondary", true); homeOutboundSummary_->setWordWrap(true); layout->addWidget(homeOutboundSummary_);
    layout->addStretch();
    return panel;
}
void MainWindow::copyDiagnosticsSummary() {
    // 只拼可公开的事实：计数、速率、状态词。订阅、令牌与原始 YAML 都不进入这里。
    const auto& state = controller_->state();
    const auto& totals = controller_->homeMetrics()->snapshot();
    const auto amount = [](const QJsonValue& value) { return value.isDouble() ? ActivityTableModel::formatBytes(value.toDouble()) : QString("—"); };
    const auto text = QString("Mlash %1 · %2\n内核：%3 · 系统代理：%4 · TUN：%5\n上传 %6 · 下载 %7 · 活动连接 %8\n最近日志 %9 行")
        .arg(QString(MLASH_VERSION), QDateTime::currentDateTime().toString(Qt::ISODate), state.ready ? QString("运行中") : QString("未就绪"),
             proxyStateText(state.proxy), state.tunObserved ? QString("已接管") : QString("未接管"),
             amount(totals.value("uploadTotal")), amount(totals.value("downloadTotal")),
             QString::number(controller_->connections().size()), QString::number(controller_->diagnostics()->lines().size()));
    QApplication::clipboard()->setText(text);
    message_->setText("诊断摘要已复制到剪贴板"); message_->setToolTip(text);
}
QWidget* MainWindow::diagnosticsPanel() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(UiTheme::PanelGap);
    auto* toolbar=new QWidget; toolbar->setObjectName("logToolbar"); auto* actions = new WrapLayout(toolbar);
    auto* logPanel = panel(); auto* logLayout = new QVBoxLayout(logPanel); logLayout->setContentsMargins(10, 8, 10, 8); logLayout->setSpacing(6); layout->addWidget(logPanel, 1);
    logSearch_ = new QLineEdit; logSearch_->setObjectName("logSearch"); logSearch_->setPlaceholderText("搜索日志…"); logSearch_->setAccessibleName("搜索日志"); logSearch_->setClearButtonEnabled(true); actions->addWidget(logSearch_);
    logFilter_ = new CompactComboBox; logFilter_->setObjectName("logLevelFilter"); for (const auto& pair : QList<QPair<QString,QString>>{{"全部","all"},{"调试","debug"},{"信息","info"},{"警告","warning"},{"错误","error"},{"应用","app"}}) logFilter_->addItem(pair.first,pair.second); actions->addWidget(logFilter_);
    auto* pause = new QPushButton("暂停显示"); actions->addWidget(pause); pause->setObjectName("pauseLogs"); pause->setCheckable(true);
    auto* more = new MenuPushButton("日志选项"); actions->addWidget(more); auto* menu = new QMenu(more); menu->setObjectName("logOptionsMenu"); more->setMenu(menu);
    auto* report = new QAction("导出脱敏诊断…", this); report->setObjectName("exportDiagnostics");
    auto* exportButton=new QToolButton; exportButton->setObjectName("exportDiagnosticsButton"); exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly); exportButton->setDefaultAction(report); exportButton->setText("导出诊断…"); actions->addWidget(exportButton);
    connect(controller_, &AppController::stateChanged, report, [this, report] { report->setEnabled(!controller_->state().busy && !controller_->state().exiting); });
    report->setEnabled(!controller_->state().busy && !controller_->state().exiting);
    logLayout->addWidget(toolbar);
    logView_ = new QPlainTextEdit; logView_->setObjectName("logView"); logView_->setReadOnly(true); logView_->setLineWrapMode(QPlainTextEdit::NoWrap); logView_->setMaximumBlockCount(2000); logLayout->addWidget(logView_, 1);
    auto* wrap=menu->addAction("自动换行"); wrap->setObjectName("wrapLogs"); wrap->setCheckable(true); connect(wrap,&QAction::toggled,logView_,[this](bool enabled) { const auto cursor=logView_->textCursor(); logView_->setLineWrapMode(enabled?QPlainTextEdit::WidgetWidth:QPlainTextEdit::NoWrap); logView_->setTextCursor(cursor); });
    menu->addSeparator(); auto* copyLogAction=menu->addAction("复制选中"); copyLogAction->setEnabled(false);
    connect(logView_,&QPlainTextEdit::copyAvailable,copyLogAction,&QAction::setEnabled);
    connect(copyLogAction,&QAction::triggered,logView_,&QPlainTextEdit::copy);
    auto* clear=menu->addAction("清空视图"); clear->setObjectName("clearLogView");
    menu->addSeparator(); auto* directory=menu->addAction("日志目录");
    logView_->setToolTip("仅日志页可见时接收内核日志；最多保留 2000 行。");
    // 日志页的空/无匹配/未就绪不能表现为一块空白面板（状态呈现规格：禁止空表格）。
    logEmpty_ = new QLabel(logView_); logEmpty_->setObjectName("logEmptyState"); logEmpty_->setAlignment(Qt::AlignCenter); logEmpty_->setWordWrap(true); logEmpty_->setTextFormat(Qt::PlainText); logEmpty_->setAttribute(Qt::WA_TransparentForMouseEvents); logEmpty_->hide(); followLogs_=new QPushButton("回到最新"); actions->addWidget(followLogs_); followLogs_->setObjectName("followLatestLogs"); followLogs_->setProperty("quiet",true); followLogs_->setToolTip("滚动到最新记录");
    connect(followLogs_,&QPushButton::clicked,this,[this] { auto cursor=logView_->textCursor(); cursor.clearSelection(); logView_->setTextCursor(cursor); logView_->verticalScrollBar()->setValue(logView_->verticalScrollBar()->maximum()); });
    connect(logView_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value) { const bool history=value<logView_->verticalScrollBar()->maximum(); followLogs_->setEnabled(history); followLogs_->setVisible(history); });
    followLogs_->hide();
    connect(logSearch_, &QLineEdit::textChanged, this, &MainWindow::updateLogs);
    controller_->setDiagnosticsLevel("all");
    connect(logFilter_, &QComboBox::currentIndexChanged, this, [this] { controller_->setDiagnosticsLevel(logFilter_->currentData().toString()); updateLogs(); });
    connect(pause, &QPushButton::toggled, this, [this, pause](bool paused) { logsPaused_ = paused; pause->setText(paused ? "恢复显示" : "暂停显示"); pausedLogs_ = paused ? controller_->diagnostics()->lines() : QStringList{}; controller_->setDiagnosticsVisible(!paused && isVisible() && route_.view == ViewIds::Data && currentDataTab() == int(ViewIds::DataTab::Logs)); updateLogs(); });
    connect(clear, &QAction::triggered, this, [this] {
        pausedLogs_.clear();
        controller_->clearLogView();
        updateLogs();
    });
    connect(directory, &QAction::triggered, this, [this] { openDirectory(controller_->dataRoot() + "/logs"); });
    connect(report, &QAction::triggered, this, [this] { const auto path = QFileDialog::getSaveFileName(this, "导出脱敏诊断（不含订阅配置/连接快照）", QDir::homePath()+"/Mlash-diagnostics.json", "JSON (*.json)"); if (!path.isEmpty()) controller_->exportDiagnostics(path); });
    return page;
}
QWidget* MainWindow::rulesPage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(UiTheme::PanelGap);
    auto* back=PrototypeUi::back(layout,"rulesBack","返回订阅"); connect(back,&QPushButton::clicked,this,[this] {
        if (rulesSaving_) return;
        if (rulesDirty_) { if (QMessageBox::question(this,"未保存的规则","放弃当前规则修改并返回？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return; rulesDirty_=false; loadRuleDraft(); }
        navigate(Route{ViewIds::Profiles});
        if (route_.view==ViewIds::Profiles && profileDetailId_!=rulesDraftProfile_ && !rulesDraftProfile_.isEmpty()) showProfileDetails(rulesDraftProfile_);
    });
    auto* contentStack=new QStackedWidget; contentStack->setObjectName("rulesContentStack"); layout->addWidget(contentStack,1);
    auto* box = panel(); box->setObjectName("rulesEditorSurface"); auto* content = new QVBoxLayout(box); content->setContentsMargins(12, 10, 12, 10); contentStack->addWidget(box);
    auto* noProfile=new QWidget; noProfile->setObjectName("rulesNoProfile"); auto* emptyLayout=new QVBoxLayout(noProfile); emptyLayout->addStretch(1);
    auto* emptyTitle=new QLabel("还没有配置"); emptyTitle->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyTitle);
    auto* emptyCaption=new QLabel("导入配置后可编辑优先规则并查看内核实际规则。"); emptyCaption->setProperty("secondary",true); emptyCaption->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyCaption);
    auto* emptyImport=new QPushButton("导入配置"); emptyImport->setObjectName("rulesImportProfile"); emptyImport->setProperty("primary",true); emptyLayout->addWidget(emptyImport,0,Qt::AlignHCenter); emptyLayout->addStretch(2);
    connect(emptyImport,&QPushButton::clicked,this,[this] { navigate(Route{ViewIds::Profiles}); if (route_.view==ViewIds::Profiles) addUrl(); });
    contentStack->addWidget(noProfile);
    auto* row = new QHBoxLayout; auto* profileLabel=new QLabel("订阅"); profileLabel->setObjectName("rulesProfileLabel"); row->addWidget(profileLabel); auto* actualProfile=new QLabel; actualProfile->setObjectName("rulesRuntimeContext"); actualProfile->setTextFormat(Qt::PlainText); actualProfile->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); row->addWidget(actualProfile,1); actualProfile->hide(); auto* ruleNext=new QPushButton("查看配置规则"); ruleNext->setObjectName("rulesNextStep"); row->addWidget(ruleNext); connect(ruleNext,&QPushButton::clicked,this,[this] { const auto id=rulesProfiles_->currentData().toString(); navigate(Route{ViewIds::Profiles}); if (route_.view==ViewIds::Profiles && !id.isEmpty()) openProfileTask(id,"rules"); }); rulesProfiles_ = new CompactComboBox; rulesProfiles_->setObjectName("rulesProfiles"); row->addWidget(rulesProfiles_, 1); content->addLayout(row);
    auto* providers = button("规则源…", row); providers->setObjectName("ruleProviders"); connect(providers, &QPushButton::clicked, this, [this] { showProviders(rulesProfiles_->currentData().toString(), "rule-providers"); });
    auto* tabs = new QTabWidget; tabs->setObjectName("rulesTabs"); rulesTabs_ = tabs; content->addWidget(tabs, 1);
    auto* editPage = new QWidget; auto* edits = new QVBoxLayout(editPage); edits->setContentsMargins(8, 10, 8, 8);
    auto* entrySurface=new QWidget; entrySurface->setObjectName("ruleBuilder"); auto* entry = new WrapLayout(entrySurface); auto* type = new CompactComboBox; type->setObjectName("ruleType"); type->setProperty("shortLabel",true); type->setMaximumWidth(150);
    for (const auto& code:RuleDraft::types()) type->addItem(RuleDraft::label(code),code);
    entry->addWidget(type);
    auto* match = new QLineEdit; match->setObjectName("ruleMatch"); match->setPlaceholderText("域名，如 example.com"); entry->addWidget(match); ruleTarget_ = new CompactComboBox; ruleTarget_->setObjectName("ruleTarget"); ruleTarget_->setEditable(true); ruleTarget_->setMaximumWidth(160); ruleTarget_->addItem("直连 · DIRECT", "DIRECT"); ruleTarget_->addItem("阻断 · REJECT", "REJECT"); ruleTarget_->setToolTip("选择直连、阻断或策略组；也可输入该订阅的真实策略组名称"); auto* target = ruleTarget_; entry->addWidget(target); auto* add = new QPushButton("添加"); entry->addWidget(add); add->setObjectName("addCustomRule"); edits->addWidget(entrySurface);
    rulesEditor_ = new QPlainTextEdit; rulesEditor_->setObjectName("customRulesEditor"); rulesEditor_->setPlaceholderText("每行一条规则"); rulesEditor_->setToolTip("例如：DOMAIN-SUFFIX,example.com,DIRECT\n最多 256 条；按从上到下的顺序优先匹配。留空沿用订阅。"); edits->addWidget(rulesEditor_, 1); rulesEditor_->hide();
    auto* list=new QTableWidget(0,4); list->setObjectName("customRulesList"); list->setHorizontalHeaderLabels({"类型","匹配内容","目标","选项"}); list->setSelectionBehavior(QAbstractItemView::SelectRows); list->setSelectionMode(QAbstractItemView::SingleSelection); list->setEditTriggers(QAbstractItemView::NoEditTriggers); list->setToolTip("双击规则打开完整编辑；删除和排序在保存后生效。"); connect(list,&QTableWidget::itemDoubleClicked,this,[this](QTableWidgetItem* item) { const int row=item->row(); findChild<QPushButton*>("advancedRules")->click(); if (auto* dialog=findChild<RuleOverlayDialog*>()) if (auto* table=dialog->findChild<QTableWidget*>("prependRulesTable")) table->selectRow(row); }); list->verticalHeader()->hide(); styleTable(list); list->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch); list->setColumnWidth(0,135); list->setColumnWidth(2,140); list->setColumnWidth(3,90); list->setColumnHidden(3,true); edits->insertWidget(1,list,1);
    auto* noRules=emptyState(list,"还没有优先规则\n在上方添加，或打开完整规则编辑");
    auto* listActions=new QHBoxLayout; auto* removeRule=button("删除选中",listActions); removeRule->setObjectName("removeQuickRule"); auto* moveUp=button("上移",listActions); moveUp->setObjectName("moveQuickRuleUp"); auto* moveDown=button("下移",listActions); moveDown->setObjectName("moveQuickRuleDown"); listActions->addStretch(); auto* textMode=new QToolButton; textMode->setObjectName("ruleTextMode"); textMode->setText("高级文本"); textMode->setCheckable(true); listActions->addWidget(textMode); edits->addLayout(listActions);
    connect(rulesEditor_,&QPlainTextEdit::textChanged,this,&MainWindow::rebuildRuleList); rebuildRuleList();
    connect(list,&QTableWidget::itemSelectionChanged,this,&MainWindow::updateActions);
    for (auto* control:{removeRule,moveUp,moveDown}) connect(control,&QPushButton::clicked,this,[this,list,control,removeRule,moveUp] { const int row=list->currentRow(); if (rulesEditor_->isReadOnly()||row<0||!control->isEnabled()) return; auto lines=rulesEditor_->toPlainText().split('\n'); const int line=list->item(row,0)->data(Qt::UserRole).toInt(); int wanted=row; if (control==removeRule) lines.removeAt(line); else { wanted=row+(control==moveUp?-1:1); const int other=list->item(wanted,0)->data(Qt::UserRole).toInt(); lines.swapItemsAt(line,other); } rulesEditor_->setPlainText(lines.join('\n')); if (list->rowCount()) list->selectRow(qBound(0,wanted,list->rowCount()-1)); });
    connect(textMode,&QToolButton::toggled,this,[list,this,removeRule,moveUp,moveDown](bool text) { list->setVisible(!text); rulesEditor_->setVisible(text); for (auto* control:{removeRule,moveUp,moveDown}) control->setVisible(!text); });
    connect(type, &QComboBox::currentIndexChanged, match, [type, match](int) { const auto value = type->currentData().toString(); match->setEnabled(value != "MATCH"); match->setPlaceholderText(value == "MATCH" ? "无需匹配内容" : value.contains("CIDR") ? "IP 段，如 192.0.2.0/24" : value.contains("PORT") ? "端口，如 443" : value.contains("PROCESS") ? "进程名称或完整路径" : value == "GEOIP" ? "地区代码，如 CN" : value == "GEOSITE" ? "域名分类，如 google" : "域名或关键词，如 example.com"); });
    connect(add, &QPushButton::clicked, this, [this, type, match, target] { const auto code = type->currentData().toString(), destination = ruleTargetValue(target); if (destination.isEmpty() || (code != "MATCH" && match->text().trimmed().isEmpty())) { rulesHint_->setText("请填写匹配内容和目标策略"); return; } rulesEditor_->appendPlainText(code + "," + (code == "MATCH" ? QString() : match->text().trimmed() + ",") + destination); match->clear(); match->setFocus(); });
    tabs->addTab(editPage, "优先规则");
    auto* readPage = new QWidget; auto* readLayout = new QVBoxLayout(readPage); auto* filter = new QLineEdit; filter->setObjectName("activeRulesSearch"); filter->setPlaceholderText("检索规则类型 / 匹配内容 / 目标策略"); filter->setClearButtonEnabled(true); readLayout->addWidget(filter);
    rulesFilter_.setSourceModel(&rulesModel_); rulesFilter_.setFilterKeyColumn(-1); rulesFilter_.setFilterCaseSensitivity(Qt::CaseInsensitive); connect(filter, &QLineEdit::textChanged, &rulesFilter_, &QSortFilterProxyModel::setFilterFixedString);
    auto* active = new QTableView; active->setObjectName("activeRulesTable"); active->setModel(&rulesFilter_); active->setEditTriggers(QAbstractItemView::NoEditTriggers); active->verticalHeader()->hide(); styleTable(active); active->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); active->setColumnWidth(0, 130); active->setColumnWidth(2, 140); readLayout->addWidget(active); auto* emptyRuntime=emptyState(active,"暂未读取到内核规则"); emptyRuntime->setObjectName("rulesRuntimeEmpty"); tabs->addTab(readPage, "内核实际规则");
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) { controller_->setRulesVisible(isVisible() && route_.view == ViewIds::Rules && index == 1); if (index == 1) controller_->refreshRules(); updateActions(); });
    rulesHint_ = new ElidedLabel; rulesHint_->setObjectName("secondaryText"); rulesHint_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    auto* advanced = new QPushButton("完整规则编辑…"); advanced->setToolTip("推荐：按类型、匹配条件和目标分列编辑；也可查看订阅来源规则。下方文本保留高级编辑能力。"); advanced->setObjectName("advancedRules"); row->insertWidget(row->indexOf(providers),advanced); connect(advanced,&QPushButton::clicked,this,[this] { if (controller_->state().busy || rulesDraftProfile_.isEmpty()) return; QStringList draft; for (const auto& raw:rulesEditor_->toPlainText().split('\n')) { const auto line=raw.trimmed(); if (line.isEmpty()||line.startsWith('#')) continue; if (line.size()>1024||line.split(',').size()>(line.section(',',0,0)=="MATCH"?3:4)||draft.size()>=256) { rulesHint_->setText("请先修正超长、额外字段或超过256条的文本，原草稿保留"); return; } draft.append(line); } auto* dialog=new RuleOverlayDialog(controller_,rulesDraftProfile_,this,{{"prepend",QJsonArray::fromStringList(draft)},{"expected",rulesExpected_}}); connect(dialog,&QDialog::accepted,this,[this] { rulesDirty_=false; loadRuleDraft(); }); presentTask(dialog); }); connect(tabs,&QTabWidget::currentChanged,advanced,[advanced](int index) { advanced->setVisible(index==0); });
    auto* bottom = new QHBoxLayout; auto* reset = button("放弃修改", bottom); reset->setObjectName("resetCustomRules"); bottom->addWidget(rulesHint_,1); saveRules_ = button("保存规则", bottom); saveRules_->setObjectName("saveCustomRules"); saveRules_->setProperty("primary", true); content->addLayout(bottom); mutations_ += {saveRules_, reset, add, type, match, target};
    connect(tabs, &QTabWidget::currentChanged, this, [this, reset](int index) { reset->setVisible(index == 0); saveRules_->setVisible(index == 0); rulesHint_->setText(index==0&&rulesDirty_?"有未保存修改":QString()); });
    connect(reset, &QPushButton::clicked, this, [this] { rulesDirty_ = false; loadRuleDraft(); });
    connect(rulesEditor_, &QPlainTextEdit::textChanged, this, [this] { QString saved; for (const auto& p:controller_->profiles()) if (p.id==rulesDraftProfile_) saved=p.customRules.join('\n'); rulesDirty_=rulesEditor_->toPlainText()!=saved; rulesHint_->setText(!rulesDirty_?QString():ruleDraftError().isEmpty()?"有未保存修改":ruleDraftError()); rulesHint_->setToolTip("最多 256 条；自定义规则优先于原规则，运行中订阅保存会安全重载"); updateActions(); });
    connect(rulesProfiles_, &QComboBox::currentIndexChanged, this, [this] { if (rulesDirty_) { const QSignalBlocker block(rulesProfiles_); rulesProfiles_->setCurrentIndex(rulesProfiles_->findData(rulesDraftProfile_)); rulesHint_->setText("尚有未保存修改，请保存或还原后再切换订阅"); return; } loadRuleDraft(); });
    connect(saveRules_, &QPushButton::clicked, this, [this] { QStringList rules; for (const auto& line : rulesEditor_->toPlainText().split('\n')) { const auto value = line.trimmed(); if (!value.isEmpty() && !value.startsWith('#')) rules.append(value); } rulesSaving_ = true; rulesHint_->setText("正在校验并保存…"); controller_->updateCustomRules(rulesProfiles_->currentData().toString(), rules,rulesExpected_); });
    return page;
}
void MainWindow::rebuildRuleList() {
    auto* list=findChild<QTableWidget*>("customRulesList"); if (!list) return;
    const int selected=list->currentRow(); const QSignalBlocker block(list); list->setRowCount(0);
    const auto lines=rulesEditor_->toPlainText().split('\n'); bool hasOptions=false;
    for (int line=0;line<lines.size()&&list->rowCount()<256;++line) {
        const auto raw=lines[line].trimmed(); if (raw.isEmpty()||raw.startsWith('#')) continue;
        const auto fields=raw.split(','); const int row=list->rowCount(); list->insertRow(row);
        auto* type=new QTableWidgetItem(RuleDraft::label(fields.value(0))); type->setData(Qt::UserRole,line); list->setItem(row,0,type);
        list->setItem(row,1,new QTableWidgetItem(fields.value(0)=="MATCH"?QString("全部请求"):fields.value(1)));
        list->setItem(row,2,new QTableWidgetItem(fields.value(fields.value(0)=="MATCH"?1:2)));
        const auto options=fields.mid(fields.value(0)=="MATCH"?2:3).join(','); hasOptions|=!options.isEmpty(); list->setItem(row,3,new QTableWidgetItem(options));
        for (int col=0;col<4;++col) list->item(row,col)->setToolTip(raw);
    }
    list->setColumnHidden(3,!hasOptions); if (list->rowCount()) list->selectRow(qBound(0,selected,list->rowCount()-1));
    list->findChild<QLabel*>("emptyState")->setVisible(list->rowCount()==0);
    setEmptyStateAction(list, list->rowCount()==0 && !rulesDraftProfile_.isEmpty() ? QString("打开完整规则编辑…") : QString(), [this] { if (auto* advanced = findChild<QPushButton*>("advancedRules")) advanced->click(); });
}
void MainWindow::updateRulesProfiles() {
    const auto selected = rulesProfiles_->currentData().toString(); const QSignalBlocker block(rulesProfiles_); rulesProfiles_->clear();
    for (const auto& p : controller_->profiles()) rulesProfiles_->addItem(p.name + (p.id == controller_->state().activeProfile ? " · 当前" : ""), p.id);
    auto index = rulesProfiles_->findData(selected); if (index < 0) index = rulesProfiles_->findData(controller_->state().activeProfile); rulesProfiles_->setCurrentIndex(index < 0 && rulesProfiles_->count() ? 0 : index);
    if (rulesDirty_ && rulesProfiles_->findData(rulesDraftProfile_) >= 0) rulesProfiles_->setCurrentIndex(rulesProfiles_->findData(rulesDraftProfile_)); else { rulesDirty_ = false; loadRuleDraft(); }
    const bool hasProfile=rulesProfiles_->count()>0;
    if (auto* stack=findChild<QStackedWidget*>("rulesContentStack")) stack->setCurrentIndex(hasProfile?0:1);
    if (auto* back=findChild<QPushButton*>("rulesBack")) back->setVisible(hasProfile);
}
void MainWindow::loadRuleDraft() {
    const QSignalBlocker block(rulesEditor_); rulesDraftProfile_ = rulesProfiles_->currentData().toString(); QStringList rules;
    for (const auto& p : controller_->profiles()) if (p.id == rulesDraftProfile_) { rules=p.customRules; rulesExpected_=RuleDraft::snapshot(p); }
    ruleReferencesKnown_=false; ruleTargets_.clear(); ruleProviders_.clear(); const auto reference=++rulesReferenceGeneration_;
    rulesEditor_->setPlainText(rules.join('\n')); rebuildRuleList(); rulesDirty_ = false; rulesHint_->clear();
    const auto target = ruleTargetValue(ruleTarget_); ruleTarget_->clear(); ruleTarget_->addItem("直连 · DIRECT", "DIRECT"); ruleTarget_->addItem("阻断 · REJECT", "REJECT");
    if (rulesDraftProfile_ == controller_->state().activeProfile) for (auto it = controller_->proxies().begin(); it != controller_->proxies().end(); ++it) if (it.value().toObject().value("all").isArray()) ruleTarget_->addItem(it.key(), it.key());
    const auto index = ruleTarget_->findData(target); if (index >= 0) ruleTarget_->setCurrentIndex(index); else if (!target.isEmpty()) ruleTarget_->setEditText(target); updateActions();
    if (isVisible() && route_.view == ViewIds::Rules && !rulesDraftProfile_.isEmpty()) controller_->inspectRuleSource(rulesDraftProfile_,[guard=QPointer<MainWindow>(this),id=rulesDraftProfile_,reference](Result result) { if (!guard||guard->rulesDraftProfile_!=id||guard->rulesReferenceGeneration_!=reference||!result.ok) return; guard->ruleTargets_.clear(); guard->ruleProviders_.clear(); const auto current=ruleTargetValue(guard->ruleTarget_); const QSignalBlocker block(guard->ruleTarget_); guard->ruleTarget_->clear(); for (const auto& value:result.data.value("targets").toArray()) { const auto code=value.toString(); guard->ruleTargets_.insert(code); guard->ruleTarget_->addItem(code=="DIRECT"?QString("直连 · DIRECT"):code=="REJECT"?QString("阻断 · REJECT"):code,code); } for (const auto& value:result.data.value("rule_providers").toArray()) guard->ruleProviders_.insert(value.toString()); const auto index=guard->ruleTarget_->findData(current); if (index>=0) guard->ruleTarget_->setCurrentIndex(index); else guard->ruleTarget_->setEditText(current); guard->ruleReferencesKnown_=true; if (guard->rulesDirty_) guard->rulesHint_->setText(guard->ruleDraftError().isEmpty()?QString("有未保存修改"):guard->ruleDraftError()); guard->updateActions(); });
}
QWidget* MainWindow::connectionsPanel() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(UiTheme::PanelGap);
    auto* box = panel(); auto* content = new QVBoxLayout(box); content->setContentsMargins(10, 8, 10, 8); layout->addWidget(box, 1);
    connectionSummary_=new QWidget(box); connectionSummary_->setObjectName("connectionSummary"); auto* summary=new QHBoxLayout(connectionSummary_); summary->setContentsMargins(0,0,0,0); connectionStats_=new ElidedLabel("流量 —"); connectionStats_->setObjectName("connectionStats"); connectionStats_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); summary->addWidget(connectionStats_,1);
    connectionPause_=new QPushButton("暂停刷新"); connectionPause_->setObjectName("pauseConnections"); connectionPause_->setCheckable(true); connectionPause_->setToolTip("暂停采样并保留阅读快照；恢复后先建立新基线"); summary->addWidget(connectionPause_); connect(connectionPause_,&QPushButton::clicked,controller_,&AppController::setConnectionsPaused);
    auto* details=new QAction("详情",this); connectionDetailsAction_=details; details->setObjectName("connectionDetailsAction"); auto* copy=new QAction("复制详情",this); connectionCopyAction_=copy; auto* order=new QAction("恢复默认顺序",this); order->setObjectName("defaultConnectionOrder"); order->setToolTip("恢复内核返回的顺序");
    closeAllConnections_=new QAction("断开全部…",this); closeAllConnections_->setObjectName("closeAllConnections"); closeAllConnections_->setToolTip("关闭内核全部活动连接，需要确认"); auto* closeAll=actionButton(closeAllConnections_,summary,"closeAllConnectionsButton"); closeAll->setProperty("danger",true); content->addWidget(connectionSummary_);
    connect(details,&QAction::triggered,this,[this] { connectionDetails(); }); connect(copy,&QAction::triggered,this,[this] { connectionDetails(true); }); connect(order,&QAction::triggered,this,[this] { connectionsTable_->sortByColumn(-1,Qt::AscendingOrder); });
    connect(closeAllConnections_,&QAction::triggered,this,[this] { if (QMessageBox::question(this,"关闭全部活动连接","将关闭内核当前的全部活动连接，正在进行的请求可能中断。继续？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes) controller_->closeAllConnections(); });
    auto* tabsRow=new QHBoxLayout; connectionView_=new QTabBar; connectionView_->setObjectName("connectionView"); connectionView_->setExpanding(false); connectionView_->setDrawBase(false); connectionView_->addTab("活动连接"); connectionView_->addTab("已结束"); connectionView_->setToolTip("离开快照仅表示本页观察到条目消失；最多保留 1000 条，隐藏后清空"); delete tabsRow; auto* searchToolbar=new QWidget; searchToolbar->setObjectName("connectionToolbar"); auto* tools=new WrapLayout(searchToolbar); tools->addWidget(connectionView_); tools->addWidget(connectionPause_);
    auto* search = new QLineEdit; search->setObjectName("connectionsSearch"); search->setPlaceholderText("搜索目标、规则、链路或进程…"); search->setClearButtonEnabled(true); tools->addWidget(search); content->addWidget(searchToolbar);
    connectionsFilter_.setSourceModel(&connectionsModel_); connectionsFilter_.setFilterKeyColumn(0); connectionsFilter_.setFilterRole(Qt::UserRole+2); connectionsFilter_.setSortRole(Qt::UserRole+1); connectionsFilter_.setFilterCaseSensitivity(Qt::CaseInsensitive); connect(connectionView_,&QTabBar::currentChanged,this,&MainWindow::updateConnections);
    connectionsTable_ = new QTableView; connectionsTable_->setObjectName("connectionsTable"); connectionsTable_->setModel(&connectionsFilter_); connectionsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers); connectionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows); connectionsTable_->setSelectionMode(QAbstractItemView::SingleSelection); connectionsTable_->verticalHeader()->hide(); styleTable(connectionsTable_); connectionsTable_->setSortingEnabled(true); connectionsTable_->sortByColumn(-1,Qt::AscendingOrder); content->addWidget(connectionsTable_, 1);
    connectionOptions_=new ConnectionOptions(controller_,connectionsTable_,&connectionsFilter_,search,nullptr); tools->addWidget(connectionOptions_); connectionOptions_->addViewAction(order); connect(connectionsTable_->horizontalHeader(),&QHeaderView::sortIndicatorChanged,order,[this,order] { order->setEnabled(connectionsFilter_.sortColumn()>=0); }); order->setEnabled(false);
    connectionEmpty_=emptyState(connectionsTable_,"暂无活动连接");
    auto* bottom = new QHBoxLayout; connectionsHint_ = new ElidedLabel("内核未就绪"); connectionsHint_->setObjectName("connectionSamplingStatus"); connectionsHint_->setProperty("secondary",true); connectionsHint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); bottom->addWidget(connectionsHint_, 1); connectionDetailsButton_=actionButton(details,bottom,"connectionDetailsButton"); connectionCopyButton_=actionButton(copy,bottom,"connectionCopyButton"); closeConnection_ = button("断开选中", bottom); closeConnection_->setObjectName("closeSelectedConnection"); closeConnection_->setProperty("danger",true); closeConnection_->setEnabled(false); content->addLayout(bottom); connectionDetailsButton_->hide(); connectionCopyButton_->hide(); closeConnection_->hide();
    auto* inspectKey=new QShortcut(QKeySequence(Qt::Key_Return),connectionsTable_); inspectKey->setContext(Qt::WidgetWithChildrenShortcut); connect(inspectKey,&QShortcut::activated,this,[this] { connectionDetails(); });
    connect(connectionsTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateActions);
    connect(closeConnection_, &QPushButton::clicked, this, [this] { const auto selected = connectionsTable_->selectionModel()->selectedRows(); if (!selected.isEmpty()) controller_->closeConnection(selected.first().data(Qt::UserRole).toString()); }); connect(connectionsTable_,&QTableView::doubleClicked,this,[this] { connectionDetails(); });
    connectionsTable_->setContextMenuPolicy(Qt::CustomContextMenu); connect(connectionsTable_,&QWidget::customContextMenuRequested,this,[this](const QPoint& point) {
        const auto index=connectionsTable_->indexAt(point);
        if (!index.isValid()) return;
        const QString id=index.data(Qt::UserRole).toString();
        if (id.isEmpty()) return;
        for (auto* old:connectionsTable_->findChildren<QMenu*>("connectionContextMenu")) { old->setObjectName({}); old->close(); }
        connectionsTable_->selectRow(index.row());
        auto* menu=new QMenu(connectionsTable_); menu->setObjectName("connectionContextMenu");
        // Sampling can replace/sort rows while the menu is open. Resolve the ID again at activation.
        const auto locate=[this,id] {
            for (int row=0;row<connectionsFilter_.rowCount();++row)
                if (connectionsFilter_.index(row,0).data(Qt::UserRole).toString()==id) { connectionsTable_->selectRow(row); return true; }
            return false;
        };
        auto* inspect=menu->addAction("详情"); connect(inspect,&QAction::triggered,this,[this,locate] { if (locate()) connectionDetails(); });
        auto* duplicate=menu->addAction("复制详情"); connect(duplicate,&QAction::triggered,this,[this,locate] { if (locate()) connectionDetails(true); });
        menu->addSeparator();
        auto* close=menu->addAction("断开选中"); close->setEnabled(closeConnection_->isEnabled());
        connect(close,&QAction::triggered,this,[this,locate] { if (locate() && closeConnection_->isEnabled()) closeConnection_->click(); });
        connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
        WidgetStyle::popupMenu(menu,connectionsTable_->viewport()->mapToGlobal(point));
    });
    connect(connectionOptions_,&ConnectionOptions::searchFeedback,this,[this](const QString& error) { connectionSearchError_=error; updateConnections(); }); connect(search,&QLineEdit::textChanged,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::rowsRemoved,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::rowsInserted,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::modelReset,this,&MainWindow::updateActions); connect(connectionsTable_->horizontalHeader(),&QHeaderView::sortIndicatorChanged,this,[this](int,Qt::SortOrder) { updateActions(); }); return page;
}
void MainWindow::connectionDetails(bool copy) {
    const auto selected=connectionsTable_->selectionModel()->selectedRows(); if (selected.isEmpty()) return; const auto text=selected.first().data(Qt::ToolTipRole).toString();
    if (copy) { QApplication::clipboard()->setText(text); return; }
    if (connectionDetail_) connectionDetail_->close(); const auto row=selected.first().data(Qt::UserRole+3).value<QJsonObject>(); const auto profile=controller_->state().activeProfile; QString version; for (const auto& item:controller_->profiles()) if (item.id==profile) version=item.active;
    auto* dialog=new ConnectionDetailsDialog(controller_,row,profile,version,this); connectionDetail_=dialog;
    connect(dialog,&ConnectionDetailsDialog::browseGroup,this,[this,profile,version](const QString& group) { bool current=false; for (const auto& item:controller_->profiles()) if (item.id==profile) current=item.active==version; if (!current||controller_->state().activeProfile!=profile) return; const auto index=profilesCombo_->findData(profile); if (index<0) return; navigate(Route{ViewIds::Home,kNoCategory,HomeAnchor::Nodes}); if (route_.view!=ViewIds::Home || route_.anchor!=HomeAnchor::Nodes) return; profilesCombo_->setCurrentIndex(index); selectGroup(group); }); connect(dialog,&ConnectionDetailsDialog::createRule,this,&MainWindow::prefillConnectionRule);
    auto* drawer=qobject_cast<DetailsDrawer*>(detailsDrawer_.data()); if (!drawer) { drawer=new DetailsDrawer(findChild<QWidget*>("contentSurface")); detailsDrawer_=drawer; }
    drawer->clearContent(); drawer->setHeader("连接详情","打开时的观察快照");
    dialog->setParent(drawer,Qt::Widget); dialog->setMinimumWidth(0); dialog->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    for (auto* box:dialog->findChildren<QDialogButtonBox*>()) box->setOrientation(Qt::Vertical);
    connect(dialog,&QDialog::finished,drawer,[drawer] { drawer->closeDrawer(); }); drawer->addWidget(dialog); dialog->show(); drawer->open();
}
void MainWindow::prefillConnectionRule(const QJsonObject& suggestion) {
    const auto id=suggestion.value("profile").toString(); Profile profile; bool found=false; for (const auto& item:controller_->profiles()) if (item.id==id) { profile=item; found=true; }
    const auto refuse=[this](const QString& error) { message_->setText(error); message_->setToolTip(error); };
    if ((suggestion.contains("generation")&&static_cast<quint64>(suggestion.value("generation").toInteger())!=controller_->state().generation)||!found||!controller_->state().ready||controller_->state().activeProfile!=id||profile.active!=suggestion.value("active_version").toString()||controller_->state().busy||controller_->state().exiting||controller_->state().readOnly) { refuse("连接来源已变化或当前不可编辑；没有修改规则草稿。"); return; }
    const auto type=suggestion.value("type").toString(),payload=suggestion.value("payload").toString(),option=suggestion.value("option").toString(); auto target=suggestion.value("target").toString();
    if (!QStringList{"DOMAIN","IP-CIDR","IP-CIDR6","PROCESS-NAME"}.contains(type)||!RuleDraft::check({RuleDraft::compose({type,payload,"DIRECT",option})},{},{},false).ok) { refuse("该连接字段无法安全预填规则，请在规则页手动编辑。"); return; }
    if (target=="GLOBAL"||target.contains(',')||(!QStringList{"DIRECT","REJECT"}.contains(target)&&!controller_->proxies().contains(target))) target.clear();
    const auto text=rulesEditor_->toPlainText(); const auto expectedMain=rulesExpected_; auto expected=RuleDraft::snapshot(profile); QStringList prepend=profile.customRules; const bool shared=rulesDraftProfile_==id&&rulesDirty_;
    if (shared) { prepend.clear(); for (const auto& line:text.split('\n')) { const auto value=line.trimmed(); if (!value.isEmpty()&&!value.startsWith('#')) prepend.append(value); } if (!RuleDraft::check(prepend,{},{},false).ok) { refuse("当前规则草稿存在错误，已保留；请先修正后再带入连接规则。"); return; } expected=rulesExpected_; }
    const auto candidate=RuleDraft::compose({type,payload,target,option}); const bool exists=prepend.contains(candidate);
    if (!exists&&prepend.size()+profile.appendRules.size()>=256) { refuse("前置和追加已达256条，当前草稿保留；请先减少条目。"); return; } if (!exists) prepend.prepend(candidate);
    auto* dialog=new RuleOverlayDialog(controller_,id,this,{{"prepend",QJsonArray::fromStringList(prepend)},{"expected",expected},{"contextNote",QString("来自连接观察快照 · 采样 %1。%2请核对匹配范围和目标，点击保存前不会改变网络。").arg(suggestion.value("observed_at").toString("未知"),exists?QString("已定位已有同样条目；"):QString("新条目置于前置首行；"))}});
    if (auto* table=dialog->findChild<QTableWidget*>("prependRulesTable")) table->setCurrentCell(prepend.indexOf(candidate),2);
    connect(dialog,&QDialog::accepted,this,[this,id,text,expectedMain] { if (rulesDraftProfile_!=id) return; if (rulesEditor_->toPlainText()==text&&rulesExpected_==expectedMain) { rulesDirty_=false; loadRuleDraft(); } else if (rulesDirty_) rulesHint_->setText("规则列表已保存，当前窗口另有草稿，已保留；请核对后重新载入。"); });
    navigate(Route{ViewIds::Profiles});
    if (route_.view != ViewIds::Profiles) { dialog->deleteLater(); return; }
    auto* workspace=taskWorkspace();
    if (!workspace->configure("profile:"+id,profile.name,{},[this] { returnFromWorkspace(); })) { dialog->deleteLater(); return; }
    profileDetailId_=id; presentTask(dialog,"rules","规则");
}

void MainWindow::updateConnections() {
    QString selected; const auto rows = connectionsTable_->selectionModel()->selectedRows(); if (!rows.isEmpty()) selected = rows.first().data(Qt::UserRole).toString();
    const auto position = connectionsTable_->verticalScrollBar()->value();
    const bool closed=connectionView_->currentIndex()==1; connectionsFilter_.setDynamicSortFilter(!controller_->connectionsPaused()&&!controller_->connectionTotals().value("_paused").toBool()); connectionsModel_.setRows(closed?controller_->closedConnections():controller_->connections()); const auto error = controller_->connectionError();
    const bool paused=controller_->connectionsPaused();
    { const QSignalBlocker block(connectionPause_); connectionPause_->setChecked(paused); connectionPause_->setText(paused?"恢复刷新":"暂停刷新"); }
    { const QSignalBlocker block(connectionView_); const auto totals=controller_->connectionTotals(); const auto count=totals.contains("active_count")?QString::number(totals.value("active_count").toInt()):QString("—"); connectionView_->setTabText(0,(paused||totals.value("_paused").toBool()?QString("快照 "):QString("活动 "))+count); connectionView_->setTabText(1,QString("已结束 %1").arg(controller_->closedConnections().size())); }
    const auto totals=controller_->connectionTotals(); const auto amount=[&totals](const QString& key,bool rate=false) { return totals.value(key).isDouble()?ActivityTableModel::formatBytes(totals.value(key).toDouble())+(rate?"/s":""):QString("—"); };
    connectionStats_->setText(QString("↑ %1  ·  ↓ %2     上传 %3  ·  下载 %4").arg(amount("_uploadTotal_rate",true),amount("_downloadTotal_rate",true),amount("uploadTotal"),amount("downloadTotal"))); connectionStats_->setToolTip("累计量来自内核启动至今的计数；速率由连续可见快照差值计算，首次或计数回退为未知");
    int restorePosition = position;
    if (controller_->connectionsVisible() && connectionReturnScroll_ >= 0 && connectionsFilter_.rowCount() > 0) {
        selected = connectionReturnSelection_;
        // 模型重置会延后重算滚动范围；先同步完成表格布局再恢复书签。
        connectionsTable_->doItemsLayout();
        restorePosition = connectionReturnScroll_;
        connectionReturnScroll_ = -1;
        connectionReturnSelection_.clear();
    }
    for (int row = 0; !selected.isEmpty() && row < connectionsFilter_.rowCount(); ++row)
        if (connectionsFilter_.index(row,0).data(Qt::UserRole).toString() == selected) { connectionsTable_->selectRow(row); break; }
    connectionsTable_->verticalScrollBar()->setValue(restorePosition);
    updateActions();
    const auto measured=QDateTime::fromString(controller_->connectionSampleTime(),Qt::ISODateWithMs).toLocalTime();
    // 采样状态与时间分开表达：过期数据必须带「已过期」，不能用旧值冒充当前值。
    const auto sampling=paused?QString("已暂停")
        :totals.value("_paused").toBool()?QString("正在恢复…")
        :!error.isEmpty()?(controller_->connectionsStale()?QString("刷新失败 · 显示上次快照"):QString("采样失败"))
        :QString();
    connectionsHint_->setText((connectionSearchError_.isEmpty()?sampling:QString("搜索条件无效"))+(measured.isValid()?(sampling.isEmpty()&&connectionSearchError_.isEmpty()?QString("更新于 "):QString(" · "))+measured.toString("HH:mm:ss"):QString())+(error.isEmpty()?QString():" · "+error)+(connectionFeedback_.isEmpty()?QString():" · "+connectionFeedback_)); connectionsHint_->setToolTip(connectionsHint_->text()+(measured.isValid()?"\n上次成功采样："+measured.toString("yyyy-MM-dd HH:mm:ss t"):QString())+(connectionSearchError_.isEmpty()?QString():"\n"+connectionSearchError_)+"\n采样时间为本机成功接收时间；累计量来自内核。离开快照不代表TCP断开原因，暂停间隙不推断离开，隐藏后清空。");
}
void MainWindow::rememberConnectionPosition() {
    if (!connectionsTable_ || !controller_->connectionsVisible()) return;
    connectionReturnScroll_ = connectionsTable_->verticalScrollBar()->value();
    connectionReturnSelection_.clear();
    const auto rows = connectionsTable_->selectionModel()->selectedRows();
    if (!rows.isEmpty()) connectionReturnSelection_ = rows.first().data(Qt::UserRole).toString();
}
QString MainWindow::ruleDraftError() const {
    QStringList rules; QList<int> lines; int row=0; for (const auto& raw:rulesEditor_->toPlainText().split('\n')) { ++row; const auto value=raw.trimmed(); if (value.isEmpty()||value.startsWith('#')) continue; rules.append(value); lines.append(row); }
    if (rules.size()+rulesExpected_.value("append").toArray().size()>256) return "前置和追加规则合计最多256条";
    const auto result=RuleDraft::check(rules,ruleTargets_,ruleProviders_,ruleReferencesKnown_); if (result.ok) return {}; return QString("第 %1 行：%2。%3").arg(lines.value(result.data.value("row").toInt()-1,result.data.value("row").toInt())).arg(result.data.value("reason").toString(),result.data.value("suggestion").toString());
}
QString MainWindow::activeProfileName() const {
    const auto id = controller_->state().activeProfile;
    if (id.isEmpty()) return {};
    for (const auto& profile : controller_->profiles()) if (profile.id == id) return profile.name;
    return {};
}
}
