#pragma once
// CONTRACT owner=app layer=app
// invariant: work 在工作线程执行，done 在主线程以 QueuedConnection 执行；owner 已销毁则丢弃回调
// forbidden: work 内不得访问 QObject 或界面状态；done 内不得做阻塞 I/O
// lifecycle: 接收者由 QPointer 守卫；调用方不持有 QThread 句柄
// tests: async_task_tests
// spec: docs/architecture.md
#include <QCoreApplication>
#include <QPointer>
#include <QThreadPool>
#include <QMetaObject>
#include <utility>

namespace mlash {
template<class Work, class Done>
void runTask(QObject* owner, Work work, Done done) {
    const QPointer<QObject> guarded(owner);
    QThreadPool::globalInstance()->start([guarded, work = std::move(work), done = std::move(done)]() mutable {
        auto result = work();
        QMetaObject::invokeMethod(QCoreApplication::instance(),
            [guarded, result = std::move(result), done = std::move(done)]() mutable {
                if (guarded) done(std::move(result));
            }, Qt::QueuedConnection);
    });
}
}
