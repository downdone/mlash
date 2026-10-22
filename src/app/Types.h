#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVector>

namespace mlash {
struct Result {
    bool ok = true;
    QString error;
    QJsonObject data;
    static Result fail(const QString& message) { return {false, message, {}}; }
};
enum class CoreState { Stopped, Starting, Running, Stopping, Failed };
enum class ProxyState { Disabled, Owned, External, Unknown };
struct DownloadPolicy {
    QString route = "direct", userAgent;
    int timeoutSeconds = 30;
    bool valid() const;
    QJsonObject toJson() const;
    static DownloadPolicy fromJson(const QJsonObject& object);
};
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
    QJsonArray websiteTargets;
    QJsonObject hotkeys;
    QJsonObject connectionView;
    QString appearance="system";
    int textScale=100;
    int homeProbeSeconds=0;
    int homeSummaryMask=15;
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
