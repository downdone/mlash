#pragma once
#include "domain/Types.h"
namespace mlash {
Result processMetrics(qint64 pid);
Result processMemory(qint64 pid);
QJsonObject measurementEnvironment();
}
