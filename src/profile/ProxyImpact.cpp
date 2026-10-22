#include "ProxyImpact.h"
#include "ProxyEdits.h"
#include "RuleDraft.h"
#include "YamlFields.h"
#include <QSet>

namespace mlash {
Result inspectProxyReferences(const YAML::Node& source,const YAML::Node& effective,
                              const QJsonObject& draft,const QString& name) {
    const auto valid=validateProxyEdits(draft);
    if (!valid.ok) return valid;
    if (name.isEmpty()||name.size()>256) return Result::fail("待移除对象名称无效");
    try {
        QJsonArray references; QSet<QString> seen;
        int total=0; bool incomplete=false,found=false; QSet<QString> nodeNames;
        const auto reference=[&](const QString& kind,const QString& owner) {
            const auto key=kind+'\n'+owner;
            if (seen.contains(key)) return;
            seen.insert(key); ++total;
            if (references.size()<100) references.append(QJsonObject{{"kind",kind},{"name",owner}});
        };
        for (const bool groups:{false,true}) {
            const auto key=groups?"groups":"nodes";
            const auto field=groups?"proxy-groups":"proxies";
            const auto replacements=draft.value(key).toObject();
            const auto removed=draft.value(groups?"remove_groups":"remove_nodes").toArray();
            QJsonObject definitions;
            const auto original=source[field];
            if (original&&original.IsSequence()) for (const auto& entry:original) {
                const auto owner=entry["name"];
                if (owner&&owner.IsScalar()) definitions[QString::fromStdString(owner.Scalar())]=QString::fromStdString(YAML::Dump(entry));
            }
            for (auto entry=replacements.begin();entry!=replacements.end();++entry) definitions[entry.key()]=entry.value();
            if (!groups) for (auto entry=definitions.begin();entry!=definitions.end();++entry) if (!removed.contains(entry.key())) nodeNames.insert(entry.key());
            for (auto entry=definitions.begin();entry!=definitions.end();++entry) {
                if (entry.key()==name) { found=true; continue; }
                if (removed.contains(entry.key())) continue;
                const auto text=entry.value().toString();
                const auto inspected=inspectYamlFields(text,key);
                if (!inspected.ok) { incomplete=true; continue; }
                const auto values=inspected.data.value("values").toObject();
                if (values.value("name").toString()!=entry.key()||values.value("type").toString().isEmpty()) incomplete=true;
                const auto definition=YAML::Load(text.toStdString());
                if (groups) {
                    const auto type=values.value("type").toString();
                    if (!QStringList{"select","url-test","fallback","load-balance"}.contains(type)) incomplete=true;
                    for (const auto* field:{"include-all","include-all-proxies","include-all-providers"}) {
                        const auto value=static_cast<const YAML::Node&>(definition)[field];
                        if (value&&(!value.IsScalar()||value.Scalar()!="false")) incomplete=true;
                    }
                    const auto members=static_cast<const YAML::Node&>(definition)["proxies"];
                    if (members&&!members.IsSequence()) incomplete=true;
                    else if (members) for (const auto& member:members) {
                        if (!member.IsScalar()) incomplete=true;
                        else if (QString::fromStdString(member.Scalar())==name) reference("group",entry.key());
                    }
                } else {
                    const auto dialers=draft.value("dialers").toObject();
                    if (dialers.contains(entry.key())) {
                        if (dialers.value(entry.key()).toString()==name) reference("dialer",entry.key());
                    } else {
                        const auto upstream=static_cast<const YAML::Node&>(definition)["dialer-proxy"];
                        if (upstream&&!upstream.IsScalar()) incomplete=true;
                        else if (upstream&&QString::fromStdString(upstream.Scalar())==name) reference("dialer",entry.key());
                    }
                }
            }
        }
        if (!found) return Result::fail("对象已不存在，请重新选择");
        const auto dialers=draft.value("dialers").toObject(); const auto removedNodes=draft.value("remove_nodes").toArray();
        for (auto entry=dialers.begin();entry!=dialers.end();++entry) if (entry.key()!=name&&!removedNodes.contains(entry.key())&&!nodeNames.contains(entry.key())) {
            incomplete=true; if (entry.value().toString()==name) reference("dialer",entry.key());
        }
        const auto rules=effective["rules"];
        if (rules&&rules.IsSequence()) for (size_t index=0;index<rules.size();++index) {
            const auto rule=rules[index];
            if (!rule.IsScalar()) { incomplete=true; continue; }
            const auto text=QString::fromStdString(rule.Scalar());
            if (!RuleDraft::check({text},{},{},false).ok) { incomplete=true; continue; }
            if (RuleDraft::fields(text).value(2).trimmed()==name) reference("rule",QString::number(index+1));
        }
        const auto providers=source["proxy-providers"],subrules=source["sub-rules"];
        incomplete|=(providers&&providers.size()>0)||(subrules&&subrules.size()>0);
        return {true,{},{{"references",references},{"total",total},{"incomplete",incomplete}}};
    } catch (const std::exception&) {
        return Result::fail("无法分析当前静态引用；原草稿保留，请检查定义或重新打开");
    }
}
}
