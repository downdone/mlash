#pragma once
#include "app/Types.h"
namespace mlash {
// Validate frozen bytes before they reach the core's allocation-based reader.
Result inspectMrs(const QByteArray& compressed, const QString& expectedBehavior = {});
}
