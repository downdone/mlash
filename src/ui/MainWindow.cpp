#include "ProviderPolicyDialog.h"
#include "ProfileChangesDialog.h"
#include "MainWindow.h"
#include "MainWindowParts.h"
#include "WrapLayout.h"
#include "DialogDraftGuard.h"
#include "ImportDialog.h"
#include "DownloadPolicyEditor.h"
#include "BackupDialog.h"
#include "LocalBackupDialog.h"
#include "RuleOverlayDialog.h"
#include "ConnectionDetailsDialog.h"
#include "DnsDialog.h"
#include "ProxyEditorDialog.h"
#include "ProxyChainDialog.h"
#include "ProtocolSettingsDialog.h"
#include "ProxyBypassDialog.h"
#include "CoreUpdateDialog.h"
#include "RemoteBackupDialog.h"
#include "TunDialog.h"
#include "GeoDataDialog.h"
#include "HomeMetricsView.h"
#include "OutboundDialog.h"
#include "HotkeysDialog.h"
#include "SettingsSearch.h"
#include "HomeSummaryDialog.h"
#include "ConnectionOptions.h"
#include "TrayController.h"
#include "NodePresentation.h"
#include "WidgetStyle.h"
#include "UiTheme.h"
#include "WebsiteTestsPage.h"
#include "CompactComboBox.h"
#include "UiIcons.h"
#include "profile/SubscriptionInfo.h"
#include "profile/RuleDraft.h"
#include "qrcodegen.hpp"
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
MainWindow::MainWindow(AppController* controller, QWidget* parent) : QMainWindow(parent), controller_(controller), model_(this), filter_(this) {
    setAcceptDrops(true);
    // Use the same bundled Widgets style for the window and its dialogs on Windows.
    WidgetStyle::install();
    UiTheme::setPreferences(controller_->settings().appearance,controller_->settings().textScale);
    setWindowTitle("Mlash"); setWindowIcon(TrayController::icon()); resize(940, 620); setMinimumSize(840, 560);

    auto* central = new QWidget(this); auto* layout = new QHBoxLayout(central);
    central->setObjectName("mainSurface"); central->setAttribute(Qt::WA_StyledBackground, true);
    layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto* sidebar = new QWidget; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(UiTheme::SidebarWidth); sidebar->setAttribute(Qt::WA_StyledBackground); auto* sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(12,20,12,16); sideLayout->setSpacing(8);
    auto* brandRow = new QHBoxLayout; auto* mark = new QLabel; mark->setPixmap(TrayController::icon().pixmap(28, 28)); brandRow->addWidget(mark); auto* brand = new QLabel("Mlash"); brand->setObjectName("brand"); brandRow->addWidget(brand, 1); sideLayout->addLayout(brandRow); sideLayout->addSpacing(16);
    navigation_ = new QListWidget; navigation_->setObjectName("navigation"); navigation_->addItems({"首页", "配置", "数据", "设置"}); navigation_->setIconSize(QSize(18, 18));
    const QList<int> pageOrder{0,1,4,6};
    for (int i=0;i<pageOrder.size();++i) { navigation_->item(i)->setData(Qt::UserRole,pageOrder[i]); navigation_->item(i)->setIcon(navigationIcon(pageOrder[i])); }
    sideLayout->addWidget(navigation_, 1); auto* version = new QLabel("v" MLASH_VERSION); version->setObjectName("secondaryText"); sideLayout->addWidget(version);
    pages_ = new QStackedWidget; pages_->addWidget(homePage()); pages_->addWidget(profilesPage()); pages_->addWidget(proxyPage()); pages_->addWidget(rulesPage()); pages_->addWidget(connectionsPage()); pages_->addWidget(diagnosticsPage()); pages_->addWidget(settingsPage()); websitePage_=new WebsiteTestsPage(controller_); pages_->addWidget(websitePage_); connect(websitePage_,&WebsiteTestsPage::pageRequested,this,&MainWindow::setPage); connect(websitePage_,&WebsiteTestsPage::diagnosticsRequested,this,&MainWindow::showOutboundDiagnostics);
    for (int page = 0; page < pages_->count(); ++page) { pages_->widget(page)->setObjectName("pageSurface"); pages_->widget(page)->setAttribute(Qt::WA_StyledBackground, true); pages_->widget(page)->setAutoFillBackground(true); }
    layout->addWidget(sidebar); auto* content = new QVBoxLayout; content->setContentsMargins(UiTheme::ContentInset,18,UiTheme::ContentInset,16); sectionTabs_=new QTabBar(central); sectionTabs_->hide(); sectionTabs_->setObjectName("sectionNavigation"); sectionTabs_->setAccessibleName("当前分区的功能"); sectionTabs_->setExpanding(false); sectionTabs_->setDrawBase(false); connect(sectionTabs_,&QTabBar::currentChanged,this,[this](int tab) { if (tab>=0) setPage(sectionTabs_->tabData(tab).toInt()); }); content->addWidget(pages_, 1); layout->addLayout(content, 1); setCentralWidget(central);
    operationStatus_=new QStatusBar(this); operationStatus_->setObjectName("operationStatusBar"); operationStatus_->setSizeGripEnabled(false); setStatusBar(operationStatus_);
    message_=new ElidedLabel("正在初始化"); message_->setObjectName("operationMessage"); message_->setAccessibleName("操作状态"); message_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding); operationStatus_->addWidget(message_,1);
    auto* details=new QToolButton; details->setObjectName("statusDetails"); details->setText("详情"); details->setToolTip("查看及复制完整状态信息"); operationStatus_->addPermanentWidget(details);
    connect(details,&QToolButton::clicked,this,[this] { const auto text=message_->toolTip().isEmpty()?message_->text():message_->toolTip(); QMessageBox dialog(QMessageBox::Information,"操作信息 · Mlash",text,QMessageBox::Ok,this); dialog.setObjectName("statusDetailsDialog"); dialog.setTextFormat(Qt::PlainText); dialog.setTextInteractionFlags(Qt::TextSelectableByMouse); dialog.button(QMessageBox::Ok)->setText("关闭"); auto* copy=dialog.addButton("复制消息",QMessageBox::ActionRole); connect(copy,&QPushButton::clicked,&dialog,[text] { QApplication::clipboard()->setText(text); }); dialog.exec(); });
    auto* resolve=new QToolButton; resolve->setObjectName("statusResolve"); resolve->setText("处理"); resolve->setPopupMode(QToolButton::InstantPopup); auto* resolveMenu=new QMenu(resolve); resolve->setMenu(resolveMenu); resolveMenu->addAction("查看日志",this,[this] { setPage(5); }); resolveMenu->addAction("订阅管理",this,[this] { setPage(1); }); resolveMenu->addAction("TUN 与服务…",this,[this] { (new TunDialog(controller_,this))->show(); }); resolveMenu->addAction("刷新运行状态",controller_,&AppController::refresh); operationStatus_->addPermanentWidget(resolve); resolve->hide();
    connect(&refreshTimer_, &QTimer::timeout, this, [this] { if (!controller_->state().busy) controller_->refresh(); });
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int index) {
        if (index >= 0) { const auto* item=navigation_->item(index); setPage(item->data(Qt::UserRole+1).isValid()?item->data(Qt::UserRole+1).toInt():item->data(Qt::UserRole).toInt()); }
    });
    navigation_->setCurrentRow(0);
    connect(controller_, &AppController::stateChanged, this, &MainWindow::updateState);
    connect(controller_,&AppController::stateChanged,this,[this] { UiTheme::setPreferences(controller_->settings().appearance,controller_->settings().textScale); });
    connect(controller_, &AppController::profilesChanged, this, &MainWindow::updateProfiles);
    connect(controller_, &AppController::profilesChanged, this, &MainWindow::updateRulesProfiles);
    connect(controller_, &AppController::connectionsChanged, this, &MainWindow::updateConnections);
    connect(controller_->homeMetrics(), &HomeMetrics::changed, this, &MainWindow::updateHome);
    connect(controller_, &AppController::rulesChanged, this, [this] { rulesModel_.setRows(controller_->activeRules()); updateActions(); });
    connect(controller_, &AppController::proxiesChanged, this, &MainWindow::updateProxies);
    connect(controller_, &AppController::nodeDelay, this, [this](const QString& name, int delay, const QString& error) { if (controller_->lastNodeTestUrl()==controller_->groupTestUrl(selectedGroup_)) model_.setDelay(name, delay, error); updateCurrentMember(); updateHome(); });
    feedbackTimer_.setSingleShot(true); feedbackTimer_.setInterval(3000);
    connect(&feedbackTimer_,&QTimer::timeout,this,[this] { recentFeedback_=false; if (!settingsFeedback_.startsWith("应用失败")) settingsFeedback_.clear(); updateState(); });
    connect(controller_, &AppController::operationStarted, this, [this](const QString& text) { recentFeedback_=false; feedbackError_.clear(); feedbackTimer_.stop(); message_->setText(text); message_->setToolTip(text); findChild<QToolButton*>("statusResolve")->hide(); findChild<QToolButton*>("statusDetails")->show(); });
    connect(controller_, &AppController::operationFinished, this, [this](bool ok, const QString& text) {
        message_->setText(text); message_->setToolTip(text);
        if (!ok) message_->setText("操作未完成 · " + text);
        findChild<QToolButton*>("statusResolve")->setVisible(!ok);
        feedbackTimer_.stop(); recentFeedback_=ok; feedbackError_=ok?QString():text; if (ok) feedbackTimer_.start();
        if (ok && controller_->state().readOnly) message_->setText(controller_->state().lastError);
        testProgress_->setText(text=="测速已取消" ? "已取消" : QString()); updateActions();
        if (settingsApplying_) { settingsApplying_=false; settingsFeedback_=ok?"网络设置已应用":"应用失败："+text; }
        if (rulesSaving_) { rulesSaving_ = false; if (ok) { rulesDirty_ = false; loadRuleDraft(); if (pages_->currentIndex() == 3) controller_->refreshRules(); rulesHint_->clear(); } else rulesHint_->setText("保存失败，修改已保留 · "+text); }
        updateActions();
    });
    connect(controller_, &AppController::delayProgress, this, [this](int done, int total) { testProgress_->setText(QString("%1 / %2").arg(done).arg(total)); message_->setText(QString("测延迟 %1 / %2").arg(done).arg(total)); message_->setToolTip(message_->text()); updateActions(); });
    connect(controller_->diagnostics(), &Diagnostics::lineAdded, this, [this](const QString& line) {
        if (!logsPaused_ && isVisible() && pages_->currentIndex() == 5 && logMatches(line)) appendLogLine(line);
        if (isVisible() && pages_->currentIndex()==0) updateHome();
    });
    connect(controller_->diagnostics(), &Diagnostics::viewCleared, this, [this] { pausedLogs_.clear(); displayedLogLines_.clear(); logView_->clear(); });
    applyAppearance();
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,&MainWindow::applyAppearance);
    updateProfiles(); updateRulesProfiles(); updateProxies(); updateState();
}
void MainWindow::applyAppearance() {
    const int statusHeight=qMax(34,QFontMetrics(qApp->font()).height()+14); operationStatus_->setFixedHeight(statusHeight); if (auto* details=findChild<QToolButton*>("statusDetails")) details->setFixedHeight(statusHeight-8);
    for (int row=0;row<navigation_->count();++row) navigation_->item(row)->setIcon(navigationIcon(navigation_->item(row)->data(Qt::UserRole).toInt()));
    for (auto* card:findChildren<QWidget*>("dashboardCard")) if (auto* mark=card->findChild<QLabel*>("dashboardIcon")) mark->setPixmap(navigationIcon(card->property("page").toInt()).pixmap(18,18,QIcon::Normal));
    using G=UiIcons::Glyph;
    for (const auto& pair:QList<QPair<QString,G>>{{"addSubscription",G::Plus},{"updateProfile",G::Refresh},{"clearLogViewButton",G::Clear},{"followLatestLogs",G::Refresh},{"saveCustomRules",G::Save}}) if (auto* control=findChild<QAbstractButton*>(pair.first)) control->setIcon(UiIcons::icon(pair.second,control->property("primary").toBool()?UiTheme::OnPrimary:UiTheme::Secondary));
    for (const auto& name:{"nodeSearch","logSearch","connectionsSearch","activeRulesSearch"}) if (auto* field=findChild<QLineEdit*>(name)) {
        if (!field->property("searchGlyph").toBool()) { field->addAction(UiIcons::icon(G::Search),QLineEdit::LeadingPosition)->setObjectName("searchGlyph"); field->setProperty("searchGlyph",true); }
        else if (auto* action=field->findChild<QAction*>("searchGlyph")) action->setIcon(UiIcons::icon(G::Search));
    }
    for (auto* view:QList<QAbstractItemView*>{nodes_,nodeCards_,proxyGroups_,profileTable_,connectionsTable_}) view->viewport()->update();
    updateState();
}
MainWindow::~MainWindow() { NodePresentation::clearTransientCache(); if (connectionOptions_) connectionOptions_->flushPreferences(); controller_->setHomeVisible(false); controller_->setWebsiteTestsVisible(false); controller_->clearWebsiteTests(); controller_->setRulesVisible(false); controller_->setConnectionsVisible(false); controller_->setDiagnosticsVisible(false); }
QWidget* MainWindow::diagnosticsPage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
    auto* logHeading=new QHBoxLayout; auto* logTitle=new QLabel("日志"); logTitle->setObjectName("pageTitle"); logHeading->addWidget(logTitle); logHeading->addStretch(); layout->addLayout(logHeading); auto* toolbar=new QWidget; toolbar->setObjectName("logToolbar"); auto* actions = new WrapLayout(toolbar);
    auto* logPanel = panel(); auto* logLayout = new QVBoxLayout(logPanel); logLayout->setContentsMargins(12, 10, 12, 10); logLayout->setSpacing(8); layout->addWidget(logPanel, 1);
    logSearch_ = new QLineEdit; logSearch_->setObjectName("logSearch"); logSearch_->setPlaceholderText("搜索日志…"); logSearch_->setAccessibleName("搜索日志"); logSearch_->setClearButtonEnabled(true); actions->addWidget(logSearch_);
    logFilter_ = new CompactComboBox; logFilter_->setObjectName("logLevelFilter"); for (const auto& pair : QList<QPair<QString,QString>>{{"全部","all"},{"调试","debug"},{"信息","info"},{"警告","warning"},{"错误","error"},{"应用","app"}}) logFilter_->addItem(pair.first,pair.second); actions->addWidget(logFilter_);
    auto* pause = new QPushButton("暂停显示"); actions->addWidget(pause); pause->setObjectName("pauseLogs"); pause->setCheckable(true); auto* copy = new QPushButton("复制选中"); actions->addWidget(copy);
    auto* clear = new QAction("清空视图", this); clear->setObjectName("clearLogView"); auto* clearButton=new QToolButton; clearButton->setObjectName("clearLogViewButton"); clearButton->setToolButtonStyle(Qt::ToolButtonTextOnly); clearButton->setDefaultAction(clear); actions->addWidget(clearButton);
    auto* more = new MenuPushButton("日志选项"); actions->addWidget(more); auto* menu = new QMenu(more); more->setMenu(menu); auto* directory = menu->addAction("日志目录"); auto* report = menu->addAction("导出脱敏诊断…"); report->setObjectName("exportDiagnostics"); auto* exportButton=new QToolButton; exportButton->setObjectName("exportDiagnosticsButton"); exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly); exportButton->setDefaultAction(report); exportButton->setText("导出诊断…"); logHeading->addWidget(exportButton); menu->removeAction(report); connect(menu, &QMenu::aboutToShow, this, [this, report] { report->setEnabled(!controller_->state().busy && !controller_->state().exiting); }); logLayout->addWidget(toolbar);
    logView_ = new QPlainTextEdit; logView_->setObjectName("logView"); logView_->setReadOnly(true); logView_->setLineWrapMode(QPlainTextEdit::NoWrap); logView_->setMaximumBlockCount(2000); logLayout->addWidget(logView_, 1);
    auto* wrap=menu->addAction("自动换行"); wrap->setObjectName("wrapLogs"); wrap->setCheckable(true); connect(wrap,&QAction::toggled,logView_,[this](bool enabled) { const auto cursor=logView_->textCursor(); logView_->setLineWrapMode(enabled?QPlainTextEdit::WidgetWidth:QPlainTextEdit::NoWrap); logView_->setTextCursor(cursor); });
    logView_->setToolTip("仅日志页可见时接收内核日志；最多保留 2000 行。"); followLogs_=new QPushButton("回到最新"); actions->addWidget(followLogs_); followLogs_->setObjectName("followLatestLogs"); followLogs_->setProperty("quiet",true); followLogs_->setToolTip("滚动到最新记录");
    connect(followLogs_,&QPushButton::clicked,this,[this] { auto cursor=logView_->textCursor(); cursor.clearSelection(); logView_->setTextCursor(cursor); logView_->verticalScrollBar()->setValue(logView_->verticalScrollBar()->maximum()); });
    connect(logView_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value) { followLogs_->setEnabled(value<logView_->verticalScrollBar()->maximum()); });
    copy->setEnabled(false); connect(logView_,&QPlainTextEdit::copyAvailable,copy,&QPushButton::setEnabled);
    connect(logSearch_, &QLineEdit::textChanged, this, &MainWindow::updateLogs);
    controller_->setDiagnosticsLevel("all");
    connect(logFilter_, &QComboBox::currentIndexChanged, this, [this] { controller_->setDiagnosticsLevel(logFilter_->currentData().toString()); updateLogs(); });
    connect(pause, &QPushButton::toggled, this, [this, pause](bool paused) { logsPaused_ = paused; pause->setText(paused ? "恢复显示" : "暂停显示"); pausedLogs_ = paused ? controller_->diagnostics()->lines() : QStringList{}; controller_->setDiagnosticsVisible(!paused && isVisible() && this->page() == 5); updateLogs(); });
    connect(copy, &QPushButton::clicked, this, [this] { auto text = logView_->textCursor().selectedText(); text.replace(QChar::ParagraphSeparator,'\n'); QApplication::clipboard()->setText(text); });
    connect(clear, &QAction::triggered, controller_, &AppController::clearLogView); connect(directory, &QAction::triggered, this, [this] { openDirectory(controller_->dataRoot() + "/logs"); });
    connect(report, &QAction::triggered, this, [this] { const auto path = QFileDialog::getSaveFileName(this, "导出脱敏诊断（不含订阅配置/连接快照）", QDir::homePath()+"/Mlash-diagnostics.json", "JSON (*.json)"); if (!path.isEmpty()) controller_->exportDiagnostics(path); });
    return page;
}
QWidget* MainWindow::rulesPage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
    heading(layout, "规则", {});
    auto* box = panel(); auto* content = new QVBoxLayout(box); content->setContentsMargins(12, 10, 12, 10); layout->addWidget(box, 1);
    auto* row = new QHBoxLayout; auto* profileLabel=new QLabel("订阅"); profileLabel->setObjectName("rulesProfileLabel"); row->addWidget(profileLabel); auto* actualProfile=new QLabel; actualProfile->setObjectName("rulesRuntimeContext"); actualProfile->setTextFormat(Qt::PlainText); actualProfile->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); row->addWidget(actualProfile,1); actualProfile->hide(); auto* ruleNext=new QPushButton("前往订阅"); ruleNext->setObjectName("rulesNextStep"); row->addWidget(ruleNext); connect(ruleNext,&QPushButton::clicked,this,[this] { setPage(rulesTabs_->currentIndex()==1?0:1); }); rulesProfiles_ = new CompactComboBox; rulesProfiles_->setObjectName("rulesProfiles"); row->addWidget(rulesProfiles_, 1); content->addLayout(row);
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
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) { controller_->setRulesVisible(isVisible() && this->page() == 3 && index == 1); if (index == 1) controller_->refreshRules(); updateActions(); });
    rulesHint_ = new ElidedLabel; rulesHint_->setObjectName("secondaryText"); rulesHint_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    auto* advanced = new QPushButton("完整规则编辑…"); advanced->setToolTip("推荐：按类型、匹配条件和目标分列编辑；也可查看订阅来源规则。下方文本保留高级编辑能力。"); advanced->setObjectName("advancedRules"); row->insertWidget(row->indexOf(providers),advanced); connect(advanced,&QPushButton::clicked,this,[this] { if (controller_->state().busy || rulesDraftProfile_.isEmpty()) return; QStringList draft; for (const auto& raw:rulesEditor_->toPlainText().split('\n')) { const auto line=raw.trimmed(); if (line.isEmpty()||line.startsWith('#')) continue; if (line.size()>1024||line.split(',').size()>(line.section(',',0,0)=="MATCH"?3:4)||draft.size()>=256) { rulesHint_->setText("请先修正超长、额外字段或超过256条的文本，原草稿保留"); return; } draft.append(line); } auto* dialog=new RuleOverlayDialog(controller_,rulesDraftProfile_,this,{{"prepend",QJsonArray::fromStringList(draft)},{"expected",rulesExpected_}}); connect(dialog,&QDialog::accepted,this,[this] { rulesDirty_=false; loadRuleDraft(); }); dialog->open(); }); connect(tabs,&QTabWidget::currentChanged,advanced,[advanced](int index) { advanced->setVisible(index==0); });
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
}
void MainWindow::updateRulesProfiles() {
    const auto selected = rulesProfiles_->currentData().toString(); const QSignalBlocker block(rulesProfiles_); rulesProfiles_->clear();
    for (const auto& p : controller_->profiles()) rulesProfiles_->addItem(p.name + (p.id == controller_->state().activeProfile ? " · 当前" : ""), p.id);
    auto index = rulesProfiles_->findData(selected); if (index < 0) index = rulesProfiles_->findData(controller_->state().activeProfile); rulesProfiles_->setCurrentIndex(index < 0 && rulesProfiles_->count() ? 0 : index);
    if (rulesDirty_ && rulesProfiles_->findData(rulesDraftProfile_) >= 0) rulesProfiles_->setCurrentIndex(rulesProfiles_->findData(rulesDraftProfile_)); else { rulesDirty_ = false; loadRuleDraft(); }
}
void MainWindow::loadRuleDraft() {
    const QSignalBlocker block(rulesEditor_); rulesDraftProfile_ = rulesProfiles_->currentData().toString(); QStringList rules;
    for (const auto& p : controller_->profiles()) if (p.id == rulesDraftProfile_) { rules=p.customRules; rulesExpected_=RuleDraft::snapshot(p); }
    ruleReferencesKnown_=false; ruleTargets_.clear(); ruleProviders_.clear(); const auto reference=++rulesReferenceGeneration_;
    rulesEditor_->setPlainText(rules.join('\n')); rebuildRuleList(); rulesDirty_ = false; rulesHint_->clear();
    const auto target = ruleTargetValue(ruleTarget_); ruleTarget_->clear(); ruleTarget_->addItem("直连 · DIRECT", "DIRECT"); ruleTarget_->addItem("阻断 · REJECT", "REJECT");
    if (rulesDraftProfile_ == controller_->state().activeProfile) for (auto it = controller_->proxies().begin(); it != controller_->proxies().end(); ++it) if (it.value().toObject().value("all").isArray()) ruleTarget_->addItem(it.key(), it.key());
    const auto index = ruleTarget_->findData(target); if (index >= 0) ruleTarget_->setCurrentIndex(index); else if (!target.isEmpty()) ruleTarget_->setEditText(target); updateActions();
    if (isVisible()&&page()==3&&!rulesDraftProfile_.isEmpty()) controller_->inspectRuleSource(rulesDraftProfile_,[guard=QPointer<MainWindow>(this),id=rulesDraftProfile_,reference](Result result) { if (!guard||guard->rulesDraftProfile_!=id||guard->rulesReferenceGeneration_!=reference||!result.ok) return; guard->ruleTargets_.clear(); guard->ruleProviders_.clear(); const auto current=ruleTargetValue(guard->ruleTarget_); const QSignalBlocker block(guard->ruleTarget_); guard->ruleTarget_->clear(); for (const auto& value:result.data.value("targets").toArray()) { const auto code=value.toString(); guard->ruleTargets_.insert(code); guard->ruleTarget_->addItem(code=="DIRECT"?QString("直连 · DIRECT"):code=="REJECT"?QString("阻断 · REJECT"):code,code); } for (const auto& value:result.data.value("rule_providers").toArray()) guard->ruleProviders_.insert(value.toString()); const auto index=guard->ruleTarget_->findData(current); if (index>=0) guard->ruleTarget_->setCurrentIndex(index); else guard->ruleTarget_->setEditText(current); guard->ruleReferencesKnown_=true; if (guard->rulesDirty_) guard->rulesHint_->setText(guard->ruleDraftError().isEmpty()?QString("有未保存修改"):guard->ruleDraftError()); guard->updateActions(); });
}
QWidget* MainWindow::connectionsPage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10); heading(layout, "连接", {});
    auto* box = panel(); auto* content = new QVBoxLayout(box); content->setContentsMargins(12, 10, 12, 10); layout->addWidget(box, 1);
    auto* summary=new QHBoxLayout; connectionStats_=new QLabel("流量 —"); connectionStats_->setObjectName("connectionStats"); connectionStats_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); summary->addWidget(connectionStats_,1);
    auto* next=new QPushButton("前往首页启动"); next->setObjectName("connectionNextStep"); summary->addWidget(next); connect(next,&QPushButton::clicked,this,[this] { setPage(0); });
    connectionPause_=new QPushButton("暂停刷新"); connectionPause_->setObjectName("pauseConnections"); connectionPause_->setCheckable(true); connectionPause_->setToolTip("暂停采样并保留阅读快照；恢复后先建立新基线"); summary->addWidget(connectionPause_); connect(connectionPause_,&QPushButton::clicked,controller_,&AppController::setConnectionsPaused);
    auto* details=new QAction("详情",this); connectionDetailsAction_=details; details->setObjectName("connectionDetailsAction"); auto* copy=new QAction("复制详情",this); connectionCopyAction_=copy; auto* order=new QAction("恢复默认顺序",this); order->setObjectName("defaultConnectionOrder"); order->setToolTip("恢复内核返回的顺序");
    closeAllConnections_=new QAction("断开全部…",this); closeAllConnections_->setObjectName("closeAllConnections"); closeAllConnections_->setToolTip("关闭内核全部活动连接，需要确认"); auto* closeAll=actionButton(closeAllConnections_,summary,"closeAllConnectionsButton"); closeAll->setProperty("danger",true); content->addLayout(summary);
    connect(details,&QAction::triggered,this,[this] { connectionDetails(); }); connect(copy,&QAction::triggered,this,[this] { connectionDetails(true); }); connect(order,&QAction::triggered,this,[this] { connectionsTable_->sortByColumn(-1,Qt::AscendingOrder); });
    connect(closeAllConnections_,&QAction::triggered,this,[this] { if (QMessageBox::question(this,"关闭全部活动连接","将关闭内核当前的全部活动连接，正在进行的请求可能中断。继续？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes) controller_->closeAllConnections(); });
    auto* tabsRow=new QHBoxLayout; connectionView_=new QTabBar; connectionView_->setObjectName("connectionView"); connectionView_->setExpanding(false); connectionView_->setDrawBase(false); connectionView_->addTab("活动连接"); connectionView_->addTab("已结束"); connectionView_->setToolTip("离开快照仅表示本页观察到条目消失；最多保留 1000 条，隐藏后清空"); tabsRow->addWidget(connectionView_); tabsRow->addStretch(); content->addLayout(tabsRow); auto* searchToolbar=new QWidget; searchToolbar->setObjectName("connectionToolbar"); auto* tools=new WrapLayout(searchToolbar);
    auto* search = new QLineEdit; search->setObjectName("connectionsSearch"); search->setPlaceholderText("搜索目标、规则、链路或进程…"); search->setClearButtonEnabled(true); tools->addWidget(search); content->addWidget(searchToolbar);
    connectionsFilter_.setSourceModel(&connectionsModel_); connectionsFilter_.setFilterKeyColumn(0); connectionsFilter_.setFilterRole(Qt::UserRole+2); connectionsFilter_.setSortRole(Qt::UserRole+1); connectionsFilter_.setFilterCaseSensitivity(Qt::CaseInsensitive); connect(connectionView_,&QTabBar::currentChanged,this,&MainWindow::updateConnections);
    connectionsTable_ = new QTableView; connectionsTable_->setObjectName("connectionsTable"); connectionsTable_->setModel(&connectionsFilter_); connectionsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers); connectionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows); connectionsTable_->setSelectionMode(QAbstractItemView::SingleSelection); connectionsTable_->verticalHeader()->hide(); styleTable(connectionsTable_); connectionsTable_->setSortingEnabled(true); connectionsTable_->sortByColumn(-1,Qt::AscendingOrder); content->addWidget(connectionsTable_, 1);
    connectionOptions_=new ConnectionOptions(controller_,connectionsTable_,&connectionsFilter_,search,nullptr); tools->addWidget(connectionOptions_); connectionOptions_->addViewAction(order); connect(connectionsTable_->horizontalHeader(),&QHeaderView::sortIndicatorChanged,order,[this,order] { order->setEnabled(connectionsFilter_.sortColumn()>=0); }); order->setEnabled(false);
    connectionEmpty_=emptyState(connectionsTable_,"暂无活动连接");
    auto* bottom = new QHBoxLayout; connectionsHint_ = new ElidedLabel("内核未就绪"); connectionsHint_->setObjectName("connectionSamplingStatus"); connectionsHint_->setProperty("secondary",true); connectionsHint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); bottom->addWidget(connectionsHint_, 1); connectionDetailsButton_=actionButton(details,bottom,"connectionDetailsButton"); connectionCopyButton_=actionButton(copy,bottom,"connectionCopyButton"); closeConnection_ = button("断开选中", bottom); closeConnection_->setObjectName("closeSelectedConnection"); closeConnection_->setProperty("danger",true); closeConnection_->setEnabled(false); content->addLayout(bottom);
    connect(connectionsTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateActions);
    connect(closeConnection_, &QPushButton::clicked, this, [this] { const auto selected = connectionsTable_->selectionModel()->selectedRows(); if (!selected.isEmpty()) controller_->closeConnection(selected.first().data(Qt::UserRole).toString()); }); connect(connectionsTable_,&QTableView::doubleClicked,this,[this] { connectionDetails(); });
    connectionsTable_->setContextMenuPolicy(Qt::CustomContextMenu); connect(connectionsTable_,&QWidget::customContextMenuRequested,this,[this,details,copy](const QPoint& point) { const auto index=connectionsTable_->indexAt(point); if (!index.isValid()) return; connectionsTable_->selectRow(index.row()); QMenu menu(this); menu.addAction(details); menu.addAction(copy); auto* close=menu.addAction("断开选中"); close->setEnabled(closeConnection_->isEnabled()); connect(close,&QAction::triggered,closeConnection_,&QPushButton::click); menu.exec(connectionsTable_->viewport()->mapToGlobal(point)); });
    connect(connectionOptions_,&ConnectionOptions::searchFeedback,this,[this](const QString& error) { connectionSearchError_=error; updateConnections(); }); connect(search,&QLineEdit::textChanged,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::rowsRemoved,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::rowsInserted,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::modelReset,this,&MainWindow::updateActions); connect(connectionsTable_->horizontalHeader(),&QHeaderView::sortIndicatorChanged,this,[this](int,Qt::SortOrder) { updateActions(); }); return page;
}
void MainWindow::connectionDetails(bool copy) {
    const auto selected=connectionsTable_->selectionModel()->selectedRows(); if (selected.isEmpty()) return; const auto text=selected.first().data(Qt::ToolTipRole).toString();
    if (copy) { QApplication::clipboard()->setText(text); return; }
    if (connectionDetail_) connectionDetail_->close(); const auto row=selected.first().data(Qt::UserRole+3).value<QJsonObject>(); const auto profile=controller_->state().activeProfile; QString version; for (const auto& item:controller_->profiles()) if (item.id==profile) version=item.active;
    auto* dialog=new ConnectionDetailsDialog(controller_,row,profile,version,this); connectionDetail_=dialog;
    connect(dialog,&ConnectionDetailsDialog::browseGroup,this,[this,profile,version](const QString& group) { bool current=false; for (const auto& item:controller_->profiles()) if (item.id==profile) current=item.active==version; if (!current||controller_->state().activeProfile!=profile) return; const auto index=profilesCombo_->findData(profile); if (index<0) return; profilesCombo_->setCurrentIndex(index); setPage(2); selectGroup(group); }); connect(dialog,&ConnectionDetailsDialog::createRule,this,&MainWindow::prefillConnectionRule); dialog->show();
}
void MainWindow::prefillConnectionRule(const QJsonObject& suggestion) {
    const auto id=suggestion.value("profile").toString(); Profile profile; bool found=false; for (const auto& item:controller_->profiles()) if (item.id==id) { profile=item; found=true; }
    const auto refuse=[this](const QString& error) { message_->setText(error); message_->setToolTip(error); };
    if (!found||!controller_->state().ready||controller_->state().activeProfile!=id||profile.active!=suggestion.value("active_version").toString()||controller_->state().busy||controller_->state().exiting||controller_->state().readOnly) { refuse("连接来源已变化或当前不可编辑；没有修改规则草稿。"); return; }
    const auto type=suggestion.value("type").toString(),payload=suggestion.value("payload").toString(),option=suggestion.value("option").toString(); auto target=suggestion.value("target").toString();
    if (!QStringList{"DOMAIN","IP-CIDR","IP-CIDR6","PROCESS-NAME"}.contains(type)||!RuleDraft::check({RuleDraft::compose({type,payload,"DIRECT",option})},{},{},false).ok) { refuse("该连接字段无法安全预填规则，请在规则页手动编辑。"); return; }
    if (target=="GLOBAL"||target.contains(',')||(!QStringList{"DIRECT","REJECT"}.contains(target)&&!controller_->proxies().contains(target))) target.clear();
    const auto text=rulesEditor_->toPlainText(); const auto expectedMain=rulesExpected_; auto expected=RuleDraft::snapshot(profile); QStringList prepend=profile.customRules; const bool shared=rulesDraftProfile_==id&&rulesDirty_;
    if (shared) { prepend.clear(); for (const auto& line:text.split('\n')) { const auto value=line.trimmed(); if (!value.isEmpty()&&!value.startsWith('#')) prepend.append(value); } if (!RuleDraft::check(prepend,{},{},false).ok) { refuse("当前规则草稿存在错误，已保留；请先修正后再带入连接规则。"); return; } expected=rulesExpected_; }
    const auto candidate=RuleDraft::compose({type,payload,target,option}); const bool exists=prepend.contains(candidate);
    if (!exists&&prepend.size()+profile.appendRules.size()>=256) { refuse("前置和追加已达256条，当前草稿保留；请先减少条目。"); return; } if (!exists) prepend.prepend(candidate);
    auto* dialog=new RuleOverlayDialog(controller_,id,this,{{"prepend",QJsonArray::fromStringList(prepend)},{"expected",expected},{"contextNote",QString("来自连接观察快照 · 采样 %1。%2请核对匹配范围和目标，点击保存前不会改变网络。").arg(suggestion.value("observed_at").toString("未知"),exists?QString("已定位已有同样条目；"):QString("新条目置于前置首行；"))}});
    if (auto* table=dialog->findChild<QTableWidget*>("prependRulesTable")) table->setCurrentCell(prepend.indexOf(candidate),2);
    connect(dialog,&QDialog::accepted,this,[this,id,text,expectedMain] { if (rulesDraftProfile_!=id) return; if (rulesEditor_->toPlainText()==text&&rulesExpected_==expectedMain) { rulesDirty_=false; loadRuleDraft(); } else if (rulesDirty_) rulesHint_->setText("规则列表已保存，当前窗口另有草稿，已保留；请核对后重新载入。"); }); dialog->open();
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
    for (int row = 0; !selected.isEmpty() && row < connectionsFilter_.rowCount(); ++row) if (connectionsFilter_.index(row,0).data(Qt::UserRole).toString() == selected) { connectionsTable_->selectRow(row); break; }
    connectionsTable_->verticalScrollBar()->setValue(position); updateActions();
    const auto measured=QDateTime::fromString(controller_->connectionSampleTime(),Qt::ISODateWithMs).toLocalTime();
    const auto sampling=paused?QString("已暂停"):totals.value("_paused").toBool()?QString("正在恢复…"):!error.isEmpty()?QString("采样失败"):QString();
    connectionsHint_->setText((connectionSearchError_.isEmpty()?sampling:QString("搜索条件无效"))+(measured.isValid()?(sampling.isEmpty()&&connectionSearchError_.isEmpty()?QString("更新于 "):QString(" · "))+measured.toString("HH:mm:ss"):QString())+(error.isEmpty()?QString():" · "+error)); connectionsHint_->setToolTip(connectionsHint_->text()+(measured.isValid()?"\n上次成功采样："+measured.toString("yyyy-MM-dd HH:mm:ss t"):QString())+(connectionSearchError_.isEmpty()?QString():"\n"+connectionSearchError_)+"\n采样时间为本机成功接收时间；累计量来自内核。离开快照不代表TCP断开原因，暂停间隙不推断离开，隐藏后清空。");
}
QString MainWindow::ruleDraftError() const {
    QStringList rules; QList<int> lines; int row=0; for (const auto& raw:rulesEditor_->toPlainText().split('\n')) { ++row; const auto value=raw.trimmed(); if (value.isEmpty()||value.startsWith('#')) continue; rules.append(value); lines.append(row); }
    if (rules.size()+rulesExpected_.value("append").toArray().size()>256) return "前置和追加规则合计最多256条";
    const auto result=RuleDraft::check(rules,ruleTargets_,ruleProviders_,ruleReferencesKnown_); if (result.ok) return {}; return QString("第 %1 行：%2。%3").arg(lines.value(result.data.value("row").toInt()-1,result.data.value("row").toInt())).arg(result.data.value("reason").toString(),result.data.value("suggestion").toString());
}
void MainWindow::updateState() {
    const auto& state = controller_->state();
    if (state.core == CoreState::Starting) model_.clearDelays();
    coreLabel_->setText("内核：" + coreStateText(state.core) + (state.ready ? " · mihomo " + state.coreVersion : QString())); proxyLabel_->setText("系统代理：" + (state.proxy==ProxyState::External?QString("由其他程序管理"):proxyStateText(state.proxy))+" · TUN："+(state.tunObserved?QString("已开启"):controller_->settings().tunEnabled?QString("未确认"):QString("未开启")));
    QString title = state.ready ? "内核运行中" : state.core==CoreState::Running ? "正在确认内核" : "内核已停止";
    if (state.core == CoreState::Starting) title = "正在启动内核…";
    if (state.core == CoreState::Failed) title = "内核启动或运行失败";
    if (state.core==CoreState::Stopping) title="正在停止内核…";
    overviewTitle_->setText(title);
    const auto dotColor = state.core == CoreState::Failed ? UiTheme::Error : state.core == CoreState::Starting || state.proxy == ProxyState::Unknown ? UiTheme::Warning : state.ready && (state.proxy == ProxyState::Owned||state.tunObserved) ? UiTheme::Success : state.ready ? UiTheme::Brand : UiTheme::Secondary;
    statusDot_->setStyleSheet(QString("color: %1; font-size: 18px;").arg(dotColor)); statusDot_->setToolTip(title);
    for (auto* widget : mutations_) widget->setEnabled(!state.busy && !state.exiting && !state.readOnly);
    nodes_->setEnabled(!state.exiting); nodeCards_->setEnabled(!state.exiting);
    proxyEnabled_->setEnabled(!state.busy && (state.ready || state.proxy == ProxyState::Owned));
    const QSignalBlocker block(proxyEnabled_); proxyEnabled_->setChecked(state.proxy == ProxyState::Owned);
    { const QSignalBlocker tunBlock(tunEnabled_); tunEnabled_->setChecked(state.tunObserved); }
    tunEnabled_->setEnabled(!state.busy&&!state.exiting&&!state.readOnly&&!controller_->state().activeProfile.isEmpty());
    tunEnabled_->setToolTip(state.tunObserved?"TUN 接口与路由已确认；点击关闭":"首次使用请在设置中安装 TUN 权限服务");
    proxyEnabled_->setToolTip("只影响遵循系统代理设置的应用；内核就绪后可开启。外部代理/PAC 需要确认接管。");
    autoStart_->setToolTip(!state.autoStartKnown ? "系统自启状态无法确认；应用设置时重新读取" : state.autoStartConflict ? "系统自启项指向其他路径，修改会被拒绝" : state.autoStartEnabled ? "系统当前已启用 Mlash 自启；勾选修改后点击应用" : "系统当前未启用 Mlash 自启；勾选修改后点击应用");
    for (auto* control : proxyModes_) { const QSignalBlocker block(control); control->setChecked(control->property("mode").toString() == state.mode); control->setEnabled(state.ready && !state.busy && !state.exiting && !state.readOnly); }
    if (!state.lastError.isEmpty()) { message_->setText(state.lastError); message_->setToolTip(state.lastError); }
    else if (!feedbackError_.isEmpty()) { message_->setText("操作未完成 · "+feedbackError_); message_->setToolTip(feedbackError_); }
    else if (!state.busy && !recentFeedback_) { message_->clear(); message_->setToolTip({}); }
    findChild<QToolButton*>("statusDetails")->setVisible(!message_->text().isEmpty());
    updateActions(); updateCurrentMember(); updateHome();
}
void MainWindow::updateActions() {
    if (!saveRules_ || !rulesEditor_) return;
    findChild<QToolButton*>("statusDetails")->setVisible(!message_->text().isEmpty());
    const auto& state = controller_->state(); const bool idle = !state.busy && !state.exiting && !state.readOnly;
    findChild<QAction*>("exportDiagnostics")->setEnabled(!state.busy&&!state.exiting); findChild<QPushButton*>("nodePreferences")->setEnabled(idle&&!selectedGroup_.isEmpty()&&profilesCombo_->currentData().toString()==state.activeProfile); findChild<QPushButton*>("ruleProviders")->setEnabled(!state.busy&&!state.exiting&&!rulesProfiles_->currentData().toString().isEmpty());
    findChild<QWidget*>("profileObjectActions")->setVisible(!selectedProfile().isEmpty());
    findChild<QPushButton*>("batchProfiles")->setVisible(!controller_->profiles().isEmpty());
    const auto draftError=ruleDraftError();
    saveRules_->setEnabled(idle && !rulesProfiles_->currentData().toString().isEmpty() && rulesDirty_ && draftError.isEmpty());
    saveRules_->setToolTip(!draftError.isEmpty()?draftError:rulesDirty_?"校验并保存所选订阅的规则":"规则没有未保存修改");
    findChild<QPushButton*>("resetCustomRules")->setEnabled(!rulesSaving_ && !state.exiting && rulesDirty_);
    rulesEditor_->setReadOnly(rulesSaving_ || state.readOnly || rulesProfiles_->currentData().toString().isEmpty());
    const bool ruleDraftAvailable=!rulesProfiles_->currentData().toString().isEmpty() && !rulesSaving_ && !state.exiting && !state.readOnly;
    auto* quickRules=findChild<QTableWidget*>("customRulesList"); quickRules->setEnabled(!rulesSaving_ && !state.exiting);
    const int quickRow=quickRules->currentRow(); const bool canEditQuick=!rulesEditor_->isReadOnly()&&!rulesSaving_;
    findChild<QPushButton*>("removeQuickRule")->setEnabled(canEditQuick&&quickRow>=0);
    findChild<QPushButton*>("moveQuickRuleUp")->setEnabled(canEditQuick&&quickRow>0);
    findChild<QPushButton*>("moveQuickRuleDown")->setEnabled(canEditQuick&&quickRow>=0&&quickRow+1<quickRules->rowCount());
    rulesProfiles_->setEnabled(!rulesSaving_ && !state.exiting && rulesProfiles_->count()>0);
    findChild<QComboBox*>("ruleType")->setEnabled(ruleDraftAvailable); ruleTarget_->setEnabled(ruleDraftAvailable); findChild<QPushButton*>("addCustomRule")->setEnabled(ruleDraftAvailable);
    findChild<QPushButton*>("advancedRules")->setEnabled(ruleDraftAvailable && !state.busy);
    findChild<QLineEdit*>("ruleMatch")->setEnabled(ruleDraftAvailable && findChild<QComboBox*>("ruleType")->currentData().toString()!="MATCH");
    findChild<QPushButton*>("nodeNextStep")->setVisible(!state.ready); findChild<QPushButton*>("nodeNextStep")->setText(state.activeProfile.isEmpty()?"前往订阅":"前往首页启动"); findChild<QPushButton*>("connectionNextStep")->setVisible(!state.ready);
    const bool actualRules=rulesTabs_->currentIndex()==1;
    rulesProfiles_->setVisible(!actualRules); findChild<QLabel*>("rulesProfileLabel")->setVisible(!actualRules); findChild<QPushButton*>("ruleProviders")->setVisible(!actualRules);
    auto* runtimeContext=findChild<QLabel*>("rulesRuntimeContext"); runtimeContext->setVisible(actualRules); QString runtimeName; for (const auto& profile:controller_->profiles()) if (profile.id==state.activeProfile) runtimeName=profile.name;
    runtimeContext->setText(runtimeName.isEmpty()?QString("没有正在运行的订阅"):"当前运行："+runtimeName); runtimeContext->setToolTip(runtimeContext->text());
    findChild<QPushButton*>("rulesNextStep")->setVisible(actualRules?!state.ready:rulesProfiles_->count()==0); findChild<QPushButton*>("rulesNextStep")->setText(actualRules?"前往首页启动":"前往订阅");
    findChild<QLabel*>("rulesRuntimeEmpty")->setVisible(rulesFilter_.rowCount()==0); findChild<QLabel*>("rulesRuntimeEmpty")->setText(!state.ready?"内核未就绪":"暂未读取到匹配的内核规则");
    auto* quickEmpty=quickRules->findChild<QLabel*>("emptyState"); quickEmpty->setText(rulesProfiles_->count()==0?"请先添加或选择订阅":"还没有优先规则\n在上方添加，或打开完整规则编辑");
    const int networkChanges=networkSettingCount(); applySettings_->setEnabled(idle && networkChanges>0); applySettings_->setVisible(networkChanges>0); resetSettings_->setEnabled(!state.exiting && networkChanges>0); resetSettings_->setVisible(networkChanges>0);
    settingsDraftHint_->setText(settingsApplying_?"正在校验并应用…":!settingsFeedback_.isEmpty()?settingsFeedback_:networkChanges>0?QString("有 %1 项需要重启内核的修改").arg(networkChanges):QString()); settingsDraftHint_->setToolTip(settingsDraftHint_->text());
    findChild<QAction*>("importGeoData")->setEnabled(idle); findChild<QAction*>("previewActiveConfig")->setEnabled(!state.busy && !state.exiting && !state.activeProfile.isEmpty());
    cancelTestAction_->setEnabled(controller_->testingNodes()); cancelTestButton_->setVisible(controller_->testingNodes()); testProgress_->setVisible(true);
    nodeEmpty_->setVisible(filter_.rowCount()==0); nodeEmpty_->setText(model_.rowCount()>0 ? "没有匹配的节点\n请调整搜索条件" : controller_->profiles().isEmpty()?"暂无节点\n请先添加订阅":state.activeProfile.isEmpty()?"请先使用订阅":!state.ready ? "内核未就绪" : "使用所选订阅后，选择策略组查看节点"); nodeCardsEmpty_->setText(nodeEmpty_->text()); nodeCardsEmpty_->setVisible(filter_.rowCount()==0);
    const auto findProfile = [this](const QString& id) -> const Profile* { for (const auto& p : controller_->profiles()) if (p.id == id) return &p; return nullptr; };
    const auto* activeProfile = findProfile(state.activeProfile);
    const auto summary = activeProfile ? activeProfile->name : "尚未激活订阅";
    profileSummary_->setToolTip(summary); profileSummary_->setText(summary);
    const auto* selectedSource = findProfile(profilesCombo_->currentData().toString());
    useProfile_->setEnabled(idle && selectedSource); profilesCombo_->setEnabled(!state.exiting);
    const bool viewingActive = selectedSource && selectedSource->id == state.activeProfile;
    const bool pending=selectedSource && selectedSource->candidate!=selectedSource->active;
    useProfile_->setText(viewingActive && pending ? "应用更新" : "使用并启动"); useProfile_->setVisible(!viewingActive || pending || !state.ready);
    useProfile_->setToolTip("校验并切换所选候选版本；更新下载不会自动替换当前运行版本"); proxyProfileHint_->setText(!selectedSource ? "先添加订阅" : !viewingActive ? "所选订阅尚未使用" : ""); proxyProfileHint_->hide(); profilesCombo_->setToolTip(profilesCombo_->currentText()+"\n"+proxyProfileHint_->text());
    nodes_->setEnabled(!state.exiting); nodeCards_->setEnabled(!state.exiting);
    coreAction_->setText(state.core == CoreState::Running ? "停止内核" : state.activeProfile.isEmpty() ? (controller_->profiles().isEmpty()?"添加订阅":"选择订阅") : "启动内核");
    coreAction_->setEnabled(idle);
    const bool mainAction=!state.ready&&state.core!=CoreState::Running; if (coreButton_->property("primary").toBool()!=mainAction) { coreButton_->setProperty("primary",mainAction); coreButton_->style()->unpolish(coreButton_); coreButton_->style()->polish(coreButton_); }
    const bool stopping=state.core==CoreState::Running; if (coreButton_->property("danger").toBool()!=stopping) { coreButton_->setProperty("danger",stopping); coreButton_->style()->unpolish(coreButton_); coreButton_->style()->polish(coreButton_); }
    const auto group = controller_->proxies().value(selectedGroup_).toObject(); const auto type = group.value("type").toString();
    const bool hasNode = !nodes_->selectionModel()->selectedRows().isEmpty(); const bool canChoose = QStringList{"Selector", "URLTest", "Fallback"}.contains(type);
    const auto rows = nodes_->selectionModel()->selectedRows(); const auto selectedName = rows.isEmpty() ? QString() : model_.nameAt(filter_.mapToSource(rows.first()).row());
    const bool isGroup = controller_->proxies().value(selectedName).toObject().value("all").isArray();
    enterGroup_->setVisible(isGroup); enterGroup_->setEnabled(!state.exiting && state.ready && viewingActive && isGroup);
    backGroup_->setVisible(!groupHistory_.isEmpty()); backGroup_->setEnabled(!state.exiting && viewingActive);
    chooseAction_->setText(QStringList{"URLTest","Fallback"}.contains(type)?"固定节点":"切换节点");
    chooseAction_->setEnabled(idle && state.ready && viewingActive && hasNode && canChoose && selectedName!=group.value("now").toString());
    chooseAction_->setToolTip(!hasNode?"单击预选；双击或 Enter 使用，Esc 取消预选":selectedName==group.value("now").toString()?"此成员已是内核当前选择":!canChoose?"此策略组不支持手动选择":QStringList{"URLTest","Fallback"}.contains(type)?"固定自动组到此节点；点击恢复自动选择可取消固定":"校验内核回读后确认生效；失败保留原选择");
    testAction_->setEnabled(idle && state.ready && viewingActive && model_.rowCount() > 0); testSelectedAction_->setEnabled(idle && state.ready && viewingActive && hasNode);
    static_cast<NodeCardView*>(nodeCards_)->setTestEnabled(testAction_->isEnabled()); groupSwitcher_->setEnabled(!state.exiting && state.ready && viewingActive && proxyGroups_->count()>0);
    automaticAction_->setEnabled(idle && state.ready && viewingActive && !group.value("fixed").toString().isEmpty() && QStringList{"URLTest", "Fallback"}.contains(type)); automaticAction_->setText(group.value("fixed").toString().isEmpty()?"自动选择中":"恢复自动选择");
    automaticAction_->setVisible(QStringList{"URLTest", "Fallback"}.contains(type));
    locateAction_->setEnabled(state.ready && viewingActive && !group.value("now").toString().isEmpty()); proxyGroups_->setEnabled(!state.exiting && state.ready && viewingActive);
    const auto* profile = findProfile(selectedProfile()); const bool single=profileTable_->selectionModel()->selectedRows().size()<=1; const bool valid = idle && profile && single;
    const bool current=profile && profile->id==state.activeProfile, newVersion=profile && profile->candidate!=profile->active;
    findChild<QPushButton*>("profileChanges")->setEnabled(profile && single && !state.exiting); findChild<QPushButton*>("profileEdit")->setEnabled(valid); findChild<QToolButton*>("profilePolicyButton")->setEnabled(valid); findChild<QPushButton*>("profileManage")->setEnabled(profile&&single&&!state.exiting);
    profileActivate_->setEnabled(valid && (!current || newVersion || !state.ready)); profileActivate_->setVisible(true); profileUpdate_->setEnabled(idle&&profile&&(!single||!profile->source.isEmpty())); profileReplace_->setEnabled(valid && profile->sourceType == "file");
    profileActivate_->setText(current&&state.ready&&!newVersion?"正在使用":current && newVersion ? "应用更新" : current&&!state.ready?"重新加载并启动":"使用并启动");
    profileUpdate_->setText(!single?"更新选中订阅":profile && profile->sourceType=="file" ? "同步原文件" : "更新订阅"); profileUpdate_->setToolTip(profile && profile->source.isEmpty()?QString("本地快照没有外部来源；请编辑节点或更换文件来源"):profile && profile->sourceType=="file" ? "重新读取原文件；只更新候选，当前运行版本保持不变" : "下载并校验新版本；不会自动切换正在运行的配置"); profileReplace_->setVisible(profile && profile->sourceType == "file");
    profileReload_->setVisible(current && !newVersion); profileReload_->setEnabled(valid && current && !newVersion);
    const auto selectionText = profile ? "选中：" + profile->name + (current ? " · 当前使用" : "") : QString("选择订阅后可更新或使用"); profileSelection_->setToolTip(selectionText); profileSelection_->clear();
    profileRollback_->setEnabled(valid && !profile->lastGood.isEmpty()); profileRename_->setEnabled(valid); profileDelete_->setEnabled(valid); profilePolicy_->setEnabled(valid);
    QString details;
    if (profile) {
        details = profile->autoUpdate ? (profile->sourceType == "file" ? "监测文件变化，更新候选" : QString("每 %1 小时更新候选").arg(profile->updateIntervalHours)) : "自动更新关闭";
        const auto usage = SubscriptionInfo::summary(profile->subscriptionInfo); if (!usage.isEmpty()) details += " · " + usage;
        if (!profile->updateError.isEmpty()) details += " · 更新失败，保留原版本";
        profileDetails_->setToolTip(details + (profile->updateError.isEmpty() ? QString() : "\n" + profile->updateError));
    }
    if (profile) details = (current ? "当前运行版本" : "尚未使用") + QString(newVersion?" · 有更新待应用":" · 无待应用更新") + " · " + details;
    profileDetails_->setToolTip(details + (profile && !profile->updateError.isEmpty()?"\n"+profile->updateError:QString()));
    const auto notice=!single?QString("已选择 %1 份订阅").arg(profileTable_->selectionModel()->selectedRows().size()):profile&&!profile->updateError.isEmpty()?QString("更新失败，原版本已保留"):profile&&!current?QString("尚未使用"):profile&&newVersion?QString("有更新待应用"):QString();
    profileDetails_->setText(notice);
    bool hasRecent = false;
    if (activeProfile) for (const auto& value : activeProfile->recentSelections) { const auto pair = value.toObject(); if (pair.value("group").toString() == selectedGroup_ && group.value("all").toArray().contains(pair.value("member"))) hasRecent = true; }
    recentNodes_->setEnabled(idle && state.ready && viewingActive && hasRecent && QStringList{"Selector","URLTest","Fallback"}.contains(type));
    closeConnection_->setEnabled(idle && state.ready && controller_->connectionsCurrent() && connectionView_->currentIndex()==0 && !connectionsTable_->selectionModel()->selectedRows().isEmpty());
    const bool connectionSelected=!connectionsTable_->selectionModel()->selectedRows().isEmpty(); connectionDetailsAction_->setEnabled(connectionSelected); connectionCopyAction_->setEnabled(connectionSelected);
    closeAllConnections_->setEnabled(idle && state.ready && controller_->connectionsCurrent() && !controller_->connections().isEmpty());  connectionPause_->setEnabled(!state.exiting&&state.ready&&!state.busy&&controller_->connectionsVisible());
    connectionEmpty_->setVisible(connectionsFilter_.rowCount()==0); connectionEmpty_->setText(!controller_->connectionError().isEmpty() ? "连接读取失败" : !state.ready ? "内核未就绪" : connectionsModel_.rowCount()>0 ? "没有匹配的连接\n请调整搜索条件" : connectionView_->currentIndex()==1 ? "暂无已结束的连接" : controller_->connectionsPaused()?"已暂停":!controller_->connectionsCurrent()?"正在读取连接…":"暂无活动连接");
}
bool MainWindow::logMatches(const QString& line) const { return line.contains(logSearch_->text(),Qt::CaseInsensitive) && (logFilter_->currentData().toString() == "all" || Diagnostics::levelOf(line) == logFilter_->currentData().toString()); }
void MainWindow::updateLogs() {
    QStringList filtered; for (const auto& line : logsPaused_ ? pausedLogs_ : controller_->diagnostics()->lines()) if (logMatches(line)) filtered.append(line);
    if (filtered==displayedLogLines_) return;
    bool prefix=filtered.size()>=displayedLogLines_.size();
    for (int i=0;prefix && i<displayedLogLines_.size();++i) prefix=filtered[i]==displayedLogLines_[i];
    if (prefix) { const int before=displayedLogLines_.size(); for (int i=before;i<filtered.size();++i) appendLogLine(filtered[i]); return; }
    const auto text=filtered.join('\n'); displayedLogLines_=filtered;
    auto* scroll=logView_->verticalScrollBar(); const int position=scroll->value(); const bool follow=position==scroll->maximum() && !logView_->textCursor().hasSelection();
    logView_->setPlainText(text); scroll->setValue(follow?scroll->maximum():position);
    followLogs_->setEnabled(scroll->value()<scroll->maximum());
}
void MainWindow::appendLogLine(const QString& line) {
    displayedLogLines_.append(line); if (displayedLogLines_.size()>2000) displayedLogLines_.removeFirst();
    auto* scroll=logView_->verticalScrollBar(); const int position=scroll->value();
    const bool follow=position==scroll->maximum() && !logView_->textCursor().hasSelection();
    auto cursor=QTextCursor(logView_->document()); cursor.movePosition(QTextCursor::End);
    if (!logView_->document()->isEmpty()) cursor.insertBlock();
    cursor.insertText(line);
    scroll->setValue(follow?scroll->maximum():position);
    followLogs_->setEnabled(scroll->value()<scroll->maximum());
}
void MainWindow::syncSectionNavigation(int page) {
    const int section=page==0||page==2||page==7?0:page==1||page==3?1:page==4||page==5?4:6;
    const QSignalBlocker navigationBlock(navigation_),tabsBlock(sectionTabs_);
    for (int row=0;row<navigation_->count();++row) if (navigation_->item(row)->data(Qt::UserRole).toInt()==section) { navigation_->setCurrentRow(row); navigation_->item(row)->setData(Qt::UserRole+1,page); }
    if (!sectionTabs_->count()||sectionTabs_->property("section").toInt()!=section) {
        while (sectionTabs_->count()) sectionTabs_->removeTab(0);
        // Settings uses the same title slot: its category page tabs stand where other pages show section tabs.
        const auto views=section==0?QList<QPair<QString,int>>{{"状态",0},{"节点",2},{"网站测试",7}}:section==1?QList<QPair<QString,int>>{{"订阅",1},{"规则",3}}:section==4?QList<QPair<QString,int>>{{"连接",4},{"日志",5}}:QList<QPair<QString,int>>{};
        for (const auto& view:views) { const int tab=sectionTabs_->addTab(view.first); sectionTabs_->setTabData(tab,view.second); }
        sectionTabs_->setProperty("section",section);
    }
    auto* title=pages_->widget(page)->findChild<QLabel*>("pageTitle");
    if (sectionTitle_!=title) {
        if (sectionTitle_) { delete sectionTitle_->parentWidget()->layout()->replaceWidget(sectionTabs_,sectionTitle_); sectionTitle_->show(); sectionTitle_=nullptr; }
        if (section!=6&&title) { delete title->parentWidget()->layout()->replaceWidget(title,sectionTabs_); title->hide(); sectionTitle_=title; }
    }
    for (int tab=0;tab<sectionTabs_->count();++tab) if (sectionTabs_->tabData(tab).toInt()==page) sectionTabs_->setCurrentIndex(tab);
    // Settings reuses this title slot: its category tabs replace the section tabs and search stays right-aligned.
    sectionTabs_->setVisible(section!=6);
}
void MainWindow::setPage(int index) {
    if (index < 0 || index >= pages_->count()) return;
    if (index!=4 && connectionDetail_) connectionDetail_->close();
    if (index!=6 && configPreview_) configPreview_->close();
    pages_->setCurrentIndex(index);
    if (index==0) updateHome();
    websitePage_->setActive(isVisible() && index==7,selectedGroup_);
    syncSectionNavigation(index);
    controller_->setDiagnosticsVisible(isVisible() && index == 5 && !logsPaused_);
    controller_->setConnectionsVisible(isVisible() && index == 4);
    controller_->setHomeVisible(isVisible() && index == 0);
    controller_->setRulesVisible(isVisible() && index == 3 && rulesTabs_->currentIndex() == 1);
    if (index==3&&!rulesDirty_&&isVisible()) loadRuleDraft();
    if (index == 0 || index == 2) { controller_->refresh(); if (isVisible()) refreshTimer_.start(8000); }
    else refreshTimer_.stop();
    if (index == 3 && rulesTabs_->currentIndex() == 1) controller_->refreshRules();
    if (index == 5) updateLogs();
}
void MainWindow::showOutboundDiagnostics() { if (outboundDialog_) { outboundDialog_->show(); outboundDialog_->raise(); return; } outboundDialog_=new OutboundDialog(controller_,this); outboundDialog_->show(); }
bool MainWindow::confirmDiscardEdits() {
    if (DialogDraftGuard::hasPending(this) || rulesSaving_ || settingsApplying_) return false;
    if (!rulesDirty_ && !settingsDirty() && !DialogDraftGuard::hasDrafts(this)) return true;
    return QMessageBox::question(this,"未保存的修改","继续会放弃主页面及编辑窗口中未保存的修改，是否继续？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (controller_->state().exiting) { event->accept(); return; }
    if (!trayAvailable_) { event->ignore(); if (confirmDiscardEdits()) controller_->requestExit(); return; }
    if (!controller_->settings().lightweight) { event->ignore(); hide(); return; }
    if (!confirmDiscardEdits()) { event->ignore(); return; }
    emit guiReleaseRequested(); event->accept();
}
int MainWindow::page() const { return pages_->currentIndex(); }
int MainWindow::memberView() const { return memberViewMode_->currentIndex(); }
void MainWindow::showEvent(QShowEvent* event) { if (page()==3&&!rulesDirty_) loadRuleDraft(); websitePage_->setActive(page()==7,selectedGroup_); QMainWindow::showEvent(event); controller_->setHomeVisible(page()==0); controller_->setRulesVisible(pages_->currentIndex() == 3 && rulesTabs_->currentIndex() == 1); controller_->setDiagnosticsVisible(pages_->currentIndex() == 5 && !logsPaused_); controller_->setConnectionsVisible(pages_->currentIndex() == 4); if (pages_->currentIndex() == 3 && rulesTabs_->currentIndex() == 1) controller_->refreshRules(); controller_->refresh(); if (pages_->currentIndex() == 0 || pages_->currentIndex() == 2) refreshTimer_.start(8000); }
void MainWindow::hideEvent(QHideEvent* event) { homeOutboundSnapshot_={}; updateHomeOutbound(); if (connectionOptions_) connectionOptions_->flushPreferences(); if (outboundDialog_) outboundDialog_->close(); controller_->setHomeVisible(false); websitePage_->setActive(false); if (connectionDetail_) connectionDetail_->close(); if (configPreview_) configPreview_->close(); refreshTimer_.stop(); controller_->setRulesVisible(false); controller_->setDiagnosticsVisible(false); controller_->setConnectionsVisible(false); QMainWindow::hideEvent(event); }
}
