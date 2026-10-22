#pragma once
#include "core/MihomoClient.h"
#include <QDateTime>
#include <QHash>

namespace mlash {
struct WebsiteTarget {
    QString id, name, url, mark;
    bool operator==(const WebsiteTarget&) const = default;
    QJsonObject toJson() const { return {{"id",id},{"name",name},{"url",url},{"mark",mark}}; }
};
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
    explicit WebsiteTests(MihomoClient* api,QObject* parent=nullptr,QList<WebsiteTarget> targets=defaults());
    ~WebsiteTests() override;
    static QList<WebsiteTarget> defaults();
    static Result validateTargets(const QJsonArray& targets);
    void configureTargets(const QJsonArray& targets);
    const QList<WebsiteTarget>& targets() const { return targets_; }
    WebsiteResult result(const QString& id) const { return results_.value(id); }
    QString node() const { return node_; }
    bool running() const { return !queue_.isEmpty() || !requests_.isEmpty(); }
    void start(const QString& node,const QString& onlyId={});
    void cancel();
    void clear();
signals:
    void changed();
    void targetsChanged();
private:
    void dispatch();
    MihomoClient* api_;
    QList<WebsiteTarget> targets_;
    QList<WebsiteTarget> initialTargets_;
    QHash<QString,WebsiteResult> results_;
    QHash<QString,QPointer<QNetworkReply>> requests_;
    QStringList queue_;
    QString node_;
    quint64 generation_=0;
};
}
