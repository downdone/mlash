#pragma once
#include "app/Types.h"
#include <yaml-cpp/yaml.h>
namespace mlash {
Result validateProxyEdits(const QJsonObject& edits);
// Worker only. Upsert by immutable name, then validate the final reference graph.
Result applyProxyEdits(YAML::Node& config,const QJsonObject& edits);
}
