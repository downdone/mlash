#pragma once
#include "app/Types.h"
namespace mlash {
// Internal Windows adapter; production callers pass the fixed mlash class key.
Result readProtocolRegistry(const QString& path,const QString& executable);
Result writeProtocolRegistry(const QString& path,const QString& executable,bool enabled);
}
