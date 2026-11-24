// Async.h 的线程与生命周期契约守护（原则 13 触发条件：跨线程状态）。
// 目标只链接 Qt Core/Test，不链接 mlash_engine 或 UI（原则 5）。
#include "app/Async.h"
#include <QThreadPool>
#include <QtTest>
#include <atomic>

using namespace mlash;

class AsyncTaskTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // 固定线程池上限，避免测试并发影响断言（生产入口同样设置 2）。
        QThreadPool::globalInstance()->setMaxThreadCount(2);
    }

    void workRunsOffTheMainThread() {
        QObject owner;
        QThread* workThread = nullptr;
        bool done = false;
        runTask(&owner,
                [&workThread] {
                    workThread = QThread::currentThread();
                    return 0;
                },
                [&done](int) { done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(workThread != nullptr);
        QVERIFY(workThread != qApp->thread());
    }

    void callbackRunsOnTheMainThread() {
        QObject owner;
        QThread* callbackThread = nullptr;
        runTask(&owner, [] { return 0; }, [&callbackThread](int) { callbackThread = QThread::currentThread(); });
        QTRY_VERIFY_WITH_TIMEOUT(callbackThread != nullptr, 5000);
        QCOMPARE(callbackThread, qApp->thread());
    }

    void resultValueIsForwarded() {
        QObject owner;
        int received = -1;
        runTask(&owner, [] { return 42; }, [&received](int value) { received = value; });
        QTRY_COMPARE_WITH_TIMEOUT(received, 42, 5000);
    }

    void callbackIsDroppedWhenReceiverIsDestroyed() {
        auto* owner = new QObject;
        std::atomic<bool> workDone{false};
        bool called = false;
        runTask(owner, [&workDone] { workDone = true; return 1; }, [&called](int) { called = true; });
        // 先确认 work 真的跑完，再销毁接收者：否则用例可能在事件投递前结束，什么都没验证到。
        QTRY_VERIFY_WITH_TIMEOUT(workDone.load(), 5000);
        delete owner;
        QCoreApplication::processEvents();
        QTest::qWait(50);
        QVERIFY(!called);
    }

    void liveReceiverProvesEventsAreFlushed() {
        // 反向对照：同一个事件循环里，未销毁的接收者必须收到回调。
        // 没有这条，"回调未到达"就分不清是"被丢弃"还是"事件根本没投递"。
        QObject owner;
        std::atomic<bool> workDone{false};
        bool called = false;
        runTask(&owner, [&workDone] { workDone = true; return 1; }, [&called](int) { called = true; });
        QTRY_VERIFY_WITH_TIMEOUT(workDone.load(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(called, 5000);
    }

    void callbackSurvivesNothingWhenResultIsUnused() {
        // 空 work 也不能阻断队列：后续任务必须照常完成。
        QObject owner;
        int completed = 0;
        for (int index = 0; index < 4; ++index)
            runTask(&owner, [] { return 0; }, [&completed](int) { ++completed; });
        QTRY_COMPARE_WITH_TIMEOUT(completed, 4, 5000);
    }
};

QTEST_GUILESS_MAIN(AsyncTaskTests)
#include "AsyncTaskTests.moc"