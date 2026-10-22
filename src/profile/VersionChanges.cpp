#include "VersionChanges.h"
#include "storage/Storage.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <yaml-cpp/yaml.h>
#include <stdexcept>
namespace mlash {
namespace {
struct Budget { int nodes=0; qsizetype bytes=0; };
void digest(const YAML::Node& node,QCryptographicHash& hash,Budget& budget,int depth=0) {
    if (++budget.nodes>100000 || depth>48) throw std::runtime_error("budget");
    if (!node || node.IsNull()) { hash.addData(QByteArrayView("null")); return; }
    hash.addData(QByteArray::number(int(node.Type()))+':');
    if (node.IsScalar()) { const auto bytes=QByteArray::fromStdString(node.Scalar()); budget.bytes+=bytes.size(); if (budget.bytes>32*1024*1024) throw std::runtime_error("budget"); hash.addData(QByteArray::number(bytes.size())+':'); hash.addData(bytes); }
    else if (node.IsSequence()) { for (const auto& child:node) digest(child,hash,budget,depth+1); }
    else if (node.IsMap()) { QMap<QString,YAML::Node> fields; for (const auto& field:node) { if (!field.first.IsScalar()) throw std::runtime_error("key"); const auto key=QString::fromStdString(field.first.Scalar()); if (fields.contains(key)) throw std::runtime_error("duplicate"); fields[key]=field.second; } for (auto it=fields.begin();it!=fields.end();++it) { digest(YAML::Node(it.key().toStdString()),hash,budget,depth+1); digest(it.value(),hash,budget,depth+1); } }
    hash.addData(QByteArrayView(";"));
}
QByteArray fingerprint(const YAML::Node& node,Budget& budget) { QCryptographicHash hash(QCryptographicHash::Sha256); digest(node,hash,budget); return hash.result(); }
using Entries=QMap<QString,QByteArray>;
Entries named(const YAML::Node& sequence,Budget& budget) {
    Entries result; if (!sequence || sequence.IsNull()) return result; if (!sequence.IsSequence()) throw std::runtime_error("shape");
    for (const auto& node:sequence) { if (!node.IsMap()||!node["name"].IsScalar()) throw std::runtime_error("shape"); const auto name=QString::fromStdString(node["name"].Scalar()); if (name.isEmpty()||name.size()>512||result.contains(name)) throw std::runtime_error("name"); result[name]=fingerprint(node,budget); } return result;
}
QJsonObject difference(const QString& label,const Entries& before,const Entries& after) {
    QJsonArray added,removed,changed; int addCount=0,removeCount=0,changeCount=0;
    for (auto it=after.begin();it!=after.end();++it) { if (!before.contains(it.key())) { if (addCount++<100) added.append(it.key()); } else if (before.value(it.key())!=it.value()) { if (changeCount++<100) changed.append(it.key()); } }
    for (auto it=before.begin();it!=before.end();++it) if (!after.contains(it.key())) { if (removeCount++<100) removed.append(it.key()); }
    return {{"label",label},{"before",before.size()},{"after",after.size()},{"added_count",addCount},{"removed_count",removeCount},{"changed_count",changeCount},{"added",added},{"removed",removed},{"changed",changed},{"truncated",addCount>100||removeCount>100||changeCount>100}};
}
struct Snapshot { YAML::Node config; QJsonObject providers; QString time; };
Snapshot readSnapshot(const ProfileStore& store,const Profile& profile,const QString& version) {
    Snapshot snapshot; if (version.isEmpty()) { snapshot.config=YAML::Node(YAML::NodeType::Map); return snapshot; }
    const auto root=store.versionRoot(profile.id,version); if (root.isEmpty()) throw std::runtime_error("version");
    const auto expected=QFileInfo(store.root()).canonicalFilePath()+"/profiles/"+profile.id+"/versions/"+version;
    if (QFileInfo(root).canonicalFilePath().compare(expected,Qt::CaseInsensitive)!=0) throw std::runtime_error("path");
    for (const auto& path:QStringList{root+"/work",root+"/work/config.yaml",root+"/work/provider-snapshots.json"}) if (QFileInfo(path).isSymLink()) throw std::runtime_error("link");
    QByteArray bytes; if (!readFile(root+"/work/config.yaml",bytes,8*1024*1024).ok) throw std::runtime_error("read"); const auto documents=YAML::LoadAll(bytes.toStdString()); if (documents.size()!=1||!documents[0].IsMap()) throw std::runtime_error("shape"); snapshot.config=documents[0];
    snapshot.time=QFileInfo(root+"/source.yaml").lastModified().toUTC().toString(Qt::ISODate);
    if (QFileInfo::exists(root+"/work/provider-snapshots.json")) { if (!readFile(root+"/work/provider-snapshots.json",bytes,1024*1024).ok) throw std::runtime_error("metadata"); QJsonParseError error; const auto document=QJsonDocument::fromJson(bytes,&error); if (error.error!=QJsonParseError::NoError||!document.isObject()) throw std::runtime_error("metadata"); snapshot.providers=document.object(); }
    return snapshot;
}
Entries providers(const Snapshot& snapshot,Budget& budget) {
    Entries result; for (const auto* kind:{"proxy-providers","rule-providers"}) { const auto definitions=snapshot.config[kind]; if (!definitions) continue; if (!definitions.IsMap()) throw std::runtime_error("provider"); for (const auto& entry:definitions) { const auto name=QString::fromStdString(entry.first.as<std::string>()); const auto path=QString::fromStdString(entry.second["path"].as<std::string>("")); const auto sha=snapshot.providers.value(path).toObject().value("sha256").toString(); if (name.size()>512) throw std::runtime_error("name"); result[QString::fromLatin1(kind)+" / "+name]=fingerprint(entry.second,budget)+sha.toLatin1()+snapshot.providers.value(path).toObject().value("source_sha256").toString().toLatin1(); } } return result;
}
QMap<QByteArray,int> rules(const YAML::Node& root,Budget& budget,int& count) { QMap<QByteArray,int> result; const auto values=root["rules"]; count=0; if (!values) return result; if (!values.IsSequence()) throw std::runtime_error("rules"); for (const auto& value:values) { ++count; ++result[fingerprint(value,budget)]; } return result; }
}
Result VersionChanges::inspect(const ProfileStore& store,const Profile& profile) {
    try {
        if (profile.candidate.isEmpty()) return Result::fail("没有可查看的候选版本"); const auto old=readSnapshot(store,profile,profile.active),next=readSnapshot(store,profile,profile.candidate); Budget budget; QJsonArray sections;
        const auto beforeNodes=named(old.config["proxies"],budget),afterNodes=named(next.config["proxies"],budget); sections.append(difference("静态节点",beforeNodes,afterNodes)); sections.append(difference("策略组",named(old.config["proxy-groups"],budget),named(next.config["proxy-groups"],budget))); sections.append(difference("提供者快照",providers(old,budget),providers(next,budget)));
        int beforeCount=0,afterCount=0,added=0,removed=0; const auto before=rules(old.config,budget,beforeCount),after=rules(next.config,budget,afterCount); for (auto it=after.begin();it!=after.end();++it) added+=qMax(0,it.value()-before.value(it.key())); for (auto it=before.begin();it!=before.end();++it) removed+=qMax(0,it.value()-after.value(it.key()));
        sections.append(QJsonObject{{"label","规则"},{"before",beforeCount},{"after",afterCount},{"added_count",added},{"removed_count",removed},{"changed_count",0},{"order_changed",fingerprint(old.config["rules"],budget)!=fingerprint(next.config["rules"],budget)}});
        Entries oldSettings,newSettings; for (const auto* key:{"dns","hosts","ipv6","unified-delay","sniffer","tcp-concurrent","find-process-mode","interface-name"}) { if (old.config[key]) oldSettings[key]=fingerprint(old.config[key],budget); if (next.config[key]) newSettings[key]=fingerprint(next.config[key],budget); } sections.append(difference("核心选项",oldSettings,newSettings));
        QMap<QString,YAML::Node> groups; const auto list=next.config["proxy-groups"]; if (list) for (const auto& group:list) groups[QString::fromStdString(group["name"].as<std::string>())]=static_cast<const YAML::Node&>(group);
        QJsonArray lost,unknown; for (auto it=profile.selections.begin();it!=profile.selections.end();++it) { if (lost.size()+unknown.size()>=256) break; const auto group=groups.value(it.key()); const auto member=it.value().toString(); bool found=it.key()=="GLOBAL"&&(QStringList{"DIRECT","REJECT","REJECT-DROP"}.contains(member)||afterNodes.contains(member)||groups.contains(member)); if (group&&group["proxies"]) for (const auto& value:group["proxies"]) if (value.IsScalar()&&QString::fromStdString(value.Scalar())==member) found=true; if (!found) { const auto text=it.key()+" → "+member; if ((group&&group["use"])||(it.key()=="GLOBAL"&&next.config["proxy-providers"])) unknown.append(text); else lost.append(text); } }
        return {true,{},{{"sections",sections},{"active_version",profile.active},{"candidate_version",profile.candidate},{"active_time",old.time},{"candidate_time",next.time},{"first_use",profile.active.isEmpty()},{"lost_selections",lost},{"unverified_selections",unknown}}};
    } catch (const std::exception&) { return Result::fail("版本快照缺失、结构无效或超过比较预算；当前运行版本保持不变，请更新候选后重试"); }
}
}
