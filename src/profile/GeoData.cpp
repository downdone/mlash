#include "GeoData.h"
#include "storage/Storage.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <yaml-cpp/yaml.h>

namespace mlash {
namespace {
const QStringList names{"country.mmdb","geoip.dat","geosite.dat","asn.mmdb"};
QString hash(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
bool validStage(const QString& root,const QString& path) {
    const auto info=QFileInfo(path); const auto parent=QFileInfo(root).canonicalFilePath();
    return !parent.isEmpty()&&info.isDir()&&!info.isSymLink()&&info.fileName().startsWith("geo-update-")&&info.canonicalFilePath().startsWith(parent+'/',Qt::CaseInsensitive)&&QFileInfo(info.canonicalFilePath()).absolutePath().compare(parent,Qt::CaseInsensitive)==0;
}
}
QUrl GeoData::source(const QString& name) { return names.contains(name)?QUrl("https://github.com/MetaCubeX/meta-rules-dat/releases/download/latest/"+(name=="asn.mmdb"?QString("GeoLite2-ASN.mmdb"):name)):QUrl(); }
Result GeoData::inspect(const QString& root,const QString& activeWork) {
    if (QFileInfo(root+"/geodata").isSymLink()) return Result::fail("Geo默认目录为链接，未读取");
    QJsonArray rows; QByteArray metadata; QJsonObject records;
    if (readFile(root+"/geodata/manifest.json",metadata,1024*1024).ok) for (const auto& value : QJsonDocument::fromJson(metadata).object().value("files").toArray()) { const auto record=value.toObject(); records[record.value("name").toString()]=record; }
    for (const auto& location : QStringList{root+"/geodata",activeWork}) {
        if (location.isEmpty()) continue;
        for (const auto& info : QDir(location).entryInfoList(QDir::Files|QDir::NoSymLinks)) {
            const auto name=info.fileName().toLower(); if (!QStringList{"country.mmdb","geoip.db","geoip.metadb","geoip.dat","geosite.dat","asn.mmdb"}.contains(name)) continue;
            QByteArray bytes; const auto read=readFile(info.absoluteFilePath(),bytes,128*1024*1024); if (!read.ok) return read;
            const auto record=records.value(name).toObject(); const auto digest=hash(bytes); const bool active=location==activeWork;
            rows.append(QJsonObject{{"name",name},{"scope",active?"当前运行版本":"默认快照"},{"bytes",double(bytes.size())},{"sha256",digest},{"origin",active?QString("激活版本内文件"):record.value("origin").toString("旧快照/来源未记录")},{"updated_utc",active?info.lastModified().toUTC().toString(Qt::ISODate):record.value("updated_utc").toString(info.lastModified().toUTC().toString(Qt::ISODate))},{"hash_matches",record.contains("sha256")&&record.value("sha256").toString()==digest}});
        }
    }
    QString policy="没有当前运行配置";
    if (!activeWork.isEmpty()) { QByteArray bytes; if (readFile(activeWork+"/config.yaml",bytes).ok) { try { const auto config=YAML::Load(bytes.toStdString()); policy=config["geo-auto-update"].as<bool>(false)?"来源开启内核自动Geo更新；运行文件可能自行变化":"当前来源未开启内核自动Geo更新"; if (config["geox-url"]) policy+="；来源自定义Geo地址，不自动套用默认快照"; } catch (const YAML::Exception&) { policy="当前Geo策略无法读取"; } } }
    return {true,{},{{"files",rows},{"policy",policy}}};
}
Result GeoData::stage(ProfileStore& store,const QString& name,const QByteArray& bytes) {
    if (!names.contains(name)||bytes.isEmpty()||bytes.size()>128*1024*1024) return Result::fail("Geo名称或大小无效");
    const auto stage=store.root()+"/geo-update-"+newId();
    // The store creates private ACLs when importing; the staging directory needs them too.
    const auto directory=store.createPrivateDirectory(stage); if (!directory.ok) return directory;
    auto result=writeAtomic(stage+'/'+name,bytes); if (!result.ok) { discard(store.root(),stage); return result; }
    const QString rule=name=="geosite.dat"?"GEOSITE,cn,DIRECT":name=="asn.mmdb"?"IP-ASN,13335,DIRECT":"GEOIP,CN,DIRECT";
    const QByteArray config="mixed-port: 0\nallow-lan: false\nmode: rule\ngeo-auto-update: false\ngeodata-mode: "+QByteArray(name=="geoip.dat"?"true":"false")+"\ngeox-url: {geoip: 'http://127.0.0.1:1/disabled', mmdb: 'http://127.0.0.1:1/disabled', geosite: 'http://127.0.0.1:1/disabled', asn: 'http://127.0.0.1:1/disabled'}\nrules: ['"+rule.toUtf8()+"','MATCH,DIRECT']\n";
    result=writeAtomic(stage+"/config.yaml",config); if (!result.ok) { discard(store.root(),stage); return result; }
    return {true,{},{{"stage",stage},{"name",name},{"sha256",hash(bytes)}}};
}
Result GeoData::commit(ProfileStore& store,const Result& staged) {
    const auto stage=staged.data.value("stage").toString(),name=staged.data.value("name").toString();
    if (!staged.ok||!validStage(store.root(),stage)||!names.contains(name)||QFileInfo(stage+'/'+name).isSymLink()||QFileInfo(store.root()+"/geodata").isSymLink()) return Result::fail("Geo暂存快照无效");
    QByteArray bytes; auto result=readFile(stage+'/'+name,bytes,128*1024*1024); if (!result.ok||hash(bytes)!=staged.data.value("sha256").toString()) { discard(store.root(),stage); return Result::fail("Geo文件在校验期间发生变化，未替换"); }
    QJsonObject provenance; QByteArray metadata;
    if (readFile(store.root()+"/geodata/manifest.json",metadata,1024*1024).ok) for (const auto& value : QJsonDocument::fromJson(metadata).object().value("files").toArray()) { const auto record=value.toObject(); provenance[record.value("name").toString()]=record; }
    for (const auto& info : QDir(store.root()+"/geodata").entryInfoList(QDir::Files)) {
        const auto other=info.fileName().toLower(); if (other==name||!QStringList{"country.mmdb","geoip.db","geoip.metadb","geoip.dat","geosite.dat","asn.mmdb"}.contains(other)) continue;
        if (info.isSymLink()) { discard(store.root(),stage); return Result::fail("既有Geo文件为链接，未合并"); }
        result=readFile(info.absoluteFilePath(),bytes,128*1024*1024);
        const auto expected=provenance.value(other).toObject().value("sha256").toString(); if (result.ok&&!expected.isEmpty()&&expected!=hash(bytes)) result=Result::fail("既有Geo快照哈希变化，未合并；请重新导入该文件");
        if (result.ok) result=writeAtomic(stage+'/'+other,bytes); if (!result.ok) { discard(store.root(),stage); return result; }
    }
    provenance[name]=QJsonObject{{"origin",source(name).toString()},{"updated_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    result=store.importGeoData(stage,provenance); discard(store.root(),stage); return result;
}
void GeoData::discard(const QString& root,const QString& stage) { if (validStage(root,stage)) QDir(stage).removeRecursively(); }
}
