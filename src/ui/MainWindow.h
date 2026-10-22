#pragma once
#include "app/AppController.h"
#include "ProxyTableModel.h"
#include "ActivityTableModel.h"
#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QPointer>

class QListWidget; class QStackedWidget; class QComboBox; class QLabel; class QCheckBox;
class QListView;
class QTableView; class QTableWidget; class QLineEdit; class QPlainTextEdit; class QSpinBox; class QPushButton;
class QAction; class QTabWidget; class QMenu; class QDialog;
class QToolButton; class QStatusBar;
class QTabBar; class QHBoxLayout;
class QDragEnterEvent; class QDropEvent;
namespace mlash { class WebsiteTestsPage; }
namespace mlash { class ConnectionOptions; }
namespace mlash {
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppController* controller, QWidget* parent = nullptr);
    ~MainWindow() override;
    void setTrayAvailable(bool available) { trayAvailable_ = available; }
    void setPage(int index);
    int page() const;
    int memberView() const;
    void setMemberView(int mode);
    bool confirmDiscardEdits();
    QJsonObject nodeBookmark() const;
    void restoreNodeBookmark(const QJsonObject& value);
signals:
    void guiReleaseRequested();
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
private:
    QWidget* homePage(); QWidget* proxyPage(); QWidget* profilesPage(); QWidget* diagnosticsPage(); QWidget* settingsPage(); QWidget* rulesPage(); QWidget* connectionsPage();
    void applyAppearance();
    void updateRulesProfiles(); void loadRuleDraft(); void updateConnections();
    void updateHome();
    void updateHomeOutbound(bool capture=false);
    void showOutboundDiagnostics();
    QPointer<QDialog> outboundDialog_;
    void updateState(); void updateProfiles(); void updateProxies(); void updateGroup(); void updateLogs();
    QString selectedProfile() const;
    void addUrl(); void addFile(bool replace); void applySettings(); void addSubscription(bool url);
    void updateActions(); void updateCurrentMember(); void chooseSelectedNode(); void locateCurrentNode();
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
    int settingsChangeCount() const;
    QString ruleDraftError() const;
    void appendLogLine(const QString& line);
    void resetSettingsDraft();
    void selectGroup(const QString& group, bool nested = false); void enterSelectedGroup();
    AppController* controller_;
    bool trayAvailable_ = false, updating_ = false;
    bool hasDisplayedSettings_ = false;
    Settings displayedSettings_;
    QListWidget* navigation_;
    QStackedWidget* pages_;
    WebsiteTestsPage* websitePage_;
    QStatusBar* operationStatus_;
    QLabel *homeSelection_, *homeCounts_, *homeProfileMeta_, *homeMemberIcon_, *proxyProfileHint_, *statusDot_, *profileSelection_;
    QList<QLabel*> dashboardValues_, dashboardDetails_;
    QComboBox* profilesCombo_;
    QPushButton* useProfile_;
    QLabel *coreLabel_, *proxyLabel_, *message_, *groupHint_, *overviewTitle_, *currentMember_, *nodeCount_, *emptyProfiles_, *profileSummary_;
    QAction *coreAction_, *testAction_, *testSelectedAction_, *cancelTestAction_;
    QList<QPushButton*> proxyModes_;
    QPushButton *chooseAction_, *automaticAction_, *locateAction_;
    QPushButton *profileActivate_, *profileUpdate_;
    QAction* profileReplace_;
    QPushButton *recentNodes_, *closeConnection_, *connectionPause_;
    QToolButton *coreButton_, *cancelTestButton_, *connectionDetailsButton_, *connectionCopyButton_;
    QLabel *testProgress_, *nodeEmpty_, *connectionEmpty_;
    QAction *connectionDetailsAction_, *connectionCopyAction_, *profileReload_;
    QPushButton *applySettings_, *resetSettings_;
    QMenu* recentMenu_;
    QAction *profileRollback_, *profileRename_, *profileDelete_, *profilePolicy_;
    QLabel* profileDetails_;
    QCheckBox *proxyEnabled_,*tunEnabled_;
    QComboBox* logLevel_;
    QListWidget* proxyGroups_;
    QToolButton* groupSwitcher_;
    QLabel* selectedGroupTitle_;
    QString selectedGroup_;
    QStringList groupHistory_;
    QPushButton *enterGroup_, *backGroup_;
    QTableView* nodes_;
    QListView* nodeCards_;
    QStackedWidget* nodeViews_;
    QComboBox* memberViewMode_;
    QLabel* nodeCardsEmpty_;
    ProxyTableModel model_;
    NodeFilter filter_;
    QComboBox *nodeRegion_=nullptr,*nodeProtocol_=nullptr,*nodeHealth_=nullptr,*nodeMaximum_=nullptr;
    QCheckBox* nodeFavoritesOnly_=nullptr;
    QTableWidget* profileTable_;
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
    QComboBox *homeProbeInterval_,*homeQuickGroup_,*homeQuickMember_;
    QLabel* homeProbeResult_;
    QWidget* homeSummaryRow_;
    QList<QWidget*> homeSummaryCards_;
    int displayedHomeSummaryMask_=-1;
    QLabel* homeOutboundSummary_;
    QJsonObject homeOutboundSnapshot_;
    quint64 homeOutboundGeneration_=0;
    bool homeOutboundReady_=false;
    QLabel *settingsDraftHint_, *settingsVersion_;
    QLabel* connectionStats_;
    QAction* closeAllConnections_;
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
