#include "diagnostics/ProcessMetrics.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <QSysInfo>
namespace mlash {
Result processMemory(qint64 pid) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
    if (!process) return Result::fail("无法读取内核内存");
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb=sizeof(memory);
    const bool ok=GetProcessMemoryInfo(process,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)); CloseHandle(process);
    return ok?Result{true,{},{{"working_set",double(memory.WorkingSetSize)},{"private_bytes",double(memory.PrivateUsage)}}}:Result::fail("内核内存读取失败");
}
Result processMetrics(qint64 pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, DWORD(pid));
    if (!process) return Result::fail("无法读取测量进程");
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb = sizeof(memory); DWORD handles = 0;
    FILETIME created{}, exited{}, kernel{}, user{};
    const bool ok = GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) && GetProcessHandleCount(process,&handles) && GetProcessTimes(process,&created,&exited,&kernel,&user);
    CloseHandle(process); if (!ok) return Result::fail("进程资源采样失败");
    int threads = 0; HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if (snapshot == INVALID_HANDLE_VALUE) return Result::fail("线程资源采样失败");
    THREADENTRY32 entry{}; entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot,&entry)) do { if (entry.th32OwnerProcessID == DWORD(pid)) ++threads; } while (Thread32Next(snapshot,&entry)); CloseHandle(snapshot);
    const auto ticks = [](FILETIME value) { return (quint64(value.dwHighDateTime)<<32)|value.dwLowDateTime; };
    return {true,{},{{"pid",double(pid)},{"private_bytes",double(memory.PrivateUsage)},{"working_set",double(memory.WorkingSetSize)},{"cpu_seconds",double(ticks(kernel)+ticks(user))/1e7},{"handles",int(handles)},{"threads",threads}}};
}
QJsonObject measurementEnvironment() {
    SYSTEM_INFO info{}; GetNativeSystemInfo(&info);
    auto bios = [](const wchar_t* name) { wchar_t value[512]{}; DWORD bytes=sizeof(value); const auto code=RegGetValueW(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\BIOS",name,RRF_RT_REG_SZ,nullptr,value,&bytes); return code==ERROR_SUCCESS ? QString::fromWCharArray(value) : QString(); };
    const auto maker=bios(L"SystemManufacturer"), model=bios(L"SystemProductName");
    const auto descriptor=(maker+' '+model).toLower();
    const bool vm=descriptor.contains("vmware") || descriptor.contains("virtualbox") || descriptor.contains("virtual machine") || descriptor.contains("qemu") || descriptor.contains("kvm") || descriptor.contains("parallels");
    return {{"os",QSysInfo::prettyProductName()},{"architecture",QSysInfo::currentCpuArchitecture()},{"logical_processors",int(info.dwNumberOfProcessors)},{"manufacturer",maker},{"model",model},{"controlled_vm_detected",vm}};
}
}
