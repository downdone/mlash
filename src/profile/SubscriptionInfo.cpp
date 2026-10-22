#include "SubscriptionInfo.h"
#include <QDateTime>
#include <QRegularExpression>

namespace mlash {
namespace {
constexpr qint64 maxBytes = 9007199254740991LL;
bool number(const QString& text, qint64 maximum, qint64& value) {
    static const QRegularExpression integer("^[0-9]{1,16}$");
    bool ok = false; value = text.toLongLong(&ok);
    return integer.match(text).hasMatch() && ok && value >= 0 && value <= maximum;
}
QString bytes(qint64 value) {
    double size = value; QStringList units{"B", "KiB", "MiB", "GiB", "TiB", "PiB"}; int unit = 0;
    while (size >= 1024 && unit < units.size()-1) { size /= 1024; ++unit; }
    return QString::number(size, 'f', unit ? 1 : 0) + " " + units[unit];
}
}
QJsonObject SubscriptionInfo::parse(const QByteArray& header) {
    if (header.size() > 1024) return {};
    QJsonObject result;
    for (const auto& field : QString::fromLatin1(header).split(';', Qt::SkipEmptyParts)) {
        const auto parts = field.split('='); if (parts.size() != 2) return {};
        const auto key = parts[0].trimmed().toLower();
        if (!QStringList{"upload", "download", "total", "expire"}.contains(key)) continue;
        qint64 value = 0;
        if (result.contains(key) || !number(parts[1].trimmed(), key == "expire" ? 253402300799LL : maxBytes, value)) return {};
        result[key] = QString::number(value);
    }
    if (!result.isEmpty()) result["observed_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return result;
}
bool SubscriptionInfo::valid(const QJsonObject& info) {
    for (auto it = info.begin(); it != info.end(); ++it) {
        if (it.key() == "observed_at") { if (!it.value().isString() || !QDateTime::fromString(it.value().toString(), Qt::ISODate).isValid()) return false; continue; }
        qint64 value = 0;
        if (!QStringList{"upload", "download", "total", "expire"}.contains(it.key()) || !it.value().isString() || !number(it.value().toString(), it.key() == "expire" ? 253402300799LL : maxBytes, value)) return false;
    }
    return true;
}
QString SubscriptionInfo::summary(const QJsonObject& info) {
    if (!valid(info)) return {};
    QStringList text;
    if (info.contains("upload") && info.contains("download") && info.contains("total")) {
        const auto used = info["upload"].toString().toLongLong() + info["download"].toString().toLongLong();
        const auto total = info["total"].toString().toLongLong();
        text << (total > 0 ? "剩余 " + bytes(qMax(0LL, total-used)) + " / " + bytes(total) : "已用 " + bytes(used));
    }
    const auto expire = info["expire"].toString().toLongLong();
    if (expire > 0) text << "到期 " + QDateTime::fromSecsSinceEpoch(expire).toLocalTime().toString("yyyy-MM-dd");
    return text.join(" · ");
}
}
