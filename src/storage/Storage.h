#pragma once
#include "app/Types.h"
#include <QByteArray>

namespace mlash {
Result normalizeProxyBypass(const QString& text);
bool validTestUrl(const QString& text);
Result validateNodePreferences(const QJsonObject& object);
Result validateConnectionView(const QJsonObject& object);
bool validAppearance(const QString& appearance,int scale);
Result writeAtomic(const QString& path, const QByteArray& data);
Result readFile(const QString& path, QByteArray& data, qint64 limit = 8 * 1024 * 1024);
QString newId();
bool isId(const QString& id);
}
