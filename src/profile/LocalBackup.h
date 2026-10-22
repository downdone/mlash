#pragma once
#include "BackupArchive.h"
#include <QDateTime>

namespace mlash {
Result validateLocalBackup(const QJsonObject& config);
QJsonObject localBackupUserFields(QJsonObject config);
class LocalBackup {
public:
    static bool due(const QJsonObject& config, const QDateTime& now);
    static Result create(ProfileStore& store, const Settings& settings,
                         const QVector<Profile>& profiles, const QString& reason);
    static Result list(ProfileStore& store);
};
}
