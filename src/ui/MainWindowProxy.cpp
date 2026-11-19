#include "app/AppController.h"
#include "TaskDialog.h"
#include "TaskWorkspace.h"
#include "MainWindow.h"
#include "CompactComboBox.h"
#include "DetailsDrawer.h"
#include "GroupPickerPopup.h"
#include "DialogDraftGuard.h"
#include "NodeCardView.h"
#include "NodePresentation.h"
#include "NodeRowDelegate.h"
#include "NodeRowModel.h"
#include "NodeRowView.h"
#include "NodeFilterPopup.h"
#include "PrototypeUi.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "WidgetStyle.h"
#include "WindowUiFactories.h"
#include "WebsiteTestsPage.h"
#include <QtWidgets>
#include "WrapLayout.h"

namespace mlash {
using namespace WindowParts;
// 节点区（原索引 2 的「节点页」）。ADR 0048 把它并入首页：本函数只返回节点区控件，
// 由 homePage() 放进同一个滚动视图，不再占据 QStackedWidget 的独立索引。
QWidget* MainWindow::nodeSection() {
    auto* page = new QWidget; page->setObjectName("nodeSection"); auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(4);
    // 标题行（原型 01）：左「节点选择 + 计数」，右订阅选择 / 搜索 / 测速 / 更多。
    auto* heading = new QWidget; heading->setObjectName("nodeHeading"); heading->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    auto* headingLayout = new QHBoxLayout(heading); headingLayout->setContentsMargins(2, 0, 2, 0); headingLayout->setSpacing(8);
    nodeHeadingTitle_ = new QLabel("节点"); nodeHeadingTitle_->setObjectName("nodeHeadingTitle"); nodeHeadingTitle_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    nodeCount_ = new QLabel; nodeCount_->setObjectName("nodeHeadingCount"); nodeCount_->setProperty("secondary", true);
    profilesCombo_ = new CompactComboBox; profilesCombo_->setObjectName("nodeProfile"); profilesCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); profilesCombo_->setMinimumContentsLength(4); profilesCombo_->setMaximumWidth(150); profilesCombo_->setToolTip("节点来源订阅；切换只改变浏览的订阅"); profilesCombo_->setParent(heading); profilesCombo_->hide();
    auto* groupPicker=new QToolButton; groupPicker->setFixedSize(154,32); groupPicker->setObjectName("browseGroup"); groupPicker->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); groupPicker->setToolTip("选择节点分组"); groupPicker->setIcon(UiIcons::icon(UiIcons::Glyph::Layers,UiTheme::Secondary)); groupPicker->setIconSize(QSize(16,16)); groupPicker->setText("选择策略组"); headingLayout->addWidget(groupPicker);
    connect(groupPicker,&QToolButton::clicked,this,[this,groupPicker] {
        auto* popup=new GroupPickerPopup(groupPicker);
        for (auto* action:groupMenu_->actions())
            if (action->isEnabled()) popup->addChoice(action->text(),action->isChecked());
        popup->chosen=[this](const QString& group) { groupHistory_.clear(); selectGroup(group); };
        popup->openBelow(groupPicker);
    });
    search_ = new QLineEdit; search_->setObjectName("nodeSearch"); search_->setPlaceholderText("搜索节点"); search_->setClearButtonEnabled(true); search_->setMinimumWidth(120); search_->setMaximumWidth(280); search_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); headingLayout->addWidget(search_); search_->hide();
    auto* searchToggle=new QToolButton; searchToggle->setObjectName("nodeSearchToggle"); searchToggle->setProperty("prototypeIcon",true); searchToggle->setIcon(UiIcons::icon(UiIcons::Glyph::Search,UiTheme::Secondary)); searchToggle->setToolTip("搜索节点"); searchToggle->setCheckable(true);
    connect(searchToggle,&QToolButton::toggled,search_,[this](bool shown) { search_->setVisible(shown); if (shown) search_->setFocus(); else search_->clear(); });
    testAction_ = new QAction("测速本组", this); testAction_->setObjectName("testGroup"); testSelectedAction_ = new QAction("测速所选", this);
    // 取消测速：仍挂在测速按钮的菜单里（原来在底部状态条，随 H3 一起删除）。
    cancelTestAction_ = new QAction("取消测速", this); cancelTestAction_->setObjectName("cancelNodeTests");
    auto* testMenu = new QMenu(this); testMenu->addAction(testAction_); testMenu->addAction(testSelectedAction_); testMenu->addSeparator(); testMenu->addAction(cancelTestAction_);
    connect(cancelTestAction_, &QAction::triggered, controller_, &AppController::cancelTests);
    // K4（原型 05）：测速与排序是两个图标钮，各自带一个浮层菜单。
    testAction_->setIcon(UiIcons::icon(UiIcons::Glyph::Gauge,UiTheme::Secondary));
    auto* test = new QToolButton; test->setObjectName("nodeTestButton"); test->setProperty("prototypeIcon",true); test->setDefaultAction(testAction_);
    test->setToolButtonStyle(Qt::ToolButtonIconOnly); test->setToolTip("测试本组节点延迟；只读，不切换节点"); headingLayout->addWidget(test);
    auto* sort = new QToolButton; sort->setObjectName("nodeSortButton"); sort->setIcon(UiIcons::icon(UiIcons::Glyph::Sliders)); sort->setPopupMode(QToolButton::InstantPopup); sort->setAutoRaise(true); sort->setToolTip("排序");
    auto* sortMenu = new QMenu(sort); sortMenu->setObjectName("nodeSortMenu");
    for (const auto& option:QList<QPair<QString,int>>{{"原始顺序",-1},{"按名称升序",0},{"按延迟升序",2}}) {
        auto* action = sortMenu->addAction(option.first); action->setCheckable(true); action->setData(option.second); action->setChecked(option.second==-1);
        connect(action,&QAction::triggered,this,[this,option,sortMenu] { for (auto* entry:sortMenu->actions()) entry->setChecked(entry->data().toInt()==option.second); filter_.sort(option.second, Qt::AscendingOrder); });
    }
    sort->setMenu(sortMenu); sort->setParent(heading); sort->hide();
    auto* tools = new QToolButton; tools->setObjectName("nodeToolsButton"); tools->setProperty("prototypeIcon",true); tools->setIcon(UiIcons::icon(UiIcons::Glyph::More)); tools->setPopupMode(QToolButton::InstantPopup); tools->setAutoRaise(true); tools->setToolTip("更多节点操作"); headingLayout->addWidget(tools);
    layout->addWidget(heading);
    auto* restoreFilters=new QPushButton("已显示当前成员 · 恢复筛选"); restoreFilters->setObjectName("restoreNodeFilters"); restoreFilters->setProperty("backLink",true); restoreFilters->hide(); layout->addWidget(restoreFilters,0,Qt::AlignLeft);
    connect(restoreFilters,&QPushButton::clicked,this,[this,restoreFilters] { const auto saved=revealedNodeFilters_; revealedNodeFilters_={}; restoreNodeBookmark(saved); restoreFilters->hide(); });

    auto* useProfile = new QPushButton("使用此配置"); useProfile->setObjectName("useBrowsedProfile");
    useProfile->setToolTip("显式使用所浏览配置并启动内核"); useProfile->setParent(heading); useProfile->hide();
    connect(useProfile, &QPushButton::clicked, this, [this] {
        const auto id = profilesCombo_->currentData().toString();
        if (!id.isEmpty() && id != controller_->state().activeProfile) controller_->activateProfile(id);
    });
    connect(profilesCombo_, &QComboBox::currentIndexChanged, this, [this] { groupHistory_.clear(); updateProxies(); });

    // 行列表（原型 01 的 list-card）：组头与成员行都由 NodeRowModel 拍平，
    // 视图只负责画与把点击分流到折叠、详情、切换三条路径。
    nodeList_ = new NodeRowView; nodeList_->setObjectName("nodeList"); nodeList_->setUniformItemSizes(false);
    static_cast<NodeRowView*>(nodeList_)->delayClicked=[this](const QModelIndex& index) { if (testAction_->isEnabled()) controller_->testNodes({nodeRows_->memberAt(index.row())},selectedGroup_); };
    static_cast<NodeRowView*>(nodeList_)->infoClicked=[this](const QModelIndex& index) { if (nodeRows_->rowIsHeader(index.row())) editNodePreferences(); else showNodeDetails(nodeRows_->memberAt(index.row())); };
    nodeList_->setItemDelegate(new NodeRowDelegate(nodeList_)); nodeList_->setSelectionMode(QAbstractItemView::SingleSelection);
    nodeList_->setEditTriggers(QAbstractItemView::NoEditTriggers); nodeList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    nodeList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); nodeList_->setMouseTracking(true);
    nodeList_->setAccessibleName("节点行列表"); nodeList_->setContextMenuPolicy(Qt::CustomContextMenu);
    filter_.setSourceModel(&model_); filter_.setFilterCaseSensitivity(Qt::CaseInsensitive); filter_.setFilterKeyColumn(0); filter_.setSortRole(Qt::UserRole); filter_.sort(-1);
    nodeRows_ = new NodeRowModel(this); nodeRows_->setSource(&filter_); nodeList_->setModel(nodeRows_);
    const auto favoriteMenu=[this](QWidget* view,const QPoint& point) { const auto index=qobject_cast<QAbstractItemView*>(view)->indexAt(point); if (!index.isValid()) return; const auto name=memberView()==1?index.data(Qt::UserRole).toString():nodeRows_->memberAt(index.row()); if (name.isEmpty()) return; const auto id=profilesCombo_->currentData().toString(); const auto expected=controller_->nodePreferences(id); auto* menu=new QMenu(view); menu->setObjectName("nodeFavoriteMenu"); menu->setAttribute(Qt::WA_DeleteOnClose); const bool saved=expected.value("favorites").toArray().contains(name); auto* action=menu->addAction(saved?"取消收藏此成员":"收藏此成员"); action->setEnabled(!controller_->state().busy&&!controller_->state().readOnly); connect(action,&QAction::triggered,this,[this,id,name,expected,saved] { auto next=expected; auto values=next.value("favorites").toArray(); if (saved) for (int i=values.size()-1;i>=0;--i) { if (values[i].toString()==name) values.removeAt(i); } else values.append(name); next["favorites"]=values; controller_->updateNodePreferences(id,next,expected); }); connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater); WidgetStyle::popupMenu(menu,view->mapToGlobal(point)); };
    connect(nodeList_,&QWidget::customContextMenuRequested,this,[this,favoriteMenu](const QPoint& p) { favoriteMenu(nodeList_,p); });

    // 卡片视图保留为工具条里的显示偏好（P0 推论一：不删能力）。
    nodeCards_=new NodeCardView; nodeCards_->setObjectName("nodeCards"); nodeCards_->setProperty("flat", true); nodeCards_->setModel(&filter_);
    nodeCards_->setContextMenuPolicy(Qt::CustomContextMenu); connect(nodeCards_,&QWidget::customContextMenuRequested,this,[this,favoriteMenu](const QPoint& p) { favoriteMenu(nodeCards_,p); });
    nodeCards_->setItemDelegate(new NodeCardDelegate(nodeCards_)); nodeCards_->setViewMode(QListView::IconMode);
    nodeCards_->setResizeMode(QListView::Adjust); nodeCards_->setMovement(QListView::Static); nodeCards_->setUniformItemSizes(true);
    nodeCards_->setWrapping(true); nodeCards_->setSelectionMode(QAbstractItemView::SingleSelection); nodeCards_->setSelectionBehavior(QAbstractItemView::SelectRows);
    nodeCards_->setEditTriggers(QAbstractItemView::NoEditTriggers); nodeCards_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    nodeCards_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); nodeCards_->setAccessibleName("节点卡片");
    static_cast<NodeCardView*>(nodeCards_)->gridChanged=[this] { if (memberView()==1) updateNodeCount(); };
    nodeViews_=new QStackedWidget; nodeViews_->setObjectName("nodeViews"); nodeViews_->addWidget(nodeList_); nodeViews_->addWidget(nodeCards_);
    layout->addWidget(nodeViews_, 1);
    nodeEmpty_ = emptyState(nodeList_, "添加并使用订阅后，选择策略组查看节点");
    nodeCardsEmpty_=emptyState(nodeCards_,"添加并使用订阅后，选择策略组查看节点");

    // 「更多」溢出菜单（ui-layered-design.md:622）：恢复自动 / 最近选择 / 筛选 / 排序 /
    // 显示方式 / 节点选项 / 返回上级 / 定位当前。节点区本身不再有按钮条。
    auto* more = new QMenu(tools); sortMenu->setTitle("排序方式"); more->addMenu(sortMenu); testMenu->setTitle("测速操作"); more->addMenu(testMenu);
    automaticAction_ = more->addAction("恢复自动选择"); automaticAction_->setObjectName("automaticAction"); automaticAction_->setEnabled(false);
    recentNodes_ = new QAction("最近选择…", more); recentNodes_->setObjectName("recentNodes"); recentMenu_ = new QMenu(more); recentNodes_->setMenu(recentMenu_); more->addAction(recentNodes_);
    connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentMenu);
    more->addSeparator();
    // 浏览组（原 groupSwitcher_ + proxyGroups_ 卡片选择器）：产品支持多层策略组，
    // 原型只有一屏，所以把「换组」收进菜单——它是浏览，不是节点切换，不进控制区预算。
    groupMenu_ = more->addMenu("策略组"); groupMenu_->setObjectName("nodeGroupMenu");
    auto* filterAction=more->addAction("筛选…"); filterAction->setObjectName("nodeFilterAction");
    auto* filterPanel=new NodeFilterPopup(tools);
    nodeRegion_ = new CompactComboBox; nodeRegion_->setObjectName("nodeRegion"); nodeRegion_->addItem("全部地区",""); nodeRegion_->setToolTip("位置依据名称，不代表实际 IP");
    nodeProtocol_ = new CompactComboBox; nodeProtocol_->setObjectName("nodeProtocol"); nodeProtocol_->addItem("全部协议","");
    nodeHealth_ = new CompactComboBox; nodeHealth_->setObjectName("nodeHealth"); nodeHealth_->addItems({"全部状态","有延迟结果","测速失败","尚未测速"});
    nodeMaximum_ = new CompactComboBox; nodeMaximum_->setObjectName("nodeMaximumDelay"); nodeMaximum_->addItem("不限延迟",0); for (const auto ms:{100,200,500,1000}) nodeMaximum_->addItem(QString("≤ %1 ms").arg(ms),ms);
    nodeFavoritesOnly_ = new QCheckBox("仅收藏"); nodeFavoritesOnly_->setObjectName("nodeFavoritesOnly");
    for (auto* field:QList<QWidget*>{nodeRegion_, nodeProtocol_, nodeHealth_, nodeMaximum_, nodeFavoritesOnly_}) filterPanel->addField(field);
    connect(filterAction,&QAction::triggered,filterPanel,[filterPanel,tools] { QTimer::singleShot(0,filterPanel,[filterPanel,tools] { filterPanel->openBelow(tools); }); });
    for (auto* field:{nodeRegion_,nodeProtocol_,nodeHealth_,nodeMaximum_}) connect(field,&QComboBox::currentIndexChanged,this,&MainWindow::applyNodeFilters);
    connect(nodeFavoritesOnly_,&QCheckBox::toggled,this,&MainWindow::applyNodeFilters);
    auto* viewMenu = more->addMenu("显示方式"); viewMenu->setObjectName("nodeViewMenu");
    for (const auto& item:QList<QPair<QString,int>>{{"行列表",0},{"卡片",1}}) {
        auto* action = viewMenu->addAction(item.first); action->setCheckable(true); action->setChecked(item.second==0);
        connect(action,&QAction::triggered,this,[this,item] { setMemberView(item.second); });
    }
    more->addSeparator();
    auto* preferences = more->addAction("节点选项…"); preferences->setObjectName("nodePreferences"); connect(preferences,&QAction::triggered,this,&MainWindow::editNodePreferences);
    enterGroup_ = more->addAction("进入所选策略组"); enterGroup_->setObjectName("enterGroup"); connect(enterGroup_, &QAction::triggered, this, &MainWindow::enterSelectedGroup);
    backGroup_ = more->addAction("返回上级组"); backGroup_->setObjectName("backGroup"); backGroup_->setVisible(false);
    connect(backGroup_, &QAction::triggered, this, [this] { if (groupHistory_.isEmpty()) return; const auto previous = groupHistory_.takeLast(); selectedGroup_ = previous; search_->clear(); updateProxies(); });
    locateAction_ = more->addAction("定位当前成员"); locateAction_->setObjectName("locateCurrent"); locateAction_->setIcon(UiIcons::icon(UiIcons::Glyph::Locate,UiTheme::Secondary)); connect(locateAction_, &QAction::triggered, this, &MainWindow::locateCurrentNode);
    auto* locate = new QToolButton; locate->setObjectName("nodeLocateButton"); locate->setDefaultAction(locateAction_); locate->setProperty("prototypeIcon",true);
    locate->setIcon(UiIcons::icon(UiIcons::Glyph::Locate,UiTheme::Secondary)); locate->setToolButtonStyle(Qt::ToolButtonIconOnly); locate->setAutoRaise(true);
    headingLayout->addWidget(locate);
    { QList<QLayoutItem*> cells; while (auto* item=headingLayout->takeAt(0)) cells.append(item);
      headingLayout->addWidget(nodeHeadingTitle_);
      headingLayout->addWidget(groupPicker);
      headingLayout->addWidget(nodeCount_);
      headingLayout->addStretch(1);
      for (auto* widget:QList<QWidget*>{search_,searchToggle,locate,test,tools}) headingLayout->addWidget(widget);
      for (auto* cell:cells) delete cell;
    }
    for (auto* control:QList<QToolButton*>{searchToggle,locate,test,tools}) { control->setFixedSize(32,32); control->setIconSize(QSize(20,20)); }
    tools->setMenu(more);
    nodeGroupHint_ = new QLabel; nodeGroupHint_->setObjectName("nodeGroupHint"); nodeGroupHint_->setProperty("secondary", true); nodeGroupHint_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum); nodeGroupHint_->setTextFormat(Qt::PlainText);
    layout->addWidget(nodeGroupHint_);
    layout->addStretch(0); // Absorb surplus below short lists, never between the toolbar and group.

    connect(testAction_, &QAction::triggered, this, [this] { controller_->testNodes(model_.names(),selectedGroup_); });
    // 测选中：两个视图共用一条路径，名字由当前显示的视图决定。
    connect(testSelectedAction_, &QAction::triggered, this, [this] {
        const auto name = memberView() == 1 ? nodeCards_->currentIndex().data(Qt::UserRole).toString()
                                            : nodeRows_->memberAt(nodeList_->currentIndex().row());
        if (!name.isEmpty()) controller_->testNodes({name}, selectedGroup_);
    });
    connect(automaticAction_, &QAction::triggered, this, [this] { controller_->clearAutomaticSelection(selectedGroup_); });
    connect(search_, &QLineEdit::textChanged, &filter_, &QSortFilterProxyModel::setFilterFixedString);
    connect(search_, &QLineEdit::textChanged, this, [this] { updateNodeCount(); updateActions(); });

    // 行列表的三条点击路径：组头折叠、行尾信息按钮看详情、成员按组类型决定是否提交。
    connect(nodeList_, &QListView::clicked, this, [this](const QModelIndex& index) {
        if (!index.isValid()) return;
        const auto row = index.row();
        if (nodeRows_->rowIsHeader(row)) { nodeRows_->setCollapsed(nodeRows_->rowAutomatic(row), !nodeRows_->collapsed(nodeRows_->rowAutomatic(row))); return; }
        selectNodeInHome(index);
    });
    auto* enter = new QShortcut(QKeySequence(Qt::Key_Return), nodeList_); enter->setContext(Qt::WidgetWithChildrenShortcut); connect(enter, &QShortcut::activated, this, &MainWindow::chooseSelectedNode);
    auto* rowTest=new QShortcut(QKeySequence(Qt::Key_F5),nodeList_); rowTest->setContext(Qt::WidgetWithChildrenShortcut); connect(rowTest,&QShortcut::activated,this,[this] { if (testSelectedAction_->isEnabled()) testSelectedAction_->trigger(); });
    auto* rowInfo=new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_I),nodeList_); rowInfo->setContext(Qt::WidgetWithChildrenShortcut); connect(rowInfo,&QShortcut::activated,this,[this] { const auto index=nodeList_->currentIndex(); if (!index.isValid()) return; if (nodeRows_->rowIsHeader(index.row())) editNodePreferences(); else showNodeDetails(nodeRows_->memberAt(index.row())); });
    auto* right = new QShortcut(QKeySequence(Qt::Key_Right), nodeList_); right->setContext(Qt::WidgetWithChildrenShortcut); connect(right, &QShortcut::activated, this, &MainWindow::enterSelectedGroup);
    connect(nodeCards_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { updateActions(); });
    connect(nodeCards_, &QListView::clicked, this, [this](const QModelIndex& index) { if (index.isValid()) selectNodeInHomeFromCards(index); });
    connect(nodeCards_, &QListView::doubleClicked, this, [this] { chooseSelectedNode(); });
    auto* cardEnter = new QShortcut(QKeySequence(Qt::Key_Return), nodeCards_); cardEnter->setContext(Qt::WidgetWithChildrenShortcut); connect(cardEnter, &QShortcut::activated, this, &MainWindow::chooseSelectedNode);
    auto* cardRight = new QShortcut(QKeySequence(Qt::Key_Right), nodeCards_); cardRight->setContext(Qt::WidgetWithChildrenShortcut); connect(cardRight, &QShortcut::activated, this, &MainWindow::enterSelectedGroup);
    static_cast<NodeCardView*>(nodeCards_)->delayClicked = [this](const QModelIndex& index) { if (testAction_->isEnabled() && index.isValid()) controller_->testNodes({index.data(Qt::UserRole).toString()}, selectedGroup_); };
    static_cast<NodeCardView*>(nodeCards_)->infoClicked = [this](const QModelIndex& index) { if (index.isValid()) showNodeDetails(index.data(Qt::UserRole).toString()); };
    auto* retest = new QShortcut(QKeySequence(Qt::Key_F5), nodeCards_); retest->setContext(Qt::WidgetWithChildrenShortcut); connect(retest, &QShortcut::activated, this, [this] { if (testSelectedAction_->isEnabled()) testSelectedAction_->trigger(); });
    auto* cardInfo = new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_I), nodeCards_); cardInfo->setContext(Qt::WidgetWithChildrenShortcut); connect(cardInfo,&QShortcut::activated,this,[this] { const auto index=nodeCards_->currentIndex(); if (index.isValid()) showNodeDetails(index.data(Qt::UserRole).toString()); });
    connect(nodeList_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { updateActions(); updateCurrentMember(); });
    connect(nodeList_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });

    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), page); escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, [this] { if (auto* popup = QApplication::activePopupWidget()) { popup->close(); return; } nodeList_->selectionModel()->clear(); updateActions(); });
    auto* searchShortcut = new QShortcut(QKeySequence::Find, page); searchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(searchShortcut, &QShortcut::activated, search_, [this] { search_->show(); if (auto* button=findChild<QToolButton*>("nodeSearchToggle")) button->setChecked(true); search_->setFocus(Qt::ShortcutFocusReason); search_->selectAll(); });
    return page;
}

void MainWindow::applyNodeFilters() {
    if (!nodeRegion_) return;
    QSet<QString> favorites; for (const auto& value:controller_->nodePreferences(profilesCombo_->currentData().toString()).value("favorites").toArray()) favorites.insert(value.toString());
    model_.setFavorites(favorites);
    filter_.setConditions(nodeRegion_->currentData().toString(),nodeProtocol_->currentData().toString(),nodeHealth_->currentIndex(),nodeMaximum_->currentData().toInt(),nodeFavoritesOnly_->isChecked(),favorites);
    updateNodeCount();
}

// 计数只在标题行显示（原型 01 的 section-count）：过滤后与全部不一致时给出两个数。
void MainWindow::updateNodeCount() {
    if (!nodeCount_) return;
    if (nodeViews_) {
        const int cardWidth=nodeCards_->gridSize().width();
        const int columns=cardWidth>0?qMax(1,nodeCards_->viewport()->width()/cardWidth):1;
        const int rows=memberView()==1?(filter_.rowCount()+columns-1)/columns:nodeRows_->rowCount();
        nodeViews_->setMaximumHeight(rows>0?qMax(64,rows*(memberView()==1?UiTheme::NodeCardHeight:UiTheme::twoLineRowHeight(nodeList_->font()))+4):QWIDGETSIZE_MAX);
    }
    nodeCount_->setText(filter_.rowCount() == model_.rowCount() ? QString("%1 个").arg(model_.rowCount())
                                                                : QString("%1 / %2 个").arg(filter_.rowCount()).arg(model_.rowCount()));
}

QJsonObject MainWindow::nodeBookmark() const {
    return {{"profile",profilesCombo_->currentData().toString()},{"group",selectedGroup_},{"view",memberView()},{"sort",0},{"search",search_->text().left(512)},{"region",nodeRegion_->currentData() .toString()},{"protocol",nodeProtocol_->currentData().toString()},{"health",nodeHealth_->currentIndex()},{"maximum",nodeMaximum_->currentData().toInt()},{"favorites",nodeFavoritesOnly_->isChecked()},{"member",selectedVisibleNode()},{"scroll",memberView()==1?nodeCards_->verticalScrollBar()->value():nodeList_->verticalScrollBar()->value()}};
}

void MainWindow::restoreNodeBookmark(const QJsonObject& value) {
    if (value.value("profile").toString()!=controller_->state().activeProfile || value.value("group").toString().isEmpty()) return;
    selectGroup(value.value("group").toString()); setMemberView(value.value("view").toInt()); search_->setText(value.value("search").toString().left(512));
    nodeRegion_->setCurrentIndex(qMax(0,nodeRegion_->findData(value.value("region").toString()))); nodeProtocol_->setCurrentIndex(qMax(0,nodeProtocol_->findData(value.value("protocol").toString()))); nodeHealth_->setCurrentIndex(qBound(0,value.value("health").toInt(),3)); nodeMaximum_->setCurrentIndex(qMax(0,nodeMaximum_->findData(value.value("maximum").toInt()))); nodeFavoritesOnly_->setChecked(value.value("favorites").toBool());
    // 行列表在 updateProxies 之后才填好；恢复书签要等它出现，否则选中位会落空。
    const auto member=value.value("member").toString();
    const auto scrollTo=qMax(0,value.value("scroll").toInt());
    const auto restore=[this,member,scrollTo] {
        const auto row=nodeRows_->rowOfMember(member);
        if (row>=0) {
            if (memberView()==1) {
                for (int i=0;i<filter_.rowCount();++i) if (filter_.index(i,0).data(Qt::UserRole).toString()==member) { nodeCards_->setCurrentIndex(filter_.index(i,0)); break; }
            } else nodeList_->setCurrentIndex(nodeRows_->indexForRow(row));
        }
        auto* scroll=memberView()==1?nodeCards_->verticalScrollBar():nodeList_->verticalScrollBar();
        scroll->setValue(scrollTo);
        return row>=0 || member.isEmpty();
    };
    if (restore()) return;
    QTimer::singleShot(0,this,[restore] { restore(); });
}

void MainWindow::editNodePreferences() {
    const auto id=profilesCombo_->currentData().toString(),group=selectedGroup_; if (id.isEmpty()||group.isEmpty()||controller_->state().busy) return;
    if (groupEditor_) {
        if (groupEditor_->property("profileId").toString()==id&&groupEditor_->property("groupName").toString()==group) { groupEditor_->show(); return; }
        if (!DialogDraftGuard::allowLeave(groupEditor_)) return;
        groupEditor_->done(QDialog::Rejected); groupEditor_=nullptr;
    }
    const auto expected=controller_->nodePreferences(id),initial=expected.value("groups").toObject().value(group).toObject();
    auto* dialog=new TaskDialog(this); groupEditor_=dialog; dialog->setProperty("profileId",id); dialog->setProperty("groupName",group); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("nodePreferencesDialog"); dialog->setWindowTitle("节点选项"); dialog->resize(510,280); auto* box=new QVBoxLayout(dialog); auto* label=new QLabel(group); label->setTextFormat(Qt::PlainText); label->setObjectName("sectionTitle"); box->addWidget(label);
    auto* form=new QFormLayout; auto* url=new QLineEdit(initial.value("test_url").toString()); url->setObjectName("groupTestUrl"); url->setMaxLength(2048); url->setPlaceholderText("留空沿用默认测试地址"); form->addRow("本组延迟测试地址",url);
    auto* view=new CompactComboBox; view->addItems({"行列表","卡片"}); view->setCurrentIndex(memberView()); form->addRow("默认视图",view); box->addLayout(form);
    auto* common=new QCheckBox("作为常用组，在列表和托盘中优先显示"); common->setChecked(expected.value("common_groups").toArray().contains(group)); box->addWidget(common);
    auto* subscriptionScope=new QLabel("此订阅的切换行为"); subscriptionScope->setProperty("secondary",true); box->addWidget(subscriptionScope);
    auto* disconnect=new QCheckBox("切换成员后断开受影响连接"); disconnect->setObjectName("disconnectAffected"); disconnect->setChecked(expected.value("disconnect_affected").toBool()); disconnect->setToolTip("此开关用于该订阅的所有策略组。默认保留。只处理切换前经过所切策略组的连接，单次最多32条；其他连接保留，结果以回读为准。"); box->addWidget(disconnect);
    auto* note=new QLabel; note->setTextFormat(Qt::PlainText); note->setWordWrap(true); box->addWidget(note); auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText("保存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); box->addWidget(buttons);
    const auto draft=[expected,group,url,view,common,disconnect] { auto value=expected; auto groups=value.value("groups").toObject(); groups[group]=QJsonObject{{"test_url",url->text().trimmed()},{"view",view->currentIndex()}}; value["groups"]=groups; value["last_group"]=group; value["disconnect_affected"]=disconnect->isChecked(); auto commons=value.value("common_groups").toArray(); for (int i=commons.size()-1;i>=0;--i) if (commons[i].toString()==group) commons.removeAt(i); if (common->isChecked()) commons.append(group); value["common_groups"]=commons; return value; };
    const auto baseline=draft(); auto pending=std::make_shared<bool>(false); DialogDraftGuard::protect(dialog,[draft,baseline] { return draft()!=baseline; },[pending] { return *pending; }); connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,id,expected,draft,pending,buttons] { *pending=true; buttons->setEnabled(false); controller_->updateNodePreferences(id,draft(),expected); });
    connect(controller_,&AppController::operationFinished,dialog,[this,dialog,pending,buttons,note,view](bool ok,const QString& message) { if (!*pending) return; *pending=false; buttons->setEnabled(true); if (ok) { setMemberView(view->currentIndex()); dialog->accept(); } else note->setText(message); });
    dialog->setProperty("embeddedTask",true); dialog->setParent(nodeSection_,Qt::Widget); dialog->setMinimumSize(0,0); dialog->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    static_cast<QVBoxLayout*>(nodeSection_->layout())->insertWidget(1,dialog);
    connect(dialog,&QDialog::finished,dialog,&QObject::deleteLater); dialog->show();
}

void MainWindow::updateProxies() {
    const auto lastGroup = selectedGroup_;
    const bool viewingActive = !controller_->state().activeProfile.isEmpty() && profilesCombo_->currentData().toString() == controller_->state().activeProfile;
    if (auto* use = findChild<QPushButton*>("useBrowsedProfile")) {
        use->setVisible(false);
        use->setEnabled(!controller_->state().busy && !controller_->state().readOnly);
    }
    const auto proxies = viewingActive ? controller_->proxies() : QJsonObject{}; QStringList groups;
    for (auto it = proxies.begin(); it != proxies.end(); ++it) if (it.value().toObject().value("all").isArray()) groups.append(it.key());
    groups.sort(Qt::CaseInsensitive);
    const auto preferences=controller_->nodePreferences(profilesCombo_->currentData().toString()); const auto common=preferences.value("common_groups").toArray(); std::stable_sort(groups.begin(),groups.end(),[common](const QString& a,const QString& b) { return common.contains(a)&&!common.contains(b); });
    if (selectedGroup_.isEmpty()&&groups.contains(preferences.value("last_group").toString())) selectedGroup_=preferences.value("last_group").toString();
    if (!groups.contains(selectedGroup_)) {
        selectedGroup_.clear(); groupHistory_.clear(); int largest = -1;
        for (const auto& name : groups) if (name != "GLOBAL" || groups.size() == 1) {
            const int count = proxies.value(name).toObject().value("all").toArray().size();
            if (count > largest) { selectedGroup_ = name; largest = count; }
        }
    }
    // 组菜单随可用组刷新；勾选表示当前浏览的组。空组列表时整个子菜单禁用。
    groupMenu_->clear();
    for (const auto& name : groups) {
        auto* action = groupMenu_->addAction(name); action->setCheckable(true); action->setChecked(name == selectedGroup_);
        action->setEnabled(!name.isEmpty());
        connect(action, &QAction::triggered, this, [this, name] { selectGroup(name); });
    }
    if (groups.isEmpty()) groupMenu_->addAction("没有可浏览的策略组")->setEnabled(false);
    groupMenu_->menuAction()->setEnabled(!groups.isEmpty());
    const bool groupChanged=selectedGroup_!=lastGroup;
    updateGroup();
    if (groupChanged) { const auto stored=preferences.value("groups").toObject().value(selectedGroup_).toObject(); if (stored.contains("view")) setMemberView(stored.value("view").toInt()); }
    updateHome();
}

void MainWindow::updateGroup() {
    if (groupEditor_) groupEditor_->setVisible(groupEditor_->property("profileId").toString()==profilesCombo_->currentData().toString()&&groupEditor_->property("groupName").toString()==selectedGroup_);
    websitePage_->suggestGroup(selectedGroup_);
    const auto group = selectedGroup_; const bool sameProfile = profilesCombo_->currentData().toString() == controller_->state().activeProfile;
    const auto object = controller_->proxies().value(group).toObject();
    const auto type = object.value("type").toString();
    const bool automatic = QStringList{"URLTest", "Fallback"}.contains(type);
    model_.setTestContext(controller_->groupTestUrl(group)); model_.setGroup(sameProfile ? controller_->proxies() : QJsonObject{}, group);
    model_.setCurrent(object.value("now").toString());
    controller_->setHomeProbeGroup(sameProfile?group:QString());
    for (const auto& name:model_.names()) { const auto result=controller_->trayNodeDelay(name,group); if (result.value("cached").toBool()) model_.setDelay(name,result.value("delay").toInt(-1),result.value("error").toString(),QDateTime::fromString(result.value("measured_at").toString(),Qt::ISODateWithMs)); }
    if (nodeRegion_) {
        const QSignalBlocker a(nodeRegion_),b(nodeProtocol_); const auto oldRegion=nodeRegion_->currentData().toString(),oldProtocol=nodeProtocol_->currentData().toString(); QSet<QString> regions,protocols; for (const auto& name:model_.names()) { const auto code=NodePresentation::location(name).code; if (!code.isEmpty()) regions.insert(code); protocols.insert(controller_->proxies().value(name).toObject().value("type").toString()); }
        auto regionList=regions.values(),protocolList=protocols.values(); regionList.sort(); protocolList.sort();
        const auto matches=[](QComboBox* combo,const QStringList& values) {
            if (combo->count()!=values.size()+1) return false;
            for (int i=0;i<values.size();++i) if (combo->itemData(i+1).toString()!=values[i]) return false;
            return true;
        };
        if (!matches(nodeRegion_,regionList)) {
            nodeRegion_->clear(); nodeRegion_->addItem("全部地区","");
            for (const auto& code:regionList) nodeRegion_->addItem(NodePresentation::icon(code),NodePresentation::location(code).label+" ("+code+")",code);
            nodeRegion_->setCurrentIndex(qMax(0,nodeRegion_->findData(oldRegion)));
        }
        if (!matches(nodeProtocol_,protocolList)) {
            nodeProtocol_->clear(); nodeProtocol_->addItem("全部协议","");
            for (const auto& type:protocolList) nodeProtocol_->addItem(type,type);
            nodeProtocol_->setCurrentIndex(qMax(0,nodeProtocol_->findData(oldProtocol)));
        }
    }
    // 组类型决定整段归到「手动选择」还是「自动策略组」（原型 01 的两个分组）。
    nodeRows_->setGroupContext(group,object.value("now").toString());
    for (const auto& source:controller_->profiles()) if (source.id==profilesCombo_->currentData().toString()) {
        const auto when=QDateTime::fromString(source.updated,Qt::ISODate).toLocalTime();
        nodeRows_->setSourceContext(source.name,(when.isValid()?when.toString("MM-dd HH:mm")+" · ":QString())+group); break;
    }
    nodeRows_->setGroupAutomatic(automatic);
    nodeRows_->setCurrent(object.value("now").toString());
    applyNodeFilters();
    const bool fixed = !object.value("fixed").toString().isEmpty();
    if (auto* picker=findChild<QToolButton*>("browseGroup")) {
        picker->setText(group.isEmpty()?QString("选择策略组"):QFontMetrics(picker->font()).elidedText(group,Qt::ElideRight,104));
        picker->setToolTip(automatic ? group + (fixed ? QString(" · 已手动固定") : QString(" · 自动选择中")) : group + QString(" · 手动选择"));
    }
    nodeGroupHint_->setText(group.isEmpty() ? QString("无节点：导入并激活订阅，或检查内核状态。")
        : automatic ? QString("自动策略组由内核按规则选路，不能手动切换组内成员%1").arg(fixed ? QString("；当前已手动固定") : QString())
        : QString("单击成员即切换；自动策略组不可手动切换。"));
    nodeGroupHint_->hide();
    nodeList_->setToolTip(nodeGroupHint_->text()); nodeCards_->setToolTip(nodeGroupHint_->text());
    updateCurrentMember();
    updateNodeCount();
    // 构造期 updateProxies() 会走到这里，此时订阅/设置页可能还不存在，updateActions 自己判空。
    updateActions(); updateHome();
}

void MainWindow::selectGroup(QString group, bool nested) {
    if (!controller_->proxies().value(group).toObject().value("all").isArray()) return;
    if (nested) {
        if (group == selectedGroup_ || groupHistory_.contains(group) || groupHistory_.size() >= 16) {
            message_->setText("策略组引用形成循环或层级过深，未继续进入"); message_->setToolTip(message_->text()); return;
        }
        groupHistory_.append(selectedGroup_);
    } else groupHistory_.clear();
    if (selectedGroup_ != group) search_->clear(); selectedGroup_ = group;
    updateProxies();
    const auto preferences=controller_->nodePreferences(profilesCombo_->currentData().toString()).value("groups").toObject().value(group).toObject(); if (preferences.contains("view")) setMemberView(preferences.value("view").toInt());
}

void MainWindow::enterSelectedGroup() {
    const auto name = selectedVisibleNode(); if (name.isEmpty()) return;
    if (!controller_->proxies().value(name).toObject().value("all").isArray()) return;
    if (!(memberView()==1?nodeCards_:nodeList_)->isEnabled()) return;
    selectGroup(name, true);
}

QString MainWindow::selectedVisibleNode() const {
    return memberView()==1 ? nodeCards_->currentIndex().data(Qt::UserRole).toString()
                           : nodeRows_->memberAt(nodeList_->currentIndex().row());
}

void MainWindow::updateCurrentMember() {
    const auto now = controller_->proxies().value(selectedGroup_).toObject().value("now").toString();
    const bool viewingActive = profilesCombo_->currentData().toString() == controller_->state().activeProfile;
    // 底部状态条已删除（H3）：当前成员与选路说明改由组头（计数 + 「内核自动选路」）
    // 与成员行勾选表达；这里只维护行的当前标记与组的选路提示。
    nodeRows_->setCurrent(viewingActive ? now : QString());
    if (nodeGroupHint_) {
        const auto type = controller_->proxies().value(selectedGroup_).toObject().value("type").toString();
        const QString hint = type == "Selector" ? QString("单击成员即切换；自动策略组不可手动切换。")
            : (type == "URLTest" || type == "Fallback") ? QString("自动策略组由内核选路，不能手动切换组内成员。") : QString();
        nodeGroupHint_->setText(hint); nodeGroupHint_->hide();
    }
}

void MainWindow::rebuildRecentMenu() {
    recentMenu_->clear(); const auto group = selectedGroup_; const auto members = controller_->proxies().value(group).toObject().value("all").toArray();
    for (const auto& profile : controller_->profiles()) if (profile.id == controller_->state().activeProfile) for (const auto& value : profile.recentSelections) {
        const auto pair = value.toObject(); const auto member = pair.value("member").toString(); if (pair.value("group").toString() != group || !members.contains(member)) continue;
        auto* action = recentMenu_->addAction(member); action->setCheckable(true); action->setChecked(member == controller_->proxies().value(group).toObject().value("now").toString()); connect(action, &QAction::triggered, this, [this, group, member] { controller_->selectMember(group, member); });
    }
    if (recentMenu_->isEmpty()) recentMenu_->addAction("暂无最近选择")->setEnabled(false);
}

// 行列表的写入入口：只有手动组（Selector）单击/Enter 才提交；自动组只移动高亮。
// index 是 NodeRowModel 的行号，成员名经 memberAt 取回。
void MainWindow::selectNodeInHome(const QModelIndex& index) {
    if (!index.isValid()) return;
    const auto name=nodeRows_->memberAt(index.row()); if (nodeRows_->rowAutomatic(index.row())) { showNodeDetails(name); return; } commitMemberSelection(name);
}
// 卡片视图的 index 直接带成员名（过滤模型的 UserRole），与行列表共用同一条守卫。
void MainWindow::selectNodeInHomeFromCards(const QModelIndex& index) {
    if (!index.isValid()) return;
    const auto name=index.data(Qt::UserRole).toString(); if (controller_->proxies().value(selectedGroup_).toObject().value("type").toString()!="Selector") { showNodeDetails(name); return; } commitMemberSelection(name);
}
// 唯一的切换守卫：自动组不提交，重复点当前成员不提交，忙碌/只读不提交。
void MainWindow::commitMemberSelection(const QString& name) {
    if (name.isEmpty()) return;
    const auto& state = controller_->state();
    const auto group = controller_->proxies().value(selectedGroup_).toObject();
    if (group.value("type").toString() != "Selector") { updateActions(); return; }
    if (name == group.value("now").toString() || state.busy || state.exiting || state.readOnly) { updateActions(); return; }
    controller_->selectMember(selectedGroup_, name);
}

void MainWindow::chooseSelectedNode() {
    // Enter and double click must address the visible view, never a stale row
    // selection in the other view. Reuse the same write guard as a single click.
    if (controller_->state().busy || controller_->state().exiting || controller_->state().readOnly) return;
    const bool inCards = memberView() == 1;
    const auto index = inCards ? nodeCards_->currentIndex() : nodeList_->currentIndex();
    if (!index.isValid()) return;
    const auto name = inCards ? index.data(Qt::UserRole).toString() : nodeRows_->memberAt(index.row());
    if (name.isEmpty()) return;
    const auto type = controller_->proxies().value(selectedGroup_).toObject().value("type").toString();
    if (type != "Selector") { showNodeDetails(name); return; }
    commitMemberSelection(name);
}

void MainWindow::locateCurrentNode() {
    const auto now=controller_->proxies().value(selectedGroup_).toObject().value("now").toString(); if (now.isEmpty()) return;
    if (nodeRows_->rowOfMember(now)<0) {
        revealedNodeFilters_=nodeBookmark();
        if (auto* restore=findChild<QPushButton*>("restoreNodeFilters")) restore->show();
        search_->clear(); for (auto* box:{nodeRegion_,nodeProtocol_,nodeHealth_,nodeMaximum_}) box->setCurrentIndex(0); nodeFavoritesOnly_->setChecked(false); applyNodeFilters();
    }
    if (memberView()==1) {
        for (int row=0;row<filter_.rowCount();++row) if (filter_.index(row,0).data(Qt::UserRole).toString()==now) { const auto index=filter_.index(row,0); nodeCards_->setCurrentIndex(index); nodeCards_->scrollTo(index,QAbstractItemView::PositionAtCenter); nodeCards_->setFocus(); break; }
    } else {
        const auto row=nodeRows_->rowOfMember(now); if (row<0) return; const auto index=nodeRows_->indexForRow(row); nodeList_->setCurrentIndex(index); nodeList_->scrollTo(index,QAbstractItemView::PositionAtCenter); nodeList_->setFocus();
    }
}


// 「节点详情」（原型 04）：行尾信息按钮打开右侧抽屉（410px，贴主窗右缘），
// 不再用模态对话框。自动组不提供切换入口，只给一条说明。
void MainWindow::showNodeDetails(const QString& name) {
    if (name.isEmpty()) return;
    const auto group = selectedGroup_;
    const auto object = controller_->proxies().value(group).toObject();
    const bool automatic = QStringList{"URLTest", "Fallback"}.contains(object.value("type").toString());
    const auto node = controller_->proxies().value(name).toObject();
    auto* drawer = qobject_cast<DetailsDrawer*>(detailsDrawer_.data());
    if (!drawer) {
        auto* host = findChild<QWidget*>("contentSurface");
        drawer = new DetailsDrawer(host);
        detailsDrawer_ = drawer;
    }
    drawer->clearContent();
    drawer->setHeader(name, QString("%1 · 节点详情").arg(group));
    const auto region = NodePresentation::location(name);
    drawer->addKeyValue("策略组", group);
    drawer->addKeyValue("类型", node.value("all").isArray() ? QString("子策略组") : node.value("type").toString("未知"));
    drawer->addKeyValue("位置", region.code.isEmpty() ? QString("未识别（按名称判断，不代表实际 IP）") : region.label + " · " + region.code);
    auto* latencyValue=drawer->addKeyValue("延迟", model_.data(model_.index(model_.names().indexOf(name), 2)).toString());
    drawer->addKeyValue("选择状态", automatic ? (name == object.value("now").toString() ? QString("内核当前选路") : QString("未被选中"))
                                             : name == object.value("now").toString() ? QString("当前生效") : QString("未选中"));
    drawer->addHint(automatic ? QString("该策略组由内核按规则自动选路，不能通过点击成员强制改写当前结果。")
                              : QString("延迟只表示最近一次测速结果，不代表所有网站的实际访问性能。"));
    drawer->addKeyValue("测试目标",controller_->groupTestUrl(group));
    auto* operations=new QWidget; auto* column=new QVBoxLayout(operations); column->setContentsMargins(0,0,0,0); column->setSpacing(8); auto* rows=PrototypeUi::group(column,"操作");
    const auto profile=controller_->state().activeProfile; QString version; for (const auto& item:controller_->profiles()) if (item.id==profile) version=item.active;
    const auto url=controller_->groupTestUrl(group);
    const auto contextCurrent=[this,profile,version,group,url] { if (controller_->state().activeProfile!=profile||controller_->groupTestUrl(group)!=url) return false; for (const auto& item:controller_->profiles()) if (item.id==profile) return item.active==version; return false; };
    connect(controller_,&AppController::nodeDelay,latencyValue,[this,latencyValue,name,group](const QString& tested,int delay,const QString& error) { if (tested==name&&controller_->lastNodeTestUrl()==controller_->groupTestUrl(group)) latencyValue->setText(error.isEmpty()?QString("%1 ms").arg(delay):error); });
    auto* favorite=PrototypeUi::row(rows,"nodeDetailsFavorite","收藏节点",UiIcons::Glyph::Layers);
    const auto updateFavorite=[this,favorite,profile,name] { const bool saved=controller_->nodePreferences(profile).value("favorites").toArray().contains(name); favorite->setTitle(saved?"取消收藏":"收藏节点"); };
    connect(controller_,&AppController::profilesChanged,favorite,updateFavorite); updateFavorite();
    connect(favorite,&QPushButton::clicked,this,[this,profile,name,contextCurrent] {
        if (!contextCurrent()) return;
        const auto expected=controller_->nodePreferences(profile); auto next=expected; auto values=next.value("favorites").toArray();
        if (values.contains(name)) { for (int i=values.size()-1;i>=0;--i) if (values[i].toString()==name) values.removeAt(i); } else values.append(name);
        next["favorites"]=values; controller_->updateNodePreferences(profile,next,expected);
    });
    auto* retest=PrototypeUi::row(rows,"nodeDetailsRetest","重新测试",UiIcons::Glyph::Gauge);
    const auto enabled=[this,retest,favorite,contextCurrent] { const auto& state=controller_->state(); const bool valid=state.ready&&contextCurrent()&&!state.busy&&!state.exiting; retest->setEnabled(valid); favorite->setEnabled(valid&&!state.readOnly); };
    connect(controller_,&AppController::stateChanged,retest,enabled); enabled();
    connect(retest,&QPushButton::clicked,this,[this,name,group,contextCurrent] { if (contextCurrent()) controller_->testNodes({name},group); });
    drawer->addWidget(operations);
    drawer->open();
}

void MainWindow::setMemberView(int mode) {
    const int index = mode == 1 ? 1 : 0;
    if (!nodeViews_ || nodeViews_->currentIndex() == index) return;
    const auto name=selectedVisibleNode();
    nodeViews_->setCurrentIndex(index); updateNodeCount();
    if (!name.isEmpty()) {
        const auto row = model_.names().indexOf(name);
        if (row>=0) {
            const auto source = filter_.mapFromSource(model_.index(row, 0));
            if (index == 1 && source.isValid()) { nodeCards_->setCurrentIndex(source); nodeCards_->scrollTo(source); }
            else if (index == 0) { const auto target=nodeRows_->indexForRow(nodeRows_->rowOfMember(name)); if (target.isValid()) { nodeList_->setCurrentIndex(target); nodeList_->scrollTo(target); } }
        }
    }
    updateActions();
}

}
