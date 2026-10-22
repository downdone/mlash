#include "TunNative.h"
#include "core/TunPolicy.h"
#include "core/TunApiGate.h"
#include "storage/Storage.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <netioapi.h>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTcpServer>
#include <thread>
#include <mutex>
#include <yaml-cpp/yaml.h>
#include <vector>

namespace mlash::tunNative {
namespace {
const wchar_t* w(const QString& s) { return reinterpret_cast<const wchar_t*>(s.utf16()); }
SERVICE_STATUS_HANDLE serviceHandle=nullptr;
HANDLE stopEvent=nullptr;
SERVICE_STATUS serviceStatus{};
std::mutex statusMutex;
void report(DWORD state,DWORD code=NO_ERROR) { std::lock_guard<std::mutex> lock(statusMutex); serviceStatus.dwServiceType=SERVICE_WIN32_OWN_PROCESS; serviceStatus.dwCurrentState=state; serviceStatus.dwWin32ExitCode=code; serviceStatus.dwControlsAccepted=state==SERVICE_RUNNING?SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN:0; serviceStatus.dwWaitHint=state==SERVICE_STOP_PENDING?10000:0; SetServiceStatus(serviceHandle,&serviceStatus); }
DWORD WINAPI control(DWORD code,DWORD,void*,void*) { if (code==SERVICE_CONTROL_STOP||code==SERVICE_CONTROL_SHUTDOWN) { report(SERVICE_STOP_PENDING); SetEvent(stopEvent); } return NO_ERROR; }
bool safePath(const QString& path) {
    auto current=QFileInfo(path).absoluteFilePath();
    while (!current.isEmpty()) { const auto attributes=GetFileAttributesW(w(QDir::toNativeSeparators(current))); if (attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)) return false; const auto parent=QFileInfo(current).absolutePath(); if (parent==current) break; current=parent; }
    return true;
}
bool acl(const QString& path) {
    PSECURITY_DESCRIPTOR sd=nullptr; if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)",SDDL_REVISION_1,&sd,nullptr)) return false;
    PACL dacl=nullptr; BOOL present=FALSE,def=FALSE; GetSecurityDescriptorDacl(sd,&present,&dacl,&def); const auto native=QDir::toNativeSeparators(path);
    BYTE owner[SECURITY_MAX_SID_SIZE]; DWORD ownerSize=sizeof(owner); CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,owner,&ownerSize);
    const auto code=SetNamedSecurityInfoW(const_cast<wchar_t*>(w(native)),SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,owner,nullptr,dacl,nullptr); LocalFree(sd); return code==ERROR_SUCCESS;
}
bool privateAcl(const QString& path) {
    PSECURITY_DESCRIPTOR sd=nullptr; if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)",SDDL_REVISION_1,&sd,nullptr)) return false;
    PACL dacl=nullptr; BOOL present=FALSE,def=FALSE; GetSecurityDescriptorDacl(sd,&present,&dacl,&def); const auto native=QDir::toNativeSeparators(path); BYTE owner[SECURITY_MAX_SID_SIZE]; DWORD size=sizeof(owner); CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,owner,&size); const auto code=SetNamedSecurityInfoW(const_cast<wchar_t*>(w(native)),SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,owner,nullptr,dacl,nullptr); LocalFree(sd); return code==ERROR_SUCCESS;
}
QByteArray hash(const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)||file.size()>128*1024*1024) return {}; QCryptographicHash h(QCryptographicHash::Sha256); if (!h.addData(&file)) return {}; return h.result().toHex(); }
bool transfer(HANDLE pipe,void* data,DWORD length,bool write,DWORD timeout) {
    OVERLAPPED op{}; op.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr); if (!op.hEvent) return false;
    DWORD count=0; const auto ok=write?WriteFile(pipe,data,length,&count,&op):ReadFile(pipe,data,length,&count,&op);
    bool success=ok;
    if (!ok&&GetLastError()==ERROR_IO_PENDING) { HANDLE events[]{stopEvent,op.hEvent}; if (WaitForMultipleObjects(2,events,FALSE,timeout)==WAIT_OBJECT_0+1) success=GetOverlappedResult(pipe,&op,&count,FALSE); else { CancelIoEx(pipe,&op); GetOverlappedResult(pipe,&op,&count,TRUE); } }
    CloseHandle(op.hEvent); return success&&count==length;
}
bool readLine(HANDLE pipe,QByteArray& line,DWORD timeout) {
    line.clear(); QElapsedTimer timer; timer.start(); char c;
    while (line.size()<8192&&timer.elapsed()<timeout) { if (!transfer(pipe,&c,1,false,DWORD(qMax<qint64>(1,timeout-timer.elapsed())))) return false; if (c=='\n') return true; line+=c; } return false;
}
bool respond(HANDLE pipe,Result result) { auto o=result.data; o["ok"]=result.ok; o["protocol"]=1; if (!result.ok) o["error"]=result.error; auto bytes=QJsonDocument(o).toJson(QJsonDocument::Compact)+'\n'; return transfer(pipe,bytes.data(),DWORD(bytes.size()),true,2000); }
QString pipeSid(HANDLE pipe) {
    if (!ImpersonateNamedPipeClient(pipe)) return {};
    HANDLE token=nullptr; QString sid;
    if (OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&token)) { DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size); std::vector<BYTE> buffer(size); if (GetTokenInformation(token,TokenUser,buffer.data(),size,&size)) { LPWSTR text=nullptr; if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&text)) { sid=QString::fromWCharArray(text); LocalFree(text); } } CloseHandle(token); }
    if (!RevertToSelf()) ExitProcess(40); return sid;
}
bool clientPresent(HANDLE pipe) { DWORD available=0; return WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0&&PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr); }
Result copySnapshot(HANDLE pipe,const QString& work,const QString& target,QByteArray& config) {
    if (work.size()>4096||work.contains(QChar::Null)||!QRegularExpression("^[A-Za-z]:[/\\\\]").match(work).hasMatch()||!QDir::isAbsolutePath(work)||GetDriveTypeW(w(work.left(3)))!=DRIVE_FIXED) return Result::fail("TUN 快照必须位于本机固定磁盘");
    QByteArray raw; QStringList providers; auto result=Result{};
    if (!ImpersonateNamedPipeClient(pipe)) return Result::fail("无法模拟安装账户读取快照");
    if (!safePath(work)||!safePath(work+"/config.yaml")) result=Result::fail("TUN 快照不允许重解析路径");
    else result=readFile(work+"/config.yaml",raw,8*1024*1024);
    if (!RevertToSelf()) ExitProcess(40);
    if (!result.ok) return result; result=prepareTunRuntime(raw,config,providers); if (!result.ok) return result;
    if (!QDir().mkpath(target)||!privateAcl(target)||!writeAtomic(target+"/config.yaml",config).ok) return Result::fail("无法创建受保护 TUN 会话");
    QStringList paths=providers; paths.append({"country.mmdb","geoip.db","geoip.metadb","geoip.dat","geosite.dat","ASN.mmdb"}); qint64 total=config.size();
    for (const auto& path:paths) {
        if (!clientPresent(pipe)) return Result::fail("TUN 启动已取消");
        QByteArray bytes; bool exists=false;
        if (!ImpersonateNamedPipeClient(pipe)) return Result::fail("无法读取 TUN 数据快照");
        exists=QFileInfo::exists(work+'/'+path);
        if (exists) { result=safePath(work+'/'+path)?readFile(work+'/'+path,bytes,128*1024*1024):Result::fail("TUN 快照不允许重解析点"); }
        if (!RevertToSelf()) ExitProcess(40);
        if (!result.ok) return Result::fail("TUN 快照复制失败");
        if (!exists) { if (providers.contains(path)) return Result::fail("TUN Provider 快照缺失"); continue; }
        if (providers.contains(path)&&!checkTunProvider(config,path,bytes).ok) return Result::fail("TUN Provider 快照校验失败，未启动权限内核");
        total+=bytes.size(); if (total>512*1024*1024) return Result::fail("TUN 快照超过 512 MiB");
        result=writeAtomic(target+'/'+path,bytes); if (!result.ok) return Result::fail("写入受保护 TUN 快照失败");
    }
    return {};
}
void disableTun(const QByteArray& config,qint64 pid) {
    try {
        const auto node=YAML::Load(config.toStdString()); const auto endpoint=QString::fromStdString(node["external-controller"].as<std::string>()); const int port=endpoint.section(':',1).toInt(); const auto secret=QString::fromStdString(node["secret"].as<std::string>());
        WSADATA data{}; if (WSAStartup(MAKEWORD(2,2),&data)!=0) return; auto finishWsa=qScopeGuard([] { WSACleanup(); });
        const auto socket=WSASocketW(AF_INET,SOCK_STREAM,IPPROTO_TCP,nullptr,0,WSA_FLAG_NO_HANDLE_INHERIT); if (socket==INVALID_SOCKET) return; auto close=qScopeGuard([&] { closesocket(socket); });
        u_long nonblocking=1; ioctlsocket(socket,FIONBIO,&nonblocking); sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK); address.sin_port=htons(USHORT(port));
        if (connect(socket,reinterpret_cast<sockaddr*>(&address),sizeof(address))==SOCKET_ERROR) { if (WSAGetLastError()!=WSAEWOULDBLOCK) return; fd_set writable; FD_ZERO(&writable); FD_SET(socket,&writable); timeval timeout{0,500000}; if (select(0,nullptr,&writable,nullptr,&timeout)<=0) return; int error=0,length=sizeof(error); if (getsockopt(socket,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&length)!=0||error) return; }
        sockaddr_in local{}; int length=sizeof(local); if (getsockname(socket,reinterpret_cast<sockaddr*>(&local),&length)!=0||!ownsCoreConnection(pid,ntohs(local.sin_port),port)) return;
        nonblocking=0; ioctlsocket(socket,FIONBIO,&nonblocking); DWORD timeout=1000; setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout)); setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));
        const QByteArray body="{\"tun\":{\"enable\":false}}"; const auto request="PATCH /configs HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer "+secret.toUtf8()+"\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body;
        int sent=0; while (sent<request.size()) { const int n=send(socket,request.constData()+sent,int(request.size()-sent),0); if (n<=0) return; sent+=n; }
        char reply[512]; recv(socket,reply,sizeof(reply),0);
    } catch (...) {}
}
void removeSession(const QString& target) { const auto root=installRoot()+"/sessions/"; if (target.startsWith(root)&&safePath(target)) QDir(target).removeRecursively(); }
bool cleanStaleSession() {
    const auto root=installRoot(),marker=root+"/session.json"; if (!QFileInfo::exists(marker)) return true;
    QByteArray bytes; if (!readFile(marker,bytes,4096).ok) return false; const auto record=QJsonDocument::fromJson(bytes).object(); const auto observed=observeAdapter();
    if (!observed.ok||observed.data.value("adapter_up").toBool()||observed.data.value("any_routes").toBool()||!isId(record.value("session").toString())||(observed.data.contains("luid")&&observed.data.value("luid")!=record.value("luid"))) return false;
    removeSession(root+"/sessions/"+record.value("session").toString()); return QFile::remove(marker);
}
Result runConnection(HANDLE pipe,const QString& owner) {
    QByteArray line; if (!readLine(pipe,line,5000)||pipeSid(pipe)!=owner) return Result::fail("TUN 客户端身份不符");
    const auto request=QJsonDocument::fromJson(line).object();
    if (request.value("op")=="idle"&&request.value("protocol").toInt()==1&&request.size()==2) { respond(pipe,{true,{},{{"op","idle"},{"cleanup_confirmed",cleanStaleSession()}}}); return {}; }
    if (request.value("op")!="start"||request.value("protocol").toInt()!=1||request.size()!=3) return Result::fail("TUN 启动协议无效");
    if (!cleanStaleSession()) return Result::fail("上次 TUN 会话清理尚未确认，保留恢复标记");
    if (hash(installRoot()+"/mihomo.exe")!=TunCoreSha256) return Result::fail("受保护内核哈希不符");
    const auto existing=observeAdapter(); if (!existing.ok||existing.data.contains("luid")) return Result::fail("已有 Mlash 接口或待恢复路由；未覆盖现有网络");
    const auto target=installRoot()+"/sessions/"+newId(); QByteArray config;
    auto result=copySnapshot(pipe,request.value("work").toString(),target,config); if (!result.ok) { removeSession(target); return result; }
    if (!clientPresent(pipe)) { removeSession(target); return Result::fail("TUN 启动已取消"); }
    TunApiGate* gate=nullptr; Result gateResult; bool ipv6Required=false;
    QMetaObject::invokeMethod(QCoreApplication::instance(),[&] {
        try {
            auto node=YAML::Load(config.toStdString()); const auto endpoint=QString::fromStdString(node["external-controller"].as<std::string>()); const auto publicSecret=QString::fromStdString(node["secret"].as<std::string>());
            ipv6Required=node["tun"]["inet6-address"].IsSequence()&&node["tun"]["inet6-address"].size()>0;
            QString secret; for (int i=0;i<8;++i) secret+=QString("%1").arg(QRandomGenerator::system()->generate(),8,16,QChar('0'));
            QTcpServer reservation; if (!reservation.listen(QHostAddress::LocalHost,0)) { gateResult=Result::fail("无法分配私有 TUN 控制端口"); return; }
            const auto port=reservation.serverPort(); reservation.close(); gate=new TunApiGate;
            gateResult=gate->listen(endpoint.section(':',1).toInt(),port,publicSecret,secret);
            if (!gateResult.ok) { delete gate; gate=nullptr; return; }
            node["external-controller"]=QString("127.0.0.1:%1").arg(port).toStdString(); node["secret"]=secret.toStdString(); config=QByteArray::fromStdString(YAML::Dump(node));
            gateResult=writeAtomic(target+"/config.yaml",config);
        } catch (...) { gateResult=Result::fail("TUN 控制入口配置失败"); }
    },Qt::BlockingQueuedConnection);
    const auto destroyGate=[&] { if (gate) QMetaObject::invokeMethod(QCoreApplication::instance(),[&] { delete gate; gate=nullptr; },Qt::BlockingQueuedConnection); };
    if (!gateResult.ok) { destroyGate(); removeSession(target); return gateResult; }
    // Persist the recovery marker before the elevated process can change networking.
    const auto marker=installRoot()+"/session.json";
    QJsonObject recovery{{"session",QFileInfo(target).fileName()},{"owner",owner}};
    if (!writeAtomic(marker,QJsonDocument(recovery).toJson()).ok) { destroyGate(); removeSession(target); return Result::fail("TUN 恢复标记写入失败"); }
    HANDLE job=CreateJobObjectW(nullptr,nullptr); JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) { if (job) CloseHandle(job); destroyGate(); removeSession(target); return Result::fail("TUN 进程容器创建失败"); }
    const auto executable=QDir::toNativeSeparators(installRoot()+"/mihomo.exe"),cwd=QDir::toNativeSeparators(target);
    auto command=("\""+executable+"\" -d \""+cwd+"\" -f \""+cwd+"\\config.yaml\"").toStdWString();
    auto env=QProcessEnvironment::systemEnvironment(); for (const auto& key:env.keys()) if (key.startsWith("CLASH_")||key=="SAFE_PATHS"||key=="SKIP_SAFE_PATH_CHECK") env.remove(key); env.insert("SKIP_SAFE_PATH_CHECK","false");
    // The trusted core must not resolve driver/DLL assets in a caller-writable directory.
    env.insert("PATH",QDir::toNativeSeparators(installRoot())+';'+env.value("SystemRoot")+"\\System32");
    auto entries=env.toStringList(); entries.sort(Qt::CaseInsensitive); std::wstring environment; for (const auto& entry:entries) { environment+=entry.toStdWString(); environment+=L'\0'; } environment+=L'\0';
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    PSECURITY_DESCRIPTOR processDescriptor=nullptr; const auto processAcl="D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x00101000;;;"+owner+")";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(w(processAcl),SDDL_REVISION_1,&processDescriptor,nullptr)) { CloseHandle(job); destroyGate(); removeSession(target); return Result::fail("TUN 内核查询权限创建失败"); }
    SECURITY_ATTRIBUTES processSecurity{sizeof(processSecurity),processDescriptor,FALSE};
    const bool created=CreateProcessW(w(executable),command.data(),&processSecurity,nullptr,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,environment.data(),w(cwd),&startup,&process); LocalFree(processDescriptor);
    if (!created) { CloseHandle(job); destroyGate(); removeSession(target); return Result::fail("TUN 内核无法启动"); }
    if (!AssignProcessToJobObject(job,process.hProcess)) { TerminateProcess(process.hProcess,1); WaitForSingleObject(process.hProcess,2000); CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job); destroyGate(); removeSession(target); return Result::fail("TUN 内核容器关联失败"); }
    if (!clientPresent(pipe)) { TerminateJobObject(job,0); WaitForSingleObject(process.hProcess,2000); CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job); destroyGate(); removeSession(target); return Result::fail("TUN 启动已取消"); }
    QMetaObject::invokeMethod(QCoreApplication::instance(),[&] { gate->setBackendVerifier([pid=process.dwProcessId](int clientPort,int serverPort) { return ownsCoreConnection(pid,clientPort,serverPort); }); },Qt::BlockingQueuedConnection);
    ResumeThread(process.hThread); CloseHandle(process.hThread);
    bool replied=false,stop=false;
    do {
        auto observed=observeAdapter(); observed.data["op"]=replied?"status":"started"; observed.data["ipv6_required"]=ipv6Required; observed.data["pid"]=double(process.dwProcessId); observed.data["api_pid"]=double(GetCurrentProcessId()); if (!observed.ok||WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0) { result=Result::fail("TUN 内核已退出或接口无法读取"); break; }
        if (observed.data.contains("luid")&&recovery.value("luid")!=observed.data.value("luid")) { recovery["luid"]=observed.data.value("luid"); if (!writeAtomic(marker,QJsonDocument(recovery).toJson()).ok) { result=Result::fail("TUN 接口恢复身份写入失败"); break; } }
        if (!respond(pipe,observed)) { result=Result::fail("TUN 客户端已离开"); break; } replied=true;
        if (!readLine(pipe,line,6000)) { result=Result::fail("TUN 会话心跳结束"); break; }
        const auto op=QJsonDocument::fromJson(line).object(); if (op.size()!=1||!op.value("op").isString()) { result=Result::fail("TUN 会话命令无效"); break; }
        stop=op.value("op")=="stop"; if (!stop&&op.value("op")!="status") { result=Result::fail("TUN 会话不支持此命令"); break; }
    } while (!stop&&WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0);
    disableTun(config,process.dwProcessId); TerminateJobObject(job,0); WaitForSingleObject(process.hProcess,3000); CloseHandle(process.hProcess); CloseHandle(job); destroyGate();
    auto final=observeAdapter(); QElapsedTimer cleanupTimer; cleanupTimer.start();
    while (final.ok&&(final.data.value("any_routes").toBool()||final.data.value("adapter_up").toBool())&&cleanupTimer.elapsed()<2000) { Sleep(50); final=observeAdapter(); }
    const bool clean=final.ok&&!final.data.value("any_routes").toBool()&&!final.data.value("adapter_up").toBool();
    if (clean) { QFile::remove(marker); removeSession(target); }
    if (stop) respond(pipe,clean?Result{true,{},{{"op","stopped"},{"cleanup_confirmed",true}}}:Result::fail("TUN 已停止，但路由清理未确认；恢复标记已保留"));
    return replied?Result{}:result;
}
void WINAPI serviceMain(DWORD,wchar_t**) {
    serviceHandle=RegisterServiceCtrlHandlerExW(L"MlashTun",control,nullptr); if (!serviceHandle) return;
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr); if (!stopEvent) { report(SERVICE_STOPPED,GetLastError()); return; }
    report(SERVICE_START_PENDING); QByteArray ownerBytes; auto root=installRoot(); QString owner;
    if (safePath(root)&&readFile(root+"/owner.json",ownerBytes,4096).ok) owner=QJsonDocument::fromJson(ownerBytes).object().value("sid").toString();
    // A killed Job closes Wintun's handles. Only clear stale evidence after an
    // actual read confirms there is no active route on that virtual interface.
    cleanStaleSession();
    PSECURITY_DESCRIPTOR sd=nullptr;
    // Native client uses specific data rights (no FILE_CREATE_PIPE_INSTANCE).
    // Connection happens on a worker; Qt's Windows connectToServer can block 5s.
    const auto descriptor="D:P(A;;GA;;;SY)(A;;0x0012019B;;;"+owner+")";
    if (owner.isEmpty()||!ConvertStringSecurityDescriptorToSecurityDescriptorW(w(descriptor),SDDL_REVISION_1,&sd,nullptr)) { report(SERVICE_STOPPED,ERROR_ACCESS_DENIED); CloseHandle(stopEvent); return; }
    SECURITY_ATTRIBUTES security{sizeof(security),sd,FALSE}; const auto pipeName=QString("\\\\.\\pipe\\")+TunPipeName;
    HANDLE pipe=CreateNamedPipeW(w(pipeName),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,16384,16384,0,&security); LocalFree(sd);
    if (pipe==INVALID_HANDLE_VALUE) { report(SERVICE_STOPPED,GetLastError()); CloseHandle(stopEvent); return; }
    report(SERVICE_RUNNING);
    while (WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0) {
        OVERLAPPED op{}; op.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr); bool connected=ConnectNamedPipe(pipe,&op); const auto code=GetLastError();
        if (!connected&&code==ERROR_PIPE_CONNECTED) connected=true;
        else if (!connected&&code==ERROR_IO_PENDING) { HANDLE events[]{stopEvent,op.hEvent}; connected=WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0+1; if (!connected) { CancelIoEx(pipe,&op); DWORD ignored=0; GetOverlappedResult(pipe,&op,&ignored,TRUE); } }
        CloseHandle(op.hEvent); if (!connected) break;
        const auto result=runConnection(pipe,owner); if (!result.ok) respond(pipe,result); DisconnectNamedPipe(pipe);
    }
    CloseHandle(pipe); CloseHandle(stopEvent); report(SERVICE_STOPPED);
}
int install(const QString& owner) {
    PSID sid=nullptr; if (!ConvertStringSidToSidW(w(owner),&sid)) return 10; LocalFree(sid);
    const auto root=installRoot(); if (root.isEmpty()||!QDir(root).entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System).isEmpty()||!QDir().mkpath(root)||!safePath(root)||!acl(root)) return 11;
    auto cleanup=qScopeGuard([&] { if (safePath(root)) QDir(root).removeRecursively(); });
    SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CREATE_SERVICE|SC_MANAGER_CONNECT); if (!scm) return 12;
    SC_HANDLE previous=OpenServiceW(scm,L"MlashTun",SERVICE_QUERY_STATUS); if (previous) { CloseServiceHandle(previous); CloseServiceHandle(scm); return 13; }
    const auto source=QCoreApplication::applicationDirPath(); if (hash(source+"/cores/mihomo.exe")!=TunCoreSha256) { CloseServiceHandle(scm); return 14; }
    const QStringList dependencies{"MlashTunService.exe","Qt6Core.dll","Qt6Network.dll","libstdc++-6.dll","libgcc_s_seh-1.dll","libwinpthread-1.dll"};
    for (const auto& name:dependencies) { if (QFileInfo::exists(root+'/'+name)&&!QFile::remove(root+'/'+name)) { CloseServiceHandle(scm); return 15; } if (!safePath(source+'/'+name)||!QFile::copy(source+'/'+name,root+'/'+name)||!acl(root+'/'+name)) { CloseServiceHandle(scm); return 16; } }
    QFile::remove(root+"/mihomo.exe"); if (!QFile::copy(source+"/cores/mihomo.exe",root+"/mihomo.exe")||hash(root+"/mihomo.exe")!=TunCoreSha256) { CloseServiceHandle(scm); return 17; }
    if (!acl(root+"/mihomo.exe")||!writeAtomic(root+"/owner.json",QJsonDocument(QJsonObject{{"protocol",1},{"sid",owner},{"version",MLASH_VERSION},{"advanced_options",true}}).toJson()).ok||!acl(root+"/owner.json")||!QDir().mkpath(root+"/sessions")||!privateAcl(root+"/sessions")) { CloseServiceHandle(scm); return 18; }
    const auto command="\""+QDir::toNativeSeparators(root+"/MlashTunService.exe")+"\" --service";
    auto service=CreateServiceW(scm,L"MlashTun",L"Mlash TUN 权限服务",SERVICE_ALL_ACCESS,SERVICE_WIN32_OWN_PROCESS,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,w(command),nullptr,nullptr,nullptr,nullptr,nullptr);
    if (!service) { CloseServiceHandle(scm); return 19; }
    const auto descriptor="D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;CCLCRPWPLOCRRC;;;"+owner+")"; PSECURITY_DESCRIPTOR security=nullptr;
    bool secured=ConvertStringSecurityDescriptorToSecurityDescriptorW(w(descriptor),SDDL_REVISION_1,&security,nullptr)&&SetServiceObjectSecurity(service,DACL_SECURITY_INFORMATION,security); if (security) LocalFree(security);
    if (!secured) DeleteService(service); CloseServiceHandle(service); CloseServiceHandle(scm); if (secured) cleanup.dismiss(); return secured?0:20;
}
int remove(const QString& owner) {
    const auto root=installRoot(); QByteArray bytes; if (!safePath(root)||!readFile(root+"/owner.json",bytes,4096).ok||QJsonDocument::fromJson(bytes).object().value("sid").toString()!=owner) return 21;
    SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT); if (!scm) return 22;
    SC_HANDLE service=OpenServiceW(scm,L"MlashTun",SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE|SERVICE_QUERY_CONFIG); if (!service) { CloseServiceHandle(scm); return 23; }
    DWORD size=0; QueryServiceConfigW(service,nullptr,0,&size); std::vector<BYTE> buffer(size);
    const auto expected="\""+QDir::toNativeSeparators(root+"/MlashTunService.exe")+"\" --service";
    if (!size||!QueryServiceConfigW(service,reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data()),size,&size)||QString::fromWCharArray(reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data())->lpBinaryPathName)!=expected) { CloseServiceHandle(service); CloseServiceHandle(scm); return 24; }
    SERVICE_STATUS status{}; ControlService(service,SERVICE_CONTROL_STOP,&status); QElapsedTimer timer; timer.start();
    while (timer.elapsed()<10000&&QueryServiceStatus(service,&status)&&status.dwCurrentState!=SERVICE_STOPPED) Sleep(50);
    if (status.dwCurrentState==SERVICE_STOPPED&&!cleanStaleSession()) { CloseServiceHandle(service); CloseServiceHandle(scm); return 26; }
    const bool deleted=status.dwCurrentState==SERVICE_STOPPED&&DeleteService(service); CloseServiceHandle(service); CloseServiceHandle(scm);
    // Recovery marker must be resolved, never erase evidence after incomplete cleanup.
    if (deleted&&!QFileInfo::exists(root+"/session.json")&&safePath(root)) QDir(root).removeRecursively(); return deleted?0:25;
}
}
int runService(int argc,char** argv) {
    qunsetenv("QT_PLUGIN_PATH"); qunsetenv("QT_QPA_PLATFORM_PLUGIN_PATH");
    QCoreApplication app(argc,argv); app.setApplicationName("MlashTunService"); QCoreApplication::setLibraryPaths({QCoreApplication::applicationDirPath()});
    const auto args=app.arguments();
    if (args.size()==2&&args[1]=="--service") { int result=30; std::thread dispatcher([&] { SERVICE_TABLE_ENTRYW table[]{{const_cast<wchar_t*>(L"MlashTun"),serviceMain},{nullptr,nullptr}}; result=StartServiceCtrlDispatcherW(table)?0:30; QMetaObject::invokeMethod(&app,&QCoreApplication::quit,Qt::QueuedConnection); }); app.exec(); dispatcher.join(); return result; }
    if (args.size()==4&&args[2]=="--owner"&&args[1]=="--install") return install(args[3]);
    if (args.size()==4&&args[2]=="--owner"&&args[1]=="--remove") return remove(args[3]);
    if (args.size()==4&&args[2]=="--owner"&&args[1]=="--repair") { const auto removed=remove(args[3]); if (removed) return removed; QElapsedTimer timer; timer.start(); while (timer.elapsed()<5000) { auto scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT); if (!scm) return 12; auto service=OpenServiceW(scm,L"MlashTun",SERVICE_QUERY_STATUS); const auto code=GetLastError(); if (service) CloseServiceHandle(service); CloseServiceHandle(scm); if (!service&&code==ERROR_SERVICE_DOES_NOT_EXIST) return install(args[3]); Sleep(50); } return 27; }
    return 31;
}
}
int main(int argc,char** argv) { return mlash::tunNative::runService(argc,argv); }
