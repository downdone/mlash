#include "MainWindowParts.h"
#include "WrapLayout.h"

namespace mlash {
using namespace WindowParts;
QWidget* MainWindow::proxyPage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(8);
    auto* top = new QHBoxLayout; auto* pageTitle = new QLabel("代理"); pageTitle->setObjectName("pageTitle"); top->addWidget(pageTitle); top->addStretch();
    auto* modeControl=new QFrame; modeControl->setObjectName("modeControl"); auto* modes=new QHBoxLayout(modeControl); modes->setContentsMargins(3,3,3,3); modes->setSpacing(1); top->addWidget(modeControl);
    for (const auto& mode : QList<QPair<QString, QString>>{{"规则", "rule"}, {"全局", "global"}, {"直连", "direct"}}) {
        auto* control = button(mode.first, modes); control->setCheckable(true); control->setProperty("mode", mode.second); proxyModes_.append(control);
        connect(control, &QPushButton::clicked, this, [this, mode] { controller_->setMode(mode.second); updateState(); });
    }
    layout->addLayout(top);
    auto* subscription = panel(); subscription->setObjectName("proxyContext"); auto* subscriptionLayout = new QVBoxLayout(subscription); subscriptionLayout->setContentsMargins(0,0,0,0);
    auto* row = new WrapLayout; row->addWidget(new QLabel("订阅")); profilesCombo_ = new CompactComboBox; profilesCombo_->setObjectName("proxyProfilesCombo"); profilesCombo_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed); profilesCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); profilesCombo_->setMinimumContentsLength(8); row->addWidget(profilesCombo_);
    useProfile_ = new QPushButton("使用此订阅"); useProfile_->setObjectName("useProxyProfile"); useProfile_->setProperty("primary", true); auto reserved=useProfile_->sizePolicy(); reserved.setRetainSizeWhenHidden(true); useProfile_->setSizePolicy(reserved); subscriptionLayout->addLayout(row);
    proxyProfileHint_ = new QLabel(subscription); proxyProfileHint_->setObjectName("secondaryText"); proxyProfileHint_->hide(); layout->addWidget(subscription);
    connect(useProfile_, &QPushButton::clicked, this, [this] { controller_->activateProfile(profilesCombo_->currentData().toString()); });
    connect(profilesCombo_, &QComboBox::currentIndexChanged, this, [this] { groupHistory_.clear(); updateProxies(); });
    layout->removeWidget(subscription); auto* groupLayout = subscriptionLayout; groupLayout->setSpacing(6);
    auto* groupTools = row; selectedGroupTitle_=new ElidedLabel("选择策略组",subscription); selectedGroupTitle_->setObjectName("sectionTitle"); selectedGroupTitle_->hide();
    groupSwitcher_=new QToolButton; groupSwitcher_->setObjectName("groupSwitcher"); groupSwitcher_->setText("切换策略组"); groupSwitcher_->setCheckable(true); groupSwitcher_->setArrowType(Qt::DownArrow); groupSwitcher_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); groupSwitcher_->setToolTip("展开策略组列表（Ctrl+G）；仅改变浏览组"); groupTools->addWidget(groupSwitcher_);
    auto* providers = new QPushButton("提供者",subscription); providers->setObjectName("proxyProviders"); connect(providers, &QPushButton::clicked, this, [this] { showProviders(profilesCombo_->currentData().toString(), "proxy-providers"); });
    automaticAction_ = new QPushButton("恢复自动"); groupTools->addWidget(automaticAction_); automaticAction_->setObjectName("automaticAction");
    proxyGroups_=new QListWidget; proxyGroups_->setObjectName("proxyGroups"); proxyGroups_->setViewMode(QListView::IconMode); proxyGroups_->setResizeMode(QListView::Adjust); proxyGroups_->setMovement(QListView::Static); proxyGroups_->setWrapping(true); proxyGroups_->setGridSize(QSize(154,54)); proxyGroups_->setItemDelegate(new GroupCardDelegate(proxyGroups_)); proxyGroups_->setSelectionMode(QAbstractItemView::SingleSelection); proxyGroups_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); proxyGroups_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); proxyGroups_->setFixedHeight(110); proxyGroups_->setAccessibleName("策略组卡片"); groupLayout->addWidget(proxyGroups_); layout->addWidget(subscription);
    connect(proxyGroups_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) { if (!updating_ && item && !proxyGroups_->isVisible()) selectGroup(item->text()); });
    for (const auto signal:{&QListWidget::itemClicked,&QListWidget::itemActivated}) connect(proxyGroups_,signal,this,[this](QListWidgetItem* item) { if (item) selectGroup(item->text()); });
    proxyGroups_->hide(); connect(groupSwitcher_,&QToolButton::toggled,this,[this](bool expanded) { proxyGroups_->setVisible(expanded); groupSwitcher_->setArrowType(expanded?Qt::UpArrow:Qt::DownArrow); if (expanded) { proxyGroups_->scrollToItem(proxyGroups_->currentItem()); proxyGroups_->setFocus(Qt::ShortcutFocusReason); } });
    auto* switchGroup=new QShortcut(QKeySequence("Ctrl+G"),page); switchGroup->setContext(Qt::WidgetWithChildrenShortcut); connect(switchGroup,&QShortcut::activated,groupSwitcher_,[this] { if (groupSwitcher_->isEnabled()) groupSwitcher_->toggle(); });
    auto* nodesPanel = panel(); nodesPanel->setObjectName("nodeContent"); auto* nodesLayout = new QVBoxLayout(nodesPanel); nodesLayout->setContentsMargins(0,0,0,0); nodesLayout->setSpacing(8);
    auto* toolSurface=new QWidget; auto* tools = new WrapLayout(toolSurface); search_ = new QLineEdit; search_->setObjectName("nodeSearch"); search_->setPlaceholderText("搜索节点名称…"); search_->setClearButtonEnabled(true); tools->addWidget(search_);
    backGroup_ = new QPushButton("返回上级"); tools->addWidget(backGroup_); backGroup_->setObjectName("backGroup"); backGroup_->setVisible(false);
    connect(backGroup_, &QPushButton::clicked, this, [this] { if (groupHistory_.isEmpty()) return; const auto previous = groupHistory_.takeLast(); selectedGroup_ = previous; search_->clear(); updateProxies(); });
    auto* memberSort = new CompactComboBox; memberSort->setObjectName("memberSort"); memberSort->addItem("默认排序", -1); memberSort->addItem("按名称排序", 0); memberSort->addItem("按延迟排序", 2); memberSort->setToolTip("默认保留订阅成员顺序；名称与延迟按升序排列"); tools->addWidget(memberSort);
    auto* viewMode=memberViewMode_=new CompactComboBox; viewMode->setObjectName("nodeViewMode"); viewMode->addItems({"表格","卡片"}); viewMode->setAccessibleName("节点视图"); viewMode->setToolTip("切换展示方式，搜索、排序、选择与测速共用"); tools->addWidget(viewMode);
    nodeCount_ = new QLabel; nodeCount_->setObjectName("secondaryText"); auto* testMenu = new QMenu(this);
    testAction_ = testMenu->addAction("测试本组"); testAction_->setObjectName("testGroup"); testSelectedAction_ = testMenu->addAction("测试选中成员");
    auto* test = new ChevronToolButton; test->setObjectName("testGroupButton"); test->setDefaultAction(testAction_); test->setMenu(testMenu); test->setPopupMode(QToolButton::MenuButtonPopup);
    testProgress_ = new QLabel; testProgress_->setObjectName("testProgress");
    cancelTestAction_ = new QAction("取消测速", this); cancelTestAction_->setObjectName("cancelNodeTests"); cancelTestButton_ = new QToolButton; cancelTestButton_->setObjectName("cancelNodeTestButton"); cancelTestButton_->setDefaultAction(cancelTestAction_); cancelTestButton_->setProperty("quiet",true); nodesLayout->addWidget(toolSurface);
    auto* disclose=new QPushButton("筛选"); disclose->setCheckable(true); disclose->setObjectName("nodeFilterDisclosure"); groupTools->addWidget(disclose); auto* preferences=new QPushButton("本组偏好…",subscription); preferences->setObjectName("nodePreferences"); connect(preferences,&QPushButton::clicked,this,&MainWindow::editNodePreferences); providers->hide(); preferences->hide();
    auto* management=new MenuPushButton; management->setObjectName("proxyManagement"); management->setText("管理"); auto* managementMenu=new QMenu(management); management->setMenu(managementMenu); groupTools->addWidget(management); row->addWidget(useProfile_);
    auto* groupPreferences=managementMenu->addAction("本组偏好…"); connect(groupPreferences,&QAction::triggered,preferences,&QPushButton::click);
    auto* providerManagement=managementMenu->addAction("代理提供者…"); connect(providerManagement,&QAction::triggered,providers,&QPushButton::click);
    connect(managementMenu,&QMenu::aboutToShow,this,[this,groupPreferences,providerManagement] { groupPreferences->setEnabled(!selectedGroup_.isEmpty()&&!controller_->state().busy); providerManagement->setEnabled(!profilesCombo_->currentData().toString().isEmpty()&&!controller_->state().busy); });
    auto* conditions=new QWidget; conditions->setObjectName("nodeFilterConditions"); auto* fields=new QHBoxLayout(conditions); fields->setContentsMargins(0,0,0,0);
    nodeRegion_=new CompactComboBox; nodeRegion_->setObjectName("nodeRegion"); nodeRegion_->addItem("全部地区",""); nodeRegion_->setToolTip("位置依据名称，不代表实际 IP");
    nodeProtocol_=new CompactComboBox; nodeProtocol_->setObjectName("nodeProtocol"); nodeProtocol_->addItem("全部协议","");
    nodeHealth_=new CompactComboBox; nodeHealth_->setObjectName("nodeHealth"); nodeHealth_->addItems({"全部状态","有延迟结果","测速失败","尚未测速"});
    nodeMaximum_=new CompactComboBox; nodeMaximum_->setObjectName("nodeMaximumDelay"); nodeMaximum_->addItem("不限延迟",0); for (const auto ms:{100,200,500,1000}) nodeMaximum_->addItem(QString("≤ %1 ms").arg(ms),ms);
    nodeFavoritesOnly_=new QCheckBox("仅收藏"); nodeFavoritesOnly_->setObjectName("nodeFavoritesOnly"); for (auto* field:{nodeRegion_,nodeProtocol_,nodeHealth_,nodeMaximum_}) { fields->addWidget(field,1); connect(field,&QComboBox::currentIndexChanged,this,&MainWindow::applyNodeFilters); } fields->addWidget(nodeFavoritesOnly_); connect(nodeFavoritesOnly_,&QCheckBox::toggled,this,&MainWindow::applyNodeFilters); conditions->hide(); nodesLayout->addWidget(conditions); connect(disclose,&QPushButton::toggled,conditions,&QWidget::setVisible);
    recentNodes_ = new MenuPushButton("最近选择"); recentNodes_->setObjectName("recentNodes"); recentMenu_ = new QMenu(recentNodes_); recentNodes_->setMenu(recentMenu_); tools->addWidget(recentNodes_); tools->addWidget(test); connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentMenu);
    connect(testAction_, &QAction::triggered, this, [this] { controller_->testNodes(model_.names(),selectedGroup_); });
    connect(automaticAction_, &QPushButton::clicked, this, [this] { controller_->clearAutomaticSelection(selectedGroup_); });
    connect(testSelectedAction_, &QAction::triggered, this, [this] { const auto rows = nodes_->selectionModel()->selectedRows(); if (rows.isEmpty()) return; const auto i = filter_.mapToSource(rows.first()); if (i.isValid()) controller_->testNodes({model_.nameAt(i.row())},selectedGroup_); });
    connect(cancelTestAction_, &QAction::triggered, controller_, &AppController::cancelTests);
    groupHint_ = new QLabel(page); groupHint_->hide();
    nodes_ = new QTableView; filter_.setSourceModel(&model_); filter_.setFilterCaseSensitivity(Qt::CaseInsensitive); filter_.setFilterKeyColumn(0); filter_.setSortRole(Qt::UserRole);
    nodes_->setModel(&filter_);
    const auto favoriteMenu=[this](QWidget* view,const QPoint& point) { const auto index=qobject_cast<QAbstractItemView*>(view)->indexAt(point); if (!index.isValid()) return; const auto name=filter_.mapToSource(index).data(Qt::UserRole).toString(); const auto id=profilesCombo_->currentData().toString(); const auto expected=controller_->nodePreferences(id); auto* menu=new QMenu(view); menu->setAttribute(Qt::WA_DeleteOnClose); const bool saved=expected.value("favorites").toArray().contains(name); auto* action=menu->addAction(saved?"取消收藏此成员":"收藏此成员"); action->setEnabled(!controller_->state().busy&&!controller_->state().readOnly); connect(action,&QAction::triggered,this,[this,id,name,expected,saved] { auto next=expected; auto values=next.value("favorites").toArray(); if (saved) for (int i=values.size()-1;i>=0;--i) { if (values[i].toString()==name) values.removeAt(i); } else values.append(name); next["favorites"]=values; controller_->updateNodePreferences(id,next,expected); }); connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater); menu->popup(view->mapToGlobal(point)); };
    nodes_->setContextMenuPolicy(Qt::CustomContextMenu); connect(nodes_,&QWidget::customContextMenuRequested,this,[this,favoriteMenu](const QPoint& p) { favoriteMenu(nodes_,p); }); nodes_->setSortingEnabled(false); nodes_->setSelectionBehavior(QAbstractItemView::SelectRows); nodes_->setSelectionMode(QAbstractItemView::SingleSelection);
    nodes_->setObjectName("nodesTable"); nodes_->setMinimumHeight(120); nodes_->setEditTriggers(QAbstractItemView::NoEditTriggers); nodes_->verticalHeader()->hide(); nodes_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch); filter_.setSortCaseSensitivity(Qt::CaseInsensitive); filter_.sort(-1); styleTable(nodes_); nodes_->setItemDelegateForColumn(0,new NodeNameDelegate(nodes_));
    nodeCards_=new NodeCardView; nodeCards_->setObjectName("nodeCards"); nodeCards_->setStyleSheet("QListView { border: none; background: transparent; outline: none; }"); nodeCards_->setModel(&filter_); nodeCards_->setContextMenuPolicy(Qt::CustomContextMenu); connect(nodeCards_,&QWidget::customContextMenuRequested,this,[this,favoriteMenu](const QPoint& p) { favoriteMenu(nodeCards_,p); }); nodeCards_->setSelectionModel(nodes_->selectionModel()); nodeCards_->setItemDelegate(new NodeCardDelegate(nodeCards_)); nodeCards_->setViewMode(QListView::IconMode); nodeCards_->setResizeMode(QListView::Adjust); nodeCards_->setMovement(QListView::Static); nodeCards_->setUniformItemSizes(true); nodeCards_->setWrapping(true); nodeCards_->setSelectionMode(QAbstractItemView::SingleSelection); nodeCards_->setSelectionBehavior(QAbstractItemView::SelectRows); nodeCards_->setEditTriggers(QAbstractItemView::NoEditTriggers); nodeCards_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); nodeCards_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); nodeCards_->setAccessibleName("节点卡片");
    nodeViews_=new QStackedWidget; nodeViews_->setObjectName("nodeViews"); nodeViews_->addWidget(nodes_); nodeViews_->addWidget(nodeCards_); nodesLayout->addWidget(nodeViews_,1);
    nodeCards_->viewport()->setMouseTracking(true);
    static_cast<NodeCardView*>(nodeCards_)->delayClicked=[this](const QModelIndex& index) { if (testAction_->isEnabled() && index.isValid()) controller_->testNodes({index.data(Qt::UserRole).toString()},selectedGroup_); };
    auto* retest=new QShortcut(QKeySequence(Qt::Key_F5),nodeCards_); retest->setContext(Qt::WidgetWithChildrenShortcut); connect(retest,&QShortcut::activated,this,[this] { if (testSelectedAction_->isEnabled()) testSelectedAction_->trigger(); });
    connect(nodes_->selectionModel(),&QItemSelectionModel::currentChanged,this,[this](const QModelIndex& index) { if (nodeViews_->currentIndex()==1 && index.isValid()) nodes_->selectRow(index.row()); });
    connect(nodeCards_,&QListView::clicked,this,[this](const QModelIndex& index) { nodes_->selectRow(index.row()); });
    connect(viewMode,&QComboBox::currentIndexChanged,this,[this](int index) { nodeViews_->setCurrentIndex(index); const auto selected=nodes_->selectionModel()->currentIndex(); if (selected.isValid()) { if (index==1) nodeCards_->scrollTo(selected); else nodes_->scrollTo(selected); } updateActions(); });
    connect(nodeCards_,&QListView::doubleClicked,this,[this] { chooseSelectedNode(); }); auto* cardEnter=new QShortcut(QKeySequence(Qt::Key_Return),nodeCards_); cardEnter->setContext(Qt::WidgetWithChildrenShortcut); connect(cardEnter,&QShortcut::activated,this,&MainWindow::chooseSelectedNode);
    auto* cardRight=new QShortcut(QKeySequence(Qt::Key_Right),nodeCards_); cardRight->setContext(Qt::WidgetWithChildrenShortcut); connect(cardRight,&QShortcut::activated,this,&MainWindow::enterSelectedGroup);
    connect(memberSort, &QComboBox::currentIndexChanged, this, [this, memberSort] { filter_.sort(memberSort->currentData().toInt(), Qt::AscendingOrder); });
    nodes_->verticalHeader()->setDefaultSectionSize(UiTheme::TableRow); nodes_->setAlternatingRowColors(false); nodes_->setColumnWidth(1, 105); nodes_->setColumnWidth(2, 70); nodes_->setColumnWidth(3, 95); nodes_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    nodeEmpty_ = emptyState(nodes_, "添加并使用订阅后，选择策略组查看节点");
    nodeCardsEmpty_=emptyState(nodeCards_,"添加并使用订阅后，选择策略组查看节点");
    connect(nodes_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); updateCurrentMember(); });
    connect(search_, &QLineEdit::textChanged, &filter_, &QSortFilterProxyModel::setFilterFixedString);
    connect(search_, &QLineEdit::textChanged, this, [this] { nodeCount_->setText(QString("%1 / %2 个成员").arg(filter_.rowCount()).arg(model_.rowCount())); updateActions(); });
    connect(nodes_, &QTableView::doubleClicked, this, [this] { chooseSelectedNode(); });
    auto* enter = new QShortcut(QKeySequence(Qt::Key_Return), nodes_); enter->setContext(Qt::WidgetWithChildrenShortcut); connect(enter, &QShortcut::activated, this, &MainWindow::chooseSelectedNode);
    auto* right = new QShortcut(QKeySequence(Qt::Key_Right), nodes_); right->setContext(Qt::WidgetWithChildrenShortcut); connect(right, &QShortcut::activated, this, &MainWindow::enterSelectedGroup);
    auto* bottom = new QHBoxLayout; currentMember_ = new ElidedLabel("尚无当前成员"); currentMember_->setObjectName("currentMember"); currentMember_->setTextFormat(Qt::PlainText); currentMember_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); bottom->addWidget(currentMember_, 1);
    testProgress_->setMinimumWidth(42); bottom->addWidget(testProgress_); bottom->addWidget(cancelTestButton_); auto retained=cancelTestButton_->sizePolicy(); retained.setRetainSizeWhenHidden(true); cancelTestButton_->setSizePolicy(retained); bottom->addWidget(nodeCount_);
    locateAction_ = button("定位当前", bottom); connect(locateAction_, &QPushButton::clicked, this, &MainWindow::locateCurrentNode);
    enterGroup_ = button("进入策略组", bottom); enterGroup_->setObjectName("enterGroup"); connect(enterGroup_, &QPushButton::clicked, this, &MainWindow::enterSelectedGroup);
    chooseAction_ = button("使用选中成员", bottom); chooseAction_->setProperty("primary", true); chooseAction_->setObjectName("chooseMember"); nodesLayout->addLayout(bottom); layout->addWidget(nodesPanel, 1);
    auto* escape=new QShortcut(QKeySequence(Qt::Key_Escape),page); escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape,&QShortcut::activated,this,[this,page] {
        if (auto* popup=QApplication::activePopupWidget()) {
            for (auto* combo:page->findChildren<QComboBox*>()) if (combo->view()->window()==popup) { combo->hidePopup(); return; }
            popup->close(); return;
        }
        nodes_->selectionModel()->clear(); proxyGroups_->hide(); groupSwitcher_->setChecked(false); updateActions();
    });
    auto* searchShortcut=new QShortcut(QKeySequence::Find,page); searchShortcut->setContext(Qt::WidgetWithChildrenShortcut); connect(searchShortcut,&QShortcut::activated,search_,[this] { search_->setFocus(Qt::ShortcutFocusReason); search_->selectAll(); });
    connect(chooseAction_, &QPushButton::clicked, this, &MainWindow::chooseSelectedNode);
    return page;
}

void MainWindow::applyNodeFilters() {
    if (!nodeRegion_) return;
    QSet<QString> favorites; for (const auto& value:controller_->nodePreferences(profilesCombo_->currentData().toString()).value("favorites").toArray()) favorites.insert(value.toString());
    model_.setFavorites(favorites);
    filter_.setConditions(nodeRegion_->currentData().toString(),nodeProtocol_->currentData().toString(),nodeHealth_->currentIndex(),nodeMaximum_->currentData().toInt(),nodeFavoritesOnly_->isChecked(),favorites);
    if (nodeCount_) nodeCount_->setText(QString("%1 / %2 个成员").arg(filter_.rowCount()).arg(model_.rowCount()));
}

QJsonObject MainWindow::nodeBookmark() const {
    const auto current=nodes_->currentIndex(); return {{"profile",profilesCombo_->currentData().toString()},{"group",selectedGroup_},{"view",memberView()},{"sort",findChild<QComboBox*>("memberSort")->currentIndex()},{"search",search_->text().left(512)},{"region",nodeRegion_->currentData() .toString()},{"protocol",nodeProtocol_->currentData().toString()},{"health",nodeHealth_->currentIndex()},{"maximum",nodeMaximum_->currentData().toInt()},{"favorites",nodeFavoritesOnly_->isChecked()},{"member",current.isValid()?filter_.mapToSource(current).data(Qt::UserRole).toString():QString()},{"scroll",memberView()==1?nodeCards_->verticalScrollBar()->value():nodes_->verticalScrollBar()->value()}};
}

void MainWindow::restoreNodeBookmark(const QJsonObject& value) {
    if (value.value("profile").toString()!=controller_->state().activeProfile || value.value("group").toString().isEmpty()) return;
    selectGroup(value.value("group").toString()); setMemberView(value.value("view").toInt()); findChild<QComboBox*>("memberSort")->setCurrentIndex(qBound(0,value.value("sort").toInt(),2)); search_->setText(value.value("search").toString().left(512));
    nodeRegion_->setCurrentIndex(qMax(0,nodeRegion_->findData(value.value("region").toString()))); nodeProtocol_->setCurrentIndex(qMax(0,nodeProtocol_->findData(value.value("protocol").toString()))); nodeHealth_->setCurrentIndex(qBound(0,value.value("health").toInt(),3)); nodeMaximum_->setCurrentIndex(qMax(0,nodeMaximum_->findData(value.value("maximum").toInt()))); nodeFavoritesOnly_->setChecked(value.value("favorites").toBool());
    QTimer::singleShot(0,this,[this,value] { const auto row=model_.names().indexOf(value.value("member").toString()); const auto index=filter_.mapFromSource(model_.index(row,0)); if (index.isValid()) { nodes_->setCurrentIndex(index); nodes_->selectRow(index.row()); } auto* scroll=memberView()==1?nodeCards_->verticalScrollBar():nodes_->verticalScrollBar(); scroll->setValue(qMax(0,value.value("scroll").toInt())); });
}

void MainWindow::editNodePreferences() {
    const auto id=profilesCombo_->currentData().toString(),group=selectedGroup_; if (id.isEmpty()||group.isEmpty()||controller_->state().busy) return;
    const auto expected=controller_->nodePreferences(id),initial=expected.value("groups").toObject().value(group).toObject();
    auto* dialog=new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("nodePreferencesDialog"); dialog->setWindowTitle("本组偏好"); dialog->resize(510,280); auto* box=new QVBoxLayout(dialog); auto* label=new QLabel(group); label->setTextFormat(Qt::PlainText); label->setObjectName("sectionTitle"); box->addWidget(label);
    auto* form=new QFormLayout; auto* url=new QLineEdit(initial.value("test_url").toString()); url->setObjectName("groupTestUrl"); url->setMaxLength(2048); url->setPlaceholderText("留空沿用默认测试地址"); form->addRow("测试地址",url);
    auto* view=new CompactComboBox; view->addItems({"表格","卡片"}); view->setCurrentIndex(memberView()); form->addRow("默认视图",view); auto* sort=new CompactComboBox; sort->addItems({"默认顺序","名称升序","延迟升序"}); sort->setCurrentIndex(findChild<QComboBox*>("memberSort")->currentIndex()); form->addRow("默认排序",sort); box->addLayout(form);
    auto* common=new QCheckBox("作为常用组，在列表和托盘中优先显示"); common->setChecked(expected.value("common_groups").toArray().contains(group)); box->addWidget(common);
    auto* disconnect=new QCheckBox("切换成员后断开受影响连接"); disconnect->setObjectName("disconnectAffected"); disconnect->setChecked(expected.value("disconnect_affected").toBool()); disconnect->setToolTip("默认保留。只处理切换前经过该策略组的连接，单次最多32条；其他连接保留，结果以回读为准。"); box->addWidget(disconnect);
    auto* note=new QLabel; note->setTextFormat(Qt::PlainText); note->setWordWrap(true); box->addWidget(note); auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText("保存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); box->addWidget(buttons);
    const auto draft=[expected,group,url,view,sort,common,disconnect] { auto value=expected; auto groups=value.value("groups").toObject(); groups[group]=QJsonObject{{"test_url",url->text().trimmed()},{"view",view->currentIndex()},{"sort",sort->currentIndex()}}; value["groups"]=groups; value["last_group"]=group; value["disconnect_affected"]=disconnect->isChecked(); auto commons=value.value("common_groups").toArray(); for (int i=commons.size()-1;i>=0;--i) if (commons[i].toString()==group) commons.removeAt(i); if (common->isChecked()) commons.append(group); value["common_groups"]=commons; return value; };
    const auto baseline=draft(); auto pending=std::make_shared<bool>(false); DialogDraftGuard::protect(dialog,[draft,baseline] { return draft()!=baseline; },[pending] { return *pending; }); connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,id,expected,draft,pending,buttons] { *pending=true; buttons->setEnabled(false); controller_->updateNodePreferences(id,draft(),expected); });
    connect(controller_,&AppController::operationFinished,dialog,[this,dialog,pending,buttons,note,view,sort](bool ok,const QString& message) { if (!*pending) return; *pending=false; buttons->setEnabled(true); if (ok) { setMemberView(view->currentIndex()); findChild<QComboBox*>("memberSort")->setCurrentIndex(sort->currentIndex()); dialog->accept(); } else note->setText(message); }); dialog->show();
}

void MainWindow::updateProxies() {
    const auto previousGroup=selectedGroup_;
    const auto* selectedCell = proxyGroups_->currentItem();
    const bool keepScroll = selectedCell && selectedCell->text() == selectedGroup_;
    const auto scroll = proxyGroups_->verticalScrollBar()->value();
    const bool viewingActive = !controller_->state().activeProfile.isEmpty() && profilesCombo_->currentData().toString() == controller_->state().activeProfile;
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
    updating_ = true; const QSignalBlocker block(proxyGroups_); QStringList before; for (int i=0;i<proxyGroups_->count();++i) before.append(proxyGroups_->item(i)->text()); if (before!=groups) { proxyGroups_->clear(); proxyGroups_->addItems(groups); }
    for (int row = 0; row < groups.size(); ++row) {
        const auto object = proxies.value(groups[row]).toObject();
        auto* item=proxyGroups_->item(row); item->setData(Qt::UserRole+1,object.value("all").toArray().size()); item->setToolTip(QString("%1 · %2 个成员").arg(groups[row]).arg(object.value("all").toArray().size()));
        if (groups[row] == selectedGroup_) proxyGroups_->setCurrentRow(row);
    }
    if (keepScroll) proxyGroups_->verticalScrollBar()->setValue(scroll);
    updating_ = false; updateGroup();
    if (previousGroup!=selectedGroup_) { const auto stored=preferences.value("groups").toObject().value(selectedGroup_).toObject(); if (stored.contains("view")) setMemberView(stored.value("view").toInt()); if (stored.contains("sort")) findChild<QComboBox*>("memberSort")->setCurrentIndex(stored.value("sort").toInt()); }
    updateHome();
}

void MainWindow::updateGroup() {
    websitePage_->suggestGroup(selectedGroup_);
    const auto group = selectedGroup_; const bool sameProfile = profilesCombo_->currentData().toString() == controller_->state().activeProfile;
    model_.setTestContext(controller_->groupTestUrl(group)); model_.setGroup(sameProfile ? controller_->proxies() : QJsonObject{}, group);
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
    applyNodeFilters();
    selectedGroupTitle_->setText(group.isEmpty()?"选择策略组":group+QString(" · %1 个成员").arg(model_.rowCount())); selectedGroupTitle_->setToolTip(selectedGroupTitle_->text());
    const auto type = controller_->proxies().value(group).toObject().value("type").toString();
    const auto object=controller_->proxies().value(group).toObject(); const bool automatic=QStringList{"URLTest","Fallback"}.contains(type); const bool fixed=!object.value("fixed").toString().isEmpty(); if (automatic) selectedGroupTitle_->setText(selectedGroupTitle_->text()+(fixed?" · 手动固定":" · 自动选择"));
    groupSwitcher_->setFixedWidth(qMax(140,groupSwitcher_->fontMetrics().horizontalAdvance("切换策略组")+42));
    groupSwitcher_->setText(groupSwitcher_->fontMetrics().elidedText(group.isEmpty()?QString("切换策略组"):group+QString(" · %1").arg(model_.rowCount()),Qt::ElideRight,groupSwitcher_->width()-36));
    groupSwitcher_->setToolTip(selectedGroupTitle_->text()+"\n展开策略组（Ctrl+G）；仅改变浏览组"+(automatic?(fixed?QString("\n当前为手动固定；可恢复自动选择"):QString("\n当前按自动策略选择")):QString()));
    groupHint_->setText(group.isEmpty() ? "无节点：导入并激活订阅，或检查内核状态。" : type == "Selector" ? "手动选择组；粗体标记当前成员。" : "自动/其他策略组；手动选择可能固定自动组的成员。");
    proxyGroups_->setToolTip(groupHint_->text()); nodes_->setToolTip(groupHint_->text() + " 双击或回车选择成员。");
    updateCurrentMember();
    nodeCount_->setText(QString("%1 / %2 个成员").arg(filter_.rowCount()).arg(model_.rowCount()));
    updateActions(); updateHome();
}

void MainWindow::selectGroup(const QString& group, bool nested) {
    if (!controller_->proxies().value(group).toObject().value("all").isArray()) return;
    if (nested) {
        if (group == selectedGroup_ || groupHistory_.contains(group) || groupHistory_.size() >= 16) {
            message_->setText("策略组引用形成循环或层级过深，未继续进入"); message_->setToolTip(message_->text()); return;
        }
        groupHistory_.append(selectedGroup_);
    } else groupHistory_.clear();
    if (selectedGroup_ != group) search_->clear(); selectedGroup_ = group;
    const bool expanded=groupSwitcher_->isChecked(); groupSwitcher_->setChecked(false);
    updateProxies();
    const auto preferences=controller_->nodePreferences(profilesCombo_->currentData().toString()).value("groups").toObject().value(group).toObject(); if (preferences.contains("view")) setMemberView(preferences.value("view").toInt()); if (preferences.contains("sort")) findChild<QComboBox*>("memberSort")->setCurrentIndex(preferences.value("sort").toInt());
    if (expanded) { if (nodeViews_->currentIndex()==1) nodeCards_->setFocus(Qt::ShortcutFocusReason); else nodes_->setFocus(Qt::ShortcutFocusReason); }
}

void MainWindow::enterSelectedGroup() {
    if (!enterGroup_->isEnabled()) return;
    const auto rows = nodes_->selectionModel()->selectedRows(); if (rows.isEmpty()) return;
    selectGroup(model_.nameAt(filter_.mapToSource(rows.first()).row()), true);
}

void MainWindow::updateCurrentMember() {
    const auto now = controller_->proxies().value(selectedGroup_).toObject().value("now").toString();
    const auto path = (groupHistory_ + QStringList{selectedGroup_}).join(" / ");
    const bool viewingActive = profilesCombo_->currentData().toString() == controller_->state().activeProfile;
    auto text = !viewingActive ? "使用所选订阅后显示当前成员" : !controller_->state().ready ? "内核就绪后显示当前成员" : now.isEmpty() ? "尚无当前成员" : path + " · 当前：" + now;
    const auto row = model_.names().indexOf(now);
    if (controller_->state().ready && row >= 0) { const auto delay = model_.data(model_.index(row, 2)).toString(); if (delay.endsWith(" ms")) text += " · " + delay; }
    const auto rows=nodes_->selectionModel()->selectedRows();
    if (viewingActive && controller_->state().ready && !rows.isEmpty()) {
        const auto selected=model_.nameAt(filter_.mapToSource(rows.first()).row());
        if (!selected.isEmpty() && selected!=now) text+=" · 待使用："+selected;
    }
    currentMember_->setToolTip(text); currentMember_->setText(text);
}

void MainWindow::rebuildRecentMenu() {
    recentMenu_->clear(); const auto group = selectedGroup_; const auto members = controller_->proxies().value(group).toObject().value("all").toArray();
    for (const auto& profile : controller_->profiles()) if (profile.id == controller_->state().activeProfile) for (const auto& value : profile.recentSelections) {
        const auto pair = value.toObject(); const auto member = pair.value("member").toString(); if (pair.value("group").toString() != group || !members.contains(member)) continue;
        auto* action = recentMenu_->addAction(member); action->setCheckable(true); action->setChecked(member == controller_->proxies().value(group).toObject().value("now").toString()); connect(action, &QAction::triggered, this, [this, group, member] { controller_->selectMember(group, member); });
    }
}

void MainWindow::chooseSelectedNode() {
    if (!chooseAction_->isEnabled()) return;
    const auto rows = nodes_->selectionModel()->selectedRows(); if (rows.isEmpty()) return;
    const auto index = filter_.mapToSource(rows.first());
    if (index.isValid()) controller_->selectMember(selectedGroup_, model_.nameAt(index.row()));
}

void MainWindow::locateCurrentNode() {
    const auto now = controller_->proxies().value(selectedGroup_).toObject().value("now").toString();
    const auto row = model_.names().indexOf(now); if (row < 0) return;
    search_->clear(); const auto index = filter_.mapFromSource(model_.index(row, 0)); nodes_->setCurrentIndex(index); nodes_->selectRow(index.row()); nodes_->scrollTo(index); if (nodeViews_->currentIndex()==1) { nodeCards_->scrollTo(index); nodeCards_->setFocus(); } else nodes_->setFocus();
}

void MainWindow::setMemberView(int mode) { memberViewMode_->setCurrentIndex(mode==1?1:0); }

}
