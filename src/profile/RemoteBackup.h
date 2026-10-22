#pragma once
#include "BackupArchive.h"
namespace mlash {
Result validateRemoteBackup(const QJsonObject& config);
QJsonObject remoteBackupUserFields(QJsonObject config);
bool remoteBackupFileName(const QString& name);
struct RemoteBackupPayload { Result result; QByteArray bytes; QString path,name; };
RemoteBackupPayload createRemoteBackup(ProfileStore& store,const Settings& settings,const QVector<Profile>& profiles);
Result saveRemoteBackupDownload(ProfileStore& store,const QByteArray& bytes,const QString& password,const QString& destination);
}
