#pragma once
#include <QJsonObject>
#include <QString>

namespace mlash {
class SubscriptionInfo {
public:
    static QJsonObject parse(const QByteArray& header);
    static bool valid(const QJsonObject& info);
    static QString summary(const QJsonObject& info);
};
}
