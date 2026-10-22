#include "app/AppController.h"
#include "storage/Storage.h"
#include "profile/ConfigBuilder.h"
#include "profile/SubscriptionInfo.h"
#include "app/ProfileMaintenance.h"
#include "MockHttp.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QFile>
#include <QElapsedTimer>
#include <QTimer>
#include <QTcpServer>
#include <QSslSocket>
#include "ui/ProxyTableModel.h"
#include <QSortFilterProxyModel>
#include <QRandomGenerator>
#include <yaml-cpp/yaml.h>

using namespace mlash;
namespace {
// Simulate an external editor: Windows read handles/AV may briefly deny rename.
// This is fixture input setup, not a change to production atomic-write semantics.
Result rewriteFixture(const QString& path,const QByteArray& bytes) {
    QElapsedTimer elapsed; elapsed.start();
    for (;;) {
        auto result=writeAtomic(path,bytes);
        if (result.ok||elapsed.elapsed()>=1000) return result;
        QTest::qWait(10);
    }
}
}

class MemoryPlatform final : public PlatformServices {
public:
    QJsonObject current{{"flags", 1}, {"server", ""}, {"bypass", "original"}, {"pac", ""}};
    int writes = 0; bool failWrite = false, failRead = false, failSeal = false;
    bool autoEnabled = false;
    int autoWrites = 0;
    Result readProxy() override { return failRead ? Result::fail("read failure") : Result{true, {}, current}; }
    Result writeProxy(const QJsonObject& o) override { ++writes; if (failWrite) return Result::fail("write failure"); current = o; return {}; }
    QJsonObject proxyTarget(int port) const override { return {{"flags", 3}, {"server", QString("127.0.0.1:%1").arg(port)}, {"bypass", "local"}, {"pac", ""}}; }
    bool proxyEnabled(const QJsonObject& o) const override { return o.value("flags").toInt() != 1; }
    Result autoStart(bool enabled, const QString&) override { ++autoWrites; autoEnabled = enabled; return {}; }
    Result readAutoStart(const QString&) override { return {true, {}, {{"enabled", autoEnabled}}}; }
    Result seal(const QByteArray& b, QByteArray& out) override { if (failSeal) return Result::fail("seal failure"); out = "sealed:" + b.toBase64(); return {}; }
    Result open(const QByteArray& b, QByteArray& out) override { if (!b.startsWith("sealed:")) return Result::fail("invalid ciphertext"); out = QByteArray::fromBase64(b.mid(7)); return {}; }
    Result secureDirectory(const QString& root) override { return QDir().mkpath(root) ? Result{} : Result::fail("mkdir failure"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64, int) override { return {true, {}, {{"owned", true}}}; }
};
class EngineTests : public QObject {
    Q_OBJECT
private slots:
    void settingsRoundTrip();
    void pendingCandidateSurvivesRuntimeChanges();
    void malformedAndOversizedStateNeverReplacesGoodIndex();
    void failedExitRecoveryCanRetry();
    void autoStartObservationSurvivesIndexFailure();
    void connectionSessionRatesClosureAndBounds();
    void coreOptionsAndGeoSnapshots();
    void damagedJournalNeverWritesProxy();
    void readOnlyDiagnosticsPreserveState();
    void diagnosticApiPreferencesAndRecentMembers();
    void recentMembersBoundedAndRestored();
    void subscriptionInfoAndPolicyCompatibility();
    void maintenanceDeadlinesAndRetry();
    void fixtureRewriteWaitsForShortReadHandle();
    void localMaintenancePreservesActive();
    void managedYamlPreservesRules();
    void halfCloseDefaultsAndSourceOverrides();
    void compatibilityReportNoSecrets();
    void customRulesOverlayAndIsolation();
    void unsafeConfigurationRejected_data();
    void unsafeConfigurationRejected();
    void sourceSnapshotAndEncryptedIndex();
    void providerSnapshotsAndProtocolBoundary();
    void providerRefreshIsolationAndMetadata();
    void tlsAvailable();
    void corruptStateDoesNotReset();
    void proxyOwnershipAndPacRestore();
    void externalProxyNeverOverwritten();
    void journalPrecedesOsWrite();
    void proxyFailureAndUnknownRead();
    void redaction();
    void api204AndEncodedNames();
    void apiTimeoutInvalidJsonAndCancel();
    void logStreamStopsWhenHidden();
    void largeNodeModelSorting();
    void privateAddresses_data();
    void privateAddresses();
    void downloadRedirectAndSize();
    void downloadPolicyRouteAndProtection();
    void fakeCoreLifecycleAndValidation();
    void slowValidationTimeoutAndCancel();
    void geoSnapshotReuse();
    void fakeCoreUnexpectedExit();
    void realCoreApi();
    void controllerImportActivateRollback();
};
static int freePort() {
    // Fixed proxy ports should not race with automatic client ports in Windows' dynamic range.
    for (int i = 0; i < 100; ++i) {
        QTcpServer server; const int port = QRandomGenerator::global()->bounded(20000, 40000);
        if (server.listen(QHostAddress::LocalHost, port)) return port;
    }
    return 0;
}
static QByteArray minimalYaml() { return "mode: rule\nproxies: []\nproxy-groups:\n  - name: Test\n    type: select\n    proxies: [DIRECT, REJECT]\nrules: [\"MATCH,Test\"]\n"; }
static QByteArray runnableYaml(int mixed, int api, bool crash = false) {
    return "mixed-port: " + QByteArray::number(mixed) + "\nexternal-controller: 127.0.0.1:" + QByteArray::number(api) + "\nsecret: " + QByteArray(64, 'a') + "\n" + minimalYaml() + (crash ? "fake-crash: true\n" : "");
}
void EngineTests::settingsRoundTrip() {
    Settings settings; settings.mixedPort = 19001; settings.desiredProxy = true; settings.activeProfile = newId();
    const auto copy = Settings::fromJson(settings.toJson()); QCOMPARE(copy.mixedPort, settings.mixedPort); QCOMPARE(copy.desiredProxy, true); QCOMPARE(copy.activeProfile, settings.activeProfile);
    auto object = settings.toJson(); object["mixed_port"] = -1; QCOMPARE(Settings::fromJson(object).mixedPort, 17890);
    QVERIFY(Settings::fromJson(QJsonObject{}).lightweight); settings.lightweight = false; QVERIFY(!Settings::fromJson(settings.toJson()).lightweight);
}
void EngineTests::managedYamlPreservesRules() {
    const auto source = minimalYaml() + "ipv6: false\ndns:\n  enable: true\n  enhanced-mode: fake-ip\n  nameserver: [https://dns.google/dns-query]\n";
    const auto built = ConfigBuilder::build(source, Settings{}, 19090, QString(64, 'a')); QVERIFY2(built.result.ok, qUtf8Printable(built.result.error));
    const auto output = YAML::Load(built.yaml.toStdString());
    QCOMPARE(output["mixed-port"].as<int>(), 17890); QVERIFY(!output["allow-lan"].as<bool>()); QVERIFY(!output["tun"]["enable"].as<bool>());
    QVERIFY(!output["ipv6"].as<bool>()); QCOMPARE(output["proxy-groups"][0]["name"].as<std::string>(), std::string("Test"));
    QCOMPARE(output["dns"]["nameserver"][0].as<std::string>(), std::string("https://dns.google/dns-query"));
}
void EngineTests::halfCloseDefaultsAndSourceOverrides() {
    const auto secret=QString(64,'a');
    const QList<QByteArray> additions{"", "tcp-half-close-timeout: {}\n", "tcp-half-close-timeout: {client: 0, server: 0}\n", "tcp-half-close-timeout: {client: 12}\n", "tcp-half-close-timeout: {server: 45}\n"};
    const QList<int> clients{300,300,0,12,300},servers{30,30,0,30,45};
    for (int i=0;i<additions.size();++i) { const auto built=ConfigBuilder::build(minimalYaml()+additions[i],Settings{},19090,secret); QVERIFY2(built.result.ok,qUtf8Printable(built.result.error)); const auto value=YAML::Load(built.yaml.toStdString())["tcp-half-close-timeout"]; QCOMPARE(value["client"].as<int>(),clients[i]); QCOMPARE(value["server"].as<int>(),servers[i]); }
    for (const auto& value:QList<QByteArray>{"null","true","[]","{other: private-halfclose-token}","{client: -1}","{server: 0.5}","{client: 9223372037}","{client: null}","{server: private-halfclose-token}"}) { const auto built=ConfigBuilder::build(minimalYaml()+"tcp-half-close-timeout: "+value+"\n",Settings{},19090,secret); QVERIFY(!built.result.ok); QVERIFY(built.result.data.value("code").toString().startsWith("HALFCLOSE_")); QVERIFY(!built.result.error.contains("private-halfclose-token")); }
}
void EngineTests::compatibilityReportNoSecrets() {
    const auto secret = QString(64, 'a');
    auto result = ConfigBuilder::build(minimalYaml() + "dns: {listen: '0.0.0.0:53'}\n", Settings{}, 19090, secret);
    QVERIFY(!result.result.ok); QCOMPARE(result.result.data.value("code").toString(), QString("DNS_LISTENER")); QCOMPARE(result.result.data.value("path").toString(), QString("dns.listen")); QVERIFY(!result.result.data.value("suggestion").toString().isEmpty());
    result = ConfigBuilder::build("proxy-providers: {private-name: {type: file, path: 'C:/private-key-secret'}}\n", Settings{}, 19090, secret);
    QVERIFY(!result.result.ok); QCOMPARE(result.result.data.value("path").toString(), QString("proxy-providers[0].type")); QVERIFY(!result.result.error.contains("private-name")); QVERIFY(!result.result.error.contains("private-key-secret"));
    result = ConfigBuilder::build("proxies: [{name: hidden, type: unknown, password: private-password}]", Settings{}, 19090, secret);
    QVERIFY(!result.result.ok); QCOMPARE(result.result.data.value("path").toString(), QString("proxies[0].type")); QVERIFY(!result.result.error.contains("private-password"));
    result = ConfigBuilder::build("dns: [private-token\n", Settings{}, 19090, secret);
    QVERIFY(!result.result.ok); QCOMPARE(result.result.data.value("code").toString(), QString("YAML_SYNTAX")); QVERIFY(result.result.data.value("line").toInt() > 0); QVERIFY(!result.result.error.contains("private-token"));
    result = ConfigBuilder::build(minimalYaml() + "secret: private-password\nallow-lan: true\n", Settings{}, 19090, secret);
    QVERIFY(result.result.ok); QVERIFY(result.result.data.value("managed_fields").toArray().contains("secret")); QVERIFY(result.result.data.value("managed_fields").toArray().contains("allow-lan")); QVERIFY(!QJsonDocument(result.result.data).toJson().contains("private-password"));
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(directory.path(), platform);
    Profile profile; profile.id = newId(); profile.name = "Report"; profile.sourceType = "file";
    auto prepared = store.prepare(profile, minimalYaml(), Settings{}, 19090, secret); QVERIFY(prepared.result.ok); QVERIFY(store.save(Settings{}, {prepared.profile}).ok);
    const auto before = QDir(store.versionRoot(profile.id, prepared.version) + "/..").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    AppController controller(directory.path(), true, nullptr, platform); controller.initialize(); QTRY_VERIFY(!controller.state().busy);
    bool completed = false; controller.inspectProfileCompatibility(profile.id, [&](Result report) { QVERIFY(report.ok); completed = true; }); QTRY_VERIFY(completed);
    QCOMPARE(controller.profiles()[0].candidate, prepared.version); QCOMPARE(platform->writes, 0);
    QCOMPARE(QDir(store.versionRoot(profile.id, prepared.version) + "/..").entryList(QDir::Dirs | QDir::NoDotAndDotDot), before);
}
void EngineTests::customRulesOverlayAndIsolation() {
    const QStringList rules{"DOMAIN-SUFFIX,example.test,DIRECT", "IP-CIDR,192.0.2.0/24,Test,no-resolve"};
    const auto built = ConfigBuilder::build(minimalYaml(), Settings{}, 19090, QString(64, 'a'), rules); QVERIFY2(built.result.ok, qUtf8Printable(built.result.error));
    const auto config = YAML::Load(built.yaml.toStdString()); QCOMPARE(config["rules"].size(), size_t(3)); QCOMPARE(config["rules"][0].as<std::string>(), rules[0].toStdString()); QCOMPARE(config["rules"][2].as<std::string>(), std::string("MATCH,Test"));
    QVERIFY(!ConfigBuilder::build(minimalYaml(), Settings{}, 19090, "secret", {"DOMAIN,a,Missing"}).result.ok);
    QVERIFY(!ConfigBuilder::build(minimalYaml(), Settings{}, 19090, "secret", {"RULE-SET,a,DIRECT"}).result.ok);
    QVERIFY(!ConfigBuilder::build(minimalYaml(), Settings{}, 19090, "secret", {"DOMAIN,a,DIRECT,no-resolve"}).result.ok);
    QVERIFY(!ConfigBuilder::build(minimalYaml(), Settings{}, 19090, "secret", {QString(1025, 'x')}).result.ok);
    QTemporaryDir data; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(data.path(), platform); QVERIFY(store.load().result.ok);
    Profile a; a.id = newId(); a.name = "First"; a.sourceType = "file"; a.customRules = rules;
    Profile b; b.id = newId(); b.name = "Second"; b.sourceType = "file";
    const auto first = store.prepare(a, minimalYaml(), Settings{}, 19090, QString(64, 'a')); QVERIFY(first.result.ok);
    const auto second = store.prepare(b, minimalYaml(), Settings{}, 19090, QString(64, 'b')); QVERIFY(second.result.ok);
    QVERIFY(store.save(Settings{}, {first.profile, second.profile}).ok); const auto loaded = store.load(); QVERIFY(loaded.result.ok); QCOMPARE(loaded.profiles[0].customRules, rules); QVERIFY(loaded.profiles[1].customRules.isEmpty());
    QByteArray bytes; QVERIFY(readFile(store.versionRoot(a.id, first.version) + "/source.yaml", bytes).ok); QCOMPARE(bytes, minimalYaml());
    QVERIFY(readFile(second.configPath, bytes).ok); QCOMPARE(YAML::Load(bytes.toStdString())["rules"].size(), size_t(1));
    auto old = first.profile.toJson(); old.remove("custom_rules"); QVERIFY(Profile::fromJson(old).customRules.isEmpty());
    const auto again = store.prepare(first.profile, minimalYaml(), Settings{}, 19090, QString(64, 'c')); QVERIFY(again.result.ok); QVERIFY(readFile(again.configPath, bytes).ok); QCOMPARE(YAML::Load(bytes.toStdString())["rules"].size(), size_t(3));
}
void EngineTests::unsafeConfigurationRejected_data() {
    QTest::addColumn<QByteArray>("source");
    QTest::newRow("listener") << QByteArray("listeners: [{name: evil, type: http, port: 1337}]\n");
    QTest::newRow("dns-listener") << QByteArray("dns: {listen: 0.0.0.0:53}\n");
    QTest::newRow("file-provider") << QByteArray("proxy-providers: {x: {type: file, path: ../../secret}}\n");
    QTest::newRow("merge") << QByteArray("x: &x {mode: direct}\n<<: *x\n");
    QTest::newRow("duplicate") << QByteArray("mode: rule\nmode: direct\n");
    QTest::newRow("unknown") << QByteArray("unknown-capability: true\n");
    QTest::newRow("html") << QByteArray("<html>bad subscription</html>");
    QTest::newRow("multiple-documents") << QByteArray("mode: rule\n---\nmode: direct\n");
    QTest::newRow("recursive-alias") << QByteArray("dns: &a {x: *a}\n");
}
void EngineTests::unsafeConfigurationRejected() {
    QFETCH(QByteArray, source); const auto built = ConfigBuilder::build(source, Settings{}, 19090, QString(64, 'a')); QVERIFY(!built.result.ok); QVERIFY(built.yaml.isEmpty());
}
void EngineTests::sourceSnapshotAndEncryptedIndex() {
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(directory.path(), platform);
    Profile profile; profile.id = newId(); profile.name = "../显示名"; profile.sourceType = "url"; profile.source = "https://example.test/sub?token=private";
    const auto prepared = store.prepare(profile, minimalYaml(), Settings{}, 19090, QString(64, 'a')); QVERIFY(prepared.result.ok);
    QVERIFY(store.save(Settings{}, {prepared.profile}).ok);
    const auto loaded = store.load(); QVERIFY2(loaded.result.ok, qUtf8Printable(loaded.result.error)); QCOMPARE(loaded.profiles[0].source, profile.source);
    QFile index(directory.path() + "/state.dat"); QVERIFY(index.open(QIODevice::ReadOnly)); QVERIFY(!index.readAll().contains("token=private"));
    QByteArray source; QVERIFY(readFile(store.versionRoot(profile.id, prepared.version) + "/source.yaml", source).ok); QCOMPARE(source, minimalYaml());
    QVERIFY(store.versionRoot("../bad", prepared.version).isEmpty());
}
void EngineTests::providerSnapshotsAndProtocolBoundary() {
    const QByteArray source = "proxy-providers:\n  remote:\n    type: http\n    url: https://example.test/nodes.yaml\n    path: ../../outside.yaml\n    interval: 600\nproxy-groups:\n  - name: Test\n    type: select\n    use: [remote]\nrules: [\"MATCH,Test\"]\n";
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(directory.path(), platform);
    Profile profile; profile.id = newId(); profile.name = "Provider"; profile.sourceType = "file";
    auto prepared = store.prepare(profile, source, Settings{}, 19090, QString(64, 'a')); QVERIFY2(prepared.result.ok, qUtf8Printable(prepared.result.error));
    const auto runtime = YAML::Load(QString::fromUtf8([&] { QByteArray b; readFile(prepared.configPath, b); return b; }()).toStdString());
    QCOMPARE(runtime["proxy-providers"]["remote"]["type"].as<std::string>(), std::string("file")); QVERIFY(!runtime["proxy-providers"]["remote"]["url"]);
    const QByteArray payload = "proxies: [{name: Sample, type: ss, server: example.test, port: 443, cipher: aes-128-gcm, password: private-value}]\n";
    auto checked = ConfigBuilder::checkProvider(payload, "proxy-providers", "yaml"); QVERIFY(checked.result.ok); QVERIFY(checked.secrets.contains("private-value"));
    const auto downloads = prepared.result.data.value("providers").toArray(); QCOMPARE(downloads.size(), 1);
    const auto provider = downloads.at(0).toObject(); QVERIFY(writeAtomic(prepared.workPath + "/" + provider.value("path").toString(), payload).ok);
    const auto reactivated = store.prepareExisting(prepared.profile, prepared.version, Settings{}, 19091, QString(64, 'b')); QVERIFY2(reactivated.result.ok, qUtf8Printable(reactivated.result.error)); QVERIFY(reactivated.result.data.value("providers").toArray().isEmpty()); QVERIFY(reactivated.secrets.contains("private-value"));
    QVERIFY(!ConfigBuilder::checkProvider("proxies: [{name: unsafe, type: easytier}]", "proxy-providers", "yaml").result.ok);
}
void EngineTests::providerRefreshIsolationAndMetadata() {
    const QByteArray source = "proxy-providers:\n  remote:\n    type: http\n    url: https://example.invalid/nodes?token=hidden-source-token\nproxy-groups: [{name: Test, type: select, use: [remote]}]\nrule-providers:\n  rules:\n    type: http\n    behavior: domain\n    url: https://example.invalid/rules\nrules: [\"MATCH,Test\"]\n";
    const QByteArray nodes = "proxies: [{name: Sample, type: http, server: example.test, port: 443}]\n", rules = "payload: [example.test]\n";
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(directory.path(), platform); QVERIFY(store.load().result.ok);
    Profile profile; profile.id = newId(); profile.name = "Frozen providers"; profile.sourceType = "file"; profile.download.route = "core";
    auto original = store.prepare(profile, source, Settings{}, 19090, QString(64, 'a')); QVERIFY(original.result.ok); const auto providers = original.result.data.value("providers").toArray(); QCOMPARE(providers.size(), 2);
    QString nodesId, rulesId;
    for (const auto& value : providers) { const auto provider = value.toObject(); const auto payload = provider.value("kind") == "proxy-providers" ? nodes : rules; QVERIFY(writeAtomic(original.workPath + '/' + provider.value("path").toString(), payload).ok); QVERIFY(store.recordProviderSnapshot(original.workPath, provider, payload).ok); if (provider.value("kind") == "proxy-providers") nodesId = provider.value("path").toString(); else rulesId = provider.value("path").toString(); }
    original.profile.active = original.version; QVERIFY(store.save(Settings{}, {original.profile}).ok);
    auto listing = store.inspectProviders(original.profile, original.version, Settings{}); QVERIFY(listing.ok); QCOMPARE(listing.data.value("providers").toArray().size(), 2); QVERIFY(!QJsonDocument(listing.data).toJson().contains("hidden-source-token"));
    for (const auto& item : listing.data.value("providers").toArray()) { const auto row = item.toObject(); QCOMPARE(row.value("count").toInt(), 1); QVERIFY(!row.value("sha256").toString().isEmpty()); QCOMPARE(row.value("status").toString(), QString("已冻结")); }
    auto refresh = store.prepareExisting(original.profile, original.version, Settings{}, 19091, QString(64, 'b'), true, {nodesId}); QVERIFY2(refresh.result.ok, qUtf8Printable(refresh.result.error)); QCOMPARE(refresh.result.data.value("providers").toArray().size(), 1); QCOMPARE(refresh.result.data.value("providers").toArray()[0].toObject().value("path").toString(), nodesId);
    QByteArray copied; QVERIFY(readFile(refresh.workPath + '/' + rulesId, copied).ok); QCOMPARE(copied, rules); QVERIFY(!QFileInfo::exists(refresh.workPath + '/' + nodesId)); QCOMPARE(refresh.profile.active, original.version);
    const auto replacement = nodes + "# updated\n"; QVERIFY(writeAtomic(refresh.workPath + '/' + nodesId, replacement).ok); QVERIFY(store.recordProviderSnapshot(refresh.workPath, providers[0].toObject(), replacement).ok);
    QVERIFY(readFile(original.workPath + '/' + nodesId, copied).ok); QCOMPARE(copied, nodes);
    auto onlyRules = store.prepareExisting(original.profile, original.version, Settings{}, 19092, QString(64, 'c'), true, {"kind:rule-providers"}); QVERIFY(onlyRules.result.ok); QCOMPARE(onlyRules.result.data.value("providers").toArray().size(), 1); QCOMPARE(onlyRules.result.data.value("providers").toArray()[0].toObject().value("path").toString(), rulesId);
    QVERIFY(!store.prepareExisting(original.profile, original.version, Settings{}, 19093, QString(64, 'd'), true, {"../../outside"}).result.ok);
    AppController controller(directory.path(), true, nullptr, platform); QSignalSpy initialized(&controller, &AppController::initialized), finished(&controller, &AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(), 1);
    controller.refreshProviders(profile.id, nodesId); QTRY_COMPARE(finished.size(), 1); QVERIFY(!finished[0][0].toBool()); QCOMPARE(controller.profiles()[0].candidate, original.version); QCOMPARE(controller.profiles()[0].active, original.version); QCOMPARE(platform->writes, 0);
    bool inspected = false; controller.inspectProviders(profile.id, [&](Result response) { QVERIFY(response.ok); bool found = false; for (const auto& row : response.data.value("providers").toArray()) if (row.toObject().value("id").toString() == nodesId) found = !row.toObject().value("error").toString().isEmpty(); QVERIFY(found); inspected = true; }); QTRY_VERIFY(inspected);
    QVERIFY(writeAtomic(original.workPath + '/' + rulesId, "payload: [tampered.test]\n").ok); listing = store.inspectProviders(original.profile, original.version, Settings{}); QVERIFY(listing.ok); bool tampered = false; for (const auto& row : listing.data.value("providers").toArray()) tampered |= row.toObject().value("status") == "哈希不一致"; QVERIFY(tampered);
    QVERIFY(!store.prepareExisting(original.profile, original.version, Settings{}, 19094, QString(64,'e')).result.ok);
}
void EngineTests::tlsAvailable() { QVERIFY2(QSslSocket::supportsSsl(), "Qt TLS backend unavailable"); }
void EngineTests::corruptStateDoesNotReset() {
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(directory.path(), platform);
    QVERIFY(writeAtomic(directory.path() + "/state.dat", "broken").ok); QVERIFY(!store.load().result.ok);
    QByteArray bytes; QVERIFY(readFile(directory.path() + "/state.dat", bytes).ok); QCOMPARE(bytes, QByteArray("broken"));
}
void EngineTests::proxyOwnershipAndPacRestore() {
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); const auto original = platform->current;
    ProxyCoordinator coordinator(directory.path(), platform); QVERIFY(coordinator.acquire(17890).ok); QCOMPARE(coordinator.inspect().data.value("state").toString(), "owned");
    QVERIFY(coordinator.release().ok); QCOMPARE(platform->current, original); QVERIFY(!coordinator.hasJournal());
    platform->current = {{"flags", 5}, {"server", "old"}, {"bypass", "enterprise"}, {"pac", "https://company.test/proxy.pac"}};
    const auto pac = platform->current; QVERIFY(!coordinator.acquire(17890).ok); QCOMPARE(platform->current, pac);
    QVERIFY(coordinator.acquire(17890, true).ok); QVERIFY(coordinator.release().ok); QCOMPARE(platform->current, pac);
}
void EngineTests::externalProxyNeverOverwritten() {
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); ProxyCoordinator coordinator(directory.path(), platform);
    QVERIFY(coordinator.acquire(17890).ok); platform->current["server"] = "127.0.0.1:9999"; const auto external = platform->current; const int writes = platform->writes;
    const auto result = coordinator.release(); QVERIFY(!result.ok); QVERIFY(result.data.value("conflict").toBool()); QCOMPARE(platform->writes, writes); QCOMPARE(platform->current, external); QVERIFY(coordinator.hasJournal());
}
void EngineTests::journalPrecedesOsWrite() {
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); platform->failSeal = true; ProxyCoordinator coordinator(directory.path(), platform);
    QVERIFY(!coordinator.acquire(17890).ok); QCOMPARE(platform->writes, 0);
}
void EngineTests::proxyFailureAndUnknownRead() {
    QTemporaryDir directory; auto platform = std::make_shared<MemoryPlatform>(); ProxyCoordinator coordinator(directory.path(), platform);
    platform->failWrite = true; QVERIFY(!coordinator.acquire(17890).ok); QCOMPARE(platform->current.value("flags").toInt(), 1);
    platform->failWrite = false; QVERIFY(coordinator.acquire(17890).ok); platform->failRead = true; QVERIFY(!coordinator.release().ok); QVERIFY(coordinator.hasJournal());
}
void EngineTests::damagedJournalNeverWritesProxy() {
    QTemporaryDir root; auto platform = std::make_shared<MemoryPlatform>(); ProxyCoordinator coordinator(root.path(),platform);
    const auto original = platform->current, target = platform->proxyTarget(17890);
    const QJsonObject valid{{"schema",1},{"token",newId()},{"time",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"before",original},{"written",target}};
    QList<QJsonObject> invalid; auto journal = valid; journal["token"] = "not-id"; invalid.append(journal); journal = valid; journal["time"] = "invalid"; invalid.append(journal);
    for (const auto& value : QList<QJsonValue>{"3",1.5,16}) { journal = valid; auto snapshot = original; snapshot["flags"] = value; journal["before"] = snapshot; invalid.append(journal); }
    journal = valid; auto snapshot = original; snapshot.remove("bypass"); journal["before"] = snapshot; invalid.append(journal);
    journal = valid; snapshot = original; snapshot["server"] = QString("invalid")+QChar::Null; journal["before"] = snapshot; invalid.append(journal);
    for (const auto& input : invalid) { QByteArray sealed; QVERIFY(platform->seal(QJsonDocument(input).toJson(),sealed).ok); QVERIFY(writeAtomic(root.path()+"/recovery/proxy-journal.dat",sealed).ok); platform->current = target;
        QVERIFY(!coordinator.release().ok); QVERIFY(!coordinator.acquire(17890,true).ok); QCOMPARE(platform->writes,0); QCOMPARE(platform->current,target); QVERIFY(coordinator.hasJournal()); QByteArray retained; QVERIFY(readFile(root.path()+"/recovery/proxy-journal.dat",retained).ok); QCOMPARE(retained,sealed); }
    QVERIFY(QFile::remove(root.path()+"/recovery/proxy-journal.dat")); platform->current = original; platform->current.remove("pac"); QVERIFY(!coordinator.acquire(17890).ok); QCOMPARE(platform->writes,0);
}
void EngineTests::readOnlyDiagnosticsPreserveState() {
    QTemporaryDir root; auto platform = std::make_shared<MemoryPlatform>(); QVERIFY(writeAtomic(root.path()+"/state.dat","damaged-index").ok);
    AppController controller(root.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
    QVERIFY(controller.state().readOnly); const auto error = controller.state().lastError; QVERIFY(!error.isEmpty()); controller.exportDiagnostics(root.path()+"/report.json"); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool());
    QByteArray bytes; QVERIFY(readFile(root.path()+"/report.json",bytes).ok); const auto report = QJsonDocument::fromJson(bytes).object(); QVERIFY(report.value("read_only").toBool()); QCOMPARE(report.value("last_error").toString(),error); QCOMPARE(controller.state().lastError,error);
    controller.updateSettings({}); QCOMPARE(finished.size(),2); QVERIFY(!finished.last()[0].toBool()); QVERIFY(readFile(root.path()+"/state.dat",bytes).ok); QCOMPARE(bytes,QByteArray("damaged-index")); QCOMPARE(platform->writes,0); QCOMPARE(platform->autoWrites,0);
    QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
}
void EngineTests::redaction() {
    const auto line = Diagnostics::redact("https://user:pass@example.test/sub?token=abcdef secret: abcdef password=hunter2 Authorization: Bearer jwt-private");
    QVERIFY(!line.contains("hunter2")); QVERIFY(!line.contains("abcdef")); QVERIFY(!line.contains("user:pass")); QVERIFY(line.contains("[redacted]"));
    QVERIFY(!line.contains("jwt-private"));
}
void EngineTests::api204AndEncodedNames() {
    MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost, 0)); server.handler = [](const QByteArray&) { return HttpResponse{204, {}}; };
    MihomoClient api; api.configure(server.serverPort(), "test-secret"); bool complete = false; Result result;
    api.select("香港/组 ?#%", "DIRECT", [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(result.ok);
    QVERIFY(server.requests[0].startsWith("PUT /proxies/" + MihomoClient::segment("香港/组 ?#%"))); QVERIFY(server.requests[0].contains("Bearer test-secret"));
}
void EngineTests::apiTimeoutInvalidJsonAndCancel() {
    MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost, 0)); server.handler = [](const QByteArray&) { return HttpResponse{200, "not json"}; };
    MihomoClient api; api.configure(server.serverPort(), "secret"); bool complete = false; Result result;
    api.version([&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(!result.ok);
    server.handler = [](const QByteArray&) { return HttpResponse{200, "{}", 500}; }; complete = false;
    api.request("GET", "/version", {}, [&](Result r) { result = r; complete = true; }, 30); QTRY_VERIFY(complete); QVERIFY(!result.ok); QVERIFY(result.data.value("timeout").toBool());
    complete = false; api.version([&](Result r) { result = r; complete = true; }); api.cancelAll(); QTRY_VERIFY(complete); QVERIFY(!result.ok);
}
void EngineTests::logStreamStopsWhenHidden() {
    MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    server.handler = [](const QByteArray&) { return HttpResponse{200, "{\"type\":\"info\",\"payload\":\"stream line\"}\n", 0, {}, true}; };
    MihomoClient api; api.configure(server.serverPort(), "secret"); QSignalSpy lines(&api, &MihomoClient::logReceived);
    api.setLogsVisible(true); QTRY_COMPARE(lines.size(), 1); QCOMPARE(server.activeConnections(), 1);
    api.setLogsVisible(false); QTRY_COMPARE(server.activeConnections(), 0); QCOMPARE(lines[0][0].toString(), "info stream line");
}
void EngineTests::diagnosticApiPreferencesAndRecentMembers() {
    const QString target = "https://example.test/check?a=1&b=2"; QVERIFY(validTestUrl(target));
    for (const auto& invalid : {QString("file:///secret"), QString("https://user:pass@example.test/"), QString("https://example.test/#fragment"), QString(2049,'x')}) QVERIFY(!validTestUrl(invalid));
    Settings settings; settings.testUrl = target; QCOMPARE(Settings::fromJson(settings.toJson()).testUrl,target);
    MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost,0)); server.handler = [](const QByteArray&) { return HttpResponse{200,"{\"delay\":42}"}; };
    MihomoClient api; api.configure(server.serverPort(),"test-secret"); bool done = false; Result result;
    api.delay("香港/节点",[&](Result r) { result = r; done = true; },30,target); QTRY_VERIFY(done); QVERIFY(result.ok); QCOMPARE(result.data.value("delay").toInt(),42);
    const auto request = server.requests.last().split(' ')[1]; const QUrl requestUrl(QString("http://test")+QString::fromLatin1(request)); QCOMPARE(QUrlQuery(requestUrl).queryItemValue("url",QUrl::FullyDecoded),target);
    server.handler = [](const QByteArray&) { return HttpResponse{200,"{\"type\":\"warning\",\"payload\":\"level test\"}\n",0,{},true}; }; QSignalSpy lines(&api,&MihomoClient::logReceived);
    api.setLogsVisible(true,"warning"); QTRY_COMPARE(lines.size(),1); QVERIFY(server.requests.last().contains("level=warning")); const auto count = server.requests.size();
    api.setLogsVisible(true,"warning"); QTest::qWait(100); QCOMPARE(server.requests.size(),count); api.setLogsVisible(true,"debug"); QTRY_COMPARE(lines.size(),2); QVERIFY(server.requests.last().contains("level=debug")); api.setLogsVisible(false); QTRY_COMPARE(server.activeConnections(),0);
    Profile p; p.id = newId(); p.candidate = newId(); p.sourceType = "file"; p.recentSelections = {QJsonObject{{"group","Test"},{"member","REJECT"}}};
    QCOMPARE(Profile::fromJson(p.toJson()).recentSelections,p.recentSelections); QTemporaryDir root; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); QVERIFY(store.save(settings,{p}).ok);
    p.recentSelections.append(QJsonObject{{"group","Test"},{"secret","must-not-be-stored"}}); QVERIFY(!store.save(settings,{p}).ok); QCOMPARE(store.load().profiles[0].recentSelections.size(),1);
    p.recentSelections = {QJsonObject{{"group","Test"},{"member","REJECT"}}}; auto prepared = store.prepare(p,minimalYaml()+"profile: {store-selected: false}\n",settings,freePort(),QString(64,'s')); QVERIFY2(prepared.result.ok,qUtf8Printable(prepared.result.error)); QVERIFY(prepared.profile.recentSelections.isEmpty());
    QCOMPARE(Diagnostics::levelOf("[core-log] warning warn"),QString("warning")); QCOMPARE(Diagnostics::levelOf("[core] level=fatal message"),QString("error"));
}
void EngineTests::largeNodeModelSorting() {
    QJsonObject proxies; QJsonArray members;
    for (int i = 0; i < 500; ++i) { const auto name = QString("香港 %1").arg(i, 3, 10, QChar('0')); members.append(name); proxies[name] = QJsonObject{{"type", "Shadowsocks"}}; }
    proxies["Group"] = QJsonObject{{"all", members}, {"now", "香港 250"}, {"type", "Selector"}};
    ProxyTableModel model; model.setGroup(proxies, "Group"); QCOMPARE(model.rowCount(), 500); QVERIFY(model.data(model.index(250, 0)).toString().startsWith("●"));
    QCOMPARE(model.data(model.index(42, 2)).toString(), "—"); model.setDelay("香港 042", 5, {}); model.setDelay("香港 043", -1, "timeout");
    QSortFilterProxyModel filter; filter.setSourceModel(&model); filter.setSortRole(Qt::UserRole); filter.sort(2); QCOMPARE(model.nameAt(filter.mapToSource(filter.index(0, 0)).row()), "香港 042");
    QCOMPARE(model.data(model.index(43, 2)).toString(), "失败"); filter.setFilterKeyColumn(0); filter.setFilterFixedString("香港 04"); QCOMPARE(filter.rowCount(), 10);
    const QPersistentModelIndex selected(model.index(42, 0)); model.setGroup(proxies, "Group"); QVERIFY(selected.isValid()); QCOMPARE(selected.row(), 42);
    QCOMPARE(model.columnCount(), 4); QCOMPARE(model.data(model.index(42, 3)).toString(), QString("已测")); QCOMPARE(model.data(model.index(43, 3)).toString(), QString("失败"));
    QVERIFY(model.headerData(0, Qt::Horizontal, Qt::TextAlignmentRole).toInt() & Qt::AlignLeft);
    proxies["Child"] = QJsonObject{{"all", QJsonArray{"香港 042"}}, {"now", "香港 042"}, {"type", "Selector"}};
    proxies["Parent"] = QJsonObject{{"all", QJsonArray{"Child", "香港 042"}}, {"now", "Child"}, {"type", "Selector"}};
    model.setGroup(proxies, "Parent"); QCOMPARE(model.data(model.index(0, 1)).toString(), QString("策略组 ›")); QVERIFY(model.data(model.index(0, 3)).toString().contains("子策略组"));
}
void EngineTests::privateAddresses_data() {
    QTest::addColumn<QString>("address"); QTest::addColumn<bool>("allowed");
    QTest::newRow("loopback") << "127.0.0.1" << false; QTest::newRow("private") << "192.168.1.2" << false;
    QTest::newRow("cgnat") << "100.64.0.1" << false; QTest::newRow("link-local") << "169.254.169.254" << false;
    QTest::newRow("ipv6-loopback") << "::1" << false; QTest::newRow("ipv4-mapped") << "::ffff:127.0.0.1" << false;
    QTest::newRow("ipv6-private") << "fd00::1" << false; QTest::newRow("public") << "1.1.1.1" << true;
    QTest::newRow("ipv4-documentation") << "192.0.2.1" << false; QTest::newRow("ipv4-documentation-2") << "198.51.100.1" << false; QTest::newRow("ipv4-documentation-3") << "203.0.113.1" << false;
    QTest::newRow("ipv6-documentation") << "2001:db8::1" << false; QTest::newRow("ipv6-documentation-new") << "3fff::1" << false; QTest::newRow("ipv6-discard") << "100::1" << false;
    QTest::newRow("nat64-private") << "64:ff9b::c0a8:102" << false; QTest::newRow("nat64-public") << "64:ff9b::101:101" << true; QTest::newRow("nat64-local") << "64:ff9b:1::1" << false;
    QTest::newRow("ipv6-public") << "2606:4700:4700::1111" << true; QTest::newRow("ipv6-transition") << "2002:c0a8:102::1" << false;
}
void EngineTests::privateAddresses() { QFETCH(QString, address); QFETCH(bool, allowed); QCOMPARE(SubscriptionDownloader::publicAddress(QHostAddress(address)), allowed); }
void EngineTests::downloadRedirectAndSize() {
    SubscriptionDownloader boundary; bool rejected = false;
    boundary.checkPublicUrl(QUrl("https://example.test/"+QString(4096,'x')),[&](Result r) { rejected = !r.ok; }); QVERIFY(rejected);
    MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    server.handler = [](const QByteArray& request) { return request.startsWith("GET /start ") ? HttpResponse{302, {}, 0, "Location: /final\r\n"} : HttpResponse{200, "mode: rule\n"}; };
    SubscriptionDownloader downloader(nullptr, true); bool complete = false; Result result; QByteArray bytes;
    downloader.download(QUrl(QString("http://127.0.0.1:%1/start").arg(server.serverPort())), [&](Result r, QByteArray b) { result = r; bytes = b; complete = true; });
    QTRY_VERIFY(complete); QVERIFY2(result.ok, qUtf8Printable(result.error)); QCOMPARE(bytes, QByteArray("mode: rule\n")); QCOMPARE(server.requests.size(), 2);
    SubscriptionDownloader strict; complete = false;
    strict.download(QUrl(QString("http://127.0.0.1:%1/start").arg(server.serverPort())), [&](Result r, QByteArray) { result = r; complete = true; });
    QTRY_VERIFY(complete); QVERIFY(!result.ok); QCOMPARE(server.requests.size(), 2);
    server.handler = [](const QByteArray&) { return HttpResponse{200, QByteArray(8 * 1024 * 1024 + 1, 'a')}; }; complete = false;
    downloader.download(QUrl(QString("http://127.0.0.1:%1/large").arg(server.serverPort())), [&](Result r, QByteArray) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(!result.ok);
}
void EngineTests::downloadPolicyRouteAndProtection() {
    DownloadPolicy policy; QVERIFY(policy.valid()); policy.route = "core"; policy.userAgent = "Compatibility-Agent/1"; policy.timeoutSeconds = 5;
    Profile profile; profile.download = policy; QCOMPARE(Profile::fromJson(profile.toJson()).download.toJson(), policy.toJson());
    QVERIFY(Profile::fromJson(QJsonObject{}).download.valid());
    auto invalid = policy; invalid.userAgent = "agent\r\nAuthorization: hidden"; QVERIFY(!invalid.valid()); invalid = policy; invalid.timeoutSeconds = 121; QVERIFY(!invalid.valid()); invalid = policy; invalid.route = "system"; QVERIFY(!invalid.valid());
    MockHttp target, proxy; QVERIFY(target.listen(QHostAddress::LocalHost, 0)); QVERIFY(proxy.listen(QHostAddress::LocalHost, 0));
    target.handler = [](const QByteArray&) { return HttpResponse{200, "direct"}; }; proxy.handler = [](const QByteArray& request) { if (request.startsWith("CONNECT ")) { HttpResponse response; response.tunnel=true; return response; } return HttpResponse{200, "proxied"}; };
    SubscriptionDownloader downloader(nullptr, true); const auto url = QUrl(QString("http://127.0.0.1:%1/profile").arg(target.serverPort()));
    bool complete = false; Result result; QByteArray bytes;
    downloader.download(url, [&](Result response, QByteArray body) { result = response; bytes = body; complete = true; }, policy, proxy.serverPort());
    QTRY_VERIFY(complete); QVERIFY2(result.ok, qUtf8Printable(result.error)); QCOMPARE(bytes, QByteArray("proxied")); QVERIFY(target.requests.isEmpty()); QCOMPARE(proxy.requests.size(), 2); QVERIFY(proxy.requests[0].startsWith("CONNECT 127.0.0.1:"+QByteArray::number(target.serverPort())+" ")); QVERIFY(proxy.requests[1].startsWith("GET /profile ")); QVERIFY(proxy.requests[1].contains("User-Agent: Compatibility-Agent/1")); QCOMPARE(result.data.value("download_route").toString(), QString("core"));
    complete = false; policy.route = "direct";
    downloader.download(url, [&](Result response, QByteArray body) { result = response; bytes = body; complete = true; }, policy);
    QTRY_VERIFY(complete); QVERIFY(result.ok); QCOMPARE(bytes, QByteArray("direct")); QCOMPARE(target.requests.size(), 1);
    SubscriptionDownloader strict; complete = false; policy.route = "core";
    strict.download(url, [&](Result response, QByteArray) { result = response; complete = true; }, policy, proxy.serverPort());
    QTRY_VERIFY(complete); QVERIFY(!result.ok); QCOMPARE(proxy.requests.size(), 2);
    complete = false; const auto missingProxy = freePort(); QVERIFY(missingProxy);
    downloader.download(url, [&](Result response, QByteArray) { result = response; complete = true; }, policy, missingProxy);
    QTRY_VERIFY_WITH_TIMEOUT(complete, 8000); QVERIFY(!result.ok); QCOMPARE(target.requests.size(), 1); QVERIFY(result.error.contains("经本地代理"));
    QTemporaryDir data; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(data.path(), platform); QVERIFY(store.load().result.ok);
    profile.id = newId(); profile.candidate = newId(); profile.name = "Policy"; profile.sourceType = "url"; profile.source = "https://example.invalid/config"; profile.download = policy; QVERIFY(store.save(Settings{}, {profile}).ok); QCOMPARE(store.load().profiles[0].download.toJson(), policy.toJson());
    AppController controller(data.path(), true, nullptr, platform); QSignalSpy initialized(&controller, &AppController::initialized), finished(&controller, &AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(), 1);
    controller.importUrl("Must not fall back", QUrl("https://example.invalid/config"), policy); QTRY_COMPARE(finished.size(), 1); QVERIFY(!finished[0][0].toBool()); QCOMPARE(controller.profiles().size(), 1); QCOMPARE(controller.profiles()[0].candidate, profile.candidate); QCOMPARE(platform->writes, 0);
    controller.updateProfilePolicy(profile.id, false, 12, invalid); QTRY_COMPARE(finished.size(), 2); QVERIFY(!finished[1][0].toBool()); QCOMPARE(controller.profiles()[0].download.toJson(), policy.toJson());
}
void EngineTests::fakeCoreLifecycleAndValidation() {
    QTemporaryDir dir; const int mixed = freePort(), apiPort = freePort(); const auto config = dir.path() + "/config.yaml";
    QVERIFY(writeAtomic(config, runnableYaml(mixed, apiPort)).ok);
    auto platform = createPlatformServices(); MihomoClient api; CoreManager core(&api, platform);
    const auto executable = QCoreApplication::applicationDirPath() + "/fake_core.exe";
    bool complete = false; Result result;
    core.validate(executable, config, dir.path(), [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(result.ok);
    complete = false; core.start(executable, config, dir.path(), mixed, apiPort, QString(64, 'a'), [&](Result r) { result = r; complete = true; });
    QTRY_VERIFY_WITH_TIMEOUT(complete, 15000); QVERIFY2(result.ok, qUtf8Printable(result.error)); QCOMPARE(core.state(), CoreState::Running);
    complete = false; core.stop([&](Result r) { result = r; complete = true; }); QTRY_VERIFY_WITH_TIMEOUT(complete, 5000); QVERIFY(result.ok); QCOMPARE(core.state(), CoreState::Stopped);
    QVERIFY(writeAtomic(config, "invalid-core: true\n").ok); complete = false;
    core.validate(executable, config, dir.path(), [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(!result.ok);
}
void EngineTests::slowValidationTimeoutAndCancel() {
    QTemporaryDir dir; const auto config = dir.path() + "/config.yaml";
    QVERIFY(writeAtomic(config, "fake-slow-validation: true\n").ok);
    MihomoClient api; CoreManager core(&api, std::make_shared<MemoryPlatform>());
    const auto executable = QCoreApplication::applicationDirPath() + "/fake_core.exe";
    bool complete = false; Result result;
    core.validate(executable, config, dir.path(), [&](Result r) { result = r; complete = true; });
    QTRY_VERIFY_WITH_TIMEOUT(complete, 24000); QVERIFY2(result.ok, qUtf8Printable(result.error));
    complete = false;
    core.validate(executable, config, dir.path(), [&](Result r) { result = r; complete = true; }, 100);
    QTRY_VERIFY(complete); QVERIFY(!result.ok); QVERIFY(result.error.contains("超时"));
    complete = false;
    core.validate(executable, config, dir.path(), [&](Result r) { result = r; complete = true; });
    QTimer::singleShot(100, &core, &CoreManager::cancelValidation);
    QTRY_VERIFY(complete); QVERIFY(!result.ok); QVERIFY(result.error.contains("已取消")); QVERIFY(!result.error.contains("超时"));
}
void EngineTests::geoSnapshotReuse() {
    QTemporaryDir dir; ProfileStore store(dir.path(), std::make_shared<MemoryPlatform>());
    Profile profile; profile.id = newId(); profile.sourceType = "file";
    const auto original = store.prepare(profile, minimalYaml(), Settings{}, 19090, QString(64, 'a'));
    QVERIFY(original.result.ok);
    QVERIFY(writeAtomic(original.workPath + "/geoip.metadb", "test-geo-bytes").ok);
    QVERIFY(writeAtomic(original.workPath + "/cache.db", "must-not-copy").ok);
    const auto activated = store.prepareExisting(original.profile, original.version, Settings{}, 19091, QString(64, 'b'));
    QVERIFY2(activated.result.ok, qUtf8Printable(activated.result.error));
    QByteArray bytes; QVERIFY(readFile(activated.workPath + "/geoip.metadb", bytes).ok); QCOMPARE(bytes, QByteArray("test-geo-bytes"));
    QVERIFY(!QFile::exists(activated.workPath + "/cache.db"));
    const auto changed = store.prepare(original.profile, minimalYaml() + "geox-url: {mmdb: https://example.test/different.mmdb}\n", Settings{}, 19090, QString(64, 'c'));
    QVERIFY(changed.result.ok); QVERIFY(!QFile::exists(changed.workPath + "/geoip.metadb"));
    QVERIFY(writeAtomic(dir.path() + "/geodata/GeoSite.dat", "local-default-geo").ok);
    Profile fresh; fresh.id = newId(); fresh.sourceType = "file";
    const auto seeded = store.prepare(fresh, minimalYaml(), Settings{}, 19090, QString(64, 'd'));
    QVERIFY(seeded.result.ok); QVERIFY(QFile::exists(seeded.workPath + "/GeoSite.dat"));
    const auto custom = store.prepare(fresh, minimalYaml() + "geox-url: {geosite: https://example.test/custom.dat}\n", Settings{}, 19090, QString(64, 'e'));
    QVERIFY(custom.result.ok); QVERIFY(!QFile::exists(custom.workPath + "/GeoSite.dat"));
}
void EngineTests::fakeCoreUnexpectedExit() {
    QTemporaryDir dir; const int mixed = freePort(), apiPort = freePort(); const auto config = dir.path() + "/config.yaml"; QVERIFY(writeAtomic(config, runnableYaml(mixed, apiPort, true)).ok);
    auto platform = createPlatformServices(); MihomoClient api; CoreManager core(&api, platform); QSignalSpy crashed(&core, &CoreManager::crashed);
    bool complete = false; core.start(QCoreApplication::applicationDirPath() + "/fake_core.exe", config, dir.path(), mixed, apiPort, QString(64, 'a'), [&](Result) { complete = true; });
    QTRY_VERIFY_WITH_TIMEOUT(complete, 15000); QTRY_COMPARE_WITH_TIMEOUT(crashed.size(), 1, 5000); QCOMPARE(core.state(), CoreState::Failed);
    QVERIFY(crashed.first().first().toString().contains("(17, exit)"));
}
void EngineTests::realCoreApi() {
    const auto executable = QString(MLASH_SOURCE_DIR) + "/cores/mihomo.exe"; if (!QFile::exists(executable)) QSKIP("Real core unavailable");
    QTemporaryDir dir; Settings settings; settings.mixedPort = freePort(); const int apiPort = freePort(); const auto config = dir.path() + "/config.yaml";
    MockHttp latency; QVERIFY(latency.listen(QHostAddress::LocalHost, 0)); latency.handler = [](const QByteArray&) { return HttpResponse{204, {}}; };
    auto source = minimalYaml(); source.replace("rules:", "  - name: 香港/组\n    type: select\n    proxies: [DIRECT, REJECT]\n  - name: Auto\n    type: url-test\n    proxies: [DIRECT, REJECT]\n    url: http://127.0.0.1:" + QByteArray::number(latency.serverPort()) + "/generate_204\n    interval: 3600\n    lazy: true\nrules:");
    auto built = ConfigBuilder::build(source, settings, apiPort, QString(64, 'a')); QVERIFY(built.result.ok); QVERIFY(writeAtomic(config, built.yaml).ok);
    MihomoClient api; auto platform = createPlatformServices(); CoreManager core(&api, platform); bool complete = false; Result result;
    core.validate(executable, config, dir.path(), [&](Result r) { result = r; complete = true; }); QTRY_VERIFY_WITH_TIMEOUT(complete, 25000); QVERIFY2(result.ok, qUtf8Printable(result.error));
    complete = false; core.start(executable, config, dir.path(), settings.mixedPort, apiPort, QString(64, 'a'), [&](Result r) { result = r; complete = true; }); QTRY_VERIFY_WITH_TIMEOUT(complete, 15000); QVERIFY2(result.ok, qUtf8Printable(result.error)); QByteArray lockedCore; QVERIFY(readFile(QString(MLASH_SOURCE_DIR)+"/dependencies.lock.json",lockedCore).ok); QCOMPARE(result.data.value("version").toString(), QJsonDocument::fromJson(lockedCore).object().value("mihomo-release").toObject().value("version").toString());
    complete = false; api.request("PATCH", "/configs", {{"mode", "direct"}}, [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(result.ok);
    complete = false; api.config([&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QCOMPARE(result.data.value("mode").toString().toLower(), "direct"); QCOMPARE(result.data.value("tcp-half-close-timeout").toObject().value("client").toInt(),300); QCOMPARE(result.data.value("tcp-half-close-timeout").toObject().value("server").toInt(),30);
    complete = false; api.select("Test", "REJECT", [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(result.ok);
    complete = false; api.request("GET", "/proxies/Test", {}, [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QCOMPARE(result.data.value("now").toString(), "REJECT");
    complete = false; api.select("香港/组", "REJECT", [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY2(result.ok, qUtf8Printable(result.error));
    complete = false; api.request("GET", "/proxies/" + MihomoClient::segment("香港/组"), {}, [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QCOMPARE(result.data.value("now").toString(), "REJECT");
    complete = false; api.select("Auto", "REJECT", [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(result.ok);
    complete = false; api.request("GET", "/proxies/Auto", {}, [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QCOMPARE(result.data.value("fixed").toString(), "REJECT");
    complete = false; api.request("DELETE", "/proxies/Auto", {}, [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(result.ok);
    complete = false; api.request("GET", "/proxies/Auto", {}, [&](Result r) { result = r; complete = true; }); QTRY_VERIFY(complete); QVERIFY(result.data.value("fixed").toString().isEmpty());
    complete = false; core.stop([&](Result r) { result = r; complete = true; }); QTRY_VERIFY_WITH_TIMEOUT(complete, 5000); QVERIFY(result.ok);
}
void EngineTests::controllerImportActivateRollback() {
    const auto executable = QString(MLASH_SOURCE_DIR) + "/cores/mihomo.exe"; if (!QFile::exists(executable)) QSKIP("Real core unavailable");
    QTemporaryDir directory; auto platform = createPlatformServices(); ProfileStore store(directory.path(), platform); QVERIFY(store.load().result.ok);
    Settings settings; settings.corePath = executable; settings.mixedPort = freePort(); settings.startCore = false; settings.desiredProxy = false;
    QVERIFY(store.save(settings, {}).ok); AppController controller(directory.path(), true); QSignalSpy initialized(&controller, &AppController::initialized); QSignalSpy finished(&controller, &AppController::operationFinished);
    controller.initialize(); QTRY_COMPARE(initialized.size(), 1); QVERIFY(controller.state().lastError.isEmpty());
    const auto file = directory.path() + "/input.yaml"; QVERIFY(writeAtomic(file, minimalYaml()).ok);
    controller.importFile("First", file); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 25000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString())); QCOMPARE(controller.profiles().size(), 1);
    const auto id = controller.profiles()[0].id; QVERIFY(QFile::remove(file));
    controller.activateProfile(id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 25000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString())); QVERIFY(controller.state().ready);
    QTRY_VERIFY(!controller.proxies().isEmpty()); controller.selectMember("Test", "REJECT"); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 3, 10000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.profiles()[0].selections.value("Test").toString(), "REJECT");
    QCOMPARE(controller.profiles()[0].recentSelections.size(),1); QCOMPARE(controller.profiles()[0].recentSelections[0].toObject().value("member").toString(),QString("REJECT"));
    const auto active = controller.profiles()[0].active; const auto candidate = controller.profiles()[0].candidate;
    QVERIFY(writeAtomic(file, "<html>not YAML</html>").ok); controller.importFile({}, file, id);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 4, 25000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(controller.profiles()[0].active, active); QCOMPARE(controller.profiles()[0].candidate, candidate); QVERIFY(controller.state().ready);
    QTcpServer occupied; QVERIFY(occupied.listen(QHostAddress::LocalHost, 0)); auto changed = controller.settings(); changed.mixedPort = occupied.serverPort();
    controller.updateSettings(changed); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 5, 35000); QVERIFY(!finished.last()[0].toBool()); QVERIFY2(controller.state().ready, qUtf8Printable(finished.last()[1].toString())); QCOMPARE(controller.settings().mixedPort, settings.mixedPort); QTRY_COMPARE(controller.proxies().value("Test").toObject().value("now").toString(), "REJECT");
    QVERIFY(writeAtomic(file, minimalYaml()).ok); controller.importFile("Second", file); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 6, 25000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.profiles().size(), 2);
    const auto second = controller.profiles()[1].id; controller.activateProfile(second); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 7, 25000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString())); QCOMPARE(controller.settings().activeProfile, second); QVERIFY(controller.state().ready);
    controller.deleteProfile(second); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 8, 10000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.state().core, CoreState::Stopped); QVERIFY(controller.settings().activeProfile.isEmpty()); QCOMPARE(controller.profiles().size(), 1);
    QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.size(), 1, 5000); QCOMPARE(controller.state().core, CoreState::Stopped);
}
void EngineTests::pendingCandidateSurvivesRuntimeChanges() {
    const auto executable = QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; QVERIFY(QFile::exists(executable)); QTemporaryDir root;
    ProfileStore store(root.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.corePath = executable; settings.startCore = false; settings.mixedPort = freePort(); QVERIFY(store.save(settings,{}).ok);
    AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1); int completed = 0;
    const auto file = root.path()+"/candidate.yaml"; QVERIFY(writeAtomic(file,minimalYaml()).ok); controller.importFile("Pending",file); ++completed; QTRY_COMPARE(finished.size(),completed); const auto id = controller.profiles()[0].id;
    controller.activateProfile(id); ++completed; QTRY_COMPARE_WITH_TIMEOUT(finished.size(),completed,15000); QVERIFY(finished.last()[0].toBool());
    auto sourceB = minimalYaml(); sourceB.replace("mode: rule","mode: global"); QVERIFY(writeAtomic(file,sourceB).ok); controller.importFile({},file,id); ++completed; QTRY_COMPARE(finished.size(),completed); QVERIFY(finished.last()[0].toBool()); const auto candidate = controller.profiles()[0].candidate; QVERIFY(candidate != controller.profiles()[0].active);
    controller.stopCore(); ++completed; QTRY_COMPARE(finished.size(),completed); controller.startCore(); ++completed; QTRY_COMPARE_WITH_TIMEOUT(finished.size(),completed,15000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.state().mode,QString("rule")); QCOMPARE(controller.profiles()[0].candidate,candidate);
    auto changed = controller.settings(); changed.mixedPort = freePort(); controller.updateSettings(changed); ++completed; QTRY_COMPARE_WITH_TIMEOUT(finished.size(),completed,15000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.profiles()[0].candidate,candidate); QCOMPARE(controller.state().mode,QString("rule"));
    controller.activateProfile(id); ++completed; QTRY_COMPARE_WITH_TIMEOUT(finished.size(),completed,15000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.state().mode,QString("global")); QCOMPARE(controller.profiles()[0].candidate,controller.profiles()[0].active);
    QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
}
void EngineTests::malformedAndOversizedStateNeverReplacesGoodIndex() {
    QTemporaryDir root; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok);
    const QJsonObject valid{{"schema",1},{"settings",Settings{}.toJson()},{"profiles",QJsonArray{}}}; QList<QJsonObject> invalid;
    auto object = valid; object["settings"] = "not-object"; invalid.append(object); object = valid; object.remove("profiles"); invalid.append(object); object = valid; object["profiles"] = 1; invalid.append(object);
    for (const auto& pair : QList<QPair<QString,QJsonValue>>{{"mixed_port",1023},{"mixed_port",17890.5},{"auto_start","false"},{"log_level","invalid"},{"startup_page",8},{"startup_page",1.5},{"tray_click","invalid"},{"ipv6_mode",false},{"unified_delay_mode","invalid"}}) { object = valid; auto settings = object.value("settings").toObject(); settings[pair.first] = pair.second; object["settings"] = settings; invalid.append(object); }
    for (const auto& input : invalid) { QByteArray sealed; QVERIFY(platform->seal(QJsonDocument(input).toJson(),sealed).ok); QVERIFY(writeAtomic(root.path()+"/state.dat",sealed).ok); QVERIFY(!store.load().result.ok); QByteArray retained; QVERIFY(readFile(root.path()+"/state.dat",retained).ok); QCOMPARE(retained,sealed); }
    QVERIFY(QFile::remove(root.path()+"/state.dat")); QVERIFY(store.save({},{}).ok); QByteArray before; QVERIFY(readFile(root.path()+"/state.dat",before).ok);
    QVector<Profile> many; for (int i = 0; i < 40; ++i) { Profile p; p.id = newId(); p.candidate = newId(); p.sourceType = "file"; for (int rule = 0; rule < 256; ++rule) p.customRules.append("PROCESS-PATH,"+QString(990,'a')+",DIRECT"); many.append(p); }
    QVERIFY(!store.save({},many).ok); QByteArray after; QVERIFY(readFile(root.path()+"/state.dat",after).ok); QCOMPARE(after,before);
}
void EngineTests::failedExitRecoveryCanRetry() {
    QTemporaryDir root; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore = false;
    Profile p; p.id = newId(); p.candidate = newId(); p.sourceType = "url"; p.source = "https://example.invalid/"; p.autoUpdate = true; p.updated = QDateTime::currentDateTimeUtc().toString(Qt::ISODate); QVERIFY(store.save(settings,{p}).ok);
    AppController controller(root.path(),false,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished), exited(&controller,&AppController::exitReady); controller.initialize(); QTRY_COMPARE(init.size(),1);
    ProxyCoordinator coordinator(root.path(),platform); QVERIFY(coordinator.acquire(17890).ok); platform->failWrite = true; controller.requestExit(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(!controller.state().exiting); QVERIFY(coordinator.hasJournal()); QCOMPARE(exited.size(),0);
    auto* scheduler = controller.findChild<ProfileMaintenance*>(); QVERIFY(scheduler); QCOMPARE(scheduler->takeDue(QDateTime::currentDateTimeUtc().addSecs(13*3600)),p.id);
    platform->failWrite = false; controller.requestExit(); QTRY_COMPARE(exited.size(),1); QVERIFY(!coordinator.hasJournal()); QCOMPARE(platform->current.value("flags").toInt(),1);
}
void EngineTests::connectionSessionRatesClosureAndBounds() {
    ConnectionSession session; const auto id=newId(); auto row=QJsonObject{{"id",id},{"metadata",QJsonObject{{"host","private-test.invalid"}}},{"upload",100},{"download",200}};
    auto snapshot=QJsonObject{{"connections",QJsonArray{row}},{"uploadTotal",100},{"downloadTotal",200}}; QVERIFY(session.update(snapshot,0).ok); QVERIFY(session.active()[0].toObject().value("_download_rate").isNull());
    row["upload"]=300; row["download"]=1000; snapshot["connections"]=QJsonArray{row}; snapshot["uploadTotal"]=300; snapshot["downloadTotal"]=1000; QVERIFY(session.update(snapshot,2000).ok); QCOMPARE(session.active()[0].toObject().value("_download_rate").toDouble(),400.0); QCOMPARE(session.totals().value("_uploadTotal_rate").toDouble(),100.0);
    snapshot["connections"]=QJsonArray{}; QVERIFY(session.update(snapshot,4000).ok); QCOMPARE(session.closed().size(),1); QVERIFY(!session.closed()[0].toObject().value("_active").toBool());
    snapshot["connections"]=QJsonArray{row}; snapshot["downloadTotal"]=1; QVERIFY(session.update(snapshot,6000).ok); QVERIFY(session.closed().isEmpty()); QVERIFY(session.totals().value("_downloadTotal_rate").isNull());
    QVERIFY(session.update(snapshot,26000).ok); QVERIFY(session.active()[0].toObject().value("_download_rate").isNull());
    auto malformed=snapshot; malformed["connections"]=QJsonArray{QJsonObject{}}; QVERIFY(!session.update(malformed,28000).ok); QCOMPARE(session.active().size(),1);
    session.invalidate(); QVERIFY(session.active().isEmpty()); QVERIFY(session.totals().isEmpty());
    QJsonArray many; for (int i=0;i<1100;++i) { row["id"]=newId(); many.append(row); } snapshot["connections"]=many; QVERIFY(session.update(snapshot,30000).ok); snapshot["connections"]=QJsonArray{}; QVERIFY(session.update(snapshot,32000).ok); QCOMPARE(session.closed().size(),1000);
    session.reset(); QVERIFY(session.closed().isEmpty()); QVERIFY(session.active().isEmpty());
}
void EngineTests::coreOptionsAndGeoSnapshots() {
    QTemporaryDir root,source; auto platform=createPlatformServices(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok);
    Settings settings; settings.startCore=false; settings.mixedPort=freePort(); settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; QVERIFY(store.save(settings,{}).ok);
    AppController controller(root.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
    const auto yaml=minimalYaml()+"ipv6: false\nunified-delay: false\n"; QVERIFY(writeAtomic(source.path()+"/profile.yaml",yaml).ok); controller.importFile("fixture",source.path()+"/profile.yaml"); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(finished.last()[0].toBool()); const auto id=controller.profiles()[0].id;
    controller.activateProfile(id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished.last()[0].toBool()); const auto original=controller.profiles()[0].active;
    auto desired=controller.settings(); desired.startupPage=2; desired.trayClick="proxy-page"; desired.ipv6Mode="enabled"; desired.unifiedDelayMode="enabled"; controller.updateSettings(desired); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
    QCOMPARE(store.load().settings.startupPage,2); QCOMPARE(store.load().settings.trayClick,QString("proxy-page")); QCOMPARE(store.load().settings.ipv6Mode,QString("enabled")); QByteArray originalYaml; QVERIFY(readFile(store.versionRoot(id,controller.profiles()[0].active)+"/source.yaml",originalYaml).ok); QCOMPARE(originalYaml,yaml);
    QByteArray runtime; QVERIFY(readFile(store.versionRoot(id,controller.profiles()[0].active)+"/work/config.yaml",runtime).ok); const auto secret=QString::fromStdString(YAML::Load(runtime.toStdString())["secret"].as<std::string>()); bool previewed=false; QString preview; controller.previewActiveConfig([&](Result result,QString text) { QVERIFY(result.ok); preview=text; previewed=true; }); QTRY_VERIFY(previewed); QVERIFY(preview.contains("[redacted]")); QVERIFY(!preview.contains(secret));
    QTcpServer blocker; const int occupied=freePort(); QVERIFY(blocker.listen(QHostAddress::LocalHost,occupied)); auto invalid=controller.settings(); invalid.mixedPort=occupied; invalid.ipv6Mode="disabled"; controller.updateSettings(invalid); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,15000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(controller.settings().ipv6Mode,QString("enabled")); QVERIFY(controller.state().ready);
    QVERIFY(writeAtomic(source.path()+"/country.mmdb","old-local-bytes").ok); QVERIFY(store.importGeoData(source.path()).ok); QByteArray bytes; QVERIFY(readFile(root.path()+"/geodata/country.mmdb",bytes).ok); QCOMPARE(bytes,QByteArray("old-local-bytes"));
    QVERIFY(writeAtomic(source.path()+"/country.mmdb","new-local-bytes").ok); QVERIFY(store.importGeoData(source.path()).ok); QVERIFY(readFile(root.path()+"/geodata.previous/country.mmdb",bytes).ok); QCOMPARE(bytes,QByteArray("old-local-bytes"));
    QVERIFY(writeAtomic(source.path()+"/country.mmdb",{}).ok); QVERIFY(!store.importGeoData(source.path()).ok); QVERIFY(readFile(root.path()+"/geodata/country.mmdb",bytes).ok); QCOMPARE(bytes,QByteArray("new-local-bytes"));
    QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
}
void EngineTests::autoStartObservationSurvivesIndexFailure() {
    QTemporaryDir root; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore = false; QVERIFY(store.save(settings,{}).ok);
    AppController controller(root.path(),true,nullptr,platform); QSignalSpy init(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1); QVERIFY(controller.state().autoStartKnown); QVERIFY(!controller.state().autoStartEnabled);
    platform->failSeal = true; auto desired = controller.settings(); desired.autoStart = true; controller.updateSettings(desired); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(platform->autoEnabled); QVERIFY(controller.state().autoStartEnabled); QVERIFY(controller.settings().autoStart); QCOMPARE(store.load().settings.autoStart,false);
    platform->failSeal = false; desired = controller.settings(); desired.autoStart = false; controller.updateSettings(desired); QTRY_COMPARE(finished.size(),2); QVERIFY(finished.last()[0].toBool()); QVERIFY(!platform->autoEnabled); QVERIFY(!controller.state().autoStartEnabled); QCOMPARE(platform->autoWrites,2);
    QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
}
void EngineTests::recentMembersBoundedAndRestored() {
    const auto executable = QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; QVERIFY(QFile::exists(executable)); QTemporaryDir root;
    ProfileStore store(root.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.corePath = executable; settings.startCore = false; settings.mixedPort = freePort(); QVERIFY(store.save(settings,{}).ok);
    QByteArray yaml = "proxies: []\nproxy-groups:\n";
    for (int i = 0; i < 40; ++i) yaml += "  - {name: G"+QByteArray::number(i).rightJustified(2,'0')+", type: select, proxies: [DIRECT, REJECT]}\n";
    yaml += "rules: [\"MATCH,G00\"]\n"; const auto file = root.path()+"/many-groups.yaml"; QVERIFY(writeAtomic(file,yaml).ok);
    AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1); int completed = 0;
    controller.importFile("Many groups",file); ++completed; QTRY_COMPARE(finished.size(),completed); QVERIFY(finished.last()[0].toBool()); const auto id = controller.profiles()[0].id;
    controller.activateProfile(id); ++completed; QTRY_COMPARE_WITH_TIMEOUT(finished.size(),completed,15000); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(controller.proxies().contains("G39"));
    for (int i = 0; i < 40; ++i) { controller.selectMember(QString("G%1").arg(i,2,10,QChar('0')),"DIRECT"); ++completed; QTRY_COMPARE(finished.size(),completed); QVERIFY(finished.last()[0].toBool()); }
    QCOMPARE(controller.profiles()[0].recentSelections.size(),32); QCOMPARE(controller.profiles()[0].recentSelections.first().toObject().value("group").toString(),QString("G39")); QCOMPARE(controller.profiles()[0].recentSelections.last().toObject().value("group").toString(),QString("G08"));
    controller.selectMember("G20","DIRECT"); ++completed; QTRY_COMPARE(finished.size(),completed); QCOMPARE(controller.profiles()[0].recentSelections.size(),32); QCOMPARE(controller.profiles()[0].recentSelections.first().toObject().value("group").toString(),QString("G20"));
    const auto recent = controller.profiles()[0].recentSelections; controller.stopCore(); ++completed; QTRY_COMPARE(finished.size(),completed); controller.startCore(); ++completed; QTRY_COMPARE_WITH_TIMEOUT(finished.size(),completed,15000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.profiles()[0].recentSelections,recent); QCOMPARE(store.load().profiles[0].recentSelections,recent);
    QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
}
void EngineTests::subscriptionInfoAndPolicyCompatibility() {
    const auto info = SubscriptionInfo::parse("upload=1024; download=2048; total=4096; expire=2000000000");
    QVERIFY(SubscriptionInfo::valid(info)); QCOMPARE(info.value("total").toString(), "4096"); QVERIFY(SubscriptionInfo::summary(info).contains("剩余 1.0 KiB"));
    for (const auto& input : QList<QByteArray>{"total=-1", "total=9007199254740992", "total=1e12", "total=1;total=2", "expire=253402300800", QByteArray(1025, 'x')}) QVERIFY(SubscriptionInfo::parse(input).isEmpty());
    QVERIFY(SubscriptionInfo::summary(SubscriptionInfo::parse("expire=0")).isEmpty());
    QVERIFY(!SubscriptionInfo::valid({{"total", -1}}));
    Profile profile; profile.id = newId(); profile.candidate = newId(); profile.sourceType = "url"; profile.source = "https://example.com/subscription";
    auto old = profile.toJson(); for (const auto& field : {"auto_update", "update_interval_hours", "source_hash", "last_update_attempt", "update_error", "subscription_info"}) old.remove(field);
    const auto compatible = Profile::fromJson(old); QVERIFY(!compatible.autoUpdate); QCOMPARE(compatible.updateIntervalHours, 12);
    QTemporaryDir root; auto platform = std::make_shared<MemoryPlatform>(); ProfileStore store(root.path(), platform); QVERIFY(store.load().result.ok);
    profile.autoUpdate = true; profile.updateIntervalHours = 3; profile.subscriptionInfo = info; QVERIFY(store.save({}, {profile}).ok);
    const auto loaded = store.load(); QVERIFY(loaded.result.ok); QCOMPARE(loaded.profiles[0].subscriptionInfo, info); QVERIFY(loaded.profiles[0].autoUpdate);
    profile.updateIntervalHours = 0; QVERIFY(!store.save({}, {profile}).ok); QCOMPARE(store.load().profiles[0].updateIntervalHours, 3);
    QByteArray sealed; QVERIFY(platform->seal(QJsonDocument(QJsonObject{{"schema", 1}, {"settings", Settings{}.toJson()}, {"profiles", QJsonArray{QJsonObject{{"id",profile.id},{"candidate",profile.candidate},{"source_type","url"},{"auto_update","true"}}}}}).toJson(), sealed).ok);
    QVERIFY(writeAtomic(root.path()+"/state.dat", sealed).ok); QVERIFY(!store.load().result.ok);
    Diagnostics diagnostics(root.path()); diagnostics.setSecrets({"private-example-credential"}); QVERIFY(!diagnostics.sanitize("failure private-example-credential").contains("private-example-credential"));
    MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost, 0)); server.handler = [](const QByteArray&) { return HttpResponse{200,"mode: rule\n",0,"subscription-userinfo: upload=1; download=2; total=3; expire=4\r\n"}; };
    SubscriptionDownloader downloader(nullptr, true); bool done = false; Result result;
    downloader.download(QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())), [&](Result r, QByteArray) { result = r; done = true; });
    QTRY_VERIFY(done); QVERIFY(result.ok); QCOMPARE(result.data.value("subscription_info").toObject().value("expire").toString(), "4");
}
void EngineTests::maintenanceDeadlinesAndRetry() {
    const auto now = QDateTime::fromString("2026-10-13T12:00:00Z", Qt::ISODate);
    Profile first; first.id = "first"; first.sourceType = "url"; first.autoUpdate = true; first.updateIntervalHours = 1; first.updated = now.addSecs(-3600).toString(Qt::ISODate);
    auto second = first; second.id = "second";
    ProfileMaintenance maintenance; maintenance.setProfiles({first, second}); QVERIFY(maintenance.takeDue(now).isEmpty()); maintenance.setEnabled(true);
    QCOMPARE(maintenance.takeDue(now), first.id); maintenance.completed(first.id, false, now);
    QCOMPARE(maintenance.takeDue(now), second.id); maintenance.completed(second.id, false, now);
    QVERIFY(maintenance.takeDue(now.addSecs(299)).isEmpty()); QCOMPARE(maintenance.takeDue(now.addSecs(300)), first.id);
    maintenance.completed(first.id, false, now.addSecs(300)); QCOMPARE(maintenance.takeDue(now.addSecs(300)), second.id);
    maintenance.completed(second.id, true, now.addSecs(300)); second.updated = now.addSecs(300).toString(Qt::ISODate); maintenance.setProfiles({first, second});
    QVERIFY(maintenance.takeDue(now.addSecs(899)).isEmpty()); QCOMPARE(maintenance.takeDue(now.addSecs(900)), first.id);
    first.autoUpdate = false; second.autoUpdate = false; maintenance.setProfiles({first, second}); QVERIFY(maintenance.takeDue(now.addDays(30)).isEmpty());
    maintenance.setEnabled(false); QVERIFY(maintenance.takeDue(now.addDays(30)).isEmpty());
}
void EngineTests::fixtureRewriteWaitsForShortReadHandle() {
    QTemporaryDir root; const auto path=root.path()+"/watched.yaml";
    QVERIFY(writeAtomic(path,"old input").ok);
    QFile reader(path); QVERIFY(reader.open(QIODevice::ReadOnly));
#ifdef Q_OS_WIN
    const auto blocked=writeAtomic(path,"blocked input"); QVERIFY(!blocked.ok);
    QByteArray before; QVERIFY(readFile(path,before).ok); QCOMPARE(before,QByteArray("old input"));
#endif
    QTimer::singleShot(50,&reader,[&reader] { reader.close(); });
    const auto result=rewriteFixture(path,"next input"); QVERIFY2(result.ok,qUtf8Printable(result.error));
    QByteArray after; QVERIFY(readFile(path,after).ok); QCOMPARE(after,QByteArray("next input"));
}
void EngineTests::localMaintenancePreservesActive() {
    const auto executable = QString(MLASH_SOURCE_DIR) + "/cores/mihomo.exe"; QVERIFY(QFile::exists(executable));
    QTemporaryDir root; ProfileStore store(root.path(), createPlatformServices()); QVERIFY(store.load().result.ok);
    Settings settings; settings.corePath = executable; settings.mixedPort = freePort(); settings.startCore = false; QVERIFY(store.save(settings, {}).ok);
    AppController controller(root.path()); QSignalSpy initialized(&controller, &AppController::initialized), finished(&controller, &AppController::operationFinished);
    controller.initialize(); QTRY_COMPARE(initialized.size(), 1);
    const auto file = root.path()+"/watched.yaml"; { const auto result=rewriteFixture(file,minimalYaml()); QVERIFY2(result.ok,qUtf8Printable(result.error)); } controller.importFile("Watched", file);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 15000); QVERIFY(finished.last()[0].toBool()); const auto id = controller.profiles()[0].id;
    controller.activateProfile(id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 15000); QVERIFY(finished.last()[0].toBool());
    const auto active = controller.profiles()[0].active; const auto candidate = controller.profiles()[0].candidate;
    controller.updateProfilePolicy(id, true, 12); QTRY_COMPARE(finished.size(), 3); QVERIFY(finished.last()[0].toBool());
    QTRY_VERIFY(!controller.state().busy); { const auto result=rewriteFixture(file,minimalYaml()+"# watched update\n"); QVERIFY2(result.ok,qUtf8Printable(result.error)); }
    QTRY_VERIFY_WITH_TIMEOUT(controller.profiles()[0].candidate != candidate, 15000); QCOMPARE(controller.profiles()[0].active, active); QVERIFY(controller.state().ready);
    const auto newCandidate = controller.profiles()[0].candidate;
    { const auto result=rewriteFixture(file,"<html>invalid source</html>"); QVERIFY2(result.ok,qUtf8Printable(result.error)); } QTRY_VERIFY_WITH_TIMEOUT(!controller.profiles()[0].updateError.isEmpty(), 10000);
    QCOMPARE(controller.profiles()[0].candidate, newCandidate); QCOMPARE(controller.profiles()[0].active, active); QVERIFY(controller.state().ready);
    QTRY_VERIFY(!controller.state().busy);
    { const auto result=rewriteFixture(file,minimalYaml()+"# retry restore\n"); QVERIFY2(result.ok,qUtf8Printable(result.error)); }
    controller.updateProfilePolicy(id, false, 12); QTRY_COMPARE(finished.size(), 4); QVERIFY(finished.last()[0].toBool()); QTest::qWait(1200); QCOMPARE(controller.profiles()[0].candidate, newCandidate);
    controller.updateProfile(id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),5,15000); QVERIFY(finished.last()[0].toBool()); const auto manualCandidate=controller.profiles()[0].candidate; QVERIFY(manualCandidate!=newCandidate); QCOMPARE(controller.profiles()[0].active,active); QVERIFY(controller.state().ready);
    controller.updateProfile(id); QTRY_COMPARE(finished.size(),6); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.profiles()[0].candidate,manualCandidate);
    QVERIFY(QFile::remove(file)); controller.updateProfile(id); QTRY_COMPARE(finished.size(),7); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(controller.profiles()[0].candidate,manualCandidate); QCOMPARE(controller.profiles()[0].active,active); QVERIFY(controller.state().ready);
    QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.size(), 1, 5000);
}
QTEST_GUILESS_MAIN(EngineTests)
#include "EngineTests.moc"
