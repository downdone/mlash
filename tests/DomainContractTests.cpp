#include "domain/ConnectionViewPrefs.h"
#include "domain/CleanupState.h"
#include "domain/ErrorCategory.h"
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

    void cleanupStateOnlyAllowsConfirmedOrAbsent() {
        QVERIFY(canStartSession(CleanupState::None));
        QVERIFY(canStartSession(CleanupState::Confirmed));
        QVERIFY(!canStartSession(CleanupState::Pending));
        QVERIFY(!canStartSession(CleanupState::Failed));
    }

    void cleanupStateNamesAreStableAndDistinct() {
        const QList<CleanupState> all{CleanupState::None, CleanupState::Pending,
                                      CleanupState::Confirmed, CleanupState::Failed};
        QSet<QString> names;
        for (const auto state : all) {
            const auto name = cleanupStateName(state);
            QVERIFY(!name.isEmpty());
            QVERIFY(std::all_of(name.cbegin(), name.cend(), [](QChar c) {
                return c.unicode() < 128 && (c.isLetterOrNumber() || c == '_');
            }));
            names.insert(name);
        }
        QCOMPARE(names.size(), all.size());
    }

    void everyConfigBuilderCodeHasCategory() {
        for (const auto& entry : knownErrorCodes()) QVERIFY(entry.category != ErrorCategory::Unspecified);
        QCOMPARE(categorizeError("UNSUPPORTED_FIELD"), ErrorCategory::Rejected);
        QCOMPARE(categorizeError("YAML_SYNTAX"), ErrorCategory::Rejected);
        QCOMPARE(categorizeError("NODE_LINK"), ErrorCategory::Rejected);
        QCOMPARE(categorizeError("HALFCLOSE_VALUE"), ErrorCategory::Rejected);
    }

    void runtimeFailuresDistinguishRetryFromUserAction() {
        QCOMPARE(categorizeError("DOWNLOAD_FAILED"), ErrorCategory::Retryable);
        QCOMPARE(categorizeError("BUSY"), ErrorCategory::Retryable);
        QCOMPARE(categorizeError("PORT_CONFLICT"), ErrorCategory::UserAction);
        QCOMPARE(categorizeError("READ_ONLY"), ErrorCategory::UserAction);
        QCOMPARE(categorizeError("CLEANUP_REQUIRED"), ErrorCategory::UserAction);
        QCOMPARE(categorizeError("VERSION_CONFLICT"), ErrorCategory::Reload);
    }

    void unknownCodeIsUnspecified() {
        QCOMPARE(categorizeError("NOT_A_REAL_CODE"), ErrorCategory::Unspecified);
        QCOMPARE(categorizeError(QString()), ErrorCategory::Unspecified);
    }

    void connectionViewRejectsUnknownFieldsAndBadColumns() {
        QJsonObject unknown{{"case_sensitive", true}, {"surprise", 1}};
        QVERIFY(!ConnectionViewPrefs::fromJson(unknown).valid().ok);

        QJsonObject wrongCount{{"columns", QJsonArray{QJsonObject{{"id", 0}, {"width", 100}, {"visible", true}}}}};
        QVERIFY(!ConnectionViewPrefs::fromJson(wrongCount).valid().ok);

        QJsonArray narrow;
        for (int id = 0; id < 15; ++id)
            narrow.append(QJsonObject{{"id", id}, {"width", id == 0 ? 10 : 100}, {"visible", true}});
        QJsonObject tooNarrow{{"columns", narrow}};
        QVERIFY(!ConnectionViewPrefs::fromJson(tooNarrow).valid().ok);

        QJsonArray hiddenTarget;
        for (int id = 0; id < 15; ++id)
            hiddenTarget.append(QJsonObject{{"id", id}, {"width", 100}, {"visible", id != 0}});
        QJsonObject targetHidden{{"columns", hiddenTarget}};
        QVERIFY(!ConnectionViewPrefs::fromJson(targetHidden).valid().ok);

        QJsonArray duplicate;
        for (int id = 0; id < 15; ++id)
            duplicate.append(QJsonObject{{"id", 0}, {"width", 100}, {"visible", true}});
        QJsonObject repeated{{"columns", duplicate}};
        QVERIFY(!ConnectionViewPrefs::fromJson(repeated).valid().ok);
    }

    void connectionViewRoundTripsCanonicalForm() {
        QJsonArray columns;
        for (int id = 0; id < 15; ++id)
            columns.append(QJsonObject{{"id", id}, {"width", 100 + id}, {"visible", id < 3}});
        const QJsonObject source{{"case_sensitive", true}, {"whole_word", false},
                                 {"regex", true}, {"columns", columns}};
        const auto preferences = ConnectionViewPrefs::fromJson(source);
        QVERIFY(preferences.valid().ok);
        QCOMPARE(preferences.toJson(), source);
    }

    void connectionViewKeepsLegacySevenColumnDocument() {
        QJsonArray columns;
        for (int id = 0; id < ConnectionColumns::LegacyCount; ++id)
            columns.append(QJsonObject{{"id", id}, {"width", 120}, {"visible", true}});
        const QJsonObject source{{"case_sensitive", false}, {"whole_word", false},
                                 {"regex", false}, {"columns", columns}};
        const auto preferences = ConnectionViewPrefs::fromJson(source);
        QVERIFY(preferences.valid().ok);
        QCOMPARE(preferences.columns.size(), ConnectionColumns::LegacyCount);
    }

    void connectionViewWithoutColumnsOmitsTheKeyAndRoundTrips() {
        const QJsonObject source{{"case_sensitive", true}, {"whole_word", true}, {"regex", false}};
        const auto preferences = ConnectionViewPrefs::fromJson(source);
        QVERIFY(preferences.valid().ok);
        QCOMPARE(preferences.toJson(), source);

        // 未自定义列必须写回"没有 columns 键"；空数组与旧校验一致地判为非法。
        const QJsonObject emptyArray{{"case_sensitive", true}, {"whole_word", true},
                                     {"regex", false}, {"columns", QJsonArray{}}};
        QVERIFY(!ConnectionViewPrefs::fromJson(emptyArray).valid().ok);
        QVERIFY(!preferences.toJson().contains("columns"));
    }

    void everyProfileRuleCodeHasCategory() {
        // 与 ConfigBuilder 的 26 个码值互补：RuleDraft 产生的三个码值也必须在表内。
        QCOMPARE(categorizeError("CUSTOM_RULE"), ErrorCategory::Rejected);
        QCOMPARE(categorizeError("RULE_OPTION"), ErrorCategory::Rejected);
        QCOMPARE(categorizeError("RULE_SET_REFERENCE"), ErrorCategory::Rejected);
    }
};

QTEST_GUILESS_MAIN(DomainContractTests)
#include "DomainContractTests.moc"
