#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVector>
#include <algorithm>
#include <iterator>

namespace mlash {
struct Result {
    bool ok = true;
    QString error;
    QJsonObject data;
    static Result fail(const QString& message) { return {false, message, {}}; }
};
enum class CoreState { Stopped, Starting, Running, Stopping, Failed };
enum class ProxyState { Disabled, Owned, External, Unknown };
// 操作标识（界面分层设计 P4.1 / ADR 0046）：让 UI 知道一次结果属于哪个页面或对象。
// 只标注需要落到页面或对象上的操作；对话框内部自带 hint 的动作保持 Unspecified。
enum class OperationId : quint8 {
    Unspecified = 0,
    ProfileUpdate, ProfileActivate, ProfileBatch, ProfileImport,
    ConnectionClose, ConnectionCloseAll,
    ProxyToggle, TunToggle, ModeSwitch, MemberSelect,
    CoreStart, CoreStop, CoreUpdate, Backup, Restore, SettingsApply, RulesSave
};
struct DownloadPolicy {
    QString route = "direct", userAgent;
    int timeoutSeconds = 30;
    bool valid() const;
    QJsonObject toJson() const;
    static DownloadPolicy fromJson(const QJsonObject& object);
};
struct CoreHistoryEntry {
    QString sha256, version, path, source, recordedAt;
    qint64 bytes = 0;
    bool valid(const QStringList& allowedSources) const;
    static QStringList sources();
    QJsonObject toJson() const;
    static CoreHistoryEntry fromJson(const QJsonObject& object);
    bool operator==(const CoreHistoryEntry& other) const { return sha256==other.sha256&&version==other.version&&path==other.path&&source==other.source&&recordedAt==other.recordedAt&&bytes==other.bytes; }
};
// Bounded newest-first history; one entry per sha256, bundled core stays implicit.
inline QVector<CoreHistoryEntry> appendedCoreHistory(QVector<CoreHistoryEntry> history, const CoreHistoryEntry& entry) {
    history.erase(std::remove_if(history.begin(), history.end(), [&entry](const CoreHistoryEntry& existing) { return existing.sha256 == entry.sha256; }), history.end());
    history.prepend(entry);
    while (history.size() > 8) history.removeLast();
    return history;
}
struct Settings {
    bool autoStart = false;
    bool silentStart = true;
    bool startCore = true;
    bool restoreProxy = true;
    bool desiredProxy = false;
    bool lightweight = true;
    bool tunEnabled = false, tunStrictRoute = false;
    QString tunStack = "mixed";
    QStringList tunExcludeRoutes;
    int tunMtu = 1500;
    bool tunDnsHijack = true;
    int mixedPort = 17890;
    QString logLevel = "warning";
    QString testUrl = "https://www.gstatic.com/generate_204";
    int startupPage = 0;
    QString trayClick = "show", ipv6Mode = "source", unifiedDelayMode = "source";
    QString activeProfile;
    QString corePath;
    QString previousCorePath;
    QVector<CoreHistoryEntry> coreHistory;
    QJsonArray websiteTargets;
    QJsonObject hotkeys;
    QJsonObject connectionView;
    QString appearance="system";
    int textScale=100;
    int homeProbeSeconds=0;
    QString proxyBypass="localhost;127.*;[::1];<local>";
    QJsonObject remoteBackup;
    QJsonObject localBackup;
    QJsonObject toJson() const;
    static Settings fromJson(const QJsonObject& object);
};
struct Profile {
    QString id, name, sourceType, source, candidate, active, lastGood, updated;
    QJsonObject selections;
    bool rememberSelections = true;
    QStringList customRules;
    QStringList appendRules, disabledRules;
    QString dnsOverride;
    bool dnsOverrideEnabled = false;
    QJsonObject proxyEdits;
    bool autoUpdate = false;
    int updateIntervalHours = 12;
    QString sourceHash, lastUpdateAttempt, updateError;
    QJsonObject subscriptionInfo;
    QJsonArray recentSelections;
    QJsonObject nodePreferences;
    QJsonObject ruleProviderEdits;
    QJsonObject providerPolicies;
    DownloadPolicy download;
    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject& object);
};
struct AppState {
    CoreState core = CoreState::Stopped;
    ProxyState proxy = ProxyState::Unknown;
    QString mode = "rule", coreVersion, activeProfile, lastError;
    bool ready = false, busy = true, exiting = false, readOnly = false;
    bool autoStartKnown = false, autoStartEnabled = false, autoStartConflict = false;
    bool tunObserved = false;
    quint64 generation = 0;
};
QString coreStateText(CoreState state);
QString proxyStateText(ProxyState state);
}
// 信号签名进入 moc 元对象，QSignalSpy 与跨 TU 取用都要求它是已注册类型。
Q_DECLARE_METATYPE(mlash::OperationId)
