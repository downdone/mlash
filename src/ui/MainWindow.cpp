#include "app/AppController.h"
#include "TaskDialog.h"
#include "MainWindow.h"
#include "WindowChrome.h"
#include "ElidedLabel.h"
#include "ConnectionOptions.h"
#include "StatusSentence.h"
#include "FeedbackSync.h"
#include "TunDialog.h"
#include "TrayController.h"
#include "NodePresentation.h"
#include "WidgetStyle.h"
#include "UiTheme.h"
#include "WebsiteTestsPage.h"
#include "UiIcons.h"
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
#include <QWindow>
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
    auto* sidebar = new QWidget; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(UiTheme::SidebarWidth); sidebar->setAttribute(Qt::WA_StyledBackground); auto* sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(design::ShadowrocketStyle::geometry.sidebarInset,design::ShadowrocketStyle::geometry.sidebarInset,design::ShadowrocketStyle::geometry.sidebarInset,design::ShadowrocketStyle::geometry.sidebarInset); sideLayout->setSpacing(design::ShadowrocketStyle::geometry.sidebarGap);
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
    // 共享框架的顶栏高度和两侧动作槽由风格几何规格统一提供。
    auto* content = new QWidget(central); content->setObjectName("contentSurface"); auto* contentLayout = new QVBoxLayout(content); contentLayout->setContentsMargins(0,0,0,0); contentLayout->setSpacing(0); pages_->setContentsMargins(UiTheme::ContentInset,UiTheme::ContentTop,UiTheme::ContentInset,UiTheme::ContentTop);
    auto* topbar = new QWidget(content); topbar->setObjectName("topbar"); topbar->setFixedHeight(UiTheme::TopbarHeight);
    chromeDragArea_=topbar; topbar->installEventFilter(this);
    // Equal-width edge slots keep the title centered independently of toolbar actions.
    auto* topbarLayout = new QHBoxLayout(topbar); topbarLayout->setContentsMargins(design::ShadowrocketStyle::geometry.topbarHorizontalInset,0,design::ShadowrocketStyle::geometry.topbarHorizontalInset,0); topbarLayout->setSpacing(0);
    auto* leadingSlot = new QWidget(topbar); leadingSlot->setFixedWidth(design::ShadowrocketStyle::geometry.topbarActionSlotWidth); topbarLayout->addWidget(leadingSlot);
    chromeLeading_=leadingSlot; leadingSlot->installEventFilter(this);
    auto* leadingLayout=new QHBoxLayout(leadingSlot); leadingLayout->setContentsMargins(10,0,0,0); leadingLayout->setSpacing(6);
    auto* chromeMark=new QLabel(leadingSlot); chromeMark->setObjectName("chromeMark"); chromeMark->setPixmap(TrayController::icon().pixmap(16,16)); chromeMark->setProperty("chromeDrag",true); chromeMark->installEventFilter(this); leadingLayout->addWidget(chromeMark);
    auto* chromeName=new QLabel("Mlash",leadingSlot); chromeName->setObjectName("chromeBrand"); chromeName->setProperty("chromeDrag",true); chromeName->installEventFilter(this); leadingLayout->addWidget(chromeName); leadingLayout->addStretch();
    topbarTitle_ = new QLabel; topbarTitle_->setObjectName("topbarTitle"); topbarTitle_->setTextFormat(Qt::PlainText); topbarTitle_->setAlignment(Qt::AlignCenter);
    topbarTitle_->installEventFilter(this);
    topbarLayout->addWidget(topbarTitle_,1);
    // Retain readback labels for diagnostics without duplicating visible status.
    auto* auxiliary = new QWidget(topbar); auxiliary->hide();
    topbarSubtitle_ = new QLabel(auxiliary); topbarSubtitle_->setObjectName("topbarSubtitle");
    topbarDot_ = new QLabel(auxiliary); topbarDot_->setObjectName("topbarDot");
    topbarStatusText_ = new QLabel(auxiliary); topbarStatusText_->setObjectName("topbarStatusText");
    auto* trailingSlot = new QWidget(topbar); trailingSlot->setFixedWidth(design::ShadowrocketStyle::geometry.topbarActionSlotWidth);
    chromeTrailing_=trailingSlot;
    auto* trailingLayout = new QHBoxLayout(trailingSlot); trailingLayout->setContentsMargins(0,0,0,0); trailingLayout->setSpacing(design::ShadowrocketStyle::geometry.topbarActionGap);
    topbarLayout->addWidget(trailingSlot);
    auto* addButton=new QToolButton; addButton->setObjectName("toolbarAdd"); addButton->setProperty("prototypeIcon",true); addButton->setIcon(UiIcons::icon(UiIcons::Glyph::Plus,UiTheme::Brand)); addButton->setIconSize(QSize(22,22)); addButton->setFixedSize(32,32); addButton->setToolTip("添加配置"); trailingLayout->addStretch(); trailingLayout->addWidget(addButton);
    connect(addButton,&QToolButton::clicked,this,&MainWindow::addUrl);
    const auto chromeButton=[this,trailingLayout](const QString& label,const QString& name,const QString& hint) {
        auto* control=new QToolButton(this); control->setObjectName(name); control->setProperty("windowChromeButton",true);
        control->setText(label); control->setToolTip(hint); control->setFixedSize(30,28);
        control->setFocusPolicy(Qt::StrongFocus); trailingLayout->addWidget(control); return control;
    };
    auto* minimize=chromeButton(QStringLiteral("−"),"windowMinimize","最小化");
    auto* maximize=chromeButton(QStringLiteral("□"),"windowMaximize","最大化或还原");
    auto* closeControl=chromeButton(QStringLiteral("×"),"windowClose","关闭窗口");
    closeControl->setProperty("destructiveChrome",true);
    connect(minimize,&QToolButton::clicked,this,&QWidget::showMinimized);
    connect(maximize,&QToolButton::clicked,this,[this] { isMaximized()?showNormal():showMaximized(); });
    connect(closeControl,&QToolButton::clicked,this,&QWidget::close);
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
    connect(controller_, &AppController::stateChanged, this, &MainWindow::applyWindowChrome);
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
    applyWindowChrome();
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
    if (locateAction_) locateAction_->setIcon(UiIcons::icon(G::Locate,UiTheme::Secondary));
    if (testAction_) testAction_->setIcon(UiIcons::icon(G::Gauge,UiTheme::Secondary));
    for (const auto& entry:QList<QPair<QString,G>>{{"browseGroup",G::Layers},{"nodeToolsButton",G::More},{"nodeSortButton",G::Sliders}}) if (auto* button=findChild<QToolButton*>(entry.first)) button->setIcon(UiIcons::icon(entry.second,UiTheme::Secondary));
    for (const auto& pair:QList<QPair<QString,G>>{{"addSubscription",G::Plus},{"updateProfile",G::Refresh},{"clearLogViewButton",G::Clear},{"followLatestLogs",G::Refresh},{"saveCustomRules",G::Save}}) if (auto* control=findChild<QAbstractButton*>(pair.first)) control->setIcon(UiIcons::icon(pair.second,control->property("primary").toBool()?UiTheme::OnPrimary:UiTheme::Secondary));
    for (const auto& name:{"nodeSearch","logSearch","connectionsSearch","activeRulesSearch"}) if (auto* field=findChild<QLineEdit*>(name)) {
        if (!field->property("searchGlyph").toBool()) { field->addAction(UiIcons::icon(G::Search),QLineEdit::LeadingPosition)->setObjectName("searchGlyph"); field->setProperty("searchGlyph",true); }
        else if (auto* action=field->findChild<QAction*>("searchGlyph")) action->setIcon(UiIcons::icon(G::Search));
    }
    for (auto* view:QList<QAbstractItemView*>{nodeList_,nodeCards_,connectionsTable_}) view->viewport()->update();
    updateState();
}
MainWindow::~MainWindow() {
    // End diagnostic callbacks while the main page widgets still exist.
    if (outboundDialog_) delete outboundDialog_.data();
    NodePresentation::clearTransientCache(); if (connectionOptions_) connectionOptions_->flushPreferences(); controller_->setHomeVisible(false); controller_->setWebsiteTestsVisible(false); controller_->clearWebsiteTests(); controller_->setRulesVisible(false); controller_->setConnectionsVisible(false); controller_->setDiagnosticsVisible(false); }
}
