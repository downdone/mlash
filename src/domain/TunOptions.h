#pragma once
#include "domain/Types.h"

namespace mlash {
// Pure TUN option schema; no process, service, or privileged OS access.
bool validTunOptions(const Settings& settings);
Result normalizeTunRoutes(const QString& text);
QJsonObject tunOptionsJson(const Settings& settings);
}
