#include "RemoteBackup.h"
#include "storage/Storage.h"
#include <QJsonDocument>
#include <QUrl>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
namespace mlash {
QJsonObject remoteBackupUserFields(QJsonObject config) { config.remove("next_due_utc"); config.remove("last_success_utc"); return config; }
bool remoteBackupFileName(const QString& name) { static const QRegularExpression pattern("^[A-Za-z0-9][A-Za-z0-9._-]{0,119}\\.mlashbackup$"); return pattern.match(name).hasMatch()&&!name.contains(".."); }
Result validateRemoteBackup(const QJsonObject& config) {
    if (config.isEmpty()) return {}; if (QJsonDocument(config).toJson(QJsonDocument::Compact).size()>8192) return Result::fail("远端备份设置超过限制");
    for (auto item=config.begin();item!=config.end();++item) if (!QStringList{"url","username","password","backup_password","route","allow_private","enabled","interval_hours","next_due_utc","last_success_utc"}.contains(item.key())) return Result::fail("远端备份设置含未知字段");
    for (const auto* key:{"url","username","password","backup_password","route","next_due_utc","last_success_utc"}) if (config.contains(key)&&!config.value(key).isString()) return Result::fail("远端备份文本设置类型无效");
    for (const auto* key:{"allow_private","enabled"}) if (config.contains(key)&&!config.value(key).isBool()) return Result::fail("远端备份开关类型无效");
    const auto text=config.value("url").toString(); const QUrl url(text,QUrl::StrictMode); if (text.size()>2048||!url.isValid()||url.scheme()!="https"||url.host().isEmpty()||!url.userInfo().isEmpty()||url.hasQuery()||url.hasFragment()||!url.path().endsWith('/')||url.path().contains('\\')||url.path().split('/').contains("..")) return Result::fail("WebDAV目录须为以/结尾的HTTPS地址，不含凭据、查询或片段");
    const auto user=config.value("username").toString(),password=config.value("password").toString(),backup=config.value("backup_password").toString(); if (user.size()>256||user.contains(':')||password.size()>1024||(user.isEmpty()&&!password.isEmpty())||backup.size()<8||backup.size()>1024) return Result::fail("WebDAV账户或备份密码长度无效（备份密码至少8字符）");
    for (const auto ch:text+user+password+backup) if (ch.isNull()||ch.category()==QChar::Other_Control) return Result::fail("远端备份设置不能含控制字符");
    const auto interval=config.value("interval_hours"); if (!QStringList{"direct","core"}.contains(config.value("route").toString("direct"))||(config.contains("interval_hours")&&(!interval.isDouble()||interval.toDouble()!=interval.toInt()||interval.toInt()<1||interval.toInt()>168))) return Result::fail("备份路径或周期无效（1–168小时）");
    for (const auto* key:{"next_due_utc","last_success_utc"}) if (config.contains(key)&&!config.value(key).toString().isEmpty()&&!QDateTime::fromString(config.value(key).toString(),Qt::ISODate).isValid()) return Result::fail("备份时间字段无效"); return {};
}
RemoteBackupPayload createRemoteBackup(ProfileStore& store,const Settings& settings,const QVector<Profile>& profiles) {
    RemoteBackupPayload payload; const auto base=store.root()+"/backups/remote"; if (QFileInfo(store.root()+"/backups").isSymLink()||QFileInfo(base).isSymLink()) { payload.result=Result::fail("备份目录不能为链接"); return payload; } payload.result=store.createPrivateDirectory(base); if (!payload.result.ok) return payload;
    payload.name="mlash-"+QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmssZ")+'-'+newId()+".mlashbackup"; payload.path=base+'/'+payload.name; payload.result=BackupArchive::create(store,settings,profiles,payload.path,settings.remoteBackup.value("backup_password").toString()); if (!payload.result.ok) return payload; payload.result=readFile(payload.path,payload.bytes,64*1024*1024+64); if (!payload.result.ok) return payload;
    // Only our generated, regular local snapshots participate in the five-file cache.
    const auto files=QDir(base).entryInfoList({"mlash-*.mlashbackup"},QDir::Files|QDir::NoSymLinks,QDir::Time); int kept=1; for (const auto& file:files) { if (!remoteBackupFileName(file.fileName())||file.absoluteFilePath()==payload.path) continue; if (++kept>5) QFile::remove(file.absoluteFilePath()); } return payload;
}
Result saveRemoteBackupDownload(ProfileStore& store,const QByteArray& bytes,const QString& password,const QString& destination) {
    if (bytes.isEmpty()||bytes.size()>64*1024*1024+64||QFileInfo(destination).suffix().compare("mlashbackup",Qt::CaseInsensitive)!=0) return Result::fail("下载备份大小或目标扩展名无效"); const auto base=store.root()+"/backups/remote"; if (QFileInfo(store.root()+"/backups").isSymLink()||QFileInfo(base).isSymLink()) return Result::fail("备份目录不能为链接"); auto result=store.createPrivateDirectory(base); if (!result.ok) return result; const auto temporary=base+"/download-"+newId()+".mlashbackup"; result=writeAtomic(temporary,bytes); if (!result.ok) return result; const auto preview=BackupArchive::inspect(temporary,password); if (preview.ok) result=writeAtomic(destination,bytes); else result=preview; QFile::remove(temporary); if (result.ok) result.data=preview.data; return result;
}
}
