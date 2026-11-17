#include "core/CoreDtos.h"
#include <QtTest>

class CoreDtoTests final : public QObject {
    Q_OBJECT
private slots:
    void versionRequiresAString() {
        using namespace mlash;
        QVERIFY(!parseMihomoVersion({true, {}, {}}).ok());
        QVERIFY(!parseMihomoVersion({true, {}, {{"version", 123}}}).ok());
        const auto observed = parseMihomoVersion({true, {}, {{"version", "v1.19.32"}}});
        QVERIFY(observed.ok());
        QCOMPARE(observed.value.version, QString("v1.19.32"));
    }
    void configRejectsMissingAndMalformedObservations() {
        using namespace mlash;
        QVERIFY(!parseMihomoConfig({true, {}, {{"mode", "rule"}}}).ok());
        QVERIFY(!parseMihomoConfig({true, {}, {{"mixed-port", 17890.5}, {"mode", "rule"}}}).ok());
        QVERIFY(!parseMihomoConfig({true, {}, {{"mixed-port", 17890}, {"mode", "rule"}, {"tun", QJsonObject{{"enable", "yes"}}}}}).ok());
        const auto observed = parseMihomoConfig({true, {}, {{"mixed-port", 17890}, {"mode", "Rule"}, {"tun", QJsonObject{{"enable", true}}}}});
        QVERIFY(observed.ok());
        QCOMPARE(observed.value.mixedPort, 17890);
        QCOMPARE(observed.value.mode, QString("rule"));
        QVERIFY(observed.value.tunEnabled);
    }
    void proxyGroupsRejectMissingButPreserveTransportError() {
        using namespace mlash;
        QVERIFY(!parseMihomoProxies({true, {}, {}}).ok());
        const auto failed = parseMihomoProxies(Result::fail("read failed"));
        QVERIFY(!failed.ok());
        QCOMPARE(failed.result.error, QString("read failed"));
        const auto observed = parseMihomoProxies({true, {}, {{"proxies", QJsonObject{{"GLOBAL", QJsonObject{{"now", "DIRECT"}}}}}}});
        QVERIFY(observed.ok());
        QCOMPARE(observed.value.groups.value("GLOBAL").toObject().value("now").toString(), QString("DIRECT"));
    }
};

QTEST_GUILESS_MAIN(CoreDtoTests)
#include "CoreDtoTests.moc"
