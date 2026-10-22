#pragma once
#include "app/Types.h"
#include <QByteArray>
#include <QStringList>

namespace mlash {
inline constexpr auto TunCoreSha256 = "951bfd2f513ac7de486e53f2bbf1bf804e0a903bb19b237a744d11ca49023755";
inline constexpr auto TunPipeName = "Mlash.Tun.v1";
// Revalidate a frozen runtime before it crosses the privilege boundary.
// Output is regenerated; no caller-supplied listener/path is preserved unchecked.
Result prepareTunRuntime(const QByteArray& input, QByteArray& output, QStringList& files);
Result checkTunProvider(const QByteArray& config,const QString& relative,const QByteArray& bytes);
bool validTunOptions(const Settings& settings);
Result normalizeTunRoutes(const QString& text);
QJsonObject tunOptionsJson(const Settings& settings);
bool validTunFile(const QString& relative);
}
