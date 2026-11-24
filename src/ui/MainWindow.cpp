#include "app/AppController.h"
#include "TaskDialog.h"
#include "MainWindow.h"
#include "ProxyNodesPage.h"
#include "BrandSignature.h"
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
namespace {
constexpr int sidebarControlOffset=(design::ShadowrocketStyle::collapsedSidebarWidth-UiTheme::SidebarItemHeight)/2
    - design::ShadowrocketStyle::geometry.sidebarInset;

class SidebarNavigationDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        const auto kind=navKindOfView(index.data(Qt::UserRole).toInt());
        const QColor tint(kind==NavKind::Config?UiTheme::Success:kind==NavKind::Data?UiTheme::Warning:UiTheme::Brand);
        const bool active=index.data(ActiveNavigationRole).toBool();
        const bool collapsed=option.widget&&option.widget->property("collapsedSidebar").toBool();
        const bool focus=option.state.testFlag(QStyle::State_HasFocus) && option.widget &&
            option.widget->property("keyboardFocus").toBool();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(focus?QPen(QColor(UiTheme::Focus)):QPen(Qt::NoPen));
        painter->setBrush(QColor(active?UiTheme::Selection:option.state.testFlag(QStyle::State_MouseOver)?UiTheme::Hover:UiTheme::Surface));
        const auto bounds=option.rect;
        painter->drawRoundedRect(QRectF(bounds).adjusted(.5,.5,-.5,-.5),8,8);
        const int badgeX=bounds.left()+(collapsed?(bounds.width()-20)/2:8);
        const QRect badge(badgeX,bounds.top()+(bounds.height()-20)/2,20,20);
        painter->setPen(Qt::NoPen);
        painter->setBrush(tint);
        painter->drawEllipse(badge);
        UiIcons::icon(navigationGlyph(kind),UiTheme::OnPrimary).paint(painter,badge.adjusted(3,3,-3,-3));
        if (!collapsed) {
            auto font=UiTheme::rowCaptionFont(UiTheme::bodyFont());
            font.setWeight(active?QFont::DemiBold:QFont::Normal);
            painter->setFont(font);
            painter->setPen(active?tint:QColor(UiTheme::Ink));
            painter->drawText(bounds.adjusted(36,0,-8,0),Qt::AlignLeft | Qt::AlignVCenter,index.data().toString());
        }
        painter->restore();
    }
};
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateSidebar();
}
void MainWindow::updateSidebar() {
    auto* sidebar=findChild<QWidget*>("sidebar");
    if (!sidebar || !settingsNav_) return;
    namespace style=design::ShadowrocketStyle;
    constexpr int resizeSpan=style::fullSidebarWindowWidth-style::minimumSidebarWindowWidth;
    const int resizeProgress=qBound(0,width()-style::minimumSidebarWindowWidth,resizeSpan);
    const int expandedWidth=style::narrowSidebarWidth
        + (UiTheme::SidebarWidth-style::narrowSidebarWidth)*resizeProgress/resizeSpan;
    const int desired=sidebarCollapsed_?style::collapsedSidebarWidth:expandedWidth;
    const bool widthChanged=sidebar->minimumWidth()!=desired;
    if (widthChanged) sidebar->setFixedWidth(desired);

    auto* wordmark=sidebar->findChild<QLabel*>("sidebarBrand");
    auto* brand=wordmark->parentWidget();
    auto* mark=brand->findChild<QLabel*>("brandMark");
    auto* toggle=sidebar->findChild<QToolButton*>("toggleSidebar");
    auto* header=toggle->parentWidget();
    const int headerInset=sidebarCollapsed_?sidebarControlOffset:0;
    header->layout()->setContentsMargins(headerInset,0,headerInset,0);
    const int brandWidth=expandedWidth-2*style::geometry.sidebarInset-toggle->width()-header->layout()->spacing();
    const auto wordmarkSize=wordmark->sizeHint();
    const bool showWordmark=wordmarkSize.width()<=brandWidth;
    wordmark->setVisible(showWordmark);
    mark->setVisible(!showWordmark || wordmarkSize.width()+mark->width()+brand->layout()->spacing()<=brandWidth);
    brand->setVisible(!sidebarCollapsed_);
    header->setFixedSize(desired-2*style::geometry.sidebarInset,qMax(toggle->height(),wordmarkSize.height()));
    header->layout()->setAlignment(toggle,sidebarCollapsed_?Qt::AlignCenter:Qt::AlignVCenter);

    if (navigation_->property("collapsedSidebar").toBool()!=sidebarCollapsed_) {
        navigation_->setProperty("collapsedSidebar",sidebarCollapsed_);
        for (int i=0;i<navigation_->count();++i) {
            auto* item=navigation_->item(i);
            item->setToolTip(sidebarCollapsed_?item->text():QString());
        }
        navigation_->viewport()->update();
    }
    const QString label=sidebarCollapsed_?QStringLiteral("展开导航"):QStringLiteral("收起导航");
    toggle->setAccessibleName(label);
    toggle->setToolTip(label);
    settingsNav_->setToolButtonStyle(sidebarCollapsed_?Qt::ToolButtonIconOnly:Qt::ToolButtonTextBesideIcon);
    settingsNav_->setFixedWidth(sidebarCollapsed_?UiTheme::SidebarItemHeight:desired-2*style::geometry.sidebarInset-sidebarControlOffset);
    resizeWorkspaceNavigation();
    if (widthChanged && centralWidget()) {
        centralWidget()->layout()->activate();
        sidebar->layout()->activate();
        header->layout()->activate();
    }
}
MainWindow::MainWindow(AppController* controller, QWidget* parent) : QMainWindow(parent), controller_(controller), model_(this), filter_(this) {
    setAcceptDrops(true);
    // Use the same bundled Widgets style for the window and its dialogs on Windows.
    WidgetStyle::install();
    UiTheme::setPreferences(controller_->settings().appearance,controller_->settings().textScale);
    setWindowTitle("Mlash"); setWindowIcon(TrayController::icon());
    resize(design::ShadowrocketStyle::fullSidebarWindowWidth,720);
    setMinimumSize(design::ShadowrocketStyle::minimumSidebarWindowWidth,560);

    auto* central = new QWidget(this); auto* layout = new QHBoxLayout(central);
    central->setObjectName("mainSurface"); central->setAttribute(Qt::WA_StyledBackground, true);
    layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto* sidebar = new QWidget; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(UiTheme::SidebarWidth); sidebar->setAttribute(Qt::WA_StyledBackground); auto* sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(design::ShadowrocketStyle::geometry.sidebarInset,design::ShadowrocketStyle::geometry.sidebarInset,design::ShadowrocketStyle::geometry.sidebarInset,design::ShadowrocketStyle::geometry.sidebarInset); sideLayout->setSpacing(design::ShadowrocketStyle::geometry.sidebarGap);
    auto* brandHeader=new QWidget(sidebar); brandHeader->setObjectName("sidebarBrandHeader");
    auto* brandLayout=new QHBoxLayout(brandHeader); brandLayout->setContentsMargins(0,0,0,0); brandLayout->setSpacing(6);
    brandLayout->addWidget(makeBrandSignature(brandHeader,28,"sidebarBrand"),1);
    auto* toggleSidebar=new QToolButton(brandHeader);
    toggleSidebar->setObjectName("toggleSidebar"); toggleSidebar->setProperty("quiet",true);
    toggleSidebar->setFixedSize(32,32); toggleSidebar->setIconSize(QSize(17,17));
    toggleSidebar->setFocusPolicy(Qt::StrongFocus);
    brandLayout->addWidget(toggleSidebar,0,Qt::AlignVCenter);
    sideLayout->addWidget(brandHeader); sideLayout->addSpacing(6);
    navigation_ = new QListWidget(sidebar); navigation_->setObjectName("navigation"); navigation_->addItems({"总览", "配置"});
    navigation_->setItemDelegate(new SidebarNavigationDelegate(navigation_));
    navigation_->setViewMode(QListView::ListMode);
    navigation_->setSpacing(design::ShadowrocketStyle::geometry.sidebarGap);
    navigation_->setResizeMode(QListView::Adjust); navigation_->setUniformItemSizes(true);
    navigation_->setFrameShape(QFrame::NoFrame); navigation_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); navigation_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation_->setMouseTracking(true);
    navigation_->setAccessibleName("工作区导航");
    const QList<int> pageOrder{ViewIds::Overview,ViewIds::Profiles};
    const int navigationRowHeight=design::ShadowrocketStyle::navigationRowHeight;
    for (int i=0;i<pageOrder.size();++i) {
        auto* item=navigation_->item(i);
        item->setData(Qt::UserRole,pageOrder[i]);
        item->setSizeHint(QSize(0,navigationRowHeight));
    }
    navigation_->setFixedHeight(navigation_->count()*(navigationRowHeight+2*navigation_->spacing()));
    sideLayout->addWidget(navigation_);
    sideLayout->addWidget(workspaceNavigation());
    sideLayout->addStretch(1);
    // 常驻状态条（分层设计 P1）：四段文案由 statusSentence 统一产出，与托盘 tooltip 同源。
    statusButton_ = new QToolButton(sidebar); statusButton_->setObjectName("sidebarStatus"); statusButton_->setAccessibleName("当前运行状态，单击返回首页"); statusButton_->setCursor(Qt::PointingHandCursor);
    auto* statusLayout = new QHBoxLayout(statusButton_); statusLayout->setContentsMargins(10, 8, 8, 8); statusLayout->setSpacing(8);
    statusHintDot_ = new QLabel(statusButton_); statusHintDot_->setObjectName("sidebarStatusDot"); statusHintDot_->setFixedSize(12, 12); statusLayout->addWidget(statusHintDot_, 0, Qt::AlignTop);
    auto* statusLines = new QVBoxLayout; statusLines->setSpacing(1);
    statusLine1_ = new ElidedLabel(statusButton_); statusLine1_->setObjectName("sidebarStatusLine1"); statusLines->addWidget(statusLine1_);
    statusLine2_ = new ElidedLabel(statusButton_); statusLine2_->setObjectName("sidebarStatusLine2"); statusLines->addWidget(statusLine2_);
    statusLayout->addLayout(statusLines, 1); statusButton_->hide();
    connect(statusButton_, &QToolButton::clicked, this, [this] { navigate(Route{ViewIds::Overview}); });
    auto* divider=new QFrame(sidebar); divider->setObjectName("sidebarDivider"); divider->setFixedHeight(1); sideLayout->addWidget(divider); sideLayout->addSpacing(6);
    settingsNav_=new QToolButton(sidebar); settingsNav_->setObjectName("sidebarSettings"); settingsNav_->setText("设置");
    settingsNav_->setIconSize(QSize(18,18)); settingsNav_->setToolButtonStyle(Qt::ToolButtonIconOnly); settingsNav_->setCheckable(true);
    settingsNav_->setFixedSize(UiTheme::SidebarItemHeight,UiTheme::SidebarItemHeight);
    settingsNav_->setFocusPolicy(Qt::StrongFocus);
    settingsNav_->setAccessibleName("设置"); settingsNav_->setCursor(Qt::PointingHandCursor);
    settingsNav_->setToolTip("设置");
    auto* settingsLayout=new QHBoxLayout;
    settingsLayout->setContentsMargins(sidebarControlOffset,0,0,0);
    settingsLayout->addWidget(settingsNav_,0,Qt::AlignLeft);
    sideLayout->addLayout(settingsLayout);
    connect(toggleSidebar,&QToolButton::clicked,this,[this] {
        sidebarCollapsed_=!sidebarCollapsed_;
        updateSidebar();
    });
    connect(settingsNav_,&QToolButton::clicked,this,[this] { navigate(Route{ViewIds::Settings}); });
    // 视图栈顺序与 ViewIds 一致。工作区流量和日志复用同一数据页。
    pages_ = new QStackedWidget; pages_->setObjectName("mainPages");
    auto pagePolicy=pages_->sizePolicy(); pagePolicy.setRetainSizeWhenHidden(true); pages_->setSizePolicy(pagePolicy);
    proxyPages_=new QStackedWidget;
    proxyPages_->setObjectName("proxyPages");
    proxyPages_->addWidget(homePage());
    proxyNodesPage_=new ProxyNodesPage;
    connect(proxyNodesPage_,&ProxyNodesPage::profileSetupRequested,this,&MainWindow::openProfileSetup);
    proxyPages_->addWidget(proxyNodesPage_);
    pages_->addWidget(proxyPages_);                // ViewIds::Home
    pages_->addWidget(profilesPage());             // ViewIds::Profiles
    pages_->addWidget(rulesPage());                // ViewIds::Rules
    pages_->addWidget(dataPage());                 // ViewIds::Data
    pages_->addWidget(settingsPage());             // ViewIds::Settings
    pages_->addWidget(overviewPage());             // ViewIds::Overview
    pages_->addWidget(tunnelPage());               // ViewIds::Tunnel
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
    connect(websitePage_,&WebsiteTestsPage::pageRequested,this,[this](int page) {
        if (page==1) openProfileSetup();
        else setPage(page);
    });
    connect(websitePage_,&WebsiteTestsPage::diagnosticsRequested,this,&MainWindow::showOutboundDiagnostics);
    for (int page = 0; page < pages_->count(); ++page) { if (pages_->widget(page)->objectName().isEmpty()) pages_->widget(page)->setObjectName("pageSurface"); pages_->widget(page)->setProperty("pageSurface",true); pages_->widget(page)->setAttribute(Qt::WA_StyledBackground, true); pages_->widget(page)->setAutoFillBackground(true); }
    layout->addWidget(sidebar);
    // 共享框架的顶栏高度和两侧动作槽由风格几何规格统一提供。
    auto* content = new QWidget(central); content->setObjectName("contentSurface"); content->installEventFilter(this); auto* contentLayout = new QVBoxLayout(content); contentLayout->setContentsMargins(0,0,0,0); contentLayout->setSpacing(0); pages_->setContentsMargins(UiTheme::ContentInset,UiTheme::ContentTop,UiTheme::ContentInset,UiTheme::ContentTop);
    auto* topbar = new QWidget(content); topbar->setObjectName("topbar"); topbar->setFixedHeight(UiTheme::TopbarHeight);
    chromeDragArea_=topbar; topbar->installEventFilter(this);
    auto* topbarLayout = new QHBoxLayout(topbar); topbarLayout->setContentsMargins(design::ShadowrocketStyle::geometry.topbarHorizontalInset,0,design::ShadowrocketStyle::geometry.topbarHorizontalInset,0); topbarLayout->setSpacing(0);
    topbarTitle_ = new QLabel; topbarTitle_->setObjectName("topbarTitle"); topbarTitle_->setTextFormat(Qt::PlainText); topbarTitle_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
    operationStatus_=new QStatusBar(this); operationStatus_->setObjectName("operationStatusBar"); operationStatus_->setSizeGripEnabled(false); operationStatus_->setContentsMargins(0,0,12,0); contentLayout->addWidget(operationStatus_);
    // 两段并存（P4.1）：最近操作 = 跨页结果，当前告警 = 仍然成立的系统问题。两者互不覆盖。
    message_=new ElidedLabel("正在初始化"); message_->setObjectName("operationMessage"); message_->setAccessibleName("操作状态"); message_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding); operationStatus_->addWidget(message_,1);
    auto* details=new QToolButton; details->setObjectName("statusDetails"); details->setText("详情"); details->setToolTip("查看及复制完整状态信息"); operationStatus_->addPermanentWidget(details);
    connect(details,&QToolButton::clicked,this,[this] {
        const auto text=message_->toolTip().isEmpty()?message_->text():message_->toolTip();
        if (auto* previous=qobject_cast<QDialog*>(taskWorkspace()->editor("operation-details"))) previous->reject();
        auto* task=new TaskDialog(this); task->setObjectName("statusDetailsDialog"); task->setWindowTitle("操作信息");
        task->setAttribute(Qt::WA_DeleteOnClose);
        auto* box=new QVBoxLayout(task); auto* content=new QPlainTextEdit(text); content->setReadOnly(true); box->addWidget(content,1);
        auto* copy=new QPushButton("复制消息"); box->addWidget(copy,0,Qt::AlignRight); connect(copy,&QPushButton::clicked,task,[text] { QApplication::clipboard()->setText(text); }); presentTask(task,"operation-details");
    });
    statusAlarm_=new QFrame(operationStatus_);
    statusAlarm_->setObjectName("statusAlarm");
    auto* alarmLayout=new QHBoxLayout(statusAlarm_);
    alarmLayout->setContentsMargins(8,0,2,0);
    alarmLayout->setSpacing(5);
    statusAlarmMark_=new QLabel(statusAlarm_);
    statusAlarmMark_->setFixedSize(12,12);
    alarmLayout->addWidget(statusAlarmMark_,0,Qt::AlignVCenter);
    operationAlarm_=new QLabel(statusAlarm_);
    operationAlarm_->setObjectName("operationAlarm");
    alarmLayout->addWidget(operationAlarm_);

    statusAlarmDetails_=new QToolButton(statusAlarm_);
    statusAlarmDetails_->setObjectName("statusAlarmDetails");
    statusAlarmDetails_->setText("详情");
    statusAlarmDetails_->setAccessibleName("查看只读保护详情");
    statusAlarmDetails_->setToolTip("查看原因与数据目录");
    statusResolve_=new QToolButton(statusAlarm_);
    statusResolve_->setObjectName("statusResolve");
    statusResolve_->setText("处理");
    statusResolve_->setAccessibleName("处理告警");
    statusResolve_->setPopupMode(QToolButton::InstantPopup);
    for (auto* button : {statusAlarmDetails_,statusResolve_}) {
        button->setProperty("statusAction",true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setLayoutDirection(Qt::RightToLeft);
        button->setIconSize(QSize(12,12));
        button->setFocusPolicy(Qt::StrongFocus);
        alarmLayout->addWidget(button,0,Qt::AlignVCenter);
    }
    connect(statusAlarmDetails_,&QToolButton::clicked,this,&MainWindow::showStatusAlarm);
    auto* resolveMenu=new QMenu(statusResolve_);
    statusResolve_->setMenu(resolveMenu);
    resolveMenu->addAction("查看告警详情…",this,&MainWindow::showStatusAlarm);
    resolveMenu->addAction("查看日志",this,[this] { navigate(Route{ViewIds::Data,kNoCategory,HomeAnchor::Control,int(ViewIds::DataTab::Logs)}); });
    resolveMenu->addAction("订阅管理",this,[this] { navigate(Route{ViewIds::Profiles}); });
    resolveMenu->addAction("TUN 与服务…",this,[this] { showTool("tun",{},[this] { return new TunDialog(controller_,this); }); });
    resolveMenu->addAction("刷新运行状态",controller_,&AppController::refresh);
    operationStatus_->addPermanentWidget(statusAlarm_);
    statusAlarm_->hide();
    connect(&refreshTimer_, &QTimer::timeout, this, [this] { if (!controller_->state().busy) controller_->refresh(); });
    // 样式重绘可能重置 QListWidget 的 currentRow。路由只接受明确的点击或键盘激活。
    auto activateNavigation=[this](QListWidgetItem* item) {
        if (item) navigate(Route{item->data(Qt::UserRole).toInt()});
    };
    connect(navigation_, &QListWidget::itemClicked, this, activateNavigation);
    connect(navigation_, &QListWidget::itemActivated, this, activateNavigation);
    syncSectionNavigation(route_);
    updateSidebar();
    connect(controller_, &AppController::stateChanged, this, &MainWindow::updateState);
    connect(controller_, &AppController::wireGuardChanged, this, &MainWindow::updateState);
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
        message_->setText(text); message_->setToolTip(text);
        refreshStatusSegments();
        findChild<QToolButton*>("statusDetails")->show();
    });
    connect(controller_, &AppController::operationClassified, this, [this](ErrorCategory category, const QString&) {
        switch (category) {
        case ErrorCategory::Rejected: statusCategoryHint_ = QStringLiteral("需要修正输入或配置"); break;
        case ErrorCategory::Retryable: statusCategoryHint_ = QStringLiteral("可重试"); break;
        case ErrorCategory::UserAction: statusCategoryHint_ = QStringLiteral("需要用户处理"); break;
        case ErrorCategory::Reload: statusCategoryHint_ = QStringLiteral("需重新载入后再操作"); break;
        case ErrorCategory::Internal: statusCategoryHint_ = QStringLiteral("内部错误"); break;
        case ErrorCategory::Unspecified: statusCategoryHint_.clear(); break;
        }
    });
    connect(controller_, &AppController::operationFinished, this, [this](bool ok, const QString& text, OperationId id) {
        if (id != OperationId::Unspecified) { routeOperationFeedback(ok, text, id); statusCategoryHint_.clear(); return; }
        message_->setText(statusNoticeText(ok, text));
        message_->setToolTip(statusCategoryHint_.isEmpty() ? text : text + "\n" + statusCategoryHint_);
        statusCategoryHint_.clear();
        feedbackTimer_.stop(); recentFeedback_=ok; feedbackError_=ok?QString():text; noticeOperation_=id; if (ok) feedbackTimer_.start();
        refreshStatusSegments();
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
        for (int view=0;view<ViewIds::Count;++view) pageRouter_.registerPage(view, new Page(this, view));
    }
}
void MainWindow::applyAppearance() {
    const int statusHeight=qMax(30,QFontMetrics(qApp->font()).height()+10); operationStatus_->setFixedHeight(statusHeight); if (auto* details=findChild<QToolButton*>("statusDetails")) details->setFixedHeight(statusHeight-8);
    statusAlarm_->setFixedHeight(statusHeight-6);
    statusAlarmDetails_->setIcon(UiIcons::icon(UiIcons::Glyph::ChevronRight,UiTheme::Secondary));
    statusResolve_->setIcon(UiIcons::icon(UiIcons::Glyph::ChevronDown,UiTheme::Secondary));
    if (auto* toggle=findChild<QToolButton*>("toggleSidebar")) toggle->setIcon(UiIcons::icon(UiIcons::Glyph::Sidebar));
    updateSidebar();
    navigation_->viewport()->update();
    settingsNav_->setIcon(UiIcons::icon(UiIcons::Glyph::Settings,settingsNav_->isChecked()?UiTheme::Brand:UiTheme::Secondary));
    using G=UiIcons::Glyph;
    if (locateAction_) locateAction_->setIcon(UiIcons::icon(G::Locate,UiTheme::Secondary));
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
