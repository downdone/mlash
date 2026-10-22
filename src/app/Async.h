#pragma once
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
