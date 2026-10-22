#include "app/Types.h"
#include "diagnostics/ProcessMetrics.h"
#include <QDir>
#include <QFileInfo>
#include <windows.h>
mlash::Result terminateTestCore(qint64 pid,const QString& executable) {
    if (!mlash::measurementEnvironment().value("controlled_vm_detected").toBool()) return mlash::Result::fail("仅允许受控测试 VM");
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE,FALSE,DWORD(pid)); if (!process) return mlash::Result::fail("无法打开测试内核");
    wchar_t path[32768]{}; DWORD length=32768; const bool identified=QueryFullProcessImageNameW(process,0,path,&length) && QFileInfo(QString::fromWCharArray(path)).canonicalFilePath().compare(QFileInfo(executable).canonicalFilePath(),Qt::CaseInsensitive)==0;
    const bool stopped=identified && TerminateProcess(process,99); CloseHandle(process); return stopped?mlash::Result{}:mlash::Result::fail("测试内核身份不匹配或无法终止");
}
