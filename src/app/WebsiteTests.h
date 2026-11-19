#pragma once
#include "core/CoreApi.h"
#include "domain/WebsiteTargets.h"
#include <QDateTime>
#include <QHash>
#include <QPointer>

namespace mlash {
struct WebsiteResult {
    enum Status { Untested, Queued, Testing, Responded, Failed, Cancelled } status=Untested;
    int delay=-1;
    QString error;
    QDateTime measured;
};
// Bounded, on-demand reads. No periodic polling and no proxy-selection writes.
class WebsiteTests final : public QObject {
    Q_OBJECT
public:
    explicit WebsiteTests(CoreApi* api,QObject* parent=nullptr,QList<WebsiteTarget> targets=defaults());
    ~WebsiteTests() override;
    static QList<WebsiteTarget> defaults();
    static Result validateTargets(const QJsonArray& targets);
    void configureTargets(const QJsonArray& targets);
    const QList<WebsiteTarget>& targets() const { return targets_; }
    WebsiteResult result(const QString& id) const { return results_.value(id); }
    QString node() const { return node_; }
    bool running() const { return !queue_.isEmpty() || !requests_.isEmpty(); }
    void selectNode(const QString& node);
    void start(const QString& node,const QString& onlyId={});
    void cancel();
    void clear();
signals:
    void changed();
    void targetsChanged();
private:
    void dispatch();
    CoreApi* api_;
    QList<WebsiteTarget> targets_;
    QList<WebsiteTarget> initialTargets_;
    QHash<QString,WebsiteResult> results_;
    QHash<QString,QHash<QString,WebsiteResult>> cachedResults_;
    QStringList cacheOrder_;
    QHash<QString,QPointer<QNetworkReply>> requests_;
    QStringList queue_;
    QString node_;
    quint64 generation_=0;
};
}
