#pragma once
#include "Types.h"
#include <QUrl>
namespace mlash {
struct ServiceTarget { QString id,name; QUrl url; };
QList<ServiceTarget> serviceTargets();
QJsonObject analyzeService(const QString& id,int status,const QByteArray& body,const QUrl& redirect={});
}
