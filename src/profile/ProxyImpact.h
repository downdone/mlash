#pragma once
#include "app/Types.h"
#include <yaml-cpp/yaml.h>

namespace mlash {
// Worker only. Source and effective config have already passed ConfigBuilder.
// The draft may contain invalid individual definitions; report that boundary
// rather than requiring the very object being deleted to be valid.
Result inspectProxyReferences(const YAML::Node& source,const YAML::Node& effective,
                              const QJsonObject& draft,const QString& name);
}
