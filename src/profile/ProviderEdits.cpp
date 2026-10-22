#include "ProviderEdits.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QUrl>

namespace mlash {
QString providerSnapshotId(const QString& kind,const QString& name) { return "providers/"+QString::fromLatin1(QCryptographicHash::hash(kind.toUtf8()+name.toUtf8(),QCryptographicHash::Sha256).toHex().left(24))+".cache"; }
QString providerSourceSignature(const QJsonObject& provider) { QJsonObject source; for (const auto* key:{"kind","name","url","format","behavior"}) source[key]=provider.value(key).toString(); return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(source).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex()); }
Result validateRuleProviderEdits(const QJsonObject& edits) {
    if (edits.size()>64||QJsonDocument(edits).toJson(QJsonDocument::Compact).size()>128*1024) return Result::fail("规则集声明最多64项/128KiB");
    for (auto it=edits.begin();it!=edits.end();++it) {
        const auto name=it.key(); if (name.trimmed()!=name||name.isEmpty()||name.size()>128||name.contains(',')||name.contains('\n')||name.contains('\r')||name.contains(QChar::Null)) return Result::fail("规则集名称须为1–128字符，不能含逗号或换行");
        if (it.value().isNull()) continue;
        if (!it.value().isObject()) return Result::fail("规则集声明必须为对象或明确移除标记"); const auto row=it.value().toObject();
        if (row.size()!=3||!row.value("url").isString()||!row.value("behavior").isString()||!row.value("format").isString()) return Result::fail("规则集声明只接受URL、行为和格式");
        const auto text=row.value("url").toString(); const QUrl url(text,QUrl::StrictMode); if (text.contains('\n')||text.contains('\r')||text.contains(QChar::Null)||text.size()>2048||!url.isValid()||url.scheme()!="https"||url.host().isEmpty()||!url.userInfo().isEmpty()||url.hasFragment()) return Result::fail("规则集需要无用户凭据和片段的HTTPS地址");
        if (!QStringList{"domain","ipcidr","classical"}.contains(row.value("behavior").toString())||!QStringList{"yaml","text","mrs"}.contains(row.value("format").toString())) return Result::fail("规则集行为或格式不受支持");
        if (row.value("format")=="mrs"&&row.value("behavior")=="classical") return Result::fail("MRS仅支持domain/ipcidr；classical请使用yaml或text");
    }
    return {};
}
Result applyRuleProviderEdits(YAML::Node& config,const QJsonObject& edits) {
    const auto valid=validateRuleProviderEdits(edits); if (!valid.ok||edits.isEmpty()) return valid;
    if (config["rule-providers"]&&!config["rule-providers"].IsMap()) return Result::fail("原始rule-providers必须为映射");
    if (!config["rule-providers"]) config["rule-providers"]=YAML::Node(YAML::NodeType::Map);
    auto providers=config["rule-providers"];
    for (auto it=edits.begin();it!=edits.end();++it) { const auto name=it.key().toStdString(); if (it.value().isNull()) { providers.remove(name); continue; } const auto row=it.value().toObject(); YAML::Node definition(YAML::NodeType::Map); definition["type"]="http"; for (const auto* key:{"url","behavior","format"}) definition[key]=row.value(key).toString().toStdString(); providers[name]=definition; }
    return {};
}
}
