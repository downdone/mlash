#pragma once
#include "domain/Types.h"
#include <functional>

namespace mlash::tunNative {
inline constexpr auto WintunSha256 = "e5da8447dc2c320edc0fc52fa01885c103de8c118481f683643cacc3220dafce";
bool recoverWireGuard(const QString& root);
Result runWireGuard(const QString& root, QByteArray payload,
    const std::function<bool()>& connected,
    const std::function<bool(Result)>& reply,
    const std::function<QString()>& next);
}
