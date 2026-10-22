#include "ProtocolLink.h"
#include <QUrl>
#include <QUrlQuery>
#include <QSet>
namespace mlash {
Result parseProtocolLink(const QString& link) {
    if (link.size()>8192||link.toUtf8().size()>16384||link.contains(QChar::Null)) return Result::fail("Mlash导入链接超过限制"); const QUrl url(link,QUrl::StrictMode);
    if (!url.isValid()||url.scheme()!="mlash"||url.host()!="import"||(!url.path().isEmpty()&&url.path()!="/")||!url.userInfo().isEmpty()||url.port()!=-1||url.hasFragment()) return Result::fail("仅支持 mlash://import?url=… 的订阅确认链接");
    const auto items=QUrlQuery(url).queryItems(QUrl::FullyDecoded); QSet<QString> seen; QString source,name;
    for (const auto& item:items) { if (seen.contains(item.first)||!QStringList{"url","name"}.contains(item.first)) return Result::fail("导入链接含重复或未知参数"); seen.insert(item.first); if (item.first=="url") source=item.second; else name=item.second; }
    const QUrl target(source,QUrl::StrictMode); if (source.size()>4096||!target.isValid()||target.scheme()!="https"||target.host().isEmpty()||!target.userInfo().isEmpty()||target.hasFragment()) return Result::fail("链接中的订阅地址须为完整HTTPS地址，不含凭据或片段");
    if (name.size()>120||name.trimmed()!=name) return Result::fail("导入链接名称无效"); for (const auto ch:name+source) if (ch.isNull()||ch.category()==QChar::Other_Control) return Result::fail("导入链接含控制字符");
    return {true,{},{{"url",source},{"name",name},{"host",target.host()}}};
}
}
