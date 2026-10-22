#include "YamlFields.h"
#include <yaml-cpp/yaml.h>
#include <QSet>
#include <QHash>
#include <stdexcept>
namespace mlash {
QVector<YamlField> yamlFields(const QString& kind) {
    if (kind=="nodes") return {{"name","名称","text",{}},{"type","协议","choice",{"http","socks5","ss","trojan","vless","vmess","hysteria2","tuic","anytls"}},{"server","服务器","text",{}},{"port","端口","integer",{}},{"username","用户名","text",{}},{"password","密码","secret",{}},{"uuid","UUID","secret",{}},{"cipher","加密方式","text",{}},{"tls","TLS","bool",{}},{"servername","TLS服务器名 · servername","text",{}},{"sni","TLS服务器名 · sni","text",{}},{"skip-cert-verify","跳过证书验证","bool",{}},{"udp","UDP","bool",{}}};
    if (kind=="groups") return {{"name","名称","text",{}},{"type","组类型","choice",{"select","url-test","fallback","load-balance"}},{"proxies","成员（每行一个，按顺序）","list",{}},{"use","Provider（每行一个）","list",{}},{"url","测试地址","text",{}},{"interval","测试周期（秒）","integer",{}},{"tolerance","容差（毫秒）","integer",{}},{"lazy","按需测试","bool",{}}};
    if (kind=="dns") return {{"nameserver","主上游（每行一个）","list",{}},{"enhanced-mode","解析模式","choice",{"fake-ip","redir-host"}},{"ipv6","IPv6解析","bool",{}},{"enable","启用内核DNS","bool",{}},{"fallback","回退上游（每行一个）","list",{}},{"default-nameserver","默认解析上游（每行一个）","list",{}},{"proxy-server-nameserver","节点域名解析上游","list",{}},{"respect-rules","上游连接遵循规则","bool",{}}};
    return {};
}
namespace {
void budget(const YAML::Node& node,int depth,int& count) {
    if (depth>32||++count>5000) throw std::runtime_error("budget"); const auto tag=node.Tag(); if (!tag.empty()&&tag!="?"&&tag!="!"&&!tag.starts_with("tag:yaml.org,2002:")) throw std::runtime_error("tag");
    if (node.IsMap()) { QSet<QString> seen; for (const auto& entry:node) { if (!entry.first.IsScalar()) throw std::runtime_error("key"); const auto key=QString::fromStdString(entry.first.Scalar()); if (key=="<<"||seen.contains(key)) throw std::runtime_error("key"); seen.insert(key); budget(entry.second,depth+1,count); } } else if (node.IsSequence()) for (const auto& entry:node) budget(entry,depth+1,count);
}
YAML::Node parse(const QString& text) { if (text.size()>64*1024||text.toUtf8().size()>64*1024||text.contains(QChar::Null)) throw std::runtime_error("size"); if (text.trimmed().isEmpty()) return YAML::Node(YAML::NodeType::Map); const auto docs=YAML::LoadAll(text.toStdString()); if (docs.size()!=1||!docs[0].IsMap()) throw std::runtime_error("shape"); int count=0; budget(docs[0],0,count); return docs[0]; }
QJsonObject fields(const YAML::Node& root,const QString& kind) { QJsonObject values; QJsonArray unsupported; for (const auto& field:yamlFields(kind)) { const auto node=root[field.key.toStdString()]; if (!node) { values[field.key]=QJsonValue(QJsonValue::Null); continue; } if (field.kind=="list"&&node.IsSequence()) { QStringList lines; bool scalar=true; for (const auto& entry:node) { scalar&=entry.IsScalar(); if (entry.IsScalar()) lines.append(QString::fromStdString(entry.Scalar())); } if (scalar) { values[field.key]=lines.join('\n'); continue; } } else if (field.kind!="list"&&node.IsScalar()) { values[field.key]=QString::fromStdString(node.Scalar()); continue; } values[field.key]=QJsonObject{{"complex",true}}; unsupported.append(field.key); } return {{"values",values},{"unsupported",unsupported}}; }
Result failure(const YAML::Exception& error) { return Result::fail(QString("YAML语法或类型错误（第%1行），原文保留").arg(qMax(1,error.mark.line+1))); }
}
Result inspectYamlFields(const QString& text,const QString& kind) { try { if (yamlFields(kind).isEmpty()) return Result::fail("没有此表单类型"); return {true,{},fields(parse(text),kind)}; } catch (const YAML::Exception& error) { return failure(error); } catch (const std::exception&) { return Result::fail("草稿须为单个映射，最多64KiB/32层；不支持重复键、merge键或自定义tag"); } }
Result patchYamlFields(const QString& text,const QString& kind,const QJsonObject& changes) {
    try { auto root=parse(text); QHash<QString,YamlField> allowed; for (const auto& field:yamlFields(kind)) allowed[field.key]=field; if (changes.size()>allowed.size()) return Result::fail("表单修改字段过多");
        for (auto it=changes.begin();it!=changes.end();++it) { if (!allowed.contains(it.key())||(!it.value().isNull()&&!it.value().isString())) return Result::fail("表单字段或值无效"); const auto key=it.key().toStdString(); if (it.value().isNull()) { root.remove(key); continue; } const auto value=it.value().toString(); if (value.size()>8192||value.contains(QChar::Null)) return Result::fail("单个表单字段超过8192字符"); const auto field=allowed.value(it.key()); if (field.kind=="list") { YAML::Node list(YAML::NodeType::Sequence); for (const auto& line:value.split('\n')) if (!line.trimmed().isEmpty()) list.push_back(line.trimmed().toStdString()); root[key]=list; } else if (field.kind=="bool"&&(value=="true"||value=="false")) root[key]=value=="true"; else if (field.kind=="integer") { bool ok=false; const auto number=value.toLongLong(&ok); if (ok) root[key]=number; else root[key]=value.toStdString(); } else root[key]=value.toStdString(); }
        YAML::Emitter output; output<<root; const auto yaml=QString::fromUtf8(output.c_str()); if (!output.good()||yaml.toUtf8().size()>64*1024) return Result::fail("修改后草稿超过64KiB；未覆盖原文，请缩短字段或撤销未同步修改"); auto result=inspectYamlFields(yaml,kind); if (result.ok) result.data["yaml"]=yaml; return result;
    } catch (const YAML::Exception& error) { return failure(error); } catch (const std::exception&) { return Result::fail("草稿结构无效或超过预算；未覆盖原文"); }
}
}
