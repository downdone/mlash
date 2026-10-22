#include "diagnostics/ProcessMetrics.h"
#include <QSysInfo>
namespace mlash {
Result processMemory(qint64) { return Result::fail("Unsupported: Windows 内存测量"); }
Result processMetrics(qint64) { return Result::fail("Unsupported: Windows 资源测量"); }
QJsonObject measurementEnvironment() { return {{"os",QSysInfo::prettyProductName()},{"controlled_vm_detected",false}}; }
}
