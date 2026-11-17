#include "ui/PageLifecycle.h"
#include "ui/ViewIds.h"
#include <QtTest>

class PageRouterTests final : public QObject {
    Q_OBJECT
private slots:
    void enterLeaveAndRestore() {
        using namespace mlash;
        struct Counter final : PageLifecycle {
            int enters = 0, leaves = 0, restores = 0;
            Route last;
            void onEnter(const Route& route) override { ++enters; last = route; }
            void onLeave() override { ++leaves; }
            void onRestore(const PageMemory&) override { ++restores; }
        };
        PageRouter router;
        Counter home, settings;
        router.registerPage(ViewIds::Home, &home);
        router.registerPage(ViewIds::Settings, &settings);
        router.go(Route{ViewIds::Home, kNoCategory, HomeAnchor::Control});
        QCOMPARE(home.enters, 1);
        router.go(Route{ViewIds::Home, kNoCategory, HomeAnchor::Nodes});
        QCOMPARE(home.enters, 2);
        QCOMPARE(home.leaves, 0);
        QCOMPARE(home.last.anchor, HomeAnchor::Nodes);
        router.go(Route{ViewIds::Settings, 1});
        QCOMPARE(home.leaves, 1);
        QCOMPARE(settings.enters, 1);
        QCOMPARE(settings.last.category, 1);
        router.go(Route{ViewIds::Home});
        QCOMPARE(settings.leaves, 1);
        QCOMPARE(home.enters, 3);
        QCOMPARE(router.current(), int(ViewIds::Home));
        PageMemory memory;
        memory.filters["search"] = "abc";
        memory.tab = 2;
        router.restore(ViewIds::Settings, memory);
        QCOMPARE(settings.restores, 1);
    }

    void destroyedPageIsNotCalled() {
        using namespace mlash;
        PageRouter router;
        auto* page = new PageLifecycle;
        router.registerPage(ViewIds::Home, page);
        QVERIFY(router.page(ViewIds::Home) == page);
        delete page;
        QVERIFY(router.page(ViewIds::Home) == nullptr);
        router.go(Route{ViewIds::Home});
        router.go(Route{ViewIds::Settings});
        QCOMPARE(router.current(), int(ViewIds::Settings));
    }
};

QTEST_GUILESS_MAIN(PageRouterTests)
#include "PageRouterTests.moc"
