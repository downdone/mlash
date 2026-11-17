#pragma once
#include "domain/Types.h"
#include <QJsonArray>
#include <QList>

namespace mlash {
struct WebsiteTarget {
    QString id, name, url, mark;
    bool operator==(const WebsiteTarget&) const = default;
    QJsonObject toJson() const { return {{"id",id},{"name",name},{"url",url},{"mark",mark}}; }
};

// Pure schema validation shared by persistence and the on-demand test session.
Result validateWebsiteTargets(const QJsonArray& targets);
}
