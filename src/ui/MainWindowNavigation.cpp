#include "app/AppController.h"
#include "MainWindow.h"
#include "ViewIds.h"
#include "WebsiteTestsPage.h"
#include "ConnectionOptions.h"
#include "StatusSentence.h"
#include "WindowChrome.h"
#include "TaskWorkspace.h"
#include "DetailsDrawer.h"
#include "DialogDraftGuard.h"
#include "OutboundDialog.h"
#include "UiTheme.h"
#include "UiIcons.h"
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
#include <QListView>
#include <QScrollBar>
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
void MainWindow::syncSectionNavigation(const Route& route) {
    // 三个工作区入口与底部设置共用同一路由；规则深链归属配置。
    const int row = route.view == ViewIds::Rules ? ViewIds::Profiles : route.view;
    { const QSignalBlocker block(navigation_);
      for (int i=0;i<navigation_->count();++i) {
          auto* item=navigation_->item(i);
          item->setData(ActiveNavigationRole,item->data(Qt::UserRole).toInt()==row);
      }
      if (row == ViewIds::Settings) {
          navigation_->clearSelection();
          navigation_->setCurrentRow(-1);
      }
      else for (int i = 0; i < navigation_->count(); ++i) if (navigation_->item(i)->data(Qt::UserRole).toInt() == row) { navigation_->setCurrentRow(i); break; } }
    settingsNav_->setChecked(row==ViewIds::Settings);
    settingsNav_->setIcon(UiIcons::icon(UiIcons::Glyph::Settings,settingsNav_->isChecked()?UiTheme::Brand:UiTheme::Secondary));
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
    if (route.view != route_.view) {
        // Daymark's safe navigation keeps a draft in its owning task. Only an
        // in-flight write blocks navigation; switching sections never discards.
        if (settingsApplying_ || rulesSaving_) { syncSectionNavigation(route_); return; }
        if (contentStack_ && contentStack_->currentIndex() != 0) {
            if (auto* workspace = qobject_cast<TaskWorkspace*>(contentStack_->currentWidget())) {
                if (DialogDraftGuard::hasPending(workspace)) {
                    workspace->note("当前提交尚未结束，请稍后再切换页面。");
                    syncSectionNavigation(route_); return;
                }
                if (DialogDraftGuard::hasDrafts(workspace)) suspendedWorkspaces_[route_.view] = workspace;
            }
        }
        if (auto* workspace=workspaces_.value(ViewIds::Profiles); workspace && workspace->isVisible() && workspace->context().startsWith("profile:")) {
            if (DialogDraftGuard::hasPending(workspace)) { workspace->note("当前提交尚未结束，请稍后再切换页面。"); syncSectionNavigation(route_); return; }
            if (DialogDraftGuard::hasDrafts(workspace)) suspendedWorkspaces_[ViewIds::Profiles]=workspace;
            else suspendedWorkspaces_.remove(ViewIds::Profiles);
            workspace->hide();
        }
    }
    if (route!=route_) {
        if (homeRouteRow_) for (auto* menu:homeRouteRow_->findChildren<QMenu*>("routeModeMenu")) menu->close();
        if (connectionsTable_) for (auto* menu:connectionsTable_->findChildren<QMenu*>("connectionContextMenu")) menu->close();
        if (auto* filters=findChild<QWidget*>("nodeFilterPopup")) filters->close();
        if (auto* columns=findChild<QWidget*>("connectionColumnsPopup")) columns->close();
    }
    if (route_.view == ViewIds::Data && currentDataTab() == int(ViewIds::DataTab::Connections)
        && (route.view != ViewIds::Data || (route.tab >= 0 && route.tab != int(ViewIds::DataTab::Connections))))
        rememberConnectionPosition();
    if (contentStack_) contentStack_->setCurrentIndex(0);
    // Returning from another section resumes the profile object. The explicit
    // “返回配置” control is the only normal path back to the profile list.
    if (route.view!=route_.view) if (auto* drawer=qobject_cast<DetailsDrawer*>(detailsDrawer_.data())) drawer->closeDrawer();
    // 设置分类只在调用方显式给出时应用；kNoCategory 表示沿用当前分类。
    // K8 起分类不再是页签：域内只把视野滚到对应分组，窗口标题仍带分类名。
    if (route.view == ViewIds::Settings) {
        // kNoCategory 表示「沿用当前分类」：路由里留下的是实际生效的分类，
        // 否则同一视图内的下一次导航会把视野拨回顶部，标题也会丢掉分类名。
        if (route.category<0 && settingsStack_ && !settingsDirty()) { settingsStack_->setCurrentIndex(0); settingsTask_.clear(); }
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
    if (groupEditor_) groupEditor_->setVisible(route.view == ViewIds::Home &&
        groupEditor_->property("profileId").toString() == profilesCombo_->currentData().toString() &&
        groupEditor_->property("groupName").toString() == selectedGroup_);
    if (route.view != ViewIds::Settings && configPreview_) configPreview_->close();
    if (outboundDialog_) outboundDialog_->close();
    pages_->setCurrentIndex(stackIndex(route.view));
    if (auto* workspace = suspendedWorkspaces_.value(route.view).data()) {
        if (DialogDraftGuard::hasDrafts(workspace)) showWorkspace(workspace);
        else suspendedWorkspaces_.remove(route.view);
    }
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
    if (contentStack_->indexOf(workspace)<0) {
        workspace->hide(); workspace->setProperty("profileInspector",false);
        workspace->style()->unpolish(workspace); workspace->style()->polish(workspace);
        contentStack_->addWidget(workspace);
    }
    contentStack_->setCurrentWidget(workspace);
    refreshTimer_.stop(); controller_->setHomeVisible(false); controller_->setRulesVisible(false);
    controller_->setConnectionsVisible(false); controller_->setDiagnosticsVisible(false); websitePage_->setActive(false);
}
void MainWindow::returnFromWorkspace() {
    if (outboundDialog_) outboundDialog_->close();
    suspendedWorkspaces_.remove(route_.view);
    const bool returningProfile = route_.view == ViewIds::Profiles &&
        workspaces_.value(ViewIds::Profiles) && workspaces_.value(ViewIds::Profiles)->context().startsWith("profile:");
    contentStack_->setCurrentIndex(0); enterView(route_.view,route_);
    if (returningProfile && profiles_) {
        const QPointer<QListView> list = profiles_;
        const int position = profileListReturnScroll_;
        QTimer::singleShot(0, this, [list, position] {
            if (!list) return;
            list->verticalScrollBar()->setValue(position);
            list->setFocus(Qt::OtherFocusReason);
        });
    }
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
    // 配置页已经有带来源选择的“导入…”行；顶栏 + 只作首页快捷入口。
    // With no configuration the Home empty state is the sole import action.
    const bool configActions=route_.view==ViewIds::Home && !controller_->profiles().isEmpty();
    if (auto* button=findChild<QToolButton*>("toolbarAdd")) { button->setVisible(configActions); button->setIcon(UiIcons::icon(UiIcons::Glyph::Plus,UiTheme::Brand)); }
    WindowParts::updateTopbar(this, route_.view==ViewIds::Rules?QString("配置"):ViewIds::title(route_.view), ViewIds::subtitle(route_.view), status, dotState);
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
    if (route_.view == ViewIds::Data && currentDataTab() == int(ViewIds::DataTab::Connections))
        rememberConnectionPosition();
    homeOutboundSnapshot_={}; updateHomeOutbound();
    if (connectionOptions_) connectionOptions_->flushPreferences();
    if (outboundDialog_) outboundDialog_->close();
    controller_->setHomeVisible(false); websitePage_->setActive(false);
    if (connectionDetail_) connectionDetail_->close(); if (configPreview_) configPreview_->close();
    refreshTimer_.stop(); controller_->setRulesVisible(false);
    controller_->setDiagnosticsVisible(false); controller_->setConnectionsVisible(false);
    QMainWindow::hideEvent(event);
}
}
