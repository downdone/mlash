#include "LocalBackup.h"
#include "storage/Storage.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <algorithm>

namespace mlash {
namespace {
QString digest(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
bool ownedName(const QString& name) {
    static const QRegularExpression pattern("^snapshot-[0-9]{8}T[0-9]{6}Z-[a-f0-9-]{36}\\.mlashbackup$");
    return pattern.match(name).hasMatch();
}
Result directory(ProfileStore& store, QString& base) {
    base = store.root() + "/backups/local";
    if (QFileInfo(store.root()+"/backups").isSymLink() || QFileInfo(base).isSymLink())
        return Result::fail("本地备份目录不能为链接");
    return store.createPrivateDirectory(base);
}
Result inspectRecord(const QFileInfo& file) {
    QByteArray metadata, bytes;
    if (!ownedName(file.fileName()) || file.isSymLink() ||
        !readFile(file.absoluteFilePath()+".json", metadata, 4096).ok ||
        QFileInfo(file.absoluteFilePath()+".json").isSymLink()) return Result::fail("备份元数据缺失或不是本程序快照");
    const auto doc = QJsonDocument::fromJson(metadata);
    const auto record = doc.object();
    if (!doc.isObject() || record.size()!=6 || record.value("file")!=file.fileName() ||
        !QDateTime::fromString(record.value("created_utc").toString(),Qt::ISODateWithMs).isValid() ||
        !QStringList{"scheduled","manual","before-change"}.contains(record.value("reason").toString()) ||
        !record.value("profiles").isDouble() || record.value("profiles").toDouble()!=record.value("profiles").toInt() ||
        record.value("profiles").toInt()<0 || record.value("profiles").toInt()>256 ||
        !readFile(file.absoluteFilePath(),bytes,64*1024*1024+64).ok ||
        record.value("bytes").toDouble()!=double(bytes.size()) || record.value("sha256")!=digest(bytes))
        return Result::fail("备份完整性校验失败；保留文件，不参与自动清理");
    return {true,{},record};
}
}
QJsonObject localBackupUserFields(QJsonObject config) {
    config.remove("last_success_utc"); config.remove("next_due_utc");
    return config;
}
Result validateLocalBackup(const QJsonObject& config) {
    if (config.isEmpty()) return {};
    if (QJsonDocument(config).toJson(QJsonDocument::Compact).size()>4096) return Result::fail("本地备份设置超过限制");
    for (auto i=config.begin();i!=config.end();++i)
        if (!QStringList{"enabled","before_changes","interval_hours","retain","password","last_success_utc","next_due_utc"}.contains(i.key()))
            return Result::fail("本地备份设置含未知字段");
    for (const auto* key:{"enabled","before_changes"})
        if (config.contains(key)&&!config.value(key).isBool()) return Result::fail("本地备份开关类型无效");
    for (const auto& field:QList<QPair<QString,int>>{{"interval_hours",168},{"retain",30}}) {
        const auto value=config.value(field.first);
        if (!value.isDouble() || value.toDouble()!=value.toInt() || value.toInt()<1 || value.toInt()>field.second)
            return Result::fail("本地备份周期为1–168小时，保留数为1–30份");
    }
    const auto password=config.value("password");
    if (!password.isString() || password.toString().size()<8 || password.toString().size()>1024)
        return Result::fail("备份密码须为8–1024字符");
    for (const auto ch:password.toString()) if (ch.isNull()||ch.category()==QChar::Other_Control) return Result::fail("备份密码不能含控制字符");
    for (const auto* key:{"last_success_utc","next_due_utc"}) if (config.contains(key) &&
        (!config.value(key).isString() || !QDateTime::fromString(config.value(key).toString(),Qt::ISODate).isValid()))
        return Result::fail("本地备份时间无效");
    return {};
}
bool LocalBackup::due(const QJsonObject& config,const QDateTime& now) {
    if (!validateLocalBackup(config).ok || !config.value("enabled").toBool()) return false;
    const auto next=QDateTime::fromString(config.value("next_due_utc").toString(),Qt::ISODate);
    return !next.isValid() || next<=now;
}
Result LocalBackup::list(ProfileStore& store) {
    QString base; auto result=directory(store,base); if (!result.ok) return result;
    QList<QJsonObject> records;
    const auto files=QDir(base).entryInfoList({"snapshot-*.mlashbackup"},QDir::Files|QDir::NoSymLinks,QDir::Name|QDir::Reversed);
    for (const auto& file:files) {
        if (!ownedName(file.fileName())) continue;
        if (records.size()>=128) return Result::fail("备份目录快照超过128份，请手动整理后重试");
        const auto checked=inspectRecord(file); auto row=checked.data;
        row["file"]=file.fileName(); row["path"]=file.absoluteFilePath(); row["valid"]=checked.ok;
        if (!checked.ok) row["error"]=checked.error;
        records.append(row);
    }
    std::stable_sort(records.begin(),records.end(),[](const QJsonObject& a,const QJsonObject& b) {
        const auto left=QDateTime::fromString(a.value("created_utc").toString(),Qt::ISODateWithMs);
        const auto right=QDateTime::fromString(b.value("created_utc").toString(),Qt::ISODateWithMs);
        return left==right?a.value("file").toString()>b.value("file").toString():left>right;
    });
    QJsonArray rows; for (const auto& row:records) rows.append(row);
    return {true,{},{{"rows",rows},{"directory",base}}};
}
Result LocalBackup::create(ProfileStore& store,const Settings& settings,const QVector<Profile>& profiles,const QString& reason) {
    const auto config=settings.localBackup;
    auto result=validateLocalBackup(config);
    if (!result.ok || config.isEmpty()) return result.ok?Result::fail("请先保存本地备份设置"):result;
    if (!QStringList{"scheduled","manual","before-change"}.contains(reason)) return Result::fail("备份原因无效");
    QString base; result=directory(store,base); if (!result.ok) return result;
    const auto previous=list(store); if (!previous.ok) return previous;
    const auto now=QDateTime::currentDateTimeUtc();
    const auto name="snapshot-"+now.toString("yyyyMMddTHHmmssZ")+'-'+newId()+".mlashbackup";
    const auto path=base+'/'+name;
    result=BackupArchive::create(store,settings,profiles,path,config.value("password").toString());
    if (!result.ok) return result;
    const auto checked=BackupArchive::inspect(path,config.value("password").toString());
    QByteArray bytes;
    if (!checked.ok || !readFile(path,bytes,64*1024*1024+64).ok) { QFile::remove(path); return Result::fail("新备份验证失败；旧快照保持"); }
    const QJsonObject record{{"file",name},{"created_utc",now.toString(Qt::ISODateWithMs)},
        {"reason",reason},{"profiles",profiles.size()},{"bytes",double(bytes.size())},{"sha256",digest(bytes)}};
    result=writeAtomic(path+".json",QJsonDocument(record).toJson(QJsonDocument::Compact));
    if (!result.ok) { QFile::remove(path); return result; }
    int kept=1,removed=0;
    for (const auto& item:previous.data.value("rows").toArray()) {
        const auto row=item.toObject(); if (!row.value("valid").toBool()) continue;
        if (++kept<=config.value("retain").toInt()) continue;
        const auto old=QFileInfo(row.value("path").toString());
        // Revalidate immediately before deleting only our generated, unmodified files.
        if (!inspectRecord(old).ok) continue;
        if (!QFile::remove(old.absoluteFilePath())) continue;
        QFile::remove(old.absoluteFilePath()+".json"); ++removed;
    }
    auto data=record; data["path"]=path; data["removed"]=removed;
    return {true,{},data};
}
}
