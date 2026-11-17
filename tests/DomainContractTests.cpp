#include "domain/TunOptions.h"
#include "domain/WebsiteTargets.h"
#include <QJsonArray>
#include <QtTest>

using namespace mlash;

class DomainContractTests final : public QObject {
    Q_OBJECT
private slots:
    void tunRoutesRejectDefaultAndOversizedInputs() {
        QVERIFY(!normalizeTunRoutes("0.0.0.0/0").ok);
        QVERIFY(!normalizeTunRoutes("192.168.1.0/33").ok);
        QVERIFY(!normalizeTunRoutes(QString(4097, 'a')).ok);
        QStringList tooMany;
        for (int i=1;i<=65;++i) tooMany << QString("10.0.%1.0/24").arg(i);
        QVERIFY(!normalizeTunRoutes(tooMany.join(',')).ok);
    }

    void tunRoutesNormalizeBeforePersistence() {
        const auto result=normalizeTunRoutes("192.168.1.25/24,192.168.1.0/24;2001:db8::1/64");
        QVERIFY(result.ok);
        const auto routes=result.data.value("routes").toArray();
        QCOMPARE(routes.size(),2);
        QCOMPARE(routes[0].toString(),QString("192.168.1.0/24"));
        QCOMPARE(routes[1].toString(),QString("2001:db8::/64"));
        Settings settings;
        settings.tunExcludeRoutes={"192.168.1.0/24","2001:db8::/64"};
        QVERIFY(validTunOptions(settings));
        settings.tunExcludeRoutes={"192.168.1.25/24"};
        QVERIFY(!validTunOptions(settings));
    }

    void websiteTargetsRejectUnsafeOrAmbiguousAddresses() {
        const QJsonObject valid{{"id","example"},{"name","Example"},{"url","https://example.com/generate_204"}};
        QVERIFY(validateWebsiteTargets(QJsonArray{valid}).ok);
        auto duplicate=QJsonArray{valid,valid};
        QVERIFY(!validateWebsiteTargets(duplicate).ok);
        for (const auto& address:{"http://example.com", "https://user:secret@example.com", "https://example.com/#fragment"}) {
            auto item=valid; item.insert("url",address);
            QVERIFY(!validateWebsiteTargets(QJsonArray{item}).ok);
        }
    }
};

QTEST_GUILESS_MAIN(DomainContractTests)
#include "DomainContractTests.moc"
