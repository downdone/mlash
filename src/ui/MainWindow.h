#pragma once
#include "domain/Types.h"
#include "ProxyTableModel.h"
#include "ProfileRowModel.h"
#include "ActivityTableModel.h"
#include "Route.h"
#include "PageLifecycle.h"
#include "ToolWindowRegistry.h"
#include <QModelIndex>
#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QPointer>
#include <QTimer>

class QListWidget; class QStackedWidget; class QComboBox; class QLabel; class QCheckBox;
class QListView;
class QTableView; class QTableWidget; class QLineEdit; class QPlainTextEdit; class QSpinBox; class QPushButton;
class QAction; class QTabWidget; class QMenu; class QDialog;
class QToolButton; class QStatusBar;
class QTabBar; class QHBoxLayout; class QScrollArea;
class QDragEnterEvent; class QDropEvent;
namespace mlash { class AppController; class WebsiteTestsPage; class ImportDialog; class NodeRowModel; class ProfileRowModel; class ProfileKindFilter; }
namespace mlash { class ConnectionOptions; }
namespace mlash {
// 抽屉组件只在使用它的实现文件中包含完整定义。
namespace WindowParts { class DetailsDrawer; }
class TaskWorkspace;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppController* controller, QWidget* parent = nullptr);
    ~MainWindow() override;
    void setTrayAvailable(bool available) { trayAvailable_ = available; }
    // 唯一切换入口。setPage(int) 是保留的历史入口，等价于 navigate(fromLegacyPage(index))。
    void navigate(Route route);
    Route currentRoute() const;
    void setPage(int index);
    // 返回「入口 ID」供侧栏/页签回显（首页 Control=0、Nodes=2），由 toLegacyPage(route_) 计算；
    // currentRoute() 才是权威，page() 只是 UI 回显。
    int page() const;
    int memberView() const;
    void setMemberView(int mode);
    bool confirmDiscardEdits();
    void presentTask(QDialog* editor, const QString& key={}, const QString& title={});
    QJsonObject nodeBookmark() const;
    void restoreNodeBookmark(const QJsonObject& value);
signals:
    void guiReleaseRequested();
protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
private:
    QWidget* homePage(); QWidget* nodeSection(); QWidget* profilesPage(); QWidget* dataPage(); QWidget* settingsPage(); QWidget* rulesPage();
    // 数据页的四个面板由 dataPage() 组装后再拆进 QStackedWidget（K6）。
    QWidget* connectionsPanel(); QWidget* diagnosticsPanel(); QWidget* trafficPanel();
    // K6：数据页顶部的「复制诊断摘要」；只拼可公开的计数与状态词。
    void copyDiagnosticsSummary();
    void buildDataTabs(QWidget* page);
    void selectDataTab(int tab);
    void updateDataPanelVisibility();
    int currentDataTab() const;
    void applyAppearance();
    void updateRulesProfiles(); void loadRuleDraft();
    void rebuildRuleList(); void updateConnections();
    void updateHome();
    void updateHomeOutbound(bool capture=false);
    void showOutboundDiagnostics();
    QPointer<QDialog> outboundDialog_;
    void updateState(); void updateProfiles(); void updateProxies(); void updateGroup(); void updateLogs();
    QString selectedProfile() const;
    void presentImport(ImportDialog* dialog);
    void addUrl(); void addFile(bool replace); void applySettings(); void addSubscription(bool url);
    void syncSectionNavigation(const Route& route);
    void updateTopbar();
    // 视图进入/离开的副作用实现（由 PageLifecycle 转调）。
    void enterView(int view, const Route& route);
    void leaveView(int view);
    void updateWindowTitle();
    // 页面生命周期（S1）：副作用写在各页对象的 onEnter/onLeave 里，
    // navigate() 只负责顺序，不再逐视图加分支。
    PageRouter pageRouter_;
    // 工具窗口（S2）：按「工具+对象」复用并 raise，杜绝同一对象开出两个窗口。
    ToolWindowRegistry toolWindows_;
    QStackedWidget* contentStack_=nullptr;
    QHash<int,TaskWorkspace*> workspaces_;
    QHash<QString,TaskWorkspace*> taskFamilies_;
    TaskWorkspace* taskWorkspace();
    TaskWorkspace* taskWorkspaceFor(const QString& family);
    void showWorkspace(TaskWorkspace* workspace);
    void returnFromWorkspace();
    void openBackupTask(const QString& task);
    void openProfileTask(const QString& id,const QString& task);
    // 打开工具窗口的唯一次入口；对象键为空表示单例工具。
    QWidget* showTool(const QString& tool, const QString& objectKey, const std::function<QWidget*()>& factory);
    static int stackIndex(int view);
    QString activeProfileName() const;
    void applyHomeAnchor(HomeAnchor anchor);
    void updateActions(); void updateCurrentMember(); void chooseSelectedNode(); void selectNodeInHome(const QModelIndex& index); void selectNodeInHomeFromCards(const QModelIndex& index); void commitMemberSelection(const QString& name); void locateCurrentNode();
    void showNodeDetails(const QString& name);   // 行尾信息按钮：只读说明 + 手动组的切换入口
    // K7：配置页两张列表共用的模型、行点击分流与详情抽屉。
    QAbstractItemModel* profileRows(const QString& kind) const;
    static QString profileAt(const QModelIndex& index);
    void selectProfile(const QString& id);
    void showProfileDetails(const QString& id);
    void shareProfileScheme();
    void wireProfileList(QListView* list, const QString& kind);
    QStringList selectedProfileIds() const;
    void updateNodeCount();                      // 标题行的成员计数
    void editProfilePolicy();
    void editNodePreferences();
    void applyNodeFilters();
    void importPayload(const QString& input);
    void showProfileBatch(bool start);
    void showProviders(const QString& id, const QString& kind = {});
    bool logMatches(const QString& line) const;
    void rebuildRecentMenu();
    void connectionDetails(bool copy=false);
    void prefillConnectionRule(const QJsonObject& suggestion);
    bool settingsDirty() const;
    int networkSettingCount() const;
    void commitInstantSettings();
    QString ruleDraftError() const;
    void appendLogLine(const QString& line);
    void resetSettingsDraft();
    void selectSettingsCategory(int index);
    // 把第 index 组滚进视口（滚到组顶），「视野定位」与窗口标题共用同一处几何计算。
    void ensureSettingsGroupVisible(int index);
    void openSettingsTask(const QString& task);
    void returnToSettingsDirectory();
    QStackedWidget* settingsStack_ = nullptr;
    QWidget* settingsDirectory_ = nullptr;
    QString settingsTask_;
    QLabel* settingsTaskTitle_ = nullptr;
    QStackedWidget* profileStack_ = nullptr;
    QPointer<QWidget> profileDetailsPage_;
    QString profileDetailId_;
    void selectGroup(QString group, bool nested = false); void enterSelectedGroup();
    AppController* controller_;
    bool trayAvailable_ = false, updating_ = false;
    bool hasDisplayedSettings_ = false;
    Settings displayedSettings_;
    QListWidget* navigation_;
    QStackedWidget* pages_;
    // K1：内容区首行的固定顶栏（72px）。标题/副标题随路由变化，右侧是状态胶囊与两个图标钮。
    QLabel* topbarTitle_ = nullptr;
    QLabel* topbarSubtitle_ = nullptr;
    QLabel* topbarDot_ = nullptr;
    QLabel* topbarStatusText_ = nullptr;
    // K6：数据页的四个页签与面板（流量/连接/测试/日志）。
    QTabBar* dataTabs_ = nullptr;
    QStackedWidget* dataPanels_ = nullptr;
    // K3：节点详情的右侧抽屉。
    QPointer<QWidget> detailsDrawer_;
    Route route_;
    QToolButton* statusButton_ = nullptr;   // 侧栏常驻状态条，单击回首页控制区
    QLabel* statusHintDot_ = nullptr;
    QLabel* statusLine1_ = nullptr;
    QLabel* statusLine2_ = nullptr;
    QPointer<QWidget> homeScroll_;  // 首页唯一滚动容器，供锚点滚动使用
    QPointer<QWidget> nodeSection_;     // 首页节点区容器（R3 并入首页后由 homePage() 设置）
    WebsiteTestsPage* websitePage_;
    QStatusBar* operationStatus_;
    QLabel* operationNotice_ = nullptr;   // 状态栏「最近操作」段：跨页操作结果
    QLabel* operationAlarm_ = nullptr;    // 状态栏「当前告警」段：state.lastError / 只读保护
    // 页面级提示：操作结果按 OperationId 落到所属页面，不写全局状态栏（P4.1 反馈路由）。
    void routeOperationFeedback(bool ok, const QString& message, OperationId id);
    void setPageFeedback(int page, const QString& text);
    void refreshStatusSegments();
    QStringList pageFeedback_;   // 按页面序号缓存，切回该页时可见
    QString connectionFeedback_;  // 连接页当前操作的结论（与采样状态并存）
    OperationId noticeOperation_ = OperationId::Unspecified;
    QLabel *homeSelection_, *proxyProfileHint_, *statusDot_;
    QComboBox* profilesCombo_;
    QLabel *proxyLabel_, *message_, *groupHint_, *overviewTitle_;
    // 节点区标题行（H3 / 原型 01）：标题、计数、分组说明。
    QLabel *nodeHeadingTitle_ = nullptr, *nodeCount_ = nullptr;
    // 构造顺序敏感：这些指针在页面构造里赋值，updateActions 会在构造中调用，
    // 所以未赋值时必须是 nullptr 而不是堆垃圾（H3 曾因漏初始化 cancelTestAction_ 崩溃）。
    QAction *coreAction_ = nullptr, *testAction_ = nullptr, *testSelectedAction_ = nullptr, *cancelTestAction_ = nullptr;
    QList<QPushButton*> proxyModes_;
    // 节点区的动作都收在「更多」溢出菜单里（ui-layered-design.md:622）。
    QAction *automaticAction_ = nullptr, *locateAction_ = nullptr, *recentNodes_ = nullptr,
        *backGroup_ = nullptr, *enterGroup_ = nullptr, *memberSort_ = nullptr;
    QPushButton *closeConnection_, *connectionPause_;
    QPushButton *proxyMasterSwitch_ = nullptr;   // 更多运行信息里的代理总开关（ADR 0048）
    QLabel* homeConnectivityResult_ = nullptr;   // 控制区第三行：连通性结果
    QLabel* homeControlHint_ = nullptr;          // 控制区：代理开关/切换的对象内结果
    QLabel* nodeGroupHint_ = nullptr;            // 节点区：选中策略组的选路说明
    // 首页控制区（H1）：三行的标签与动作，文案由 updateHome 统一写入。
    QLabel* homeCurrentMember_ = nullptr;        // 第 1 行：当前代理
    QLabel* homeCurrentMeta_ = nullptr;          // 第 1 行：订阅 · 选择方式
    QLabel* homeStateLine_ = nullptr;            // 第 1 行：内核 / 接管 / 可达 状态句
    QLabel* homeRouteValue_ = nullptr;           // 第 2 行：当前路由方式
    QPushButton* homeRouteRow_ = nullptr;        // 第 2 行：整行点击弹出路由方式浮层
    QLabel* homeConnectivityCaption_ = nullptr;  // 第 3 行：结果或测试目标
    QLabel* homeInlineStat_ = nullptr;           // 列表底部一行摘要
    QToolButton* homeCoreToggle_ = nullptr;      // 抽屉：内核单独启停
    QLabel* homeRuntimeHead_ = nullptr;          // 抽屉：内核 / 接管 / 运行版本明细
    QPointer<QWidget> rulesSkeleton_;            // 内核实际规则读取中的骨架占位
    QPushButton* homeConnectivityButton_ = nullptr;
    QToolButton* homeConnectivityLink_ = nullptr;
    QToolButton *coreButton_, *cancelTestButton_, *connectionDetailsButton_, *connectionCopyButton_;
    QLabel *nodeEmpty_, *connectionEmpty_, *logEmpty_ = nullptr;
    QAction *connectionDetailsAction_ = nullptr, *connectionCopyAction_ = nullptr;
    QPushButton *applySettings_, *resetSettings_;
    QMenu* recentMenu_;
    QCheckBox *proxyEnabled_;   // 首页行尾的代理总开关（ADR 0048）
    QComboBox* logLevel_;
    QString selectedGroup_;
    QStringList groupHistory_;
    QPointer<QDialog> groupEditor_;
    QJsonObject revealedNodeFilters_;
    QListView* nodeList_ = nullptr;
    NodeRowModel* nodeRows_ = nullptr;   // 行列表模型（H3），由 nodeSection() 创建，父对象是窗口
    QListView* nodeCards_;
    QStackedWidget* nodeViews_;
    QMenu* groupMenu_ = nullptr;          // 「更多 › 策略组」：浏览另一个策略组（不改节点）
    QLabel* nodeCardsEmpty_;
    ProxyTableModel model_;
    NodeFilter filter_;
    QComboBox *nodeRegion_=nullptr,*nodeProtocol_=nullptr,*nodeHealth_=nullptr,*nodeMaximum_=nullptr;
    QCheckBox* nodeFavoritesOnly_=nullptr;
    // K7：配置页两张列表（远程订阅 / 本地配置），行模型是 controller_->profiles() 的过滤视图。
    ProfileRowModel* profileRowsModel_ = nullptr;
    QListView* remoteProfiles_ = nullptr;
    QListView* localProfiles_ = nullptr;
    ProfileKindFilter remoteProfileFilter_{this};
    ProfileKindFilter localProfileFilter_{this};
    QPointer<QWidget> profileDrawer_;
    QAction* importFilesAction_ = nullptr;   // 标题行「导入文件」
    QAction* updateAllAction_ = nullptr;     // 标题行「更新全部」
    QAction* addProfileAction_ = nullptr;    // 标题行「添加订阅」
    QLineEdit *search_, *logSearch_, *corePath_, *testUrl_;
    QComboBox* logFilter_;
    bool logsPaused_ = false;
    QPushButton* followLogs_ = nullptr;
    QStringList pausedLogs_;
    QStringList displayedLogLines_;
    QPlainTextEdit* logView_;
    QCheckBox *autoStart_, *silentStart_, *startCore_, *restoreProxy_, *lightweight_;
    QSpinBox* mixedPort_;
    QVector<QWidget*> mutations_;
    QTimer refreshTimer_;
    QTimer feedbackTimer_;
    bool recentFeedback_ = false, settingsApplying_ = false;
    QString settingsFeedback_, feedbackError_;
    ActivityTableModel connectionsModel_{true, this}, rulesModel_{false, this};
    QSortFilterProxyModel connectionsFilter_{this};
    QSortFilterProxyModel rulesFilter_{this};
    QTabWidget* rulesTabs_;
    QComboBox *rulesProfiles_, *ruleTarget_;
    QPlainTextEdit* rulesEditor_;
    QPushButton* saveRules_ = nullptr;
    QLabel *rulesHint_, *connectionsHint_;
    QTableView* connectionsTable_;
    ConnectionOptions* connectionOptions_=nullptr;
    QString connectionSearchError_;
    QTabBar* connectionView_;
    QComboBox *startupPage_, *trayClick_, *ipv6Mode_, *unifiedDelayMode_;
    QComboBox *appearance_,*textScale_;
    QComboBox* windowChrome_ = nullptr;
    QWidget* chromeDragArea_ = nullptr;
    QWidget* chromeLeading_ = nullptr;
    QWidget* chromeTrailing_ = nullptr;
    bool integratedChrome_ = false;
    void applyWindowChrome();
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    QComboBox *homeProbeInterval_;
    QLabel* homeProbeResult_;
    QLabel* homeOutboundSummary_;
    QJsonObject homeOutboundSnapshot_;
    quint64 homeOutboundGeneration_=0;
    bool homeOutboundReady_=false;
    QLabel *settingsDraftHint_, *settingsVersion_;
    QList<QWidget*> settingsSections_;
    QPointer<QScrollArea> settingsScroll_;
    QPointer<QWidget> settingsPage_;
    bool settingsSyncing_ = false;
    QLabel* connectionStats_;
    QAction* closeAllConnections_ = nullptr;
    QPointer<QDialog> connectionDetail_;
    QPointer<QDialog> configPreview_;
    QString rulesDraftProfile_;
    bool rulesDirty_ = false;
    bool rulesSaving_ = false;
    QJsonObject rulesExpected_;
    QSet<QString> ruleTargets_,ruleProviders_;
    quint64 rulesReferenceGeneration_=0;
    bool ruleReferencesKnown_=false;
};
}
