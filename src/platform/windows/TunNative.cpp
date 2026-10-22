#include "TunNative.h"
#include "core/TunPolicy.h"
#include "storage/Storage.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <vector>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QJsonArray>

namespace mlash::tunNative {
namespace {
const wchar_t* w(const QString& s) { return reinterpret_cast<const wchar_t*>(s.utf16()); }
Result error(const QString& message) { return Result::fail(message+QString(" (%1)").arg(GetLastError())); }
QString sidOf(HANDLE token) { DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size); std::vector<BYTE> bytes(size); if (!GetTokenInformation(token,TokenUser,bytes.data(),size,&size)) return {}; LPWSTR text=nullptr; if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&text)) return {}; const auto sid=QString::fromWCharArray(text); LocalFree(text); return sid; }
bool readOwner(QString& sid) { QByteArray data; if (!readFile(installRoot()+"/owner.json",data,4096).ok) return false; const auto record=QJsonDocument::fromJson(data).object(); sid=record.value("sid").toString(); return record.value("protocol").toInt()==1&&QRegularExpression("^S-1-[0-9-]{1,180}$").match(sid).hasMatch(); }
}
QString ownerSid() { HANDLE token=nullptr; if (!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return {}; const auto sid=sidOf(token); CloseHandle(token); return sid; }
QString installRoot() { PWSTR path=nullptr; if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles,KF_FLAG_DEFAULT,nullptr,&path))) return {}; const auto result=QDir::fromNativeSeparators(QString::fromWCharArray(path))+"/MlashTun"; CoTaskMemFree(path); return result; }
Result info(bool start) {
    SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT); if (!manager) return error("无法读取 TUN 服务管理器");
    SC_HANDLE service=OpenServiceW(manager,L"MlashTun",SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|(start?SERVICE_START:0));
    if (!service) { const auto code=GetLastError(); CloseServiceHandle(manager); if (code==ERROR_SERVICE_DOES_NOT_EXIST) return {true,{},{{"installed",false},{"running",false}}}; return Result::fail(QString("无法读取 TUN 服务 (%1)").arg(code)); }
    DWORD needed=0; QueryServiceConfigW(service,nullptr,0,&needed); std::vector<BYTE> config(needed);
    const QString expected="\""+QDir::toNativeSeparators(installRoot()+"/MlashTunService.exe")+"\" --service";
    QString owner; auto result=Result{};
    if (!needed||!QueryServiceConfigW(service,reinterpret_cast<QUERY_SERVICE_CONFIGW*>(config.data()),needed,&needed)||QString::fromWCharArray(reinterpret_cast<QUERY_SERVICE_CONFIGW*>(config.data())->lpBinaryPathName).compare(expected,Qt::CaseInsensitive)!=0||!readOwner(owner)||owner!=ownerSid()) result=Result::fail("TUN 服务路径或所属账户不符，未控制该服务");
    SERVICE_STATUS_PROCESS status{}; DWORD bytes=0;
    if (result.ok&&start) {
        if (!StartServiceW(service,0,nullptr)&&GetLastError()!=ERROR_SERVICE_ALREADY_RUNNING) result=error("启动 TUN 服务失败");
        QElapsedTimer timer; timer.start();
        while (result.ok&&timer.elapsed()<5000) { if (!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<BYTE*>(&status),sizeof(status),&bytes)) { result=error("读取 TUN 服务状态失败"); break; } if (status.dwCurrentState==SERVICE_RUNNING) break; Sleep(50); }
        if (result.ok&&status.dwCurrentState!=SERVICE_RUNNING) result=Result::fail("TUN 服务未在期限内就绪");
    }
    if (result.ok&&!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<BYTE*>(&status),sizeof(status),&bytes)) result=error("读取 TUN 服务状态失败");
    if (result.ok) { result.data={{"installed",true},{"running",status.dwCurrentState==SERVICE_RUNNING},{"pid",double(status.dwProcessId)},{"protocol",1},{"recovery_marker",QFileInfo::exists(installRoot()+"/session.json")}}; QByteArray record; if (readFile(installRoot()+"/owner.json",record,4096).ok) { const auto owner=QJsonDocument::fromJson(record).object(); result.data["version"]=owner.value("version").toString("未知（旧版安装）"); result.data["advanced_options"]=owner.value("advanced_options").toBool(); } result.data["core_matches"]=false; QFile file(installRoot()+"/mihomo.exe"); if (file.open(QIODevice::ReadOnly)&&file.size()<=128*1024*1024) { QCryptographicHash hash(QCryptographicHash::Sha256); if (hash.addData(&file)) result.data["core_matches"]=hash.result().toHex()==TunCoreSha256; } }
    CloseServiceHandle(service); CloseServiceHandle(manager); return result;
}
Result verifyServer(qintptr handle) {
    ULONG pid=0; if (!GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(handle),&pid)) return error("无法核验 TUN 管道身份");
    const auto service=info(false); if (!service.ok||!service.data.value("running").toBool()||service.data.value("pid").toDouble()!=pid) return Result::fail("TUN 管道与已安装服务身份不符"); return {};
}
TunPipeOpen openPipe(const QString& name) {
    if (name!=TunPipeName) return {Result::fail("TUN 管道名称无效"),{},true};
    const auto path=QString("\\\\.\\pipe\\")+name;
    HANDLE handle=CreateFileW(w(path),FILE_READ_DATA|FILE_WRITE_DATA|FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES|SYNCHRONIZE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IMPERSONATION,nullptr);
    if (handle==INVALID_HANDLE_VALUE) return {error("TUN 服务管道尚未就绪"),{},true};
    auto pipe=std::make_shared<TunPipeHandle>(); pipe->value=reinterpret_cast<qintptr>(handle); pipe->close=[](qintptr value) { CloseHandle(reinterpret_cast<HANDLE>(value)); };
    const auto verified=verifyServer(pipe->value); if (!verified.ok) return {verified,{},true}; return {{},pipe,true};
}
bool ownsCoreConnection(qint64 pid,int clientPort,int serverPort) {
    DWORD size=0; GetExtendedTcpTable(nullptr,&size,FALSE,AF_INET,TCP_TABLE_OWNER_PID_ALL,0); if (!size||size>8*1024*1024) return false;
    std::vector<BYTE> bytes(size); if (GetExtendedTcpTable(bytes.data(),&size,FALSE,AF_INET,TCP_TABLE_OWNER_PID_ALL,0)!=NO_ERROR) return false;
    const auto* table=reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(bytes.data());
    for (DWORD i=0;i<table->dwNumEntries;++i) { const auto& row=table->table[i]; if (row.dwOwningPid==DWORD(pid)&&row.dwState==MIB_TCP_STATE_ESTAB&&ntohs(USHORT(row.dwLocalPort))==serverPort&&ntohs(USHORT(row.dwRemotePort))==clientPort&&row.dwLocalAddr==htonl(INADDR_LOOPBACK)&&row.dwRemoteAddr==htonl(INADDR_LOOPBACK)) return true; }
    return false;
}
Result runInstaller(const QString& operation,const QString& helper,bool installed) {
    if (!QFileInfo(helper).isFile()) return Result::fail("未找到随包 TUN 服务工具");
    const auto sid=ownerSid(); if (sid.isEmpty()) return Result::fail("无法取得安装账户身份");
    const QString args=operation+QString(" --owner %1").arg(sid);
    const QString file=QDir::toNativeSeparators(helper),directory=QFileInfo(helper).absolutePath();
    SHELLEXECUTEINFOW request{}; request.cbSize=sizeof(request); request.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC; request.lpVerb=L"runas"; request.lpFile=w(file); request.lpParameters=w(args); request.lpDirectory=w(directory); request.nShow=SW_HIDE;
    if (!ShellExecuteExW(&request)) return GetLastError()==ERROR_CANCELLED?Result::fail("已取消管理员授权，TUN 服务未更改"):error("无法打开 TUN 服务安装工具");
    const auto wait=WaitForSingleObject(request.hProcess,120000); DWORD code=1; if (wait==WAIT_OBJECT_0) GetExitCodeProcess(request.hProcess,&code); CloseHandle(request.hProcess);
    if (wait!=WAIT_OBJECT_0) return Result::fail("TUN 服务安装工具尚未结束；请稍后刷新状态，不要重复安装");
    if (code!=0) return Result::fail(QString("TUN 服务%1失败 (%2)；已有安装请先移除，确认程序来自完整便携目录").arg(operation=="--repair"?"修复":installed?"安装":"移除").arg(code));
    const auto observed=info(false); if (!observed.ok||observed.data.value("installed").toBool()!=installed) return Result::fail("TUN 服务安装状态回读不一致"); return observed;
}
Result manage(bool install,const QString& helper) { return runInstaller(install?"--install":"--remove",helper,install); }
Result repair(const QString& helper) { const auto current=info(false); if (!current.ok||!current.data.value("installed").toBool()) return current.ok?Result::fail("请先安装属于当前账户的 TUN 服务"):current; return runInstaller("--repair",helper,true); }
Result networkInterfaces() {
    QJsonArray rows;
    for (const auto& adapter:QNetworkInterface::allInterfaces()) { if (rows.size()>=64) break; QJsonArray addresses; for (const auto& address:adapter.addressEntries()) { if (addresses.size()>=16) break; addresses.append(address.ip().toString()+'/'+QString::number(address.prefixLength())); } rows.append(QJsonObject{{"name",adapter.name()},{"label",adapter.humanReadableName()},{"up",bool(adapter.flags()&QNetworkInterface::IsUp)},{"loopback",bool(adapter.flags()&QNetworkInterface::IsLoopBack)},{"addresses",addresses}}); }
    SOCKADDR_INET target{},source{}; target.si_family=AF_INET; target.Ipv4.sin_addr.s_addr=htonl(0x01010101); MIB_IPFORWARD_ROW2 route{}; QString selected;
    if (GetBestRoute2(nullptr,0,nullptr,&target,0,&route,&source)==NO_ERROR) { wchar_t alias[IF_MAX_STRING_SIZE+1]{}; if (ConvertInterfaceLuidToAlias(&route.InterfaceLuid,alias,IF_MAX_STRING_SIZE+1)==NO_ERROR) selected=QString::fromWCharArray(alias); }
    return {true,{},{{"interfaces",rows},{"route_target","1.1.1.1"},{"route_interface",selected},{"read_only",true}}};
}
Result observeAdapter() {
    NET_LUID luid{}; if (ConvertInterfaceAliasToLuid(L"Mlash",&luid)!=NO_ERROR) return {true,{},{{"adapter_up",false},{"routes",false}}};
    MIB_IF_ROW2 row{}; row.InterfaceLuid=luid; if (GetIfEntry2(&row)!=NO_ERROR) return Result::fail("TUN 接口状态读取失败");
    bool routes=false,routes6=false,anyRoute=false; int halves=0,halves6=0; PMIB_IPFORWARD_TABLE2 table=nullptr;
    if (GetIpForwardTable2(AF_UNSPEC,&table)!=NO_ERROR) return Result::fail("TUN 路由读取失败");
    for (ULONG i=0;i<table->NumEntries;++i) {
        const auto& entry=table->Table[i]; if (entry.InterfaceLuid.Value!=luid.Value||entry.DestinationPrefix.PrefixLength>1) continue;
        anyRoute=true; const auto prefix=entry.DestinationPrefix.PrefixLength;
        if (entry.DestinationPrefix.Prefix.si_family==AF_INET) { if (prefix==0) routes=true; else halves|=(ntohl(entry.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr)&0x80000000u)?2:1; }
        else if (entry.DestinationPrefix.Prefix.si_family==AF_INET6) { if (prefix==0) routes6=true; else halves6|=(entry.DestinationPrefix.Prefix.Ipv6.sin6_addr.u.Byte[0]&0x80)?2:1; }
    }
    FreeMibTable(table);
    ULONG size=0; GetAdaptersAddresses(AF_UNSPEC,0,nullptr,nullptr,&size); if (!size||size>1024*1024) return Result::fail("TUN DNS 接口状态超过预算"); std::vector<BYTE> adapters(size); bool dns=false;
    if (GetAdaptersAddresses(AF_UNSPEC,0,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES*>(adapters.data()),&size)!=NO_ERROR) return Result::fail("TUN DNS 接口状态读取失败");
    for (auto* adapter=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(adapters.data());adapter;adapter=adapter->Next) if (adapter->Luid.Value==luid.Value) dns=adapter->FirstDnsServerAddress!=nullptr;
    return {true,{},{{"adapter_up",row.OperStatus==IfOperStatusUp},{"routes",routes||halves==3},{"routes_v6",routes6||halves6==3},{"any_routes",anyRoute},{"dns_configured",dns},{"luid",QString::number(luid.Value)},{"mtu",double(row.Mtu)}}};
}
}
