#include "WebsiteTargets.h"
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace mlash {
Result validateWebsiteTargets(const QJsonArray& targets) {
    if (targets.size() > 24) return Result::fail("测速目标最多 24 个");
    QSet<QString> ids; static const QRegularExpression idPattern("^[a-z0-9-]{1,64}$");
    for (int i = 0; i < targets.size(); ++i) {
        const auto item = targets[i].toObject(); const auto id = item.value("id").toString(), name = item.value("name").toString(), address = item.value("url").toString(); const QUrl url(address,QUrl::StrictMode);
        if (!targets[i].isObject() || !idPattern.match(id).hasMatch() || ids.contains(id) || name.trimmed().isEmpty() || name.size()>64 || name.contains(QChar::LineFeed) || name.contains(QChar::CarriageReturn) || address.size()>2048 || !url.isValid() || url.scheme()!="https" || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment() || (item.contains("mark") && (!item.value("mark").isString() || item.value("mark").toString().size()>8))) return Result::fail(QString("第 %1 个目标无效：名称 1–64 字符，HTTPS 地址不得含用户凭据/片段，目标标识不得重复").arg(i+1));
        ids.insert(id);
    }
    return {};
}
}
