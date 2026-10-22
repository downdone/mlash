#pragma once
#include "ProfileStore.h"

namespace mlash {
QStringList restoreProfileNames(const QStringList& names, const QVector<Profile>& current);
struct BackupRestore { StoreSnapshot snapshot; QStringList createdProfiles; };
class BackupArchive {
public:
    static Result create(ProfileStore& store, const Settings& settings, const QVector<Profile>& profiles, const QString& path, const QString& password, bool includeProfiles = true);
    static Result inspect(const QString& path, const QString& password);
    static BackupRestore prepareRestore(ProfileStore& store, const std::shared_ptr<PlatformServices>& platform, const Settings& current, const QVector<Profile>& profiles, const QString& path, const QString& password, const QString& expectedHash, bool restoreSettings, const QJsonValue& selectedProfiles = QJsonValue(QJsonValue::Undefined));
};
}
