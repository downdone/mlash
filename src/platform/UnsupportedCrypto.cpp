#include "PortableCrypto.h"
namespace mlash {
Result sealPortable(const QByteArray&, const QString&, QByteArray& output) { output.clear(); return Result::fail("Unsupported：当前平台未实现可移植备份加密"); }
Result openPortable(const QByteArray&, const QString&, QByteArray& output) { output.clear(); return Result::fail("Unsupported：当前平台未实现可移植备份解密"); }
}
