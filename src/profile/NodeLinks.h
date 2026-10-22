#pragma once
#include "app/Types.h"

namespace mlash {
// Private editor result: definitions/yaml contain credentials. Never log this data.
class NodeLinks {
public:
    static bool acceptsScheme(const QString& scheme);
    static Result parse(const QString& text,const QStringList& reserved={});
};
}
