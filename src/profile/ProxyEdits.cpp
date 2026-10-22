#include "ProxyEdits.h"
#include <QJsonDocument>
#include <QSet>
#include <functional>
namespace mlash {
namespace {
bool validName(const QString& name) { if (name.trimmed()!=name||name.isEmpty()||name.size()>256) return false; for (const auto ch:name) if (ch.isNull()||ch.category()==QChar::Other_Control) return false; return true; }
const QSet<QString> builtin{"DIRECT","REJECT","REJECT-DROP","PASS","COMPATIBLE","GLOBAL"};
QString nameOf(const YAML::Node& node) { return QString::fromStdString(node["name"].as<std::string>("")); }
}
Result validateProxyEdits(const QJsonObject& edits) {
    if (QJsonDocument(edits).toJson(QJsonDocument::Compact).size()>256*1024) return Result::fail("节点与策略组覆盖合计最多256KiB");
    for (auto it=edits.begin();it!=edits.end();++it) {
        const bool remove=it.key()=="remove_nodes"||it.key()=="remove_groups";
        if (it.key()=="dialers") { if (!it.value().isObject()||it.value().toObject().size()>512) return Result::fail("拨号链覆盖最多512项"); const auto dialers=it.value().toObject(); for (auto item=dialers.begin();item!=dialers.end();++item) if (!validName(item.key())||builtin.contains(item.key())||!item.value().isString()||(!item.value().toString().isEmpty()&&!validName(item.value().toString()))) return Result::fail("拨号链名称或目标无效"); continue; }
        if (!remove&&it.key()!="nodes"&&it.key()!="groups") return Result::fail("节点覆盖含未开放字段");
        if (remove) { if (!it.value().isArray()||it.value().toArray().size()>512) return Result::fail("移除列表类型或数量无效"); QSet<QString> names; for (const auto& value:it.value().toArray()) { const auto name=value.toString(); if (!value.isString()||!validName(name)||builtin.contains(name)||names.contains(name)) return Result::fail("移除列表名称无效或重复"); names.insert(name); } }
        else { if (!it.value().isObject()||it.value().toObject().size()>(it.key()=="nodes"?512:256)) return Result::fail("节点或策略组覆盖数量无效"); const auto values=it.value().toObject(); const auto removed=edits.value(it.key()=="nodes"?"remove_nodes":"remove_groups").toArray(); for (auto entry=values.begin();entry!=values.end();++entry) if (!validName(entry.key())||builtin.contains(entry.key())||!entry.value().isString()||entry.value().toString().isEmpty()||entry.value().toString().toUtf8().size()>64*1024||entry.value().toString().contains(QChar::Null)||removed.contains(entry.key())) return Result::fail("节点或策略组覆盖名称、定义或移除状态无效"); }
    }
    return {};
}
Result applyProxyEdits(YAML::Node& config,const QJsonObject& edits) {
    auto result=validateProxyEdits(edits); if (!result.ok||edits.isEmpty()) return result;
    try {
        for (const bool groups:{false,true}) {
            const auto field=groups?"proxy-groups":"proxies"; const auto key=groups?"groups":"nodes";
            const auto original=config[field]; if (original&&!original.IsSequence()) return Result::fail("节点或策略组来源必须为列表");
            const auto replacements=edits.value(key).toObject(); const auto removed=edits.value(groups?"remove_groups":"remove_nodes").toArray(); QSet<QString> seen;
            YAML::Node merged(YAML::NodeType::Sequence);
            const auto append=[&](const QString& name,const YAML::Node& source) { if (replacements.contains(name)) { const auto docs=YAML::LoadAll(replacements.value(name).toString().toStdString()); if (docs.size()!=1||!docs[0].IsMap()||nameOf(docs[0])!=name) throw std::runtime_error("invalid entry"); merged.push_back(YAML::Clone(docs[0])); } else merged.push_back(YAML::Clone(source)); };
            if (original) for (const auto& entry:original) { if (!entry.IsMap()) return Result::fail("节点或策略组条目必须为映射"); const auto name=nameOf(entry); if (!validName(name)||builtin.contains(name)||seen.contains(name)) return Result::fail("节点或策略组来源名称无效或重复"); seen.insert(name); if (!removed.contains(name)) append(name,entry); }
            for (auto entry=replacements.begin();entry!=replacements.end();++entry) if (!seen.contains(entry.key())) append(entry.key(),{});
            if (merged.size()>4096) return Result::fail("编辑后的节点或组数量超过4096"); config[field]=merged;
        }
        QSet<QString> names=builtin; QHash<QString,QStringList> edges;
        for (const auto* field:{"proxies","proxy-groups"}) if (config[field]) for (const auto& entry:config[field]) { const auto name=nameOf(entry); if (names.contains(name)) return Result::fail("节点与策略组名称冲突"); names.insert(name); }
        const auto dialers=edits.value("dialers").toObject(); QSet<QString> applied;
        if (config["proxies"]) for (auto proxy:config["proxies"]) {
            const auto name=nameOf(proxy);
            if (dialers.contains(name)) { applied.insert(name); const auto target=dialers.value(name).toString(); if (target.isEmpty()) proxy.remove("dialer-proxy"); else proxy["dialer-proxy"]=target.toStdString(); }
            if (proxy["dialer-proxy"]) { const auto target=QString::fromStdString(proxy["dialer-proxy"].as<std::string>()); if (!target.isEmpty()) { if (!names.contains(target)||QStringList{"GLOBAL","REJECT","REJECT-DROP","PASS","COMPATIBLE"}.contains(target)) return Result::fail("拨号链目标不存在或不是可拨号策略"); edges[name].append(target); } }
        }
        if (applied.size()!=dialers.size()) return Result::fail("拨号链只能配置当前静态节点，不能引用已移除节点或动态Provider节点");
        const auto groups=config["proxy-groups"];
        if (groups) for (const auto& group:groups) {
            const auto name=nameOf(group); const auto type=QString::fromStdString(group["type"].as<std::string>(""));
            if (!QStringList{"select","url-test","fallback","load-balance"}.contains(type)) return Result::fail("编辑仅支持选择、自动、故障转移和负载均衡组");
            if (group["proxies"]) { if (!group["proxies"].IsSequence()) return Result::fail("策略组成员必须为列表"); for (const auto& member:group["proxies"]) { const auto target=QString::fromStdString(member.as<std::string>()); if (!names.contains(target)||target=="GLOBAL") return Result::fail("策略组引用不存在的成员或GLOBAL"); edges[name].append(target); } }
            if (group["use"]) { if (!group["use"].IsSequence()) return Result::fail("策略组提供者必须为列表"); for (const auto& provider:group["use"]) { if (!config["proxy-providers"]||!config["proxy-providers"][provider.as<std::string>()]) return Result::fail("策略组引用不存在的节点提供者"); } }
        }
        QHash<QString,int> marks; std::function<bool(const QString&,int)> visit=[&](const QString& node,int depth) { if (depth>64||marks.value(node)==1) return false; if (marks.value(node)==2) return true; marks[node]=1; for (const auto& target:edges.value(node)) if (!visit(target,depth+1)) return false; marks[node]=2; return true; };
        for (auto edge=edges.begin();edge!=edges.end();++edge) if (!visit(edge.key(),0)) return Result::fail("策略组或拨号链存在循环或超过64层");
        return {};
    } catch (const YAML::Exception& error) { return Result::fail(QString("节点覆盖YAML类型或语法错误（第%1行）；请核对定义").arg(qMax(1,error.mark.line+1))); }
    catch (const std::exception&) { return Result::fail("节点定义必须是单个映射，name与编辑名称一致"); }
}
}
