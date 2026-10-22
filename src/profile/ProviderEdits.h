#pragma once
#include "app/Types.h"
#include <yaml-cpp/yaml.h>

namespace mlash {
Result validateRuleProviderEdits(const QJsonObject& edits);
Result applyRuleProviderEdits(YAML::Node& config,const QJsonObject& edits);
QString providerSnapshotId(const QString& kind,const QString& name);
QString providerSourceSignature(const QJsonObject& provider);
}
