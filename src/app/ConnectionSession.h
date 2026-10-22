#pragma once
#include "Types.h"
namespace mlash {
class ConnectionSession {
public:
    Result update(const QJsonObject&,qint64 nowMs);
    void reset();
    void invalidate();
    void suspend();
    const QJsonArray& active() const { return active_; }
    const QJsonArray& closed() const { return closed_; }
    const QJsonObject& totals() const { return totals_; }
    const QString& observedAt() const { return observedAt_; }
private:
    QJsonArray active_,closed_;
    QJsonObject totals_;
    qint64 previousMs_=-1;
    QString observedAt_;
};
}
