#pragma once

#include "core/CoreApi.h"
#include <QHash>
#include <QPointer>
#include <QStringList>

namespace mlash {
// Independent, bounded read-only probes. Member selection remains a separate
// serialized write in AppController and never waits for these requests.
class GroupDelayTests final : public QObject {
    Q_OBJECT
public:
    explicit GroupDelayTests(CoreApi* api,QObject* parent=nullptr):QObject(parent),api_(api) {}
    ~GroupDelayTests() override { cancel(); }
    void start(QString group,QStringList members,QString url);
    void cancel();
    bool running() const { return !queue_.isEmpty() || !requests_.isEmpty(); }
    QString group() const { return group_; }
    int total() const { return total_; }
    int completed() const { return completed_; }
    bool cancelled() const { return cancelled_; }
signals:
    void result(const QString& member,int delay,const QString& error,const QString& url);
    void changed();
private:
    void dispatch();
    CoreApi* api_;
    QString group_,url_;
    QStringList queue_;
    QHash<QString,QPointer<QNetworkReply>> requests_;
    int total_=0,completed_=0;
    quint64 generation_=0;
    bool cancelled_=false;
};
}
