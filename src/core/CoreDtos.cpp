#include "CoreDtos.h"
#include <cmath>

namespace mlash {

CoreObservation<CoreVersionInfo> parseMihomoVersion(Result response) {
    CoreObservation<CoreVersionInfo> observed{std::move(response), {}};
    if (!observed.ok()) return observed;
    const auto value = observed.result.data.value("version");
    if (!value.isString() || value.toString().trimmed().isEmpty()) {
        observed.result = Result::fail("内核版本响应缺少有效版本");
        return observed;
    }
    observed.value.version = value.toString();
    return observed;
}

CoreObservation<CoreConfigInfo> parseMihomoConfig(Result response) {
    CoreObservation<CoreConfigInfo> observed{std::move(response), {}};
    if (!observed.ok()) return observed;
    const auto port = observed.result.data.value("mixed-port");
    const auto mode = observed.result.data.value("mode");
    const auto tun = observed.result.data.value("tun");
    if (!port.isDouble() || !std::isfinite(port.toDouble()) || std::floor(port.toDouble()) != port.toDouble() ||
        port.toDouble() < 1 || port.toDouble() > 65535 ||
        !mode.isString() || mode.toString().trimmed().isEmpty() ||
        (!tun.isUndefined() && !tun.isObject()) ||
        (tun.isObject() && !tun.toObject().value("enable").isBool())) {
        observed.result = Result::fail("内核运行配置响应字段无效");
        return observed;
    }
    observed.value.mixedPort = port.toInt();
    observed.value.mode = mode.toString().toLower();
    observed.value.tunEnabled = tun.toObject().value("enable").toBool();
    return observed;
}

CoreObservation<CoreProxyGroups> parseMihomoProxies(Result response) {
    CoreObservation<CoreProxyGroups> observed{std::move(response), {}};
    if (!observed.ok()) return observed;
    const auto groups = observed.result.data.value("proxies");
    if (!groups.isObject()) {
        observed.result = Result::fail("内核代理组响应字段无效");
        return observed;
    }
    observed.value.groups = groups.toObject();
    return observed;
}

} // namespace mlash
