#pragma once
#include "domain/Types.h"

namespace mlash {

// Observations retain the transport error and source object for existing
// snapshots. Consumers must read the validated fields for decisions.
template <typename T> struct CoreObservation {
    Result result;
    T value;
    bool ok() const { return result.ok; }
};

struct CoreVersionInfo { QString version; };
struct CoreConfigInfo {
    int mixedPort = 0;
    QString mode;
    bool tunEnabled = false;
};
struct CoreProxyGroups { QJsonObject groups; };

// Mihomo wire mapping lives in core, never in the Controller or UI.
CoreObservation<CoreVersionInfo> parseMihomoVersion(Result response);
CoreObservation<CoreConfigInfo> parseMihomoConfig(Result response);
CoreObservation<CoreProxyGroups> parseMihomoProxies(Result response);

} // namespace mlash
