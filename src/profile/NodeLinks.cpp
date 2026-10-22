#include "NodeLinks.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QUrlQuery>
#include <yaml-cpp/yaml.h>

namespace mlash {
namespace {
struct Invalid { QString reason; };
[[noreturn]] void bad(const QString& reason) { throw Invalid{reason}; }
bool clean(const QString& value,int limit=2048) {
    if (value.size()>limit) return false;
    for (const auto c:value) if (c.isNull()||c.category()==QChar::Other_Control) return false;
    return true;
}
QString utf8(const QByteArray& bytes) {
    QStringDecoder decoder(QStringDecoder::Utf8); const QString text=decoder(bytes);
    if (decoder.hasError()) bad("编码不是有效 UTF-8"); return text;
}
QString unescape(const QString& value) {
    for (int i=0;i<value.size();++i) if (value[i]=='%') {
        if (i+2>=value.size()||!QString("0123456789abcdefABCDEF").contains(value[i+1])||!QString("0123456789abcdefABCDEF").contains(value[i+2])) bad("百分号编码无效"); i+=2;
    }
    const auto text=utf8(QByteArray::fromPercentEncoding(value.toUtf8())); if (!clean(text,8192)) bad("包含控制字符或字段过长"); return text;
}
QString base64(const QString& value) {
    if (value.isEmpty()||value.size()>8192||!QRegularExpression("^[A-Za-z0-9+/_-]+={0,2}$").match(value).hasMatch()) bad("Base64 编码无效");
    const bool url=value.contains('-')||value.contains('_'); if (url&&(value.contains('+')||value.contains('/'))) bad("Base64 编码混用");
    auto bytes=value.toLatin1(); if (bytes.size()%4==1) bad("Base64 长度无效"); while (bytes.size()%4) bytes+='=';
    const auto decoded=QByteArray::fromBase64Encoding(bytes,QByteArray::AbortOnBase64DecodingErrors|(url?QByteArray::Base64UrlEncoding:QByteArray::Base64Encoding));
    if (!decoded) bad("Base64 编码无效"); return utf8(decoded.decoded);
}
QString scalar(const QJsonValue& value) {
    if (value.isUndefined()||value.isNull()) return {};
    if (value.isString()) return value.toString();
    if (value.isDouble()&&value.toDouble()==value.toInt(-1)&&value.toInt(-1)>=0) return QString::number(value.toInt());
    bad("字段类型无效");
}
bool boolean(const QString& value) {
    if (value.isEmpty()||value=="1"||value.compare("true",Qt::CaseInsensitive)==0) return true;
    if (value=="0"||value.compare("false",Qt::CaseInsensitive)==0) return false;
    bad("布尔选项须为 0/1 或 true/false");
}
void uuid(const QString& value) {
    static const QRegularExpression syntax("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    if (!syntax.match(value).hasMatch()) bad("UUID 格式无效");
}
struct Parameters {
    QMap<QString,QString> values;
    QString take(const QStringList& aliases,const QString& fallback={}) {
        QString result=fallback; bool found=false;
        for (const auto& key:aliases) if (values.contains(key)) { if (found) bad("同一选项存在重复别名"); result=values.take(key); found=true; }
        return result;
    }
    bool has(const QStringList& keys) const { for (const auto& key:keys) if (values.contains(key)) return true; return false; }
};
void tlsAndTransport(QJsonObject& node,Parameters& p,const QString& protocol) {
    const bool explicitTls=protocol=="vless"||protocol=="vmess";
    const auto security=p.take({"security"},explicitTls?"none":"tls");
    if (!QStringList{"none","tls","reality"}.contains(security)||(security=="reality"&&protocol!="vless")||(!explicitTls&&security!="tls")) bad("不支持此 TLS 安全模式");
    if (explicitTls) node["tls"]=security!="none";
    const auto sni=p.take({"sni","peer","servername"});
    if (!sni.isEmpty()) { if (security=="none") bad("未开启 TLS 却填写 SNI"); node[explicitTls?"servername":"sni"]=sni; }
    const bool insecure=p.has({"insecure","allowinsecure","allow_insecure"});
    if (insecure) { const bool skip=boolean(p.take({"insecure","allowinsecure","allow_insecure"})); if (skip&&security=="none") bad("非 TLS 节点不支持证书选项"); node["skip-cert-verify"]=skip; }
    const auto fingerprint=p.take({"fp","client-fingerprint"});
    if (!fingerprint.isEmpty()) { if (security=="none"||protocol=="tuic"||protocol=="hysteria2") bad("此节点不支持客户端 TLS 指纹"); node["client-fingerprint"]=fingerprint; }
    const auto certificate=p.take({"pinsha256"}); if (!certificate.isEmpty()) { if (security=="none") bad("未开启 TLS 却填写证书指纹"); node["fingerprint"]=certificate; }
    const auto alpn=p.take({"alpn"}); if (!alpn.isEmpty()) {
        if (security=="none") bad("未开启 TLS 却填写 ALPN"); QJsonArray protocols;
        for (const auto& entry:alpn.split(',')) { const auto token=entry.trimmed(); if (token.isEmpty()||token.size()>64||protocols.size()>=8) bad("ALPN 列表无效"); protocols.append(token); } node["alpn"]=protocols;
    }
    const auto publicKey=p.take({"pbk"}),shortId=p.take({"sid"});
    if (security=="reality") {
        if (!QRegularExpression("^[A-Za-z0-9_-]{43}$").match(publicKey).hasMatch()||!QRegularExpression("^(?:[0-9a-fA-F]{2}){0,8}$").match(shortId).hasMatch()) bad("Reality 公钥或 short-id 无效");
        node["reality-opts"]=QJsonObject{{"public-key",publicKey},{"short-id",shortId}};
    } else if (!publicKey.isEmpty()||!shortId.isEmpty()) bad("Reality 参数缺少对应安全模式");
    const auto flow=p.take({"flow"}); if (!flow.isEmpty()) { if (protocol!="vless"||security=="none"||!QStringList{"xtls-rprx-vision","xtls-rprx-vision-udp443"}.contains(flow)) bad("不支持此 flow"); node["flow"]=flow; }
    const auto network=p.take({"type","net"},"tcp"),path=p.take({"path"}),host=p.take({"host"}),service=p.take({"servicename","service-name"}),mode=p.take({"mode"});
    const bool stream=QStringList{"vless","vmess","trojan"}.contains(protocol);
    if (!QStringList{"tcp","ws","grpc"}.contains(network)||(!stream&&network!="tcp")) bad("不支持此传输，请使用完整 YAML");
    if (network=="ws") {
        if (!service.isEmpty()||!mode.isEmpty()||(!path.isEmpty()&&!path.startsWith('/'))) bad("WebSocket 选项无效");
        QJsonObject options{{"path",path.isEmpty()?"/":path}}; if (!host.isEmpty()) options["headers"]=QJsonObject{{"Host",host}}; node["network"]="ws"; node["ws-opts"]=options;
    } else if (network=="grpc") {
        if (!host.isEmpty()||(!path.isEmpty()&&!service.isEmpty())||(!mode.isEmpty()&&mode!="gun")) bad("gRPC 选项有冲突或不受支持");
        node["network"]="grpc"; node["grpc-opts"]=QJsonObject{{"grpc-service-name",service.isEmpty()?path:service}};
    } else if (!path.isEmpty()||!host.isEmpty()||!service.isEmpty()||!mode.isEmpty()) bad("TCP 节点包含其他传输选项");
}
QJsonObject one(QString line,QString& name) {
    const auto scheme=line.section(':',0,0).toLower(); QString protocol=scheme=="hy2"?"hysteria2":scheme;
    if (!NodeLinks::acceptsScheme(scheme)) bad("不支持此链接协议，请使用完整 YAML");
    if (protocol=="vmess") {
        const auto body=line.mid(line.indexOf("://")+3); if (!line.contains("://")) bad("缺少链接协议分隔符");
        const auto decoded=base64(body.section('#',0,0)); QJsonParseError error; const auto doc=QJsonDocument::fromJson(decoded.toUtf8(),&error);
        if (error.error!=QJsonParseError::NoError||!doc.isObject()) bad("VMess 内容不是有效 JSON 对象");
        const QSet<QString> allowed{"v","ps","add","port","id","aid","scy","net","type","host","path","tls","sni","alpn","fp"}; QSet<QString> seen;
        for (const auto& pair:YAML::Load(decoded.toStdString())) { const auto key=QString::fromStdString(pair.first.as<std::string>()); if (!allowed.contains(key)||seen.contains(key)) bad("VMess 含不支持或重复字段"); seen.insert(key); }
        const auto o=doc.object(); const auto get=[&](const char* key) { const auto value=scalar(o.value(key)); if (!clean(value)) bad("VMess 字段过长或含控制字符"); return value; };
        const auto version=get("v"); if (!version.isEmpty()&&version!="2") bad("不支持此 VMess 分享版本");
        const auto server=get("add"); bool ok=false; const int port=get("port").toInt(&ok); QUrl endpoint; endpoint.setScheme("vmess"); endpoint.setHost(server); endpoint.setPort(port);
        if (!ok||port<1||port>65535||server.isEmpty()||server.size()>253||!endpoint.isValid()||endpoint.host().isEmpty()||server.contains(' ')||server.contains('%')) bad("VMess 服务器或端口无效");
        const auto id=get("id"); uuid(id); QJsonObject node{{"type","vmess"},{"server",server},{"port",port},{"uuid",id}};
        const auto aid=get("aid"); if (!aid.isEmpty()) { const int n=aid.toInt(&ok); if (!ok||n<0||n>65535) bad("VMess alterId 无效"); node["alterId"]=n; }
        const auto cipher=get("scy"); if (!cipher.isEmpty()) node["cipher"]=cipher;
        const auto header=get("type"); if (!header.isEmpty()&&header!="none") bad("VMess TCP 伪装暂不转换，请使用完整 YAML");
        const auto tls=o.value("tls").isBool()?(o.value("tls").toBool()?QString("tls"):QString("none")):get("tls");
        const auto security=QStringList{"tls","1","true"}.contains(tls)?QString("tls"):QStringList{"","none","0","false"}.contains(tls)?QString("none"):QString(); if (security.isEmpty()) bad("VMess TLS 模式无效");
        Parameters p; p.values={{"security",security.isEmpty()?"none":security},{"type",get("net").isEmpty()?"tcp":get("net")},{"host",get("host")},{"path",get("path")},{"sni",get("sni")},{"alpn",get("alpn")},{"fp",get("fp")}}; tlsAndTransport(node,p,"vmess");
        name=body.contains('#')?unescape(body.section('#',1)):get("ps"); return node;
    }
    if (protocol=="ss") {
        const auto body=line.mid(5); if (!body.section('#',0,0).section('?',0,0).contains('@')) {
            const auto decoded=base64(body.section('#',0,0)); const int at=decoded.lastIndexOf('@'),colon=decoded.indexOf(':');
            if (at<0||colon<1||colon>=at) bad("Shadowsocks 认证信息无效");
            QUrl legacy("ss://"+decoded.mid(at+1),QUrl::StrictMode); legacy.setUserName(decoded.left(colon)); legacy.setPassword(decoded.mid(colon+1,at-colon-1)); if (body.contains('#')) legacy.setFragment(unescape(body.section('#',1))); line=legacy.toString(QUrl::FullyEncoded);
        }
    }
    // Validate encoding before QUrl normalizes it; do not silently repair malformed escapes.
    unescape(line); const QUrl url(line,QUrl::StrictMode); int port=url.port(-1);
    if (port<0&&QStringList{"hysteria2","https"}.contains(protocol)) port=443;
    const auto host=url.host(QUrl::FullyDecoded);
    if (!url.isValid()||host.isEmpty()||host.size()>253||host.contains('%')||port<1||port>65535||(!url.path().isEmpty()&&url.path()!="/")) bad("服务器、端口或链接路径无效");
    QJsonObject node{{"type",protocol=="https"?"http":protocol},{"server",host},{"port",port}};
    Parameters p; for (const auto& pair:QUrlQuery(url).queryItems(QUrl::FullyDecoded)) { const auto key=pair.first.toLower(); if (p.values.contains(key)||!clean(pair.second)) bad("查询参数重复、过长或含控制字符"); p.values[key]=pair.second; }
    auto auth=unescape(url.userInfo(QUrl::FullyEncoded)); if (!clean(auth)) bad("认证信息过长或包含控制字符"); if (auth.isEmpty()&&protocol!="http"&&protocol!="https"&&protocol!="socks5") bad("缺少认证信息");
    if (protocol=="ss") {
        if (!auth.contains(':')) auth=base64(auth); const int colon=auth.indexOf(':'); if (colon<1||auth.mid(colon+1).isEmpty()||!clean(auth)) bad("Shadowsocks 认证信息无效");
        node["cipher"]=auth.left(colon); node["password"]=auth.mid(colon+1); if (!p.take({"plugin"}).isEmpty()) bad("插件链接暂不转换，请使用完整 YAML");
    } else if (protocol=="vless") {
        uuid(auth); node["uuid"]=auth; const auto encryption=p.take({"encryption"},"none"); if (encryption!="none") bad("不支持此 VLESS encryption");
        const auto encoding=p.take({"packetencoding","packet-encoding"}); if (!encoding.isEmpty()) { if (!QStringList{"xudp","packetaddr"}.contains(encoding)) bad("不支持此 UDP 编码"); node["packet-encoding"]=encoding; } tlsAndTransport(node,p,protocol);
    } else if (QStringList{"trojan","hysteria2","anytls"}.contains(protocol)) {
        node["password"]=auth; tlsAndTransport(node,p,protocol);
        if (protocol=="anytls") node["udp"]=true;
        if (protocol=="hysteria2") {
            const auto obfs=p.take({"obfs"}),password=p.take({"obfs-password"}); if (!obfs.isEmpty()&&obfs!="none"&&obfs!="salamander") bad("不支持此 Hysteria2 混淆");
            if (obfs=="salamander") { if (password.isEmpty()) bad("缺少 Hysteria2 混淆密码"); node["obfs"]=obfs; node["obfs-password"]=password; } else if (!password.isEmpty()) bad("混淆密码缺少对应模式");
            const auto ports=p.take({"mport"}); if (!ports.isEmpty()) node["ports"]=ports; if (p.has({"fastopen"})) node["tfo"]=boolean(p.take({"fastopen"}));
        }
    } else if (protocol=="tuic") {
        uuid(url.userName(QUrl::FullyDecoded)); if (url.password(QUrl::FullyDecoded).isEmpty()) bad("缺少 TUIC 密码"); node["uuid"]=url.userName(QUrl::FullyDecoded); node["password"]=url.password(QUrl::FullyDecoded); tlsAndTransport(node,p,protocol);
        for (const auto& pair:QList<QPair<QString,QString>>{{"congestion_control","congestion-controller"},{"udp_relay_mode","udp-relay-mode"}}) { const auto value=p.take({pair.first,pair.second}); if (!value.isEmpty()) node[pair.second]=value; }
        for (const auto& pair:QList<QPair<QString,QString>>{{"disable_sni","disable-sni"},{"reduce_rtt","reduce-rtt"}}) if (p.has({pair.first,pair.second})) node[pair.second]=boolean(p.take({pair.first,pair.second}));
    } else {
        if (!auth.isEmpty()) { node["username"]=url.userName(QUrl::FullyDecoded); node["password"]=url.password(QUrl::FullyDecoded); }
        if (protocol=="https") node["tls"]=true;
    }
    if (QStringList{"ss","trojan","vless","anytls","socks5"}.contains(protocol)&&p.has({"udp"})) node["udp"]=boolean(p.take({"udp"}));
    if (!p.values.isEmpty()) bad("包含尚不支持的参数，请使用完整 YAML；未忽略任何参数");
    name=url.fragment(QUrl::FullyDecoded); return node;
}
YAML::Node yamlValue(const QJsonValue& value) {
    if (value.isObject()) { YAML::Node n(YAML::NodeType::Map); const auto object=value.toObject(); for (auto it=object.begin();it!=object.end();++it) n[it.key().toStdString()]=yamlValue(it.value()); return n; }
    if (value.isArray()) { YAML::Node n(YAML::NodeType::Sequence); for (const auto& item:value.toArray()) n.push_back(yamlValue(item)); return n; }
    if (value.isBool()) return YAML::Node(value.toBool()); if (value.isDouble()) return YAML::Node(value.toInt()); return YAML::Node(value.toString().toStdString());
}
}
bool NodeLinks::acceptsScheme(const QString& scheme) { return QStringList{"ss","vmess","vless","trojan","hysteria2","hy2","tuic","anytls","http","https","socks5"}.contains(scheme.toLower()); }
Result NodeLinks::parse(const QString& text,const QStringList& reserved) {
    if (text.toUtf8().size()>64*1024||text.contains(QChar::Null)||reserved.size()>20000) return Result::fail("节点链接超过 64KiB 或包含无效字符");
    QSet<QString> names{"DIRECT","REJECT","REJECT-DROP","PASS","COMPATIBLE","GLOBAL","节点选择"}; for (const auto& name:reserved) names.insert(name);
    QJsonObject definitions; QJsonArray items; YAML::Node proxies(YAML::NodeType::Sequence),members(YAML::NodeType::Sequence); int lineNumber=0;
    for (const auto& input:text.split('\n')) {
        ++lineNumber; const auto line=input.trimmed(); if (line.isEmpty()) continue;
        const auto fail=[&](const QString& reason) { return Result{false,QString("第 %1 行：%2").arg(lineNumber).arg(reason),{{"line",lineNumber},{"code","NODE_LINK"}}}; };
        if (items.size()>=64||line.size()>8192) return fail("最多 64 个节点，每条链接最多 8192 字符");
        try {
            QString name; auto node=one(line,name); if (!clean(name,256)) bad("名称过长或包含控制字符"); name=name.trimmed(); if (name.isEmpty()) name=node.value("type").toString()+"-"+node.value("server").toString()+"-"+QString::number(node.value("port").toInt());
            if (name.size()>256) bad("名称过长"); const auto original=name; for (int suffix=2;names.contains(name);++suffix) name=original.left(246)+QString(" (%1)").arg(suffix); names.insert(name); node["name"]=name;
            const auto definition=yamlValue(node); definitions[name]=QString::fromStdString(YAML::Dump(definition)); proxies.push_back(definition); members.push_back(name.toStdString());
            items.append(QJsonObject{{"name",name},{"type",node.value("type")},{"server",node.value("server")},{"port",node.value("port")}});
        } catch (const Invalid& error) { return fail(error.reason); } catch (const std::exception&) { return fail("链接内容无法转换，请检查格式或使用完整 YAML"); }
    }
    if (items.isEmpty()) return Result::fail("请粘贴至少一个节点链接，每行一个");
    YAML::Node config; config["proxies"]=proxies; YAML::Node group; group["name"]="节点选择"; group["type"]="select"; group["proxies"]=members; config["proxy-groups"].push_back(group); config["rules"].push_back("MATCH,节点选择");
    return {true,{},{{"items",items},{"definitions",definitions},{"yaml",QString::fromStdString(YAML::Dump(config))}}};
}
}
