#include "app/AppController.h"
#include "WindowUiFactories.h"
#include "MainWindow.h"
#include "StatusSentence.h"
#include "ViewIds.h"
#include "NodeCardView.h"
#include "NodeRowView.h"
#include "StatusShape.h"
#include "SkeletonBlock.h"
#include <algorithm>
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
void MainWindow::refreshStatusSegments() {
    const auto& state = controller_->state();
    const bool externalConsent = state.proxy==ProxyState::External && state.lastError.contains(QStringLiteral("需明确允许接管"));
    const auto alarm = !state.lastError.isEmpty() && !externalConsent ? state.lastError
        : state.readOnly ? QString("只读保护：状态文件不可用，修改不会写入") : QString();
    operationAlarm_->setText(alarm); operationAlarm_->setToolTip(alarm); operationAlarm_->setVisible(!alarm.isEmpty());
    operationAlarm_->setProperty("state", state.readOnly && (state.lastError.isEmpty() || externalConsent) ? "warning" : "error");
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
    // 运行细节统一由固定状态栏和诊断入口承载，不塞进首页控制行。
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
    // 连接是用户意图；Controller 在必要时自行启动内核并处理回滚。
    const bool activeProfileExists=std::any_of(controller_->profiles().cbegin(),controller_->profiles().cend(),
        [&state](const Profile& profile) { return profile.id==state.activeProfile; });
    proxyEnabled_->setEnabled(!state.busy && !state.exiting && !state.readOnly
        && (activeProfileExists || state.proxy==ProxyState::Owned || state.tunObserved));
    { const QSignalBlocker block(proxyEnabled_); proxyEnabled_->setChecked(state.proxy == ProxyState::Owned || state.tunObserved); }
    proxyEnabled_->setToolTip("代理总开关：接管系统代理（必要时先启动内核）；只影响遵循系统代理设置的应用。外部代理/PAC 需要确认接管。");
    autoStart_->setToolTip(!state.autoStartKnown ? "系统自启状态无法确认；应用设置时重新读取" : state.autoStartConflict ? "系统自启项指向其他路径，修改会被拒绝" : state.autoStartEnabled ? "系统当前已启用 Mlash 自启；勾选修改后点击应用" : "系统当前未启用 Mlash 自启；勾选修改后点击应用");
    for (auto* control : proxyModes_) { const QSignalBlocker block(control); control->setChecked(control->property("mode").toString() == state.mode); control->setEnabled(state.ready && !state.busy && !state.exiting && !state.readOnly); }
    refreshStatusSegments();
    // 「最近操作」段不再被 lastError 覆盖：告警有自己的段（P4.1 / N10）。
    if (feedbackError_.isEmpty() && !recentFeedback_ && noticeOperation_ == OperationId::Unspecified && !state.busy) { message_->clear(); message_->setToolTip({}); }
    findChild<QToolButton*>("statusDetails")->setVisible(!message_->text().isEmpty());
    const bool externalConsent = state.proxy==ProxyState::External && state.lastError.contains(QStringLiteral("需明确允许接管"));
    findChild<QToolButton*>("statusResolve")->setVisible(!feedbackError_.isEmpty() || (!state.lastError.isEmpty() && !externalConsent));
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
    const auto& profiles=controller_->profiles();
    const bool batchAvailable=std::count_if(profiles.cbegin(),profiles.cend(),
        [](const Profile& profile) { return !profile.source.isEmpty(); }) > 1;
    enable("exportDiagnostics", !state.busy&&!state.exiting);
    enable("nodePreferences", idle&&!selectedGroup_.isEmpty()&&profilesCombo_->currentData().toString()==state.activeProfile);
    enable("ruleProviders", !state.busy&&!state.exiting&&!rulesProfiles_->currentData().toString().isEmpty());
    if (auto* objectActions=findChild<QWidget*>("profileObjectActions")) objectActions->setVisible(!selectedProfile().isEmpty());
    if (auto* batch=findChild<QPushButton*>("batchProfiles")) batch->setVisible(batchAvailable);
    if (auto* order=findChild<QToolButton*>("profileOrder")) order->setVisible(profiles.size()>1);
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
    const bool canTest=idle && state.ready && viewingActive && model_.rowCount()>0;
    if (testSelectedAction_) testSelectedAction_->setEnabled(canTest && hasNode);
    static_cast<NodeCardView*>(nodeCards_)->setTestEnabled(canTest);
    static_cast<NodeRowView*>(nodeList_)->setTestEnabled(canTest);
    if (auto* update=findChild<QToolButton*>("nodeUpdateProfile")) {
        const auto* active=findProfile(state.activeProfile);
        const bool canUpdate=active && !active->source.isEmpty();
        const bool local=canUpdate && active->sourceType=="file";
        update->setText(local?"同步文件":"更新订阅");
        update->setAccessibleName(update->text());
        update->setEnabled(idle && canUpdate);
        update->setToolTip(!canUpdate?"当前配置没有可更新的来源":
            local?"从原文件同步配置，更新后可在配置页查看":"下载当前订阅的更新，在配置页查看和应用");
    }
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
    if (auto* batch = findChild<QPushButton*>("batchProfiles")) batch->setEnabled(batchAvailable && idle);
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
    const auto connectionTotals=controller_->connectionTotals();
    const bool hasConnectionMetrics=connectionTotals.value("uploadTotal").isDouble() || connectionTotals.value("downloadTotal").isDouble()
        || connectionTotals.value("_uploadTotal_rate").isDouble() || connectionTotals.value("_downloadTotal_rate").isDouble();
    const bool canCloseAll=state.ready && controller_->connectionsCurrent() && !controller_->connections().isEmpty() && connectionView_->currentIndex()==0;
    if (closeAllConnections_) closeAllConnections_->setEnabled(idle && canCloseAll);
    if (auto* closeAllButton=findChild<QToolButton*>("closeAllConnectionsButton")) closeAllButton->setVisible(canCloseAll);
    if (connectionStats_) connectionStats_->setVisible(hasConnectionMetrics);
    if (connectionSummary_) connectionSummary_->setVisible(hasConnectionMetrics || canCloseAll);
    if (connectionPause_) connectionPause_->setEnabled(!state.exiting&&state.ready&&!state.busy&&controller_->connectionsVisible());
    if (connectionEmpty_) connectionEmpty_->setVisible(connectionsFilter_.rowCount()==0);
    connectionEmpty_->setText(!controller_->connectionError().isEmpty()
        ? (controller_->connectionsStale() ? QString("刷新失败 · 下方保留上次快照") + QChar(10) + QString("详情见状态栏或日志页") : QString("连接读取失败"))
        : !state.ready ? (state.activeProfile.isEmpty()?QString("请先选择配置"):state.core==CoreState::Starting?QString("正在启动内核…"):state.core==CoreState::Running?QString("正在确认内核…"):state.core==CoreState::Stopping?QString("正在停止内核…"):QString("内核未就绪"))
        : connectionsModel_.rowCount()>0 ? QString("没有匹配的连接") + QChar(10) + QString("请调整搜索条件")
        : connectionView_->currentIndex()==1 ? QString("内核已就绪，暂无已结束的连接")
        : controller_->connectionsPaused() ? QString("已暂停") + QChar(10) + QString("恢复刷新后继续采样")
        : !controller_->connectionsCurrent() ? QString("正在读取连接…")
        : QString("内核已就绪，暂无活动连接"));
    // 连接空态：失败给「查看日志」，无匹配给「清除筛选」，未就绪说明缺什么（R7）。
    {
        const bool failed = !controller_->connectionError().isEmpty(), noMatch = connectionsModel_.rowCount() > 0;
        const bool canStartCore=state.core==CoreState::Stopped || state.core==CoreState::Failed;
        const QString text = connectionsFilter_.rowCount()>0 ? QString() : failed ? QString("查看日志") : noMatch ? QString("清除筛选") : !state.ready ? (state.activeProfile.isEmpty()?QString("选择配置"):canStartCore?QString("启动内核"):QString()) : QString("刷新");
        setEmptyStateAction(connectionsTable_, text, [this, failed, noMatch] {
            if (failed) navigate(Route{ViewIds::Data, kNoCategory, HomeAnchor::Control, int(ViewIds::DataTab::Logs)});
            else if (noMatch) connectionsFilter_.setFilterFixedString({});
            else if (!controller_->state().ready) { const auto& st = controller_->state(); if (st.activeProfile.isEmpty()) navigate(Route{ViewIds::Profiles}); else if (st.core==CoreState::Stopped || st.core==CoreState::Failed) controller_->startCore(); }
            else controller_->refresh();
        });
    }}
bool MainWindow::logMatches(const QString& line) const { return line.contains(logSearch_->text(),Qt::CaseInsensitive) && (logFilter_->currentData().toString() == "all" || Diagnostics::levelOf(line) == logFilter_->currentData().toString()); }
void MainWindow::updateLogs() {
    const auto& all = logsPaused_ ? pausedLogs_ : controller_->diagnostics()->lines();
    QStringList filtered; for (const auto& line : all) if (logMatches(line)) filtered.append(line);
    if (logEmpty_) {
        QString reason;
        if (filtered.isEmpty() && !all.isEmpty()) reason = QString("没有匹配的日志") + QChar(10) + QString("请调整搜索或级别");
        else if (filtered.isEmpty() && !controller_->state().ready) reason = QString("内核未就绪") + QChar(10) + QString("启动内核后才会收到内核日志");
        else if (filtered.isEmpty() && logsPaused_) reason = QString("已暂停") + QChar(10) + QString("恢复显示后继续接收");
        else if (filtered.isEmpty()) reason = QString("还没有日志");
        logEmpty_->setText(reason);
        logEmpty_->setVisible(!reason.isEmpty());
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
}
