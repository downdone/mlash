#include "RuleDraft.h"
#include <QJsonArray>
#include <QHash>

namespace mlash {
QStringList RuleDraft::types() { return {"DOMAIN-SUFFIX","DOMAIN","DOMAIN-KEYWORD","IP-CIDR","IP-CIDR6","SRC-IP-CIDR","SRC-PORT","DST-PORT","PROCESS-NAME","PROCESS-PATH","GEOIP","GEOSITE","RULE-SET","MATCH"}; }
QString RuleDraft::label(const QString& type) {
    static const QHash<QString,QString> labels{{"DOMAIN-SUFFIX","域名后缀"},{"DOMAIN","完整域名"},{"DOMAIN-KEYWORD","域名关键词"},{"IP-CIDR","目标 IPv4 网段"},{"IP-CIDR6","目标 IPv6 网段"},{"SRC-IP-CIDR","来源 IP 网段"},{"SRC-PORT","来源端口"},{"DST-PORT","目标端口"},{"PROCESS-NAME","进程名称"},{"PROCESS-PATH","进程路径"},{"GEOIP","IP 地区"},{"GEOSITE","站点分类"},{"RULE-SET","规则集"},{"MATCH","其余流量"}};
    return labels.contains(type)?labels.value(type)+" · "+type:type;
}
QStringList RuleDraft::fields(const QString& rule) { const auto parts=rule.split(','); const bool match=parts.value(0)=="MATCH"; return {parts.value(0),match?QString():parts.value(1),parts.value(match?1:2),parts.value(match?2:3)}; }
QString RuleDraft::compose(const QStringList& fields) { QStringList value; for (const auto& field:fields) value.append(field.trimmed()); return value.value(0)+','+(value.value(0)=="MATCH"?QString():value.value(1)+',')+value.value(2)+(value.value(3).isEmpty()?QString():','+value.value(3)); }
QJsonObject RuleDraft::snapshot(const Profile& profile) { return {{"candidate",profile.candidate},{"active",profile.active},{"provider_edits",profile.ruleProviderEdits},{"prepend",QJsonArray::fromStringList(profile.customRules)},{"append",QJsonArray::fromStringList(profile.appendRules)},{"disabled",QJsonArray::fromStringList(profile.disabledRules)}}; }
Result RuleDraft::check(const QStringList& rules,const QSet<QString>& targets,const QSet<QString>& providers,bool references,const QString& location) {
    const auto bad=[&](int row,int column,const QString& code,const QString& reason,const QString& suggestion) { const auto path=location+'['+QString::number(row)+']'; return Result{false,path+"："+reason+"。"+suggestion,{{"row",row+1},{"column",column},{"code",code},{"path",path},{"reason",reason},{"suggestion",suggestion}}}; };
    if (rules.size()>256) return bad(256,0,"RULE_LIMIT","规则超过256条","减少规则条数");
    for (int row=0;row<rules.size();++row) {
        const auto text=rules[row].trimmed(); const auto parts=text.split(','); const auto type=parts.value(0); const bool match=type=="MATCH"; const int count=match?2:3;
        if (text.size()>1024||text.contains('\n')||text.contains('\r')||text.contains(QChar::Null)||!types().contains(type)||(parts.size()!=count&&parts.size()!=count+1)) return bad(row,0,"CUSTOM_RULE","规则类型或字段数量无效","选择受支持类型，并检查逗号分隔字段");
        if (!match&&parts.value(1).trimmed().isEmpty()) return bad(row,1,"CUSTOM_RULE","匹配内容为空","填写域名、网段、端口或进程");
        const auto target=parts.value(count-1).trimmed(); if (target.isEmpty()||(references&&!targets.contains(target))) return bad(row,2,"CUSTOM_RULE","目标策略无效","选择该订阅中的实际节点或策略组");
        if (parts.size()==count+1&&(parts.last()!="no-resolve"||!QStringList{"IP-CIDR","IP-CIDR6","SRC-IP-CIDR","GEOIP","RULE-SET"}.contains(type))) return bad(row,3,"RULE_OPTION","此规则不支持该选项","仅受支持的 IP/Geo/规则集类型可使用 no-resolve");
        if (references&&type=="RULE-SET"&&!providers.contains(parts.value(1))) return bad(row,1,"RULE_SET_REFERENCE","规则集未声明","选择已声明的 rule-provider 名称");
    }
    return {};
}
}
