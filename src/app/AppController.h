#pragma once
#include "Types.h"
#include "HomeMetrics.h"
#include "HomeNodeProbe.h"
#include "OutboundDiagnostics.h"
#include "platform/GlobalHotkeys.h"
#include "ProfileMaintenance.h"
#include "ConnectionSession.h"
#include "WebsiteTests.h"
#include <QElapsedTimer>
#include <QDateTime>
#include <QDeadlineTimer>
#include "core/CoreManager.h"
#include "core/CoreUpdate.h"
#include "core/OfficialCoreSource.h"
#include "profile/ProfileStore.h"
#include "profile/SubscriptionDownloader.h"
#include "profile/WebDavClient.h"
#include "platform/ProxyCoordinator.h"
#include "diagnostics/Diagnostics.h"
#include <QTimer>
#include <QJsonArray>
#include <optional>

namespace mlash {
class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QString root, bool smoke = false, QObject* parent = nullptr, std::shared_ptr<PlatformServices> platform = {}, QList<WebsiteTarget> websiteTargets = WebsiteTests::defaults(), std::unique_ptr<GlobalHotkeys> hotkeys = {},std::unique_ptr<WebDavClient> remoteClient={},std::unique_ptr<TunSession> tun={},std::unique_ptr<SubscriptionDownloader> sourceDownloader={},std::unique_ptr<SubscriptionDownloader> officialDownloader={});
    void initialize();
    void suppressAutomaticStartup() { startupSuppressed_=true; }
    const AppState& state() const { return state_; }
    const Settings& settings() const { return settings_; }
    const QVector<Profile>& profiles() const { return profiles_; }
    const QJsonObject& proxies() const { return proxies_; }
    qint64 coreProcessId() const { return core_.processId(); }
    Diagnostics* diagnostics() { return &diagnostics_; }
    QString dataRoot() const { return root_; }
    void startCore(bool allowProxyRestore = true);
    void stopCore();
    void setTunEnabled(bool enabled);
    void updateTunOptions(bool enabled,const QString& stack,bool strictRoute);
    void updateTunSettings(const Settings& options,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    void inspectTunService(Completion done);
    void manageTunService(bool install);
    void repairTunService();
    void retryTunCleanup();
    void inspectNetworkInterfaces(Completion done);
    QJsonObject tunObservation() const { return core_.tunObservation(); }
    void setSystemProxy(bool enabled, bool allowExternal = false);
    void setMode(const QString& mode);
    void selectMember(const QString& group, const QString& member);
    void clearAutomaticSelection(const QString& group);
    void importUrl(const QString& name, const QUrl& url, const DownloadPolicy& policy = {});
    void importFile(const QString& name, const QString& path, const QString& replaceId = {});
    void importNodeLinks(const QString& name,const QString& text);
    void previewNodeLinks(const QString& text,const QStringList& reserved,Completion done);
    void updateProfile(const QString& id);
    void updateProfilesBatch(const QStringList& ids);
    void cancelProfileBatch();
    const QJsonObject& profileBatch() const { return profileBatch_; }
    QJsonObject profileMaintenance(const QString& id) const;
    void editProfileSource(const QString& id,const QString& name,const QString& source,bool automatic,int hours,const DownloadPolicy& policy,const QJsonObject& expected);
    void moveProfile(const QString& id,int direction);
    void updateNodePreferences(const QString& id,const QJsonObject& preferences,const QJsonObject& expected);
    QJsonObject nodePreferences(const QString& id={}) const;
    QString groupTestUrl(const QString& group) const;
    void updateProfilePolicy(const QString& id, bool automatic, int intervalHours, std::optional<DownloadPolicy> policy = {});
    void activateProfile(const QString& id, bool lastGood = false);
    void renameProfile(const QString& id, const QString& name);
    void deleteProfile(const QString& id);
    void updateSettings(const Settings& settings);
    void importGeoData(const QString& sourceDirectory);
    void inspectGeoData(Completion done);
    void setHomeVisible(bool visible);
    void setHomeProbeGroup(const QString& group);
    void updateHomeProbe(int seconds);
    void updateHomeSummary(int mask,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    void updateProxyBypass(const QString& text,const std::optional<QString>& expected=std::nullopt);
    void inspectUrlProtocol(Completion done);
    void setUrlProtocol(bool enabled);
    void checkOfficialCore(Completion done);
    void downloadOfficialCore(Completion done);
    void previewCoreUpdate(const QString& file,Completion done,const QString& expectedVersion={});
    void cancelCoreUpdatePreview();
    void applyCoreUpdate(const QString& token);
    void cleanCoreCopies();
    void updateLocalBackupConfig(const QJsonObject& config,const QJsonObject& expected);
    void startLocalBackup(bool scheduled=false);
    void inspectLocalBackups(Completion done);
    void updateRemoteBackupConfig(const QJsonObject& config,const QJsonObject& expected);
    void startRemoteBackup(bool scheduled=false);
    void inspectRemoteBackups(Completion done);
    void downloadRemoteBackup(const QString& file,const QString& destination);
    void cancelRemoteBackup();
    const QJsonObject& remoteBackupStatus() const { return remoteBackupStatus_; }
    const HomeNodeProbe* homeProbe() const { return &homeProbe_; }
    const HomeMetrics* homeMetrics() const { return &homeMetrics_; }
    const OutboundDiagnostics* outboundDiagnostics() const { return &outboundDiagnostics_; }
    void startIpProbe(bool coreRoute=true);
    void startServiceProbe(bool coreRoute=true,const QString& id={});
    void updateHotkeys(const QJsonObject& bindings,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    quint64 beginHotkeyCapture();
    void endHotkeyCapture(quint64 token);
    bool capturingHotkey() const { return hotkeyCaptureToken_!=0; }
    void reloadActive();
    void updateConnectionView(const QJsonObject& preferences);
    void updateAppearance(const QString& appearance,int scale);
    QString hotkeyError() const { return hotkeyError_; }
    const QJsonObject& activeHotkeys() const { return activeHotkeys_; }
    void cancelOutboundDiagnostics() { outboundDiagnostics_.cancel(); }
    void updateGeoData(const QString& name);
    void previewActiveConfig(std::function<void(Result,QString)> done);
    void inspectProfileCompatibility(const QString& id, Completion done);
    void inspectProviders(const QString& id, Completion done, bool runningVersion=false);
    void inspectProfileChanges(const QString& id, Completion done);
    void refreshProviders(const QString& id, const QString& providerId = "*",bool background=false);
    void updateProviderPolicy(const QString& id,const QString& providerId,const QJsonObject& policy,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    void exportBackup(const QString& path, const QString& password, bool includeProfiles = true);
    void previewBackup(const QString& path, const QString& password, Completion done);
    void restoreBackup(const QString& path, const QString& password, const QString& expectedHash, bool restoreSettings, const QJsonValue& selectedProfiles = QJsonValue(QJsonValue::Undefined));
    void updateCustomRules(const QString& id, const QStringList& rules,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    void updateRuleDefinitions(const QString& id,const QStringList& prepend,const QStringList& append,const QStringList& disabled,const QJsonObject& providers,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    void updateRuleOverlay(const QString& id, const QStringList& prepend, const QStringList& append, const QStringList& disabled,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    void inspectRuleSource(const QString& id, Completion done);
    void updateDnsOverride(const QString& id, const QString& yaml, bool enabled,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined));
    void inspectDns(const QString& id, Completion done);
    void inspectProxyEntries(const QString& id,Completion done);
    void inspectProxyImpact(const QString& id,const QJsonObject& draft,const QString& name,const QJsonObject& sourceExpected,Completion done);
    void updateProxyEdits(const QString& id,const QJsonObject& edits,const QJsonValue& expected=QJsonValue(QJsonValue::Undefined),const QJsonValue& sourceExpected=QJsonValue(QJsonValue::Undefined));
    void updateProxyDialers(const QString& id,const QJsonObject& dialers,const QJsonObject& expected);
    quint64 queryDns(const QString& name, const QString& type, Completion done);
    void cancelDnsQuery(quint64 token = 0);
    void setConnectionsVisible(bool visible);
    void setConnectionsPaused(bool paused);
    bool connectionsPaused() const { return connectionsPaused_; }
    QString connectionSampleTime() const { return connectionSession_.observedAt(); }
    bool connectionsCurrent() const { const auto& totals=connectionSession_.totals(); return connectionsVisible_&&!connectionsPaused_&&totals.contains("_observed_at")&&!totals.value("_paused").toBool(); }
    const QJsonArray& connections() const { return connections_; }
    const QJsonArray& closedConnections() const { return connectionSession_.closed(); }
    const QJsonObject& connectionTotals() const { return connectionSession_.totals(); }
    QString connectionError() const { return connectionError_; }
    bool connectionsVisible() const { return connectionsVisible_; }
    void refreshRules();
    void setRulesVisible(bool visible);
    const QJsonArray& activeRules() const { return activeRules_; }
    void refresh();
    void testNodes(const QStringList& names,const QString& group={});
    bool testingNodes() const { return state_.busy && (delayInFlight_ > 0 || !delayQueue_.isEmpty()); }
    void cancelTests();
    void testTrayGroup(const QString& group, bool force = false);
    void cancelTrayTests();
    const WebsiteTests* websiteTests() const { return &websiteTests_; }
    QString websiteNode(const QString& groupOrNode) const;
    QString homeNode() const;
    void setWebsiteTestsVisible(bool visible);
    void startWebsiteTests(const QString& node,const QString& targetId={});
    void cancelWebsiteTests();
    void clearWebsiteTests();
    void updateWebsiteTargets(const QJsonArray& targets);
    QJsonObject trayNodeDelay(const QString& name,const QString& group={}) const;
    QString lastNodeTestUrl() const { return lastNodeTestUrl_; }
    void setDiagnosticsVisible(bool visible);
    void setDiagnosticsLevel(const QString& level);
    void clearLogView();
    void exportDiagnostics(const QString& path);
    void closeConnection(const QString& id);
    void closeAllConnections();
    void requestExit();
signals:
    void profileBatchChanged();
    void remoteBackupChanged();
    void remoteBackupDownloaded(const QString& path);
    void hotkeyCaptureChanged(quint64 token);
    void hotkeyCaptured(quint64 token,const QString& sequence);
    void openPageRequested(int page);
    void initialized();
    void stateChanged();
    void profilesChanged();
    void proxiesChanged();
    void connectionsChanged();
    void rulesChanged();
    void operationFinished(bool ok, const QString& message);
    void operationStarted(const QString& message);
    void nodeDelay(const QString& name, int delay, const QString& error);
    void delayProgress(int done, int total);
    void trayDelaysChanged();
    void exitReady();
private:
    void dispatchHotkey(const QString& action);
    bool begin(const QString& operation, bool background = false, bool allowReadOnly = false);
    void finish(Result result);
    void publish();
    int profileIndex(const QString& id) const;
    int chooseApiPort() const;
    QString createSecret() const;
    void applyProxyObservation(const Result& result);
    void saveSnapshot(Settings settings, QVector<Profile> profiles, Completion done);
    void checkPrepared(PreparedProfile prepared, std::function<void(PreparedProfile)> done, const QString& executable = {});
    void stageSource(Profile profile, QByteArray source);
    void prepareActivation(Profile profile, const QString& version, Settings settings);
    void transact(PreparedProfile prepared, Settings settings);
    void verifyRuleOverlay(const QStringList& prepend, const QStringList& append, int expectedCount, Completion done);
    void restorePrevious(Settings settings, QVector<Profile> profiles, bool wasRunning, const QString& failure);
    struct ActivationTransaction;
    enum class ActivationStage { Release, Stop, Start, Verify, Select, Save, Resume, Completed };
    using Stage = ActivationStage;
    using Activation = std::shared_ptr<ActivationTransaction>;
    void releaseForActivation(Activation transaction);
    void stopForActivation(Activation transaction);
    void startForActivation(Activation transaction);
    void verifyActivation(Activation transaction, Result started);
    void reconcileActivation(Activation transaction);
    void commitActivation(Activation transaction, Profile profile);
    void failActivation(Activation transaction, const QString& error);
    struct RollbackTransaction;
    using Rollback = std::shared_ptr<RollbackTransaction>;
    void prepareRollback(Rollback transaction);
    void startRollback(Rollback transaction, PreparedProfile prepared);
    void commitRollback(Rollback transaction, Profile profile, const QString& version);

    void resumeDesiredProxy(Completion done);
    void reconcileSelections(Profile profile, std::function<void(Result, Profile)> done);
    void restoreSelectionList(Profile profile, QJsonObject proxies, QStringList groups, std::function<void(Result, Profile)> done);
    void closeAffectedConnections(const QStringList& ids,int skipped,Completion done);
    void handleCrash(const QString& error);
    void runDelayBatch();
    void runTrayDelayBatch();
    void recordTrayDelay(const QString& name, int delay, const QString& error,const QString& url={});
    void refreshConnections();
    void cancelConnectionRead();
    void acceptConnectionSnapshot(const QJsonObject& snapshot);
    void runMaintenance();
    void configureProviderMaintenance();
    void runProviderMaintenance();
    void completeProviderRefresh(const QString& id,const QString& providerId,const Result& result);
    void refreshSource(const QString& id, bool background);
    void runProfileBatch();
    bool profileBatchActive_=false,profileBatchDispatch_=false,profileBatchCancelled_=false;
    QStringList profileBatchQueue_;
    QString profileBatchCurrent_;
    QJsonObject profileBatch_;
    void downloadProfileSource(const Profile& profile, const QUrl& url, std::function<void(Result, QByteArray)> done);
    void updateProfileOverrides(Profile profile, const QString& operation,const QStringList& refreshProviders={});
    void verifyProfileOverrides(const Profile& profile,int expectedCount,Completion done);
    void updateHomeProbeContext();
    void startOutboundProbe(bool coreRoute,bool services,const QString& id);
    void drainConnectionView();
    bool beginPreferences(const QString& operation);
    bool preferencesOnly_=false;
    std::optional<QJsonObject> pendingConnectionView_;
    void updateDiagnosticStream();
    void rememberPrivateSource(const Profile& profile);
    void runRemoteMaintenance();
    void runLocalMaintenance();
    void protectMajorChange(std::function<void()> action);
    void remoteRoute(const QJsonObject& config,std::function<void(Result,int)> done);
    void completeRemoteUpload(Result result,const QString& localPath={});
    void finishRemoteOperation(Result result);
    void rememberRemoteSecrets();
    QString root_;
    bool smoke_, readOnly_ = false, diagnosticsVisible_ = false, exitPending_ = false, crashPending_ = false;
    QString readOnlyError_;
    bool proxyRestoreAllowed_ = true;
    QString crashError_;
    int restartAttempts_ = 0, delayInFlight_ = 0, delayCompleted_ = 0, delayTotal_ = 0;
    quint64 delayGeneration_ = 0;
    QStringList delayQueue_;
    QString delayTestUrl_;
    struct TrayDelay { int delay; QString error; qint64 measured; QDateTime time; };
    QString trayTestUrl_,lastNodeTestUrl_;
    QHash<QString, TrayDelay> trayDelays_;
    QHash<QString, QPointer<QNetworkReply>> trayRequests_;
    QStringList trayQueue_;
    QElapsedTimer trayClock_;
    quint64 trayGeneration_ = 0, trayCacheGeneration_ = 0, trayRunGeneration_ = 0;
    AppState state_;
    Settings settings_;
    QVector<Profile> profiles_;
    QJsonObject proxies_;
    std::shared_ptr<PlatformServices> platform_;
    std::unique_ptr<GlobalHotkeys> hotkeys_;
    std::unique_ptr<WebDavClient> remoteClient_;
    QTimer localBackupTimer_;
    QDateTime localBackupRetryAfter_;
    bool changeBackupDone_=false,localBackupInspectPending_=false;
    QTimer remoteTimer_; QDateTime remoteRetryAfter_;
    bool remoteInitialized_=false,remoteActive_=false,remoteCancelled_=false,remoteMetadata_=false;
    QJsonObject remoteBackupStatus_;
    bool hotkeysLoading_=false;
    QString hotkeyError_;
    QJsonObject activeHotkeys_;
    quint64 hotkeyCaptureSerial_=0,hotkeyCaptureToken_=0;
    QDeadlineTimer hotkeyQuietUntil_;
    std::shared_ptr<ProfileStore> store_;
    std::shared_ptr<ProxyCoordinator> proxy_;
    Diagnostics diagnostics_;
    MihomoClient api_;
    HomeMetrics homeMetrics_{&api_,this};
    HomeNodeProbe homeProbe_{&api_,this};
    QString homeProbeGroup_;
    OutboundDiagnostics outboundDiagnostics_{this};
    WebsiteTests websiteTests_;
    bool websiteTestsVisible_=false;
    CoreManager core_;
    OfficialCoreSource officialCore_{this};
    CoreVersionProbe coreVersionProbe_{this};
    QJsonObject corePreview_;
    quint64 corePreviewGeneration_=0;
    std::unique_ptr<SubscriptionDownloader> downloader_;
    QTimer health_;
    ProfileMaintenance maintenance_{this};
    QTimer providerTimer_;
    bool providerSchedulePending_=false;
    int providerScheduleCursor_=0;
    QHash<QString,QJsonObject> providerAttempts_;
    QString sourceUpdateId_;
    bool backgroundOperation_ = false;
    bool previewPending_ = false;
    bool compatibilityPending_ = false;
    bool profileChangesPending_ = false;
    bool providersPending_ = false;
    bool backupPreviewPending_ = false;
    int ruleSourcePending_ = 0;
    bool dnsPreviewPending_ = false;
    bool startupSuppressed_=false,protocolPreviewPending_=false;
    bool proxyPreviewPending_ = false;
    bool nodeLinksPreviewPending_ = false;
    bool geoPreviewPending_ = false;
    QPointer<QNetworkReply> dnsQuery_;
    quint64 dnsQueryGeneration_ = 0;
    QHash<QString, QString> providerErrors_;
    QString diagnosticsLevel_ = "info";
    QTimer connectionsTimer_;
    QJsonArray connections_;
    ConnectionSession connectionSession_;
    QElapsedTimer connectionClock_;
    QString connectionError_;
    bool connectionsVisible_ = false, connectionsPending_ = false, connectionsPaused_ = false;
    quint64 connectionsGeneration_ = 0;
    QPointer<QNetworkReply> connectionRead_;
    QJsonArray activeRules_;
    bool rulesVisible_ = false;
    quint64 rulesGeneration_ = 0;
};
}
