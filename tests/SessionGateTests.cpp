// SessionGate 的语义守护：区分「拒绝用户动作」「静默跳过」「派生观测」三类，
// 避免把 13 处判断统一成一个模糊的 isBlocked()。
#include "app/SessionGate.h"
#include <QtTest>

using namespace mlash;

class SessionGateTests final : public QObject {
    Q_OBJECT
private slots:
    void idleGateAllowsEveryIntent() {
        const SessionGate gate{};
        for (const auto intent : {SessionIntent::ProxyTakeover, SessionIntent::TunEnable,
                                  SessionIntent::TunServiceMaintenance, SessionIntent::ProfileDelete,
                                  SessionIntent::TunnelConnect}) {
            QVERIFY(!gate.blocks(intent));
            QVERIFY(gate.reason(intent).isEmpty());
        }
        QVERIFY(!gate.blocks(SessionIntent::ProfileActivation, true));
        QVERIFY(!gate.skipsProxyRestore());
    }

    void leaseBlocksProxyTakeover() {
        const SessionGate gate{.leaseHeld = true};
        QVERIFY(gate.blocks(SessionIntent::ProxyTakeover));
        QVERIFY(gate.reason(SessionIntent::ProxyTakeover).contains("本地代理"));
    }

    void leaseBlocksTunEnable() {
        const SessionGate gate{.leaseHeld = true};
        QVERIFY(gate.blocks(SessionIntent::TunEnable));
        QVERIFY(gate.reason(SessionIntent::TunEnable).contains("系统路由"));
    }

    void leaseBlocksServiceMaintenance() {
        const SessionGate gate{.leaseHeld = true};
        QVERIFY(gate.blocks(SessionIntent::TunServiceMaintenance));
        QVERIFY(!gate.reason(SessionIntent::TunServiceMaintenance).isEmpty());
    }

    void leaseBlocksProfileDelete() {
        const SessionGate gate{.leaseHeld = true};
        QVERIFY(gate.blocks(SessionIntent::ProfileDelete));
        QVERIFY(gate.reason(SessionIntent::ProfileDelete).contains("删除"));
    }

    void activationBlocksOnlyWhenTunRequested() {
        const SessionGate gate{.leaseHeld = true};
        QVERIFY(!gate.blocks(SessionIntent::ProfileActivation, false));
        QVERIFY(gate.blocks(SessionIntent::ProfileActivation, true));
    }

    void leaseBlocksTunnelConnect() {
        const SessionGate gate{.leaseHeld = true};
        QVERIFY(gate.blocks(SessionIntent::TunnelConnect));
        QVERIFY(gate.reason(SessionIntent::TunnelConnect).contains("清理"));
    }

    void cleanupIsPendingOnlyForAnIdleLease() {
        SessionGate gate{.leaseHeld = true};
        QVERIFY(gate.cleanupPending());
        gate.tunnelActive = true;
        QVERIFY(!gate.cleanupPending());
        gate.tunnelActive = false;
        gate.connecting = true;
        QVERIFY(!gate.cleanupPending());
        gate.connecting = false;
        gate.leaseHeld = false;
        QVERIFY(!gate.cleanupPending());
    }

    void proxyRestoreIsSkippedNotRejected() {
        const SessionGate gate{.leaseHeld = true};
        QVERIFY(gate.skipsProxyRestore());
        // 跳过是静默行为，不产生用户可见错误；与「拒绝接管」是两个语义。
        QVERIFY(gate.reason(SessionIntent::ProxyTakeover) != gate.reason(SessionIntent::TunEnable));
    }

    void reasonsAreStableAndDistinctPerGroup() {
        const SessionGate gate{.leaseHeld = true};
        const QSet<QString> reasons{
            gate.reason(SessionIntent::TunEnable),
            gate.reason(SessionIntent::TunServiceMaintenance),
            gate.reason(SessionIntent::ProfileDelete),
            gate.reason(SessionIntent::TunnelConnect),
        };
        QCOMPARE(reasons.size(), 4);
        for (const auto& reason : reasons) QVERIFY(!reason.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SessionGateTests)
#include "SessionGateTests.moc"