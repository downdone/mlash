#pragma once
#include "app/Types.h"
#include <QByteArray>

namespace mlash {
Result sealPortable(const QByteArray& plain, const QString& password, QByteArray& encrypted);
Result openPortable(const QByteArray& encrypted, const QString& password, QByteArray& plain);
}
