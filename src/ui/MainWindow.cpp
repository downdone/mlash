#include "TaskDialog.h"
#include "ProviderPolicyDialog.h"
#include "ProfileChangesDialog.h"
#include "MainWindow.h"
#include "TaskWorkspace.h"
#include "MainWindowParts.h"
#include "StatusSentence.h"
#include "FeedbackSync.h"
#include "StatusShape.h"
#include "SkeletonBlock.h"
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
MainWindow::MainWindow(AppController* controller, QWidget* parent) : QMainWindow(parent), controller_(controller), model_(this), filter_(this) {
    setAcceptDrops(true);
    // Use the same bundled Widgets style for the window and its dialogs on Windows.
    WidgetStyle::install();
    UiTheme::setPreferences(controller_->settings().appearance,controller_->settings().textScale);
    setWindowTitle("Mlash"); setWindowIcon(TrayController::icon()); resize(1040, 720); setMinimumSize(840, 560);

    auto* central = new QWidget(this); auto* layout = new QHBoxLayout(central);
    central->setObjectName("mainSurface"); central->setAttribute(Qt::WA_StyledBackground, true);
    layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto* sidebar = new QWidget; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(UiTheme::SidebarWidth); sidebar->setAttribute(Qt::WA_StyledBackground); auto* sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(8,8,8,8); sideLayout->setSpacing(6);
    navigation_ = new QListWidget; navigation_->setObjectName("navigation"); navigation_->addItems({"首页", "配置", "数据", "设置"}); navigation_->setIconSize(QSize(18, 18));
    const QList<int> pageOrder{ViewIds::Home,ViewIds::Profiles,ViewIds::Data,ViewIds::Settings};
    for (int i=0;i<pageOrder.size();++i) { navigation_->item(i)->setData(Qt::UserRole,pageOrder[i]); navigation_->item(i)->setIcon(navigationIcon(navKindOfView(pageOrder[i]))); }
    sideLayout->addWidget(navigation_, 1);
    // 常驻状态条（分层设计 P1）：四段文案由 statusSentence 统一产出，与托盘 tooltip 同源。
    statusButton_ = new QToolButton(sidebar); statusButton_->setObjectName("sidebarStatus"); statusButton_->setAccessibleName("当前运行状态，单击返回首页"); statusButton_->setCursor(Qt::PointingHandCursor);
    auto* statusLayout = new QHBoxLayout(statusButton_); statusLayout->setContentsMargins(10, 8, 8, 8); statusLayout->setSpacing(8);
    statusHintDot_ = new QLabel(statusButton_); statusHintDot_->setObjectName("sidebarStatusDot"); statusHintDot_->setFixedSize(12, 12); statusLayout->addWidget(statusHintDot_, 0, Qt::AlignTop);
    auto* statusLines = new QVBoxLayout; statusLines->setSpacing(1);
    statusLine1_ = new ElidedLabel(statusButton_); statusLine1_->setObjectName("sidebarStatusLine1"); statusLines->addWidget(statusLine1_);
    statusLine2_ = new ElidedLabel(statusButton_); statusLine2_->setObjectName("sidebarStatusLine2"); statusLines->addWidget(statusLine2_);
    statusLayout->addLayout(statusLines, 1); statusButton_->hide();
    connect(statusButton_, &QToolButton::clicked, this, [this] { navigate(Route{ViewIds::Home, kNoCategory, HomeAnchor::Control}); });
    auto* signature=new QHBoxLayout; auto* mark=new QLabel; mark->setPixmap(TrayController::icon().pixmap(20,20)); signature->addWidget(mark);
    auto* brand=new QLabel("Mlash"); brand->setProperty("secondary",true); signature->addWidget(brand,1); sideLayout->addLayout(signature);
    auto* version = new QLabel(QString("v" MLASH_VERSION)); version->setObjectName("secondaryText"); sideLayout->addWidget(version);
    // K1/K6：页面栈只有五个视图，顺序与 ViewIds 一致；数据页内部再分四个页签。
    pages_ = new QStackedWidget; pages_->setObjectName("mainPages");
    auto pagePolicy=pages_->sizePolicy(); pagePolicy.setRetainSizeWhenHidden(true); pages_->setSizePolicy(pagePolicy);
    pages_->addWidget(homePage());                 // ViewIds::Home
    pages_->addWidget(profilesPage());             // ViewIds::Profiles
    pages_->addWidget(rulesPage());                // ViewIds::Rules
    pages_->addWidget(dataPage());                 // ViewIds::Data
    pages_->addWidget(settingsPage());             // ViewIds::Settings
    // K6：「测试」槽位是 dataPage() 里占位的空 QWidget。直接 addWidget 会把它留在末位，
    // 删掉占位控件后索引又会前移，页签与面板随即错位；所以按占位的位置 insertWidget。
    websitePage_=new WebsiteTestsPage(controller_);
    if (auto* placeholder = dataPanels_->widget(int(ViewIds::DataTab::Tests))) {
        if (placeholder != websitePage_) { const int slot = dataPanels_->indexOf(placeholder); dataPanels_->insertWidget(slot, websitePage_); delete placeholder; }
    } else {
        dataPanels_->addWidget(websitePage_);
    }
    Q_ASSERT(dataPanels_->indexOf(websitePage_) == int(ViewIds::DataTab::Tests));
    dataPanels_->setCurrentIndex(int(ViewIds::DataTab::Traffic));
    connect(websitePage_,&WebsiteTestsPage::pageRequested,this,&MainWindow::setPage); connect(websitePage_,&WebsiteTestsPage::diagnosticsRequested,this,&MainWindow::showOutboundDiagnostics);
    for (int page = 0; page < pages_->count(); ++page) { if (pages_->widget(page)->objectName().isEmpty()) pages_->widget(page)->setObjectName("pageSurface"); pages_->widget(page)->setProperty("pageSurface",true); pages_->widget(page)->setAttribute(Qt::WA_StyledBackground, true); pages_->widget(page)->setAutoFillBackground(true); }
    layout->addWidget(sidebar);
    // K1（原型 topbar）：内容区首行固定 72px——左标题/副标题，右状态胶囊 + 主题钮 + 更多钮。
    auto* content = new QWidget(central); content->setObjectName("contentSurface"); auto* contentLayout = new QVBoxLayout(content); contentLayout->setContentsMargins(0,0,0,0); contentLayout->setSpacing(0); pages_->setContentsMargins(UiTheme::ContentInset,UiTheme::ContentTop,UiTheme::ContentInset,UiTheme::ContentTop);
    auto* topbar = new QWidget(content); topbar->setObjectName("topbar"); topbar->setFixedHeight(UiTheme::TopbarHeight);
    // Equal-width edge slots keep the title centered independently of toolbar actions.
    auto* topbarLayout = new QHBoxLayout(topbar); topbarLayout->setContentsMargins(16,0,16,0); topbarLayout->setSpacing(0);
    auto* leadingSlot = new QWidget(topbar); leadingSlot->setFixedWidth(76); topbarLayout->addWidget(leadingSlot);
    topbarTitle_ = new QLabel; topbarTitle_->setObjectName("topbarTitle"); topbarTitle_->setTextFormat(Qt::PlainText); topbarTitle_->setAlignment(Qt::AlignCenter);
    topbarLayout->addWidget(topbarTitle_,1);
    // Retain readback labels for diagnostics without duplicating visible status.
    auto* auxiliary = new QWidget(topbar); auxiliary->hide();
    topbarSubtitle_ = new QLabel(auxiliary); topbarSubtitle_->setObjectName("topbarSubtitle");
    topbarDot_ = new QLabel(auxiliary); topbarDot_->setObjectName("topbarDot");
    topbarStatusText_ = new QLabel(auxiliary); topbarStatusText_->setObjectName("topbarStatusText");
    auto* trailingSlot = new QWidget(topbar); trailingSlot->setFixedWidth(76);
    auto* trailingLayout = new QHBoxLayout(trailingSlot); trailingLayout->setContentsMargins(0,0,0,0); trailingLayout->setSpacing(4);
    topbarLayout->addWidget(trailingSlot);
    auto* importButton=new QToolButton(leadingSlot); importButton->setObjectName("toolbarImport"); importButton->setProperty("prototypeIcon",true); importButton->setIcon(UiIcons::icon(UiIcons::Glyph::Import,UiTheme::Brand)); importButton->setIconSize(QSize(22,22)); importButton->setFixedSize(32,32); importButton->setToolTip("导入配置");
    auto* leadingLayout=new QHBoxLayout(leadingSlot); leadingLayout->setContentsMargins(0,0,0,0); leadingLayout->addWidget(importButton,0,Qt::AlignLeft);
    connect(importButton,&QToolButton::clicked,this,[this] { addSubscription(false); });
    auto* addButton=new QToolButton; addButton->setObjectName("toolbarAdd"); addButton->setProperty("prototypeIcon",true); addButton->setIcon(UiIcons::icon(UiIcons::Glyph::Plus,UiTheme::Brand)); addButton->setIconSize(QSize(22,22)); addButton->setFixedSize(32,32); addButton->setToolTip("添加配置"); trailingLayout->addStretch(); trailingLayout->addWidget(addButton);
    connect(addButton,&QToolButton::clicked,this,&MainWindow::addUrl);
    contentLayout->addWidget(topbar);
    contentStack_=new QStackedWidget(content); contentStack_->setObjectName("contentHost"); contentStack_->addWidget(pages_); contentLayout->addWidget(contentStack_,1);
    layout->addWidget(content, 1); setCentralWidget(central);
    TopbarInfo topbarInfo; topbarInfo.title=topbarTitle_; topbarInfo.subtitle=topbarSubtitle_; topbarInfo.statusText=topbarStatusText_; topbarInfo.dot=topbarDot_; registerTopbar(this,topbarInfo);
    operationStatus_=new QStatusBar(this); operationStatus_->setObjectName("operationStatusBar"); operationStatus_->setSizeGripEnabled(false); contentLayout->addWidget(operationStatus_);
    // 两段并存（P4.1）：最近操作 = 跨页结果，当前告警 = 仍然成立的系统问题。两者互不覆盖。
    message_=new ElidedLabel("正在初始化"); message_->setObjectName("operationMessage"); message_->setAccessibleName("操作状态"); message_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding); operationStatus_->addWidget(message_,1);
    operationAlarm_=new ElidedLabel; operationAlarm_->setObjectName("operationAlarm"); operationAlarm_->setAccessibleName("当前告警"); operationAlarm_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); operationAlarm_->setProperty("state","error"); operationAlarm_->hide(); operationStatus_->addPermanentWidget(operationAlarm_,1);
    auto* details=new QToolButton; details->setObjectName("statusDetails"); details->setText("详情"); details->setToolTip("查看及复制完整状态信息"); operationStatus_->addPermanentWidget(details);
    connect(details,&QToolButton::clicked,this,[this] {
        const auto text=message_->toolTip().isEmpty()?message_->text():message_->toolTip();
        if (auto* previous=qobject_cast<QDialog*>(taskWorkspace()->editor("operation-details"))) previous->reject();
        auto* task=new TaskDialog(this); task->setObjectName("statusDetailsDialog"); task->setWindowTitle("操作信息");
        task->setAttribute(Qt::WA_DeleteOnClose);
        auto* box=new QVBoxLayout(task); auto* content=new QPlainTextEdit(text); content->setReadOnly(true); box->addWidget(content,1);
        auto* copy=new QPushButton("复制消息"); box->addWidget(copy,0,Qt::AlignRight); connect(copy,&QPushButton::clicked,task,[text] { QApplication::clipboard()->setText(text); }); presentTask(task,"operation-details");
    });
    auto* resolve=new QToolButton; resolve->setObjectName("statusResolve"); resolve->setText("处理"); resolve->setPopupMode(QToolButton::InstantPopup); auto* resolveMenu=new QMenu(resolve); resolve->setMenu(resolveMenu); resolveMenu->addAction("查看日志",this,[this] { navigate(Route{ViewIds::Data, kNoCategory, HomeAnchor::Control, int(ViewIds::DataTab::Logs)}); }); resolveMenu->addAction("订阅管理",this,[this] { navigate(Route{ViewIds::Profiles}); }); resolveMenu->addAction("TUN 与服务…",this,[this] { showTool("tun",{},[this] { return new TunDialog(controller_,this); }); }); resolveMenu->addAction("刷新运行状态",controller_,&AppController::refresh); operationStatus_->addPermanentWidget(resolve); resolve->hide();
    connect(&refreshTimer_, &QTimer::timeout, this, [this] { if (!controller_->state().busy) controller_->refresh(); });
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int index) {
        // 条目里存的是视图 ID（0/1/3/4），不能再交给 setPage() 当旧编号解释：
        // 旧编号 3 是规则视图、4 是数据·连接，会把「数据」入口送到规则页，把「设置」入口送到数据页。
        if (index >= 0) navigate(Route{navigation_->item(index)->data(Qt::UserRole).toInt()});
    });
    navigation_->setCurrentRow(0);
    connect(controller_, &AppController::stateChanged, this, &MainWindow::updateState);
    connect(controller_,&AppController::stateChanged,this,[this] { UiTheme::setPreferences(controller_->settings().appearance,controller_->settings().textScale); });
    connect(controller_, &AppController::profilesChanged, this, &MainWindow::updateProfiles);
    connect(controller_, &AppController::profilesChanged, this, &MainWindow::updateRulesProfiles);
    // 对象被删除时，按它打开的窗口（DNS/节点编辑/代理链）必须一起关闭，
    // 否则窗口会对着一个已经不存在的订阅继续发命令（S2）。
    connect(controller_, &AppController::profilesChanged, this, [this] {
        QSet<QString> alive;
        for (const auto& profile : controller_->profiles()) alive.insert(profile.id);
        for (const auto& tool : {"dns", "proxy-editor", "proxy-chain"}) toolWindows_.closeMissing(tool, alive);
    });
    connect(controller_, &AppController::connectionsChanged, this, &MainWindow::updateConnections);
    connect(controller_->homeMetrics(), &HomeMetrics::changed, this, &MainWindow::updateHome);
    // 出口摘要属于「数据 › 流量」（K6）：它跟随出站诊断与运行上下文变化刷新。
    connect(controller_->outboundDiagnostics(), &OutboundDiagnostics::changed, this, [this] { updateHomeOutbound(true); });
    connect(controller_, &AppController::stateChanged, this, [this] { updateHomeOutbound(); });
    connect(controller_, &AppController::rulesChanged, this, [this] { rulesModel_.setRows(controller_->activeRules()); updateActions(); });
    connect(controller_, &AppController::proxiesChanged, this, &MainWindow::updateProxies);
    connect(controller_, &AppController::nodeDelay, this, [this](const QString& name, int delay, const QString& error) { if (controller_->lastNodeTestUrl()==controller_->groupTestUrl(selectedGroup_)) model_.setDelay(name, delay, error); updateCurrentMember(); updateHome(); });
    feedbackTimer_.setSingleShot(true); feedbackTimer_.setInterval(3000);
    connect(&feedbackTimer_,&QTimer::timeout,this,[this] { recentFeedback_=false; noticeOperation_=OperationId::Unspecified; if (!settingsFeedback_.startsWith("应用失败")) settingsFeedback_.clear(); updateState(); });
    // 「最近操作」段只承载跨页结果；页面级结果由 routeOperationFeedback 落到所属页面。
    connect(controller_, &AppController::operationStarted, this, [this](const QString& text, OperationId id) {
        if (id != OperationId::Unspecified) { updateState(); return; }
        recentFeedback_=false; feedbackError_.clear(); noticeOperation_=OperationId::Unspecified; feedbackTimer_.stop();
        message_->setText(text); message_->setToolTip(text); findChild<QToolButton*>("statusResolve")->hide(); findChild<QToolButton*>("statusDetails")->show();
    });
    connect(controller_, &AppController::operationFinished, this, [this](bool ok, const QString& text, OperationId id) {
        if (id != OperationId::Unspecified) { routeOperationFeedback(ok, text, id); return; }
        message_->setText(statusNoticeText(ok, text)); message_->setToolTip(text);
        findChild<QToolButton*>("statusResolve")->setVisible(!ok);
        feedbackTimer_.stop(); recentFeedback_=ok; feedbackError_=ok?QString():text; noticeOperation_=id; if (ok) feedbackTimer_.start();
        updateActions();
    });
    // 测速进度只落在状态栏：节点区不再有底部按钮条（H3），单独一行进度会随列表变化跳位。
    connect(controller_, &AppController::delayProgress, this, [this](int done, int total) { message_->setText(QString("测延迟 %1 / %2").arg(done).arg(total)); message_->setToolTip(message_->text()); updateActions(); });
    connect(controller_->diagnostics(), &Diagnostics::lineAdded, this, [this](const QString& line) {
        if (!logsPaused_ && isVisible() && route_.view == ViewIds::Data && currentDataTab() == int(ViewIds::DataTab::Logs) && logMatches(line)) appendLogLine(line);
        if (isVisible() && (route_.view == ViewIds::Home || currentDataTab() == int(ViewIds::DataTab::Traffic))) updateHome();
    });
    connect(controller_->diagnostics(), &Diagnostics::viewCleared, this, [this] { pausedLogs_.clear(); displayedLogLines_.clear(); logView_->clear(); });
    applyAppearance();
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,&MainWindow::applyAppearance);
    // 所有页面都在上面建好了，这里才做第一次状态刷新（updateActions 依赖各页的控件）。
    updateProfiles(); updateRulesProfiles(); updateProxies(); updateState();
    {
        // 页面生命周期（S1）：每个视图一个对象，副作用写在它自己的 onEnter/onLeave 里。
        // 视图 0/2 都指首页，统一注册在 0（PageRouter 也按 0 记账）。
        struct Page final : PageLifecycle {
            MainWindow* window;
            int view;
            explicit Page(MainWindow* w, int v) : window(w), view(v) {}
            void onEnter(const Route& route) override { window->enterView(view, route); }
            void onLeave() override { window->leaveView(view); }
        };
        for (int view : {ViewIds::Home, ViewIds::Profiles, ViewIds::Rules, ViewIds::Data, ViewIds::Settings}) pageRouter_.registerPage(view, new Page(this, view));
    }
}
void MainWindow::applyAppearance() {
    const int statusHeight=qMax(30,QFontMetrics(qApp->font()).height()+10); operationStatus_->setFixedHeight(statusHeight); if (auto* details=findChild<QToolButton*>("statusDetails")) details->setFixedHeight(statusHeight-8);
    for (int row=0;row<navigation_->count();++row) navigation_->item(row)->setIcon(navigationIcon(navKindOfView(navigation_->item(row)->data(Qt::UserRole).toInt())));
    using G=UiIcons::Glyph;
    if (locateAction_) locateAction_->setIcon(UiIcons::icon(G::Locate,UiTheme::Brand));
    if (testAction_) testAction_->setIcon(UiIcons::icon(G::Gauge,UiTheme::Brand));
    for (const auto& entry:QList<QPair<QString,G>>{{"browseGroup",G::Layers},{"nodeToolsButton",G::More},{"nodeSortButton",G::Sliders}}) if (auto* button=findChild<QToolButton*>(entry.first)) button->setIcon(UiIcons::icon(entry.second,UiTheme::Brand));
    for (const auto& pair:QList<QPair<QString,G>>{{"addSubscription",G::Plus},{"updateProfile",G::Refresh},{"clearLogViewButton",G::Clear},{"followLatestLogs",G::Refresh},{"saveCustomRules",G::Save}}) if (auto* control=findChild<QAbstractButton*>(pair.first)) control->setIcon(UiIcons::icon(pair.second,control->property("primary").toBool()?UiTheme::OnPrimary:UiTheme::Secondary));
    for (const auto& name:{"nodeSearch","logSearch","connectionsSearch","activeRulesSearch"}) if (auto* field=findChild<QLineEdit*>(name)) {
        if (!field->property("searchGlyph").toBool()) { field->addAction(UiIcons::icon(G::Search),QLineEdit::LeadingPosition)->setObjectName("searchGlyph"); field->setProperty("searchGlyph",true); }
        else if (auto* action=field->findChild<QAction*>("searchGlyph")) action->setIcon(UiIcons::icon(G::Search));
    }
    for (auto* view:QList<QAbstractItemView*>{nodeList_,nodeCards_,connectionsTable_}) view->viewport()->update();
    updateState();
}
MainWindow::~MainWindow() { NodePresentation::clearTransientCache(); if (connectionOptions_) connectionOptions_->flushPreferences(); controller_->setHomeVisible(false); controller_->setWebsiteTestsVisible(false); controller_->clearWebsiteTests(); controller_->setRulesVisible(false); controller_->setConnectionsVisible(false); controller_->setDiagnosticsVisible(false); }
QWidget* MainWindow::dataPage() {
    // K6（原型 · 数据页）：一条页签行 + 一个面板栈。四个页签各自持有自己的面板，
    // 可见性由 updateDataPanelVisibility 按「当前页签」合成——不可见的面板不再接收采样。
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0,0,0,0); layout->setSpacing(10);
    auto* heading = new QHBoxLayout; heading->setSpacing(10); heading->addStretch();
    dataTabs_ = new QTabBar; dataTabs_->setObjectName("dataTabs"); dataTabs_->setExpanding(false); dataTabs_->setDrawBase(false);
    for (int i = 0; i < ViewIds::DataTabCount; ++i) dataTabs_->addTab(ViewIds::dataTabName(ViewIds::dataTabFromIndex(i)));
    heading->addWidget(dataTabs_,0,Qt::AlignHCenter); heading->addStretch();
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
    // 流量：三个统计块 + 60 秒曲线 + 出站摘要（原型 data 页）。
    auto* panel = new QWidget; panel->setObjectName("trafficPanel"); auto* layout = new QVBoxLayout(panel); layout->setContentsMargins(0,0,0,0); layout->setSpacing(14);
    auto* tiles = new QHBoxLayout; tiles->setSpacing(1);
    for (const auto& pair : QList<QPair<QString,QString>>{{"trafficUpload","上传"},{"trafficDownload","下载"},{"trafficActive","活动连接"}}) {
        auto* tile = WindowParts::panel(); tile->setObjectName(pair.first); auto* tileLayout = new QVBoxLayout(tile); tileLayout->setContentsMargins(16,12,16,12); tileLayout->setSpacing(4);
        auto* caption = new QLabel(pair.second); caption->setProperty("secondary", true); tileLayout->addWidget(caption);
        auto* value = new QLabel("—"); value->setObjectName(pair.first + "Value"); value->setProperty("role", "value"); value->setProperty("metricValue",true); tileLayout->addWidget(value);
        tiles->addWidget(tile, 1);
    }
    layout->addLayout(tiles);
    auto* diagnosticRows=PrototypeUi::group(layout,"诊断"); auto* outbound=PrototypeUi::row(diagnosticRows,"trafficDiagnostics","出口与服务诊断",UiIcons::Glyph::Globe,"按需检测"); connect(outbound,&QPushButton::clicked,this,&MainWindow::showOutboundDiagnostics);
    auto* expand=PrototypeUi::row(diagnosticRows,"trafficDetails","流量明细",UiIcons::Glyph::Chart);
    auto* metrics = new HomeMetricsView(controller_->homeMetrics()); connect(metrics,&HomeMetricsView::inspectRequested,this,&MainWindow::showOutboundDiagnostics); layout->addWidget(metrics); metrics->hide(); connect(expand,&QPushButton::clicked,metrics,[metrics] { metrics->setVisible(!metrics->isVisible()); });
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
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
    auto* logHeading=new QHBoxLayout; logHeading->addStretch(); layout->addLayout(logHeading); auto* toolbar=new QWidget; toolbar->setObjectName("logToolbar"); auto* actions = new WrapLayout(toolbar);
    auto* logPanel = panel(); auto* logLayout = new QVBoxLayout(logPanel); logLayout->setContentsMargins(12, 10, 12, 10); logLayout->setSpacing(8); layout->addWidget(logPanel, 1);
    logSearch_ = new QLineEdit; logSearch_->setObjectName("logSearch"); logSearch_->setPlaceholderText("搜索日志…"); logSearch_->setAccessibleName("搜索日志"); logSearch_->setClearButtonEnabled(true); actions->addWidget(logSearch_);
    logFilter_ = new CompactComboBox; logFilter_->setObjectName("logLevelFilter"); for (const auto& pair : QList<QPair<QString,QString>>{{"全部","all"},{"调试","debug"},{"信息","info"},{"警告","warning"},{"错误","error"},{"应用","app"}}) logFilter_->addItem(pair.first,pair.second); actions->addWidget(logFilter_);
    auto* pause = new QPushButton("暂停显示"); actions->addWidget(pause); pause->setObjectName("pauseLogs"); pause->setCheckable(true); auto* copy = new QPushButton("复制选中"); actions->addWidget(copy);
    auto* clear = new QAction("清空视图", this); clear->setObjectName("clearLogView"); auto* clearButton=new QToolButton; clearButton->setObjectName("clearLogViewButton"); clearButton->setToolButtonStyle(Qt::ToolButtonTextOnly); clearButton->setDefaultAction(clear); actions->addWidget(clearButton);
    auto* more = new MenuPushButton("日志选项"); actions->addWidget(more); auto* menu = new QMenu(more); more->setMenu(menu); auto* directory = menu->addAction("日志目录"); auto* report = menu->addAction("导出脱敏诊断…"); report->setObjectName("exportDiagnostics"); auto* exportButton=new QToolButton; exportButton->setObjectName("exportDiagnosticsButton"); exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly); exportButton->setDefaultAction(report); exportButton->setText("导出诊断…"); logHeading->addWidget(exportButton); menu->removeAction(report); connect(menu, &QMenu::aboutToShow, this, [this, report] { report->setEnabled(!controller_->state().busy && !controller_->state().exiting); }); logLayout->addWidget(toolbar);
    logView_ = new QPlainTextEdit; logView_->setObjectName("logView"); logView_->setReadOnly(true); logView_->setLineWrapMode(QPlainTextEdit::NoWrap); logView_->setMaximumBlockCount(2000); logLayout->addWidget(logView_, 1);
    auto* wrap=menu->addAction("自动换行"); wrap->setObjectName("wrapLogs"); wrap->setCheckable(true); connect(wrap,&QAction::toggled,logView_,[this](bool enabled) { const auto cursor=logView_->textCursor(); logView_->setLineWrapMode(enabled?QPlainTextEdit::WidgetWidth:QPlainTextEdit::NoWrap); logView_->setTextCursor(cursor); });
    logView_->setToolTip("仅日志页可见时接收内核日志；最多保留 2000 行。");
    // 日志页的空/无匹配/未就绪不能表现为一块空白面板（状态呈现规格：禁止空表格）。
    logEmpty_ = new QLabel(logView_); logEmpty_->setObjectName("logEmptyState"); logEmpty_->setAlignment(Qt::AlignCenter); logEmpty_->setWordWrap(true); logEmpty_->setTextFormat(Qt::PlainText); logEmpty_->setAttribute(Qt::WA_TransparentForMouseEvents); logEmpty_->hide(); followLogs_=new QPushButton("回到最新"); actions->addWidget(followLogs_); followLogs_->setObjectName("followLatestLogs"); followLogs_->setProperty("quiet",true); followLogs_->setToolTip("滚动到最新记录");
    connect(followLogs_,&QPushButton::clicked,this,[this] { auto cursor=logView_->textCursor(); cursor.clearSelection(); logView_->setTextCursor(cursor); logView_->verticalScrollBar()->setValue(logView_->verticalScrollBar()->maximum()); });
    connect(logView_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value) { const bool history=value<logView_->verticalScrollBar()->maximum(); followLogs_->setEnabled(history); followLogs_->setVisible(history); });
    copy->hide(); clearButton->hide(); followLogs_->hide(); menu->addSeparator(); menu->addAction(clear);
    auto* copyLogAction=menu->addAction("复制选中"); copyLogAction->setEnabled(false); connect(logView_,&QPlainTextEdit::copyAvailable,copyLogAction,&QAction::setEnabled); connect(copyLogAction,&QAction::triggered,copy,&QPushButton::click);
    copy->setEnabled(false); connect(logView_,&QPlainTextEdit::copyAvailable,copy,&QPushButton::setEnabled);
    connect(logSearch_, &QLineEdit::textChanged, this, &MainWindow::updateLogs);
    controller_->setDiagnosticsLevel("all");
    connect(logFilter_, &QComboBox::currentIndexChanged, this, [this] { controller_->setDiagnosticsLevel(logFilter_->currentData().toString()); updateLogs(); });
    connect(pause, &QPushButton::toggled, this, [this, pause](bool paused) { logsPaused_ = paused; pause->setText(paused ? "恢复显示" : "暂停显示"); pausedLogs_ = paused ? controller_->diagnostics()->lines() : QStringList{}; controller_->setDiagnosticsVisible(!paused && isVisible() && route_.view == ViewIds::Data && currentDataTab() == int(ViewIds::DataTab::Logs)); updateLogs(); });
    connect(copy, &QPushButton::clicked, this, [this] { auto text = logView_->textCursor().selectedText(); text.replace(QChar::ParagraphSeparator,'\n'); QApplication::clipboard()->setText(text); });
    connect(clear, &QAction::triggered, controller_, &AppController::clearLogView); connect(directory, &QAction::triggered, this, [this] { openDirectory(controller_->dataRoot() + "/logs"); });
    connect(report, &QAction::triggered, this, [this] { const auto path = QFileDialog::getSaveFileName(this, "导出脱敏诊断（不含订阅配置/连接快照）", QDir::homePath()+"/Mlash-diagnostics.json", "JSON (*.json)"); if (!path.isEmpty()) controller_->exportDiagnostics(path); });
    return page;
}
QWidget* MainWindow::rulesPage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
    auto* back=PrototypeUi::back(layout,"rulesBack","返回配置详情"); connect(back,&QPushButton::clicked,this,[this] {
        if (rulesSaving_) return;
        if (rulesDirty_) { if (QMessageBox::question(this,"未保存的规则","放弃当前规则修改并返回？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return; rulesDirty_=false; loadRuleDraft(); }
        navigate(Route{ViewIds::Profiles}); if (profileDetailId_!=rulesDraftProfile_&&!rulesDraftProfile_.isEmpty()) showProfileDetails(rulesDraftProfile_);
    });
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
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
    auto* box = panel(); auto* content = new QVBoxLayout(box); content->setContentsMargins(12, 10, 12, 10); layout->addWidget(box, 1);
    auto* summary=new QHBoxLayout; connectionStats_=new QLabel("流量 —"); connectionStats_->setObjectName("connectionStats"); connectionStats_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); summary->addWidget(connectionStats_,1);
    auto* next=new QPushButton("前往首页启动"); next->setObjectName("connectionNextStep"); summary->addWidget(next); connect(next,&QPushButton::clicked,this,[this] { setPage(0); });
    connectionPause_=new QPushButton("暂停刷新"); connectionPause_->setObjectName("pauseConnections"); connectionPause_->setCheckable(true); connectionPause_->setToolTip("暂停采样并保留阅读快照；恢复后先建立新基线"); summary->addWidget(connectionPause_); connect(connectionPause_,&QPushButton::clicked,controller_,&AppController::setConnectionsPaused);
    auto* details=new QAction("详情",this); connectionDetailsAction_=details; details->setObjectName("connectionDetailsAction"); auto* copy=new QAction("复制详情",this); connectionCopyAction_=copy; auto* order=new QAction("恢复默认顺序",this); order->setObjectName("defaultConnectionOrder"); order->setToolTip("恢复内核返回的顺序");
    closeAllConnections_=new QAction("断开全部…",this); closeAllConnections_->setObjectName("closeAllConnections"); closeAllConnections_->setToolTip("关闭内核全部活动连接，需要确认"); auto* closeAll=actionButton(closeAllConnections_,summary,"closeAllConnectionsButton"); closeAll->setProperty("danger",true); content->addLayout(summary);
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
    connectionsTable_->setContextMenuPolicy(Qt::CustomContextMenu); connect(connectionsTable_,&QWidget::customContextMenuRequested,this,[this,details,copy](const QPoint& point) { const auto index=connectionsTable_->indexAt(point); if (!index.isValid()) return; connectionsTable_->selectRow(index.row()); QMenu menu(this); menu.addAction(details); menu.addAction(copy); auto* close=menu.addAction("断开选中"); close->setEnabled(closeConnection_->isEnabled()); connect(close,&QAction::triggered,closeConnection_,&QPushButton::click); menu.exec(connectionsTable_->viewport()->mapToGlobal(point)); });
    connect(connectionOptions_,&ConnectionOptions::searchFeedback,this,[this](const QString& error) { connectionSearchError_=error; updateConnections(); }); connect(search,&QLineEdit::textChanged,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::rowsRemoved,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::rowsInserted,this,&MainWindow::updateActions); connect(&connectionsFilter_,&QAbstractItemModel::modelReset,this,&MainWindow::updateActions); connect(connectionsTable_->horizontalHeader(),&QHeaderView::sortIndicatorChanged,this,[this](int,Qt::SortOrder) { updateActions(); }); return page;
}
void MainWindow::connectionDetails(bool copy) {
    const auto selected=connectionsTable_->selectionModel()->selectedRows(); if (selected.isEmpty()) return; const auto text=selected.first().data(Qt::ToolTipRole).toString();
    if (copy) { QApplication::clipboard()->setText(text); return; }
    if (connectionDetail_) connectionDetail_->close(); const auto row=selected.first().data(Qt::UserRole+3).value<QJsonObject>(); const auto profile=controller_->state().activeProfile; QString version; for (const auto& item:controller_->profiles()) if (item.id==profile) version=item.active;
    auto* dialog=new ConnectionDetailsDialog(controller_,row,profile,version,this); connectionDetail_=dialog;
    connect(dialog,&ConnectionDetailsDialog::browseGroup,this,[this,profile,version](const QString& group) { bool current=false; for (const auto& item:controller_->profiles()) if (item.id==profile) current=item.active==version; if (!current||controller_->state().activeProfile!=profile) return; const auto index=profilesCombo_->findData(profile); if (index<0) return; profilesCombo_->setCurrentIndex(index); setPage(2); selectGroup(group); }); connect(dialog,&ConnectionDetailsDialog::createRule,this,&MainWindow::prefillConnectionRule);
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
    for (int row = 0; !selected.isEmpty() && row < connectionsFilter_.rowCount(); ++row) if (connectionsFilter_.index(row,0).data(Qt::UserRole).toString() == selected) { connectionsTable_->selectRow(row); break; }
    connectionsTable_->verticalScrollBar()->setValue(position); updateActions();
    const auto measured=QDateTime::fromString(controller_->connectionSampleTime(),Qt::ISODateWithMs).toLocalTime();
    // 采样状态与时间分开表达：过期数据必须带「已过期」，不能用旧值冒充当前值。
    const auto sampling=paused?QString("已暂停")
        :totals.value("_paused").toBool()?QString("正在恢复…")
        :!error.isEmpty()?(controller_->connectionsStale()?QString("刷新失败 · 显示上次快照"):QString("采样失败"))
        :QString();
    connectionsHint_->setText((connectionSearchError_.isEmpty()?sampling:QString("搜索条件无效"))+(measured.isValid()?(sampling.isEmpty()&&connectionSearchError_.isEmpty()?QString("更新于 "):QString(" · "))+measured.toString("HH:mm:ss"):QString())+(error.isEmpty()?QString():" · "+error)+(connectionFeedback_.isEmpty()?QString():" · "+connectionFeedback_)); connectionsHint_->setToolTip(connectionsHint_->text()+(measured.isValid()?"\n上次成功采样："+measured.toString("yyyy-MM-dd HH:mm:ss t"):QString())+(connectionSearchError_.isEmpty()?QString():"\n"+connectionSearchError_)+"\n采样时间为本机成功接收时间；累计量来自内核。离开快照不代表TCP断开原因，暂停间隙不推断离开，隐藏后清空。");
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
void MainWindow::routeOperationFeedback(bool ok, const QString& message, OperationId id) {
    // 页面级结果落到所属页面，不写全局状态栏（P4.1 反馈路由）。目标页此刻不可见时先把
    // 文案缓存进 pageFeedback_，切回该页即可见，因此不会因为「用户当时不在这页」而丢失。
    const auto target = feedbackTarget(id);
    const auto text = ok ? message : QString("操作未完成 · ") + message;
    switch (id) {
    case OperationId::RulesSave:
        // 规则页的结论行是 rulesHint_（未保存/校验错误/读取失败共用），保存结果写回同一处。
        rulesSaving_ = false;
        if (ok) { rulesDirty_ = false; loadRuleDraft(); rulesHint_->clear(); if (route_.view == ViewIds::Rules) controller_->refreshRules(); }
        else rulesHint_->setText(QString("保存失败，修改已保留 · ") + message);
        break;
    case OperationId::SettingsApply:
        settingsApplying_ = false;
        settingsFeedback_ = ok ? QString() : QString("应用失败：") + message;
        break;
    case OperationId::ProxyToggle:
        // 必须先扩到下标 0 再写：pageFeedback_ 首次使用时是空的，operator[] 越界是未定义行为。
        while (pageFeedback_.size() < 1) pageFeedback_.append(QString());
        pageFeedback_[0] = ok ? QString() : text;
        break;
    default: break;
    }
    if (target.kind == FeedbackTarget::PageHint) {
        setPageFeedback(target.page, ok ? QString() : text);
    } else {
        // 跨页操作：仍进「最近操作」段，成功随时间消失，失败保留。
        message_->setText(statusNoticeText(ok, message)); message_->setToolTip(message);
        feedbackTimer_.stop(); recentFeedback_ = ok; feedbackError_ = ok ? QString() : message; noticeOperation_ = id;
        if (ok) feedbackTimer_.start();
    }
    updateState(); updateActions();
}
void MainWindow::setPageFeedback(int page, const QString& text) {
    if (page < 0) return;
    while (pageFeedback_.size() <= page) pageFeedback_.append(QString());
    pageFeedback_[page] = text;
    if (page == 1) {
        // 订阅页的结果写到提交区旁的提示里（profileSubmitHint），对象说明（profileDetails_）
        // 保持不动，因此「哪次更新失败」不会把那行「当前运行版本 · 有无待应用更新」冲掉。
        if (auto* hint = findChild<QLabel*>("profileSubmitHint")) {
            const bool show = route_.view == ViewIds::Profiles && !text.isEmpty();
            if (show && hint->text() != text) hint->setText(text);
            hint->setVisible(show); hint->setToolTip(text);
        }
    }
    if (page == 6 && settingsDraftHint_) settingsDraftHint_->setToolTip(text.isEmpty() ? QString() : text);
    if (page == 0 && homeControlHint_) {
        homeControlHint_->setText(text); homeControlHint_->setToolTip(text); homeControlHint_->setVisible(!text.isEmpty());
        homeControlHint_->setProperty("state", text.isEmpty() ? "idle" : "error");
        homeControlHint_->setAccessibleName(text.isEmpty() ? QString() : QString("代理开关结果：") + text);
    }
    // 连接页的结论行是 connectionsHint_（R5 已建立），这里只借它多带一句操作结论，
    // 采样状态本身仍由 updateConnections 负责，两者不互相覆盖。
    if (page == 4 && connectionsHint_) { connectionFeedback_ = text; updateConnections(); }
}
void MainWindow::refreshStatusSegments() {
    const auto& state = controller_->state();
    const auto alarm = !state.lastError.isEmpty() ? state.lastError
        : state.readOnly ? QString("只读保护：状态文件不可用，修改不会写入") : QString();
    operationAlarm_->setText(alarm); operationAlarm_->setToolTip(alarm); operationAlarm_->setVisible(!alarm.isEmpty());
    operationAlarm_->setProperty("state", state.readOnly && state.lastError.isEmpty() ? "warning" : "error");
    operationAlarm_->style()->unpolish(operationAlarm_); operationAlarm_->style()->polish(operationAlarm_);
}
void MainWindow::updateState() {
    const auto& state = controller_->state();
    if (statusLine1_ && statusLine2_ && statusHintDot_) {
        const auto sentence = statusSentence(statusFacts(state, controller_->settings(), activeProfileName()));
        statusLine1_->setText(sentence.line1); statusLine2_->setText(sentence.line2);
        statusLine1_->setToolTip(sentence.line1); statusLine2_->setToolTip(sentence.line2);
        // 状态点颜色由 QSS 状态选择器承担，不在代码里拼样式（分层设计 6.3 规则 7）。
        // 形状 + 颜色双通道（P7）：颜色仍由 property 驱动 QSS，形状由 StatusShape 绘制。
        statusHintDot_->setProperty("state", sentence.accent);
        statusHintDot_->setPixmap(StatusShape::pixmap(sentence.accent, statusHintDot_->devicePixelRatioF()));
        statusHintDot_->style()->unpolish(statusHintDot_); statusHintDot_->style()->polish(statusHintDot_);
        statusButton_->setToolTip(sentence.line1 + QChar(10) + sentence.line2);
        statusButton_->setAccessibleDescription(sentence.line1 + " " + sentence.line2);
    }
    if (state.core == CoreState::Starting) model_.clearDelays();
    // 首页控制区的当前代理行只读观测：系统代理归属 + TUN 观测（不含意图字段），
    // 内核状态由状态句 homeStateLine_ 承载，避免同一事实三处。coreLabel_ 已随 H1 删除
    // （内核状态只剩状态句一处），这里不再写它。
    proxyLabel_->setText("系统代理：" + (state.proxy==ProxyState::External?QString("由其他程序管理"):proxyStateText(state.proxy)) + " · TUN：" + (state.tunObserved ? QString("已接管") : QString("未接管")));
    QString title = state.ready ? "内核运行中" : state.core==CoreState::Running ? "正在确认内核" : "内核已停止";
    if (state.core == CoreState::Starting) title = "正在启动内核…";
    if (state.core == CoreState::Failed) title = "内核启动或运行失败";
    if (state.core==CoreState::Stopping) title="正在停止内核…";
    overviewTitle_->setText(title);
    // 首页状态点的颜色与字形同样交给 QSS：代码只声明状态，不写颜色与字号。
    const QString dotState = state.core == CoreState::Failed ? QString("error")
        : state.core == CoreState::Starting || state.core == CoreState::Stopping || state.proxy == ProxyState::Unknown ? QString("warning")
        : state.ready && (state.proxy == ProxyState::Owned || state.tunObserved) ? QString("success")
        : state.ready ? QString("active") : QString("idle");
    statusDot_->setProperty("state", dotState);
    statusDot_->setPixmap(StatusShape::pixmap(dotState, statusDot_->devicePixelRatioF()));
    statusDot_->style()->unpolish(statusDot_); statusDot_->style()->polish(statusDot_);
    statusDot_->setToolTip(title); statusDot_->setAccessibleName(StatusShape::accessibleName(dotState));
    for (auto* widget : mutations_) widget->setEnabled(!state.busy && !state.exiting && !state.readOnly);
    nodeList_->setEnabled(!state.exiting); nodeCards_->setEnabled(!state.exiting);
    // 首页 TUN 开关已移除（H1，回归 ui-layered-design.md:169「TUN 开关不放首页」）：
    // TUN 的观测结果在状态句里报告，启用入口在「设置 › 网络与内核」。
    proxyEnabled_->setEnabled(!state.busy && (state.ready || state.proxy == ProxyState::Owned));
    { const QSignalBlocker block(proxyEnabled_); proxyEnabled_->setChecked(state.proxy == ProxyState::Owned || state.tunObserved); }
    proxyEnabled_->setToolTip("代理总开关：接管系统代理（必要时先启动内核）；只影响遵循系统代理设置的应用。外部代理/PAC 需要确认接管。");
    autoStart_->setToolTip(!state.autoStartKnown ? "系统自启状态无法确认；应用设置时重新读取" : state.autoStartConflict ? "系统自启项指向其他路径，修改会被拒绝" : state.autoStartEnabled ? "系统当前已启用 Mlash 自启；勾选修改后点击应用" : "系统当前未启用 Mlash 自启；勾选修改后点击应用");
    for (auto* control : proxyModes_) { const QSignalBlocker block(control); control->setChecked(control->property("mode").toString() == state.mode); control->setEnabled(state.ready && !state.busy && !state.exiting && !state.readOnly); }
    refreshStatusSegments();
    // 「最近操作」段不再被 lastError 覆盖：告警有自己的段（P4.1 / N10）。
    if (feedbackError_.isEmpty() && !recentFeedback_ && noticeOperation_ == OperationId::Unspecified && !state.busy) { message_->clear(); message_->setToolTip({}); }
    findChild<QToolButton*>("statusDetails")->setVisible(!message_->text().isEmpty());
    findChild<QToolButton*>("statusResolve")->setVisible(!feedbackError_.isEmpty() || !state.lastError.isEmpty());
    updateActions(); updateCurrentMember(); updateHome();
}
void MainWindow::updateActions() {
    // updateActions 依赖所有页面的控件：页面全部构造完成（updateState 的首次调用）之后才可能
    // 有意义地被调用。构造期任何提前刷新都在这里被挡住，避免半构造状态下的空指针。
    if (!saveRules_ || !rulesEditor_ || !nodeList_ || !connectionsTable_) return;
    // 构造期 updateState() 会调用这里，而订阅/设置页此时可能还没建出来：所有查找都判空，
    // 让「哪些控件存在」不成为初始化顺序的隐含依赖（H3 删掉表格视图后暴露过一次空指针崩溃）。
    const auto enable=[this](const char* name, bool on) { if (auto* action=findChild<QAction*>(name)) action->setEnabled(on); else if (auto* button=findChild<QPushButton*>(name)) button->setEnabled(on); };
    {
        auto* statusDetails=findChild<QToolButton*>("statusDetails");
        if (statusDetails) statusDetails->setVisible(!message_->text().isEmpty());
    }
    const auto& state = controller_->state(); const bool idle = !state.busy && !state.exiting && !state.readOnly;
    enable("exportDiagnostics", !state.busy&&!state.exiting);
    enable("nodePreferences", idle&&!selectedGroup_.isEmpty()&&profilesCombo_->currentData().toString()==state.activeProfile);
    enable("ruleProviders", !state.busy&&!state.exiting&&!rulesProfiles_->currentData().toString().isEmpty());
    if (auto* objectActions=findChild<QWidget*>("profileObjectActions")) objectActions->setVisible(!selectedProfile().isEmpty());
    if (auto* batch=findChild<QPushButton*>("batchProfiles")) batch->setVisible(!controller_->profiles().isEmpty());
    const auto draftError=ruleDraftError();
if (saveRules_)     saveRules_->setEnabled(idle && !rulesProfiles_->currentData().toString().isEmpty() && rulesDirty_ && draftError.isEmpty());
    saveRules_->setToolTip(!draftError.isEmpty()?draftError:rulesDirty_?"校验并保存所选订阅的规则":"规则没有未保存修改");
    enable("resetCustomRules", !rulesSaving_ && !state.exiting && rulesDirty_);
    rulesEditor_->setReadOnly(rulesSaving_ || state.readOnly || rulesProfiles_->currentData().toString().isEmpty());
    const bool ruleDraftAvailable=!rulesProfiles_->currentData().toString().isEmpty() && !rulesSaving_ && !state.exiting && !state.readOnly;
    auto* quickRules=findChild<QTableWidget*>("customRulesList"); if (!quickRules) return; quickRules->setEnabled(!rulesSaving_ && !state.exiting);
    const int quickRow=quickRules->currentRow(); const bool canEditQuick=!rulesEditor_->isReadOnly()&&!rulesSaving_;
    if (auto* control=findChild<QPushButton*>("removeQuickRule")) control->setEnabled(canEditQuick&&quickRow>=0);
    if (auto* control=findChild<QPushButton*>("moveQuickRuleUp")) control->setEnabled(canEditQuick&&quickRow>0);
    if (auto* control=findChild<QPushButton*>("moveQuickRuleDown")) control->setEnabled(canEditQuick&&quickRow>=0&&quickRow+1<quickRules->rowCount());
if (rulesProfiles_)     rulesProfiles_->setEnabled(!rulesSaving_ && !state.exiting && rulesProfiles_->count()>0);
    if (auto* control=findChild<QComboBox*>("ruleType")) control->setEnabled(ruleDraftAvailable); ruleTarget_->setEnabled(ruleDraftAvailable); enable("addCustomRule", ruleDraftAvailable);
    enable("advancedRules", ruleDraftAvailable && !state.busy);
    if (auto* match=findChild<QLineEdit*>("ruleMatch")) match->setEnabled(ruleDraftAvailable && findChild<QComboBox*>("ruleType")->currentData().toString()!="MATCH");
    if (auto* next=findChild<QPushButton*>("connectionNextStep")) next->setVisible(!state.ready);
    const bool actualRules=rulesTabs_->currentIndex()==1;
    if (rulesProfiles_) rulesProfiles_->setVisible(!actualRules); if (auto* label=findChild<QLabel*>("rulesProfileLabel")) label->setVisible(!actualRules); if (auto* providers=findChild<QPushButton*>("ruleProviders")) providers->setVisible(!actualRules);
    auto* runtimeContext=findChild<QLabel*>("rulesRuntimeContext"); runtimeContext->setVisible(actualRules); QString runtimeName; for (const auto& profile:controller_->profiles()) if (profile.id==state.activeProfile) runtimeName=profile.name;
    runtimeContext->setText(runtimeName.isEmpty()?QString("没有正在运行的订阅"):"当前运行："+runtimeName); runtimeContext->setToolTip(runtimeContext->text());
    if (auto* next=findChild<QPushButton*>("rulesNextStep")) { next->setVisible(actualRules?!state.ready:rulesProfiles_->count()==0); next->setText(actualRules?"前往首页启动":"前往订阅"); }
        // 内核实际规则的五种可见结论彼此区分：未就绪 / 读取中 / 读取失败 / 无匹配 / 已就绪但为零。
    {
        auto* runtimeEmpty = findChild<QLabel*>("rulesRuntimeEmpty");
        const auto& rulesError = controller_->rulesError();
        QString text;
        if (!state.ready) text = QString("内核未就绪");
        else if (!rulesError.isEmpty()) text = QString("读取内核规则失败：") + rulesError;
        else if (!controller_->rulesLoaded()) text = QString("正在读取内核规则…");
        else if (rulesFilter_.rowCount() > 0) text = QString();
        else if (controller_->activeRules().isEmpty()) text = QString("内核已就绪，当前没有规则");
        else text = QString("没有匹配的内核规则") + QChar(10) + QString("请调整检索条件");
        // 读取中不只是一句文字：在表格视口里放三行骨架块，让「即将出现内容」与
        // 「读到了但没有」在形状上就不一样（V1）。其余四种结论仍用文字。
        const bool loading = state.ready && rulesError.isEmpty() && !controller_->rulesLoaded();
        if (auto* table = findChild<QTableView*>("activeRulesTable")) {
            if (!rulesSkeleton_) {
                rulesSkeleton_ = new SkeletonBlock(3, table->viewport());
                rulesSkeleton_->setObjectName("rulesSkeleton");
                rulesSkeleton_->setGeometry(16, 12, qMax(120, table->viewport()->width() - 32), rulesSkeleton_->height());
            }
            rulesSkeleton_->setGeometry(16, 12, qMax(120, table->viewport()->width() - 32), rulesSkeleton_->height());
if (rulesSkeleton_)             rulesSkeleton_->setVisible(loading);
        }
        runtimeEmpty->setVisible(!text.isEmpty()); runtimeEmpty->setText(text);
        const bool failed = !rulesError.isEmpty() && state.ready;
        setEmptyStateAction(findChild<QTableView*>("activeRulesTable"), failed ? QString("重试") : rulesFilter_.rowCount() == 0 ? QString("清除检索") : QString(),
                            [this, failed] { if (failed) controller_->refreshRules(); else if (auto* search = findChild<QLineEdit*>("activeRulesSearch")) search->clear(); });
    }
    auto* quickEmpty=quickRules->findChild<QLabel*>("emptyState"); quickEmpty->setText(rulesProfiles_->count()==0?"请先添加或选择订阅":"还没有优先规则\n在上方添加，或打开完整规则编辑");
    const int networkChanges=networkSettingCount();
    if (applySettings_) { applySettings_->setEnabled(idle && networkChanges>0); }
    if (applySettings_) { const bool urlOnly=networkChanges==1&&testUrl_->text().trimmed()!=controller_->settings().testUrl; applySettings_->setText(urlOnly?QString("保存设置"):QString("保存并重启")); }
    if (resetSettings_) { resetSettings_->setEnabled(!state.exiting && networkChanges>0); }
    // K8：动作条整体按需出现——没有重启类改动时不留一条空带（原型设置页没有常驻底栏）。
    if (auto* bar=findChild<QWidget*>("settingsActions")) bar->setVisible(settingsTask_=="network"||settingsTask_=="core"||networkChanges>0||!settingsFeedback_.isEmpty()||settingsApplying_);
    if (settingsDraftHint_) settingsDraftHint_->setText(settingsApplying_?"正在校验并应用…":!settingsFeedback_.isEmpty()?settingsFeedback_:networkChanges>0?QString("有 %1 项未保存的网络设置").arg(networkChanges):QString()); settingsDraftHint_->setToolTip(settingsDraftHint_->text());
    enable("importGeoData", idle); enable("previewActiveConfig", !state.busy && !state.exiting && !state.activeProfile.isEmpty());
    if (cancelTestAction_) cancelTestAction_->setEnabled(controller_->testingNodes());
    if (nodeEmpty_) nodeEmpty_->setVisible(filter_.rowCount()==0);
    if (nodeCardsEmpty_) nodeCardsEmpty_->setVisible(filter_.rowCount()==0); nodeEmpty_->setText(model_.rowCount()>0 ? "没有匹配的节点\n请调整搜索条件" : controller_->profiles().isEmpty()?"暂无节点\n请先添加订阅":state.activeProfile.isEmpty()?"请先使用订阅":!state.ready ? "内核未就绪" : "使用所选订阅后，选择策略组查看节点"); nodeCardsEmpty_->setText(nodeEmpty_->text());

    // 空态动作随原因变化：无匹配清筛选、无订阅去添加、有订阅未使用去选择、其余重试（R7）。
    {
        const bool noMatch = model_.rowCount() > 0, noProfiles = controller_->profiles().isEmpty();
        const QString text = filter_.rowCount()>0 ? QString() : noMatch ? QString("清除搜索") : noProfiles ? QString("添加订阅…") : state.activeProfile.isEmpty() ? QString("选择订阅") : QString("重试");
        const auto run = [this, noMatch, noProfiles] {
            if (noMatch) search_->clear();
            else if (noProfiles) addUrl();
            else if (controller_->state().activeProfile.isEmpty()) navigate(Route{ViewIds::Profiles});
            else controller_->refresh();
        };
        setEmptyStateAction(nodeList_, text, run);
        setEmptyStateAction(nodeCards_, text, run);
    }    const auto findProfile = [this](const QString& id) -> const Profile* { for (const auto& p : controller_->profiles()) if (p.id == id) return &p; return nullptr; };
    const auto* selectedSource = findProfile(profilesCombo_->currentData().toString());
    profilesCombo_->setEnabled(!state.exiting && !state.busy);
    const bool viewingActive = selectedSource && selectedSource->id == state.activeProfile;
    const bool pending=selectedSource && selectedSource->candidate!=selectedSource->active;
    profilesCombo_->setToolTip(selectedSource ? (viewingActive ? QString("正在使用：") : QString("选中即使用：")) + selectedSource->name + (pending ? QString("\n有更新待应用；更新下载不会自动替换当前运行版本") : QString()) : QString("先添加订阅"));
if (nodeList_)     nodeList_->setEnabled(!state.exiting); nodeCards_->setEnabled(!state.exiting);
    // 代理总开关（ADR 0048）：文案必须带动词与结果，执行中给出进行时；失败不在开关上虚报成功。
    // 内核启停不再是首页主控件，它只出现在「更多运行信息」与托盘。
    if (proxyMasterSwitch_) {
        const bool on = state.proxy == ProxyState::Owned || state.tunObserved;
        const QString text = state.busy && proxyMasterSwitch_->property("pending").toBool()
            ? (on ? QString("正在关闭…") : QString("正在开启…")) : (on ? QString("关闭代理") : QString("开启代理"));
        if (proxyMasterSwitch_->text() != text) proxyMasterSwitch_->setText(text);
        const bool usable = !state.busy && !state.exiting && !state.readOnly && !state.activeProfile.isEmpty();
if (proxyMasterSwitch_)         proxyMasterSwitch_->setEnabled(usable);
        proxyMasterSwitch_->setToolTip(on ? "撤销系统代理与 TUN 接管；内核继续运行。"
                                         : "让设置允许的接管方式生效；内核未运行时会先启动内核。");
        proxyMasterSwitch_->setAccessibleDescription(statusSentence(statusFacts(state, controller_->settings(), activeProfileName())).line1);
        if (!state.busy) proxyMasterSwitch_->setProperty("pending", false);
    }
    const auto group = controller_->proxies().value(selectedGroup_).toObject(); const auto type = group.value("type").toString();
    // 「选中了哪个成员」取决于当前显示的视图：行列表看 NodeRowModel 的成员行（组头不算），
    // 卡片视图看过滤模型的当前项。两者共用同一套动作启用条件。
    const auto selectedName = memberView() == 1
        ? nodeCards_->currentIndex().data(Qt::UserRole).toString()
        : (nodeList_->currentIndex().isValid() ? nodeRows_->memberAt(nodeList_->currentIndex().row()) : QString());
    const bool hasNode = !selectedName.isEmpty();
    if (backGroup_) { backGroup_->setVisible(!groupHistory_.isEmpty()); backGroup_->setEnabled(!state.exiting && viewingActive); }
    // 行列表里选中的是子策略组时才能进入；顶层成员没有下一层。
    if (enterGroup_) { const bool groupSelected=!selectedName.isEmpty() && controller_->proxies().value(selectedName).toObject().value("all").isArray(); enterGroup_->setVisible(groupSelected); enterGroup_->setEnabled(state.ready && viewingActive && groupSelected && !state.exiting); }
    // 提交条已取消（ADR 0048）：手动组单击/Enter 即生效，自动组行内不提供切换。
    if (testAction_) testAction_->setEnabled(idle && state.ready && viewingActive && model_.rowCount() > 0); if (testSelectedAction_) testSelectedAction_->setEnabled(idle && state.ready && viewingActive && hasNode);
    static_cast<NodeCardView*>(nodeCards_)->setTestEnabled(testAction_->isEnabled());
    static_cast<NodeRowView*>(nodeList_)->setTestEnabled(testAction_->isEnabled());
    if (automaticAction_) automaticAction_->setEnabled(idle && state.ready && viewingActive && !group.value("fixed").toString().isEmpty() && QStringList{"URLTest", "Fallback"}.contains(type)); automaticAction_->setText(group.value("fixed").toString().isEmpty()?"自动选择中":"恢复自动选择");
    if (automaticAction_) automaticAction_->setVisible(QStringList{"URLTest", "Fallback"}.contains(type));
    if (locateAction_) locateAction_->setEnabled(state.ready && viewingActive && !group.value("now").toString().isEmpty());
    const auto* profile = findProfile(selectedProfile()); const bool single=selectedProfileIds().size()<=1; const bool valid = idle && profile && single;
    const bool current=profile && profile->id==state.activeProfile, newVersion=profile && profile->candidate!=profile->active;
    enable("profileChanges", profile && !state.exiting); enable("profilePolicyButton", valid); enable("profileManage", profile&&!state.exiting);
    enable("activateProfile", valid && (!current || newVersion || !state.ready));
    enable("reloadActiveProfile", valid && current && !newVersion);
    enable("rollbackProfile", valid && profile && !profile->lastGood.isEmpty());
    enable("renameProfile", valid);
    enable("deleteProfile", valid);
    enable("profileUpdatePolicy", valid);
    if (auto* action = findChild<QAction*>("replaceProfileSource")) action->setEnabled(valid && profile->sourceType == "file");
    // K7：配置页的动作全在行详情抽屉里，顶部只剩三个与选择无关的按钮。
    if (auto* batch = findChild<QPushButton*>("batchProfiles")) batch->setEnabled(!controller_->profiles().isEmpty() && !state.busy && !state.exiting && !state.readOnly);
    // 页面级反馈（P4.1）：订阅页最近一次更新/激活/批量更新的结论。空串表示没有待说明的结果。
    if (auto* hint = findChild<QLabel*>("profileSubmitHint"); hint && pageFeedback_.size() > 1) {
        const bool show = route_.view == ViewIds::Profiles && !pageFeedback_[1].isEmpty();
        if (hint->isVisible() != show) hint->setVisible(show);
        if (show && hint->text() != pageFeedback_[1]) hint->setText(pageFeedback_[1]);
    }
    // 首页控制区同理：代理总开关/路由方式/节点切换的失败结论。
    if (homeControlHint_) {
        const bool show = !pageFeedback_.isEmpty() && !pageFeedback_[0].isEmpty();
if (homeControlHint_)         homeControlHint_->setVisible(show);
        if (show && homeControlHint_->text() != pageFeedback_[0]) { homeControlHint_->setText(pageFeedback_[0]); homeControlHint_->setToolTip(pageFeedback_[0]); }
    }
    bool hasRecent = false;
    if (const auto* activeProfile = findProfile(state.activeProfile)) for (const auto& value : activeProfile->recentSelections) { const auto pair = value.toObject(); if (pair.value("group").toString() == selectedGroup_ && group.value("all").toArray().contains(pair.value("member"))) hasRecent = true; }
    if (recentNodes_) recentNodes_->setEnabled(idle && state.ready && viewingActive && hasRecent && QStringList{"Selector","URLTest","Fallback"}.contains(type));
    if (closeConnection_) closeConnection_->setEnabled(idle && state.ready && controller_->connectionsCurrent() && connectionView_->currentIndex()==0 && !connectionsTable_->selectionModel()->selectedRows().isEmpty());
    const bool connectionSelected=!connectionsTable_->selectionModel()->selectedRows().isEmpty();
    if (connectionDetailsAction_) connectionDetailsAction_->setEnabled(connectionSelected);
    if (connectionCopyAction_) connectionCopyAction_->setEnabled(connectionSelected);
        if (closeAllConnections_) closeAllConnections_->setEnabled(idle && state.ready && controller_->connectionsCurrent() && !controller_->connections().isEmpty());
    if (connectionPause_) connectionPause_->setEnabled(!state.exiting&&state.ready&&!state.busy&&controller_->connectionsVisible());
    if (connectionEmpty_) connectionEmpty_->setVisible(connectionsFilter_.rowCount()==0);
    connectionEmpty_->setText(!controller_->connectionError().isEmpty()
        ? (controller_->connectionsStale() ? QString("刷新失败 · 下方保留上次快照") + QChar(10) + QString("详情见状态栏或日志页") : QString("连接读取失败"))
        : !state.ready ? QString("内核未就绪")
        : connectionsModel_.rowCount()>0 ? QString("没有匹配的连接") + QChar(10) + QString("请调整搜索条件")
        : connectionView_->currentIndex()==1 ? QString("内核已就绪，暂无已结束的连接")
        : controller_->connectionsPaused() ? QString("已暂停") + QChar(10) + QString("恢复刷新后继续采样")
        : !controller_->connectionsCurrent() ? QString("正在读取连接…")
        : QString("内核已就绪，暂无活动连接"));
    // 连接空态：失败给「查看日志」，无匹配给「清除筛选」，未就绪说明缺什么（R7）。
    {
        const bool failed = !controller_->connectionError().isEmpty(), noMatch = connectionsModel_.rowCount() > 0;
        const QString text = connectionsFilter_.rowCount()>0 ? QString() : failed ? QString("查看日志") : noMatch ? QString("清除筛选") : !state.ready ? QString("启动内核") : QString("刷新");
        setEmptyStateAction(connectionsTable_, text, [this, failed, noMatch] {
            if (failed) navigate(Route{ViewIds::Data, kNoCategory, HomeAnchor::Control, int(ViewIds::DataTab::Logs)});
            else if (noMatch) connectionsFilter_.setFilterFixedString({});
            else if (!controller_->state().ready) { const auto& st = controller_->state(); if (st.activeProfile.isEmpty()) navigate(Route{ViewIds::Profiles}); else controller_->startCore(); }
            else controller_->refresh();
        });
    }}
bool MainWindow::logMatches(const QString& line) const { return line.contains(logSearch_->text(),Qt::CaseInsensitive) && (logFilter_->currentData().toString() == "all" || Diagnostics::levelOf(line) == logFilter_->currentData().toString()); }
void MainWindow::updateLogs() {
    const auto& all = logsPaused_ ? pausedLogs_ : controller_->diagnostics()->lines();
    QStringList filtered; for (const auto& line : all) if (logMatches(line)) filtered.append(line);
    if (logEmpty_) {
        QString reason;
        if (!controller_->state().ready) reason = QString("内核未就绪") + QChar(10) + QString("启动内核后才会收到日志");
        else if (filtered.isEmpty() && !all.isEmpty()) reason = QString("没有匹配的日志") + QChar(10) + QString("请调整搜索或级别");
        else if (filtered.isEmpty() && logsPaused_) reason = QString("已暂停") + QChar(10) + QString("恢复显示后继续接收");
        else if (filtered.isEmpty()) reason = QString("还没有日志");
if (logEmpty_)         logEmpty_->setText(reason);
if (logEmpty_)         logEmpty_->setVisible(!reason.isEmpty());
        if (!reason.isEmpty()) {
            logEmpty_->setGeometry(QRect(QPoint(0, 0), logView_->viewport()->size()));
            logEmpty_->raise();
        }
    }
    if (filtered==displayedLogLines_) return;
    bool prefix=filtered.size()>=displayedLogLines_.size();
    for (int i=0;prefix && i<displayedLogLines_.size();++i) prefix=filtered[i]==displayedLogLines_[i];
    if (prefix) { const int before=displayedLogLines_.size(); for (int i=before;i<filtered.size();++i) appendLogLine(filtered[i]); return; }
    const auto text=filtered.join('\n'); displayedLogLines_=filtered;
    auto* scroll=logView_->verticalScrollBar(); const int position=scroll->value(); const bool follow=position==scroll->maximum() && !logView_->textCursor().hasSelection();
    logView_->setPlainText(text); scroll->setValue(follow?scroll->maximum():position);
    if (followLogs_) { const bool history=scroll->value()<scroll->maximum(); followLogs_->setEnabled(history); followLogs_->setVisible(history); }
}
void MainWindow::appendLogLine(const QString& line) {
    displayedLogLines_.append(line); if (displayedLogLines_.size()>2000) displayedLogLines_.removeFirst();
    auto* scroll=logView_->verticalScrollBar(); const int position=scroll->value();
    const bool follow=position==scroll->maximum() && !logView_->textCursor().hasSelection();
    auto cursor=QTextCursor(logView_->document()); cursor.movePosition(QTextCursor::End);
    if (!logView_->document()->isEmpty()) cursor.insertBlock();
    cursor.insertText(line);
    scroll->setValue(follow?scroll->maximum():position);
    if (followLogs_) { const bool history=scroll->value()<scroll->maximum(); followLogs_->setEnabled(history); followLogs_->setVisible(history); }
}
void MainWindow::syncSectionNavigation(const Route& route) {
    // 侧栏只有四个入口（首页 / 配置 / 数据 / 设置），规则页仍可由托盘与设置搜索进入，
    // 此时侧栏保持上一条可归属的分区高亮，不额外造第五个入口。
    const int row = route.view == ViewIds::Rules ? ViewIds::Profiles : route.view;
    { const QSignalBlocker block(navigation_);
      for (int i = 0; i < navigation_->count(); ++i) if (navigation_->item(i)->data(Qt::UserRole).toInt() == row) { navigation_->setCurrentRow(i); break; } }
    if (dataTabs_) {
        const QSignalBlocker block(dataTabs_);
        const int tab = route.view == ViewIds::Data ? currentDataTab() : dataTabs_->currentIndex();
        if (tab >= 0 && tab < dataTabs_->count() && dataTabs_->currentIndex() != tab) dataTabs_->setCurrentIndex(tab);
    }
    updateTopbar();
    updateWindowTitle();
}
// 视图 ID 与页面栈下标一一对应（K 版五个视图）。保留成函数是为了让所有调用点
// 继续经过同一处映射；不再有偏移。
int MainWindow::stackIndex(int view) { return view; }
void MainWindow::navigate(Route route) {
    // 旧编号只允许出现在 setPage()/GuiHost 这类整数入口，那里已经过 fromLegacyPage()。
    // 这里不能再按 view==2 猜测「首页节点」：K 版起 2 是规则视图，按数字归一化会把规则页吞掉。
    if (!isRoutableView(route)) return;
    if (contentStack_) contentStack_->setCurrentIndex(0);
    if (route.view==ViewIds::Profiles&&profileStack_) { profileStack_->setCurrentIndex(0); profileDetailId_.clear(); }
    if (route.view!=route_.view) if (auto* drawer=qobject_cast<DetailsDrawer*>(detailsDrawer_.data())) drawer->closeDrawer();
    // 设置分类只在调用方显式给出时应用；kNoCategory 表示沿用当前分类。
    // K8 起分类不再是页签：域内只把视野滚到对应分组，窗口标题仍带分类名。
    if (route.view == ViewIds::Settings) {
        // kNoCategory 表示「沿用当前分类」：路由里留下的是实际生效的分类，
        // 否则同一视图内的下一次导航会把视野拨回顶部，标题也会丢掉分类名。
        if (route.category<0 && settingsStack_) { settingsStack_->setCurrentIndex(0); if (!settingsDirty()) settingsTask_.clear(); }
        if (route.category >= 0 && route.category < settingsSections_.size()) {
            selectSettingsCategory(route.category);
            // 分组都在同一个滚动面里，几何要等这一轮布局结束才有效：
            // 离屏时同步读到的卡片偏移是旧布局（顶边 0），滚完再定位一次。
            const int category = route.category;
            QTimer::singleShot(0, this, [this, category] { ensureSettingsGroupVisible(category); });
        }
    }
    // 数据页页签：未指定时沿用当前页签；离开数据页后仍记住上次停在哪一页。
    if (route.view == ViewIds::Data && dataTabs_) {
        const int tab = route.tab >= 0 && route.tab < dataTabs_->count() ? route.tab : dataTabs_->currentIndex();
        route.tab = tab;
        if (dataPanels_) dataPanels_->setCurrentIndex(tab);
        { const QSignalBlocker block(dataTabs_); dataTabs_->setCurrentIndex(tab); }
    }
    route_ = route;

    if (route.view != ViewIds::Data) {
        // K6：四个页签的「正在采样」状态必须随视图一起收尾，否则从旧入口
        // （5/7）切走后，连接或日志采集会留在后台继续跑。
        if (connectionDetail_) connectionDetail_->close();
        if (dataPanels_) updateDataPanelVisibility();
    }
    if (route.view != ViewIds::Settings && configPreview_) configPreview_->close();
    if (outboundDialog_) outboundDialog_->close();
    pages_->setCurrentIndex(stackIndex(route.view));
    syncSectionNavigation(route);
    // 页面副作用集中在各页对象的 onEnter/onLeave（S1），这里只按顺序触发。
    pageRouter_.go(route);
}
void MainWindow::enterView(int view, const Route& route) {
    if (contentStack_&&contentStack_->currentIndex()!=0) return;
    // 进入某视图时的全部副作用都在这里；离开时的清理在 leaveView。
    // 两个函数合起来就是设计文档的「页面生命周期契约」表，逐行对应。
    switch (view) {
    case ViewIds::Home:   // 首页 / 节点：刷新运行状态，可见时启动 8 秒轮询
        updateHome();
        controller_->setHomeVisible(isVisible());
        controller_->refresh();
        if (isVisible()) refreshTimer_.start(8000);
        applyHomeAnchor(route.anchor);
        break;
    case ViewIds::Rules:  // 规则：草稿未脏时重载；「内核实际规则」分段还要刷新
        controller_->setRulesVisible(isVisible() && rulesTabs_->currentIndex() == 1);
        if (!rulesDirty_ && isVisible()) loadRuleDraft();
        if (rulesTabs_->currentIndex() == 1) controller_->refreshRules();
        break;
    case ViewIds::Data:   // 数据：四个页签各自决定采样、连接、日志、测试的开关
        // K6：页签状态由路由决定，先把面板与前缀页签对齐，再算各面板的开关，
        // 否则从旧入口（5/7）进来时会用上一次停留的页签去算可见性。
        if (dataTabs_ && dataPanels_) {
            const int tab = currentDataTab();
            if (tab >= 0 && tab < dataTabs_->count()) {
                dataPanels_->setCurrentIndex(tab);
                if (dataTabs_->currentIndex() != tab) { const QSignalBlocker block(dataTabs_); dataTabs_->setCurrentIndex(tab); }
            }
        }
        updateDataPanelVisibility();
        updateConnections();
        updateLogs();
        updateHome();
        if (dataTabs_ && dataTabs_->currentIndex() == int(ViewIds::DataTab::Tests)) websitePage_->setActive(isVisible(), selectedGroup_);
        if (dataTabs_ && dataTabs_->currentIndex() == int(ViewIds::DataTab::Logs)) controller_->setDiagnosticsVisible(isVisible() && !logsPaused_);
        break;
    default:
        break;
    }
}
void MainWindow::leaveView(int view) {
    switch (view) {
    case ViewIds::Home:
        refreshTimer_.stop();
        controller_->setHomeVisible(false);
        break;
    case ViewIds::Rules:
        controller_->setRulesVisible(false);
        break;
    case ViewIds::Data:
        // 离开数据页时四个面板全部停用；页签选择保留在 dataTabs_ 里。
        controller_->setHomeVisible(false);
        controller_->setConnectionsVisible(false);
        controller_->setDiagnosticsVisible(false);
        websitePage_->setActive(false, selectedGroup_);
        break;
    default:
        break;
    }
}
QWidget* MainWindow::showTool(const QString& tool, const QString& objectKey, const std::function<QWidget*()>& factory) {
    // 唯一入口（S2）：同一「工具+对象」第二次打开时 raise 已有窗口，不再新建。
    // 单例工具把 objectKey 留空。
    auto* workspace=taskWorkspace();
    if (!workspace->creating()) {
        if (route_.view==ViewIds::Settings&&(tool=="backup"||tool=="local-backup"||tool=="remote-backup")) {
            const auto task=tool=="backup"?objectKey:tool=="local-backup"?QString("plan"):QString("webdav");
            openBackupTask(task); return taskWorkspace()->editor("backup:"+task);
        }
        const QHash<QString,QString> profileTasks{{"dns","dns"},{"proxy-editor","nodes"},{"proxy-chain","chain"}};
        if (route_.view==ViewIds::Profiles&&profileTasks.contains(tool)&&!objectKey.isEmpty()) {
            openProfileTask(objectKey,profileTasks.value(tool)); return taskWorkspace()->editor(profileTasks.value(tool));
        }
    }
    if (route_.view==ViewIds::Settings&&!workspace->creating()) workspace=taskWorkspaceFor(tool);
    const auto key=tool+":"+objectKey;
    if (auto* existing=workspace->editor(key)) { workspace->select(key); showWorkspace(workspace); return existing; }
    auto* widget=toolWindows_.show(tool,objectKey,factory);
    if (auto* dialog=qobject_cast<QDialog*>(widget)) presentTask(dialog,key);
    return widget;
}
void MainWindow::setPage(int index) { navigate(fromLegacyPage(index)); }
Route MainWindow::currentRoute() const { return route_; }
void MainWindow::applyHomeAnchor(HomeAnchor anchor) {
    if (anchor != HomeAnchor::Nodes || !homeScroll_ || !nodeSection_) return;
    // 节点区已并入首页（ADR 0048）：入口 2 把共用的滚动视图带到节点区首行。
    // 布局尚未完成时 ensureWidgetVisible 会算错位置，所以等一轮事件循环。
    const auto scroll = qobject_cast<QScrollArea*>(homeScroll_.data());
    const QPointer<QWidget> section = nodeSection_;
    QTimer::singleShot(0, this, [scroll, section] { if (scroll && section) scroll->ensureWidgetVisible(section, 0, 0); });
}
void MainWindow::showOutboundDiagnostics() { showTool("outbound",{},[this] { outboundDialog_=new OutboundDialog(controller_,this); return outboundDialog_.data(); }); }
TaskWorkspace* MainWindow::taskWorkspace() {
    const int domain=route_.view==ViewIds::Rules?ViewIds::Profiles:route_.view;
    if (auto* workspace=workspaces_.value(domain)) return workspace;
    return taskWorkspaceFor("default");
}
TaskWorkspace* MainWindow::taskWorkspaceFor(const QString& family) {
    const int domain=route_.view==ViewIds::Rules?ViewIds::Profiles:route_.view;
    const auto key=QString::number(domain)+":"+family;
    if (auto* workspace=taskFamilies_.value(key)) { workspaces_[domain]=workspace; return workspace; }
    auto* workspace=new TaskWorkspace; workspaces_[domain]=workspace; taskFamilies_[key]=workspace; contentStack_->addWidget(workspace);
    workspace->configure(domain==ViewIds::Settings?QString("settings"):QString("section-%1").arg(domain),ViewIds::title(domain),{},[this] { returnFromWorkspace(); });
    workspace->requested=[this,workspace] { showWorkspace(workspace); };
    return workspace;
}
void MainWindow::showWorkspace(TaskWorkspace* workspace) {
    if (auto* drawer=qobject_cast<DetailsDrawer*>(detailsDrawer_.data())) drawer->closeDrawer();
    contentStack_->setCurrentWidget(workspace);
    refreshTimer_.stop(); controller_->setHomeVisible(false); controller_->setRulesVisible(false);
    controller_->setConnectionsVisible(false); controller_->setDiagnosticsVisible(false); websitePage_->setActive(false);
}
void MainWindow::returnFromWorkspace() {
    if (outboundDialog_) outboundDialog_->close();
    contentStack_->setCurrentIndex(0); enterView(route_.view,route_);
}
void MainWindow::presentTask(QDialog* editor,const QString& key,const QString& title) {
    auto* workspace=taskWorkspace();
    workspace->mount(editor,key,title);
}
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
int MainWindow::page() const { return toLegacyPage(route_); }
void MainWindow::updateTopbar() {
    // 顶栏只表达「现在在哪 + 现在什么状态」，与侧栏状态条共用同一句状态结论。
    if (!topbarTitle_) return;
    const auto& state = controller_->state();
    const auto sentence = statusSentence(statusFacts(state, controller_->settings(), activeProfileName()));
    const auto dotState = state.core == CoreState::Failed ? QString("error")
        : state.core == CoreState::Starting || state.core == CoreState::Stopping || state.proxy == ProxyState::Unknown ? QString("warning")
        : state.ready && (state.proxy == ProxyState::Owned || state.tunObserved) ? QString("success")
        : state.ready ? QString("active") : QString("idle");
    const QString status = state.ready ? (state.proxy == ProxyState::Owned || state.tunObserved ? QString("已接管") : QString("内核就绪"))
        : state.core == CoreState::Starting ? QString("启动中")
        : state.core == CoreState::Failed ? QString("启动失败")
        : state.core == CoreState::Stopping ? QString("停止中") : QString("未连接");
    const bool configActions=route_.view==ViewIds::Home||route_.view==ViewIds::Profiles;
    for (const auto& name:{"toolbarImport","toolbarAdd"}) if (auto* button=findChild<QToolButton*>(name)) { button->setVisible(configActions); button->setIcon(UiIcons::icon(QString(name)=="toolbarAdd"?UiIcons::Glyph::Plus:UiIcons::Glyph::Import,UiTheme::Brand)); }
    WindowParts::updateTopbar(this, route_.view==ViewIds::Home?QString("Mlash"):route_.view==ViewIds::Rules?QString("配置"):ViewIds::title(route_.view), ViewIds::subtitle(route_.view), status, dotState);
    if (topbarStatusText_) topbarStatusText_->setToolTip(sentence.line1);
}
void MainWindow::updateWindowTitle() {
    // 窗口标题由路由决定；设置页的分组名还是由三组卡片自己的标题给出。
    const QString category = route_.view == ViewIds::Settings && route_.category >= 0 && route_.category < settingsSections_.size()
        ? settingsSections_[route_.category]->findChild<QLabel*>("sectionTitle")->text() : QString();
    const QString title = routeTitle(route_, category);
    if (windowTitle() != title) setWindowTitle(title);
}
int MainWindow::memberView() const { return nodeViews_ && nodeViews_->currentIndex() == 1 ? 1 : 0; }
void MainWindow::showEvent(QShowEvent* event) {
    if (route_.view == ViewIds::Rules && !rulesDirty_) loadRuleDraft();
    QMainWindow::showEvent(event);
    // 构造期页面栈停留在索引 0、route_ 可能已是别处（GuiHost 在构造后立刻 navigate）；
    // 这里只补齐栈位置与可见性，页签/侧栏/订阅标记已由 navigate 处理过。
    if (const int target = stackIndex(route_.view); target != pages_->currentIndex()) pages_->setCurrentIndex(target);
    updateTopbar();
    if (contentStack_&&contentStack_->currentIndex()!=0) return;
    switch (route_.view) {
    case ViewIds::Home:
        controller_->setHomeVisible(true);
        controller_->refresh();
        refreshTimer_.start(8000);
        applyHomeAnchor(route_.anchor);
        break;
    case ViewIds::Rules:
        controller_->setRulesVisible(rulesTabs_->currentIndex() == 1);
        if (rulesTabs_->currentIndex() == 1) controller_->refreshRules();
        break;
    case ViewIds::Data:
        updateDataPanelVisibility();
        break;
    default:
        break;
    }
}
void MainWindow::hideEvent(QHideEvent* event) {
    homeOutboundSnapshot_={}; updateHomeOutbound();
    if (connectionOptions_) connectionOptions_->flushPreferences();
    if (outboundDialog_) outboundDialog_->close();
    controller_->setHomeVisible(false); websitePage_->setActive(false);
    if (connectionDetail_) connectionDetail_->close(); if (configPreview_) configPreview_->close();
    refreshTimer_.stop(); controller_->setRulesVisible(false);
    controller_->setDiagnosticsVisible(false); controller_->setConnectionsVisible(false);
    QMainWindow::hideEvent(event);
}
} // namespace mlash
