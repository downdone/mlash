#pragma once
#include "ProfileStore.h"
#include <QDateTime>
#include <QHash>
namespace mlash {
Result validateProviderPolicies(const QJsonObject& policies);
QDateTime providerNextDue(const QJsonObject& policy,const QString& updated,const QJsonObject& attempt,const QDateTime& now=QDateTime::currentDateTimeUtc());
Result findDueProvider(ProfileStore& store,const QVector<Profile>& profiles,const Settings& settings,const QHash<QString,QJsonObject>& attempts,int cursor,const QDateTime& now=QDateTime::currentDateTimeUtc());
}
