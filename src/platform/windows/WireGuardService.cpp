#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include "WireGuardService.h"
#include "profile/WireGuardConfig.h"
#include "storage/Storage.h"
#include "wireguard_hash.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QRegularExpression>
#include <QtEndian>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

namespace mlash::tunNative {
namespace {
constexpr GUID adapterGuid{0x206B571A,0x6242,0x4D19,{0xBE,0x25,0x90,0x78,0x8B,0x77,0x89,0x33}};
constexpr auto markerName = "/sessions/wireguard-recovery.json";
constexpr auto adapterName = L"Mlash WireGuard";
constexpr DWORD rowMetric = 0;
constexpr ULONG64 dnsNameServer = 0x0002;
constexpr ULONG64 dnsIPv6 = 0x0001;
struct DnsSettings {
    ULONG Version = 1;
    ULONG64 Flags = 0;
    PWSTR Domain = nullptr, NameServer = nullptr, SearchList = nullptr;
    ULONG RegistrationEnabled = 0, RegisterAdapterName = 0, EnableLLMNR = 0, QueryAdapterName = 0;
    PWSTR ProfileNameServer = nullptr;
};
struct DnsApi {
    using Set = DWORD (WINAPI*)(GUID,const DnsSettings*);
    HMODULE library = GetModuleHandleW(L"iphlpapi.dll");
    Set set = reinterpret_cast<Set>(GetProcAddress(library,"SetInterfaceDnsSettings"));
};

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE handle) : value(handle) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    void reset() { if (value && value!=INVALID_HANDLE_VALUE) CloseHandle(value); value=nullptr; }
};

bool fileMatches(const QString& path,const QByteArray& expected) {
    const auto flags=GetFileAttributesW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(path).utf16()));
    QFile file(path);
    if (flags==INVALID_FILE_ATTRIBUTES || (flags&FILE_ATTRIBUTE_REPARSE_POINT) ||
        !file.open(QIODevice::ReadOnly) || file.size()<1 || file.size()>128*1024*1024) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex()==expected;
}

class Helper {
public:
    bool launch(const QString& root) {
        const auto helper = root + "/MlashWireGuard.exe";
        if (!fileMatches(helper,MLASH_WIREGUARD_SHA256) || !fileMatches(root+"/wintun.dll",WintunSha256)) return false;
        SECURITY_ATTRIBUTES inherit{sizeof(inherit),nullptr,TRUE};
        Handle childInput, childOutput;
        if (!CreatePipe(&childInput.value,&request_.value,&inherit,256*1024) ||
            !CreatePipe(&response_.value,&childOutput.value,&inherit,16384) ||
            !SetHandleInformation(request_.value,HANDLE_FLAG_INHERIT,0) ||
            !SetHandleInformation(response_.value,HANDLE_FLAG_INHERIT,0)) return false;
        Handle nullOutput(CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_WRITE,&inherit,OPEN_EXISTING,0,nullptr));
        if (nullOutput.value==INVALID_HANDLE_VALUE) return false;
        HANDLE approved[]{childInput.value,childOutput.value,nullOutput.value};
        SIZE_T bytes=0;
        InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
        if (!bytes) return false;
        auto attrs = std::make_unique<std::byte[]>(bytes);
        auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.get());
        if (!InitializeProcThreadAttributeList(list,1,0,&bytes)) return false;
        const auto dispose = qScopeGuard([&] { DeleteProcThreadAttributeList(list); });
        if (!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,approved,sizeof(approved),nullptr,nullptr))
            return false;
        STARTUPINFOEXW si{};
        si.StartupInfo.cb=sizeof(si); si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        si.StartupInfo.hStdInput=childInput.value; si.StartupInfo.hStdOutput=childOutput.value;
        si.StartupInfo.hStdError=nullOutput.value; si.lpAttributeList=list;
        PROCESS_INFORMATION process{};
        const auto binary = QDir::toNativeSeparators(helper).toStdWString();
        const auto cwd = QDir::toNativeSeparators(root).toStdWString();
        auto command = L"\"" + binary + L"\" --service";
        job_.value=CreateJobObjectW(nullptr,nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job_.value || !SetInformationJobObject(job_.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
            return false;
        // Do not inherit caller environment overrides or writable binary search paths.
        wchar_t windowsPath[MAX_PATH]{};
        const auto windowsLength=GetWindowsDirectoryW(windowsPath,MAX_PATH);
        if (!windowsLength || windowsLength>=MAX_PATH) return false;
        std::wstring environment=L"PATH="+std::wstring(windowsPath)+L"\\System32";
        environment+=L'\0'; environment+=L"SystemRoot="; environment+=windowsPath; environment+=L'\0'; environment+=L'\0';
        if (!CreateProcessW(binary.c_str(),command.data(),nullptr,nullptr,TRUE,
            CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT|CREATE_UNICODE_ENVIRONMENT,
            environment.data(),cwd.c_str(),&si.StartupInfo,&process)) return false;
        process_.value=process.hProcess;
        Handle thread(process.hThread);
        if (!AssignProcessToJobObject(job_.value,process_.value)) {
            TerminateProcess(process_.value,1); WaitForSingleObject(process_.value,1000); return false;
        }
        return ResumeThread(thread.value)!=DWORD(-1);
    }

    QJsonObject command(const QString& operation, const QJsonObject& config={}) {
        QJsonObject request{{"version",1},{"sequence",++sequence_},{"operation",operation}};
        if (!config.isEmpty()) request["config"]=config;
        auto bytes=QJsonDocument(request).toJson(QJsonDocument::Compact);
        if (bytes.size()>128*1024) return {};
        QByteArray packet(4,'\0');
        qToLittleEndian<quint32>(quint32(bytes.size()),packet.data());
        packet.append(bytes);
        bytes.fill('\0');
        DWORD sent=0;
        const auto written=WriteFile(request_.value,packet.constData(),DWORD(packet.size()),&sent,nullptr);
        const auto expected=packet.size();
        packet.fill('\0');
        if (!written || sent!=expected) return {};
        QElapsedTimer timer; timer.start();
        QByteArray prefix(4,'\0');
        if (!readExact(prefix,timer)) return {};
        const auto size=qFromLittleEndian<quint32>(prefix.constData());
        if (!size || size>16384) return {};
        QByteArray response(size,'\0');
        if (!readExact(response,timer)) return {};
        const auto observed=QJsonDocument::fromJson(response).object();
        if (observed.value("version").toInt()!=1 || observed.value("sequence").toInt()!=sequence_ ||
            !observed.value("ok").toBool()) return {};
        return observed;
    }

    bool stop() {
        if (!process_.value) return true;
        request_.reset();
        if (WaitForSingleObject(process_.value,2000)==WAIT_OBJECT_0) return true;
        return job_.value && TerminateJobObject(job_.value,0) && WaitForSingleObject(process_.value,2000)==WAIT_OBJECT_0;
    }
private:
    bool readExact(QByteArray& data,const QElapsedTimer& timer) {
        DWORD total=0;
        while (total<DWORD(data.size()) && timer.elapsed()<7000) {
            DWORD available=0,read=0;
            if (!PeekNamedPipe(response_.value,nullptr,0,nullptr,&available,nullptr)) return false;
            if (!available) { if (WaitForSingleObject(process_.value,20)==WAIT_OBJECT_0) return false; continue; }
            if (!ReadFile(response_.value,data.data()+total,qMin(available,DWORD(data.size())-total),&read,nullptr) || !read)
                return false;
            total+=read;
        }
        return total==DWORD(data.size());
    }
    Handle request_,response_,job_,process_;
    int sequence_=0;
};

SOCKADDR_INET sockaddr(const QHostAddress& address) {
    SOCKADDR_INET native{};
    if (address.protocol()==QAbstractSocket::IPv4Protocol) {
        native.Ipv4.sin_family=AF_INET; native.Ipv4.sin_addr.s_addr=htonl(address.toIPv4Address());
    } else {
        native.Ipv6.sin6_family=AF_INET6;
        const auto bytes=address.toIPv6Address();
        std::memcpy(&native.Ipv6.sin6_addr,bytes.c,16);
    }
    return native;
}

QHostAddress addressOf(const SOCKADDR_INET& address) {
    if (address.si_family==AF_INET) return QHostAddress(ntohl(address.Ipv4.sin_addr.s_addr));
    if (address.si_family!=AF_INET6) return {};
    Q_IPV6ADDR bytes{};
    std::memcpy(bytes.c,&address.Ipv6.sin6_addr,16);
    return QHostAddress(bytes);
}

bool absence(DWORD code) { return code==ERROR_NOT_FOUND || code==ERROR_FILE_NOT_FOUND; }

bool adapter(NET_LUID& luid) {
    const auto found=ConvertInterfaceGuidToLuid(&adapterGuid,&luid);
    if (absence(found)) { luid={}; return true; }
    if (found!=NO_ERROR) return false;
    MIB_IF_ROW2 row{}; row.InterfaceLuid=luid;
    return GetIfEntry2(&row)==NO_ERROR && std::wstring_view(row.Alias)==adapterName;
}

bool covers(const QList<WireGuardPrefix>& routes,const QHostAddress& addr) {
    return std::any_of(routes.begin(),routes.end(),[&](const auto& route) {
        return route.address.protocol()==addr.protocol() && addr.isInSubnet(route.address,route.length);
    });
}

bool safeIP(const QHostAddress& ip,bool allowLoopback=false) {
    return !ip.isNull() && !ip.isMulticast() && !ip.isBroadcast() && !ip.isLinkLocal() &&
        (allowLoopback || !ip.isLoopback()) && ip.scopeId().isEmpty() &&
        ip!=QHostAddress(QHostAddress::AnyIPv4) && ip!=QHostAddress(QHostAddress::AnyIPv6);
}

bool safeRoute(const WireGuardPrefix& route) {
    // Avoid changing how Windows reaches local management, link-local or multicast ranges.
    if (route.length==0 || route.address.isNull()) return false;
    const auto v4 = route.address.protocol()==QAbstractSocket::IPv4Protocol;
    const QList<QPair<QHostAddress,int>> reserved = v4
        ? QList<QPair<QHostAddress,int>>{{QHostAddress("0.0.0.0"),8},{QHostAddress("127.0.0.0"),8},
            {QHostAddress("169.254.0.0"),16},{QHostAddress("224.0.0.0"),4},{QHostAddress("240.0.0.0"),4}}
        : QList<QPair<QHostAddress,int>>{{QHostAddress("::"),128},{QHostAddress("::1"),128},
            {QHostAddress("fe80::"),10},{QHostAddress("ff00::"),8},{QHostAddress("::ffff:0:0"),96}};
    return std::none_of(reserved.begin(),reserved.end(),[&](const auto& item) {
        return item.first.isInSubnet(route.address,route.length) || route.address.isInSubnet(item.first,item.second);
    });
}

bool existingConflict(const QList<WireGuardPrefix>& routes,NET_LUID owned={}) {
    PMIB_IPFORWARD_TABLE2 table=nullptr;
    if (GetIpForwardTable2(AF_UNSPEC,&table)!=NO_ERROR || !table) return true;
    const auto dispose=qScopeGuard([&] { FreeMibTable(table); });
    for (ULONG i=0;i<table->NumEntries;++i) {
        const auto& row=table->Table[i];
        if (owned.Value && row.InterfaceLuid.Value==owned.Value) continue;
        const auto target=addressOf(row.DestinationPrefix.Prefix);
        for (const auto& planned:routes)
            if (row.DestinationPrefix.PrefixLength>=planned.length && target.protocol()==planned.address.protocol() &&
                target.isInSubnet(planned.address,planned.length)) return true;
    }
    return false;
}

std::optional<NET_LUID> bestInterface(const QHostAddress& ip) {
    auto target=sockaddr(ip);
    DWORD index=0;
    NET_LUID luid{};
    if (GetBestInterfaceEx(reinterpret_cast<SOCKADDR*>(&target),&index)!=NO_ERROR ||
        ConvertInterfaceIndexToLuid(index,&luid)!=NO_ERROR) return {};
    return luid;
}

MIB_IPFORWARD_ROW2 routeRow(NET_LUID luid,const WireGuardPrefix& route) {
    MIB_IPFORWARD_ROW2 row{}; InitializeIpForwardEntry(&row);
    row.InterfaceLuid=luid; row.DestinationPrefix.Prefix=sockaddr(route.address);
    row.DestinationPrefix.PrefixLength=UCHAR(route.length); row.NextHop.si_family=row.DestinationPrefix.Prefix.si_family;
    row.Protocol=static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT); row.Metric=rowMetric;
    row.Immortal=TRUE; row.ValidLifetime=0xffffffffu; row.PreferredLifetime=0xffffffffu;
    return row;
}

bool matchesRoute(const MIB_IPFORWARD_ROW2& row) {
    return row.Protocol==MIB_IPPROTO_NETMGMT && row.Origin==NlroManual && row.Metric==rowMetric &&
        !row.Publish && !row.AutoconfigureAddress && !row.Loopback && row.Immortal &&
        row.ValidLifetime==0xffffffffu && row.PreferredLifetime==0xffffffffu;
}

MIB_UNICASTIPADDRESS_ROW addressRow(NET_LUID luid,const QHostAddress& ip) {
    MIB_UNICASTIPADDRESS_ROW row{}; InitializeUnicastIpAddressEntry(&row);
    row.InterfaceLuid=luid; row.Address=sockaddr(ip);
    row.OnLinkPrefixLength=ip.protocol()==QAbstractSocket::IPv4Protocol ? 32 : 128;
    row.PrefixOrigin=IpPrefixOriginManual; row.SuffixOrigin=IpSuffixOriginManual;
    row.ValidLifetime=0xffffffffu; row.PreferredLifetime=0xffffffffu;
    return row;
}

bool matchesAddress(const MIB_UNICASTIPADDRESS_ROW& row) {
    return row.PrefixOrigin==IpPrefixOriginManual && row.SuffixOrigin==IpSuffixOriginManual &&
        !row.SkipAsSource && row.ValidLifetime==0xffffffffu && row.PreferredLifetime==0xffffffffu &&
        row.OnLinkPrefixLength==(row.Address.si_family==AF_INET ? 32 : 128);
}

std::optional<QStringList> dnsServers(bool ipv6) {
    NET_LUID luid{};
    if (!adapter(luid) || !luid.Value) return {};
    ULONG limit=16*1024;
    QByteArray records(limit,'\0');
    constexpr ULONG flags=GAA_FLAG_SKIP_UNICAST|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|
        GAA_FLAG_SKIP_FRIENDLY_NAME|GAA_FLAG_INCLUDE_ALL_INTERFACES;
    auto adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(records.data());
    auto code=GetAdaptersAddresses(AF_UNSPEC,flags,nullptr,adapters,&limit);
    if (code==ERROR_BUFFER_OVERFLOW && limit<1024*1024) {
        records.resize(limit);
        adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(records.data());
        code=GetAdaptersAddresses(AF_UNSPEC,flags,nullptr,adapters,&limit);
    }
    if (code!=NO_ERROR) return {};
    for (auto* entry=adapters;entry;entry=entry->Next) {
        if (entry->Luid.Value!=luid.Value) continue;
        QStringList values;
        const auto family=ipv6?AF_INET6:AF_INET;
        for (auto* server=entry->FirstDnsServerAddress;server;server=server->Next) {
            if (server->Address.lpSockaddr && server->Address.lpSockaddr->sa_family==family) {
                SOCKADDR_INET address{};
                const auto bytes=ipv6?sizeof(SOCKADDR_IN6):sizeof(SOCKADDR_IN);
                if (server->Address.iSockaddrLength<int(bytes)) return {};
                std::memcpy(&address,server->Address.lpSockaddr,bytes);
                const auto ip=addressOf(address);
                if (!ip.isNull()) values.append(ip.toString());
            }
        }
        return values;
    }
    return {};
}

bool setDns(const DnsApi& api,bool ipv6,const QStringList& values) {
    if (!api.set) return false;
    auto joined=values.join(',').toStdWString();
    DnsSettings settings; settings.Flags=dnsNameServer | (ipv6 ? dnsIPv6 : 0);
    settings.NameServer=joined.data();
    return api.set(adapterGuid,&settings)==NO_ERROR;
}

bool waitForDns(bool ipv6,const QStringList& expected) {
    QElapsedTimer timer; timer.start();
    do {
        if (dnsServers(ipv6)==std::optional<QStringList>(expected)) return true;
        Sleep(40);
    } while (timer.elapsed()<1200);
    return false;
}

bool interfaceMtu(NET_LUID luid,bool ipv6,MIB_IPINTERFACE_ROW& row) {
    InitializeIpInterfaceEntry(&row);
    row.Family=ipv6?AF_INET6:AF_INET;
    row.InterfaceLuid=luid;
    return GetIpInterfaceEntry(&row)==NO_ERROR;
}

bool writeMtu(NET_LUID luid,bool ipv6,ULONG expected,ULONG target) {
    MIB_IPINTERFACE_ROW row{};
    if (!interfaceMtu(luid,ipv6,row) || row.NlMtu!=expected) return false;
    if (expected==target) return true;
    row.NlMtu=target;
    return SetIpInterfaceEntry(&row)==NO_ERROR && interfaceMtu(luid,ipv6,row) && row.NlMtu==target;
}

bool saveRecovery(const QString& root,const QJsonObject& marker) {
    return writeAtomic(root+markerName,QJsonDocument(marker).toJson(QJsonDocument::Compact)).ok;
}

QList<WireGuardPrefix> markerRoutes(const QJsonObject& marker,bool& ok) {
    QList<WireGuardPrefix> routes;
    const auto encoded=marker.value("routes").toArray();
    if (encoded.size()>256) { ok=false; return {}; }
    for (const auto& value:encoded) {
        const auto parsed=QHostAddress::parseSubnet(value.toString());
        WireGuardPrefix prefix{parsed.first,parsed.second};
        if (parsed.second<1 || !safeRoute(prefix)) { ok=false; return {}; }
        routes.append(prefix);
    }
    return routes;
}

bool removeOwned(const QJsonObject& marker, NET_LUID luid) {
    bool ok=true;
    const auto routes=markerRoutes(marker,ok);
    if (!ok) return false;
    const auto addresses=marker.value("addresses").toArray();
    if (addresses.size()>16) return false;
    const DnsApi dns;
    for (const bool ipv6:{false,true}) {
        const auto mtu=marker.value(ipv6?"mtu6":"mtu4").toObject();
        if (!mtu.isEmpty()) {
            MIB_IPINTERFACE_ROW row{};
            const auto before=ULONG(mtu.value("before").toInt());
            const auto after=ULONG(mtu.value("after").toInt());
            if (!before||!after||!interfaceMtu(luid,ipv6,row)||
                (row.NlMtu!=before && (row.NlMtu!=after || !writeMtu(luid,ipv6,after,before)))) ok=false;
        }
        const auto tag=ipv6?"dns6":"dns4";
        if (!marker.contains(tag)) continue;
        const auto entry=marker.value(tag).toObject();
        QStringList before,target;
        for (const auto& value:entry.value("before").toArray()) before.append(value.toString());
        for (const auto& value:entry.value("after").toArray()) target.append(value.toString());
        const auto actual=dnsServers(ipv6);
        if (!actual || (*actual!=before && (*actual!=target || !setDns(dns,ipv6,before)))) ok=false;
        else if (*actual!=before && !waitForDns(ipv6,before)) ok=false;
    }
    for (const auto& prefix:routes) {
        auto row=routeRow(luid,prefix);
        const auto queried=GetIpForwardEntry2(&row);
        if (absence(queried)) continue;
        if (queried!=NO_ERROR || !matchesRoute(row) || DeleteIpForwardEntry2(&row)!=NO_ERROR ||
            !absence(GetIpForwardEntry2(&row))) ok=false;
    }
    for (const auto& value:addresses) {
        const auto ip=QHostAddress(value.toString());
        if (!safeIP(ip)) return false;
        auto row=addressRow(luid,ip);
        const auto queried=GetUnicastIpAddressEntry(&row);
        if (absence(queried)) continue;
        if (queried!=NO_ERROR || !matchesAddress(row) || DeleteUnicastIpAddressEntry(&row)!=NO_ERROR ||
            !absence(GetUnicastIpAddressEntry(&row))) ok=false;
    }
    return ok;
}

struct Plan {
    QList<WireGuardPrefix> routes;
    QList<QHostAddress> addresses,endpoints,dns;
    QJsonObject device;
    ULONG mtu=1420;
};

Result parsePlan(QByteArray& payload, Plan& plan) {
    const auto input=QJsonDocument::fromJson(payload).object();
    payload.fill('\0');
    const auto bytes=input.value("source").toString().toLatin1();
    auto source=QByteArray::fromBase64Encoding(bytes,QByteArray::AbortOnBase64DecodingErrors);
    WireGuardConfig config;
    if (input.size()!=2 || !source || !parseWireGuard(source.decoded,config).ok) {
        source.decoded.fill('\0');
        return Result::fail("WireGuard 启动配置格式无效");
    }
    source.decoded.fill('\0');
    const auto summary=config.summary();
    if (summary.value("full_v4").toBool() || summary.value("full_v6").toBool())
        return Result::fail("当前 WireGuard 接管只支持分流网段");
    const auto resolved=input.value("endpoints").toArray();
    if (resolved.size()!=config.peers.size()) return Result::fail("WireGuard Endpoint 尚未解析");
    QJsonArray peers;
    QSet<QString> prefixes;
    for (qsizetype index=0;index<config.peers.size();++index) {
        auto& peer=config.peers[index];
        const auto ip=QHostAddress(resolved[index].toString()), literal=QHostAddress(peer.endpointHost);
        if (!safeIP(ip,true) || (!literal.isNull() && literal!=ip)) return Result::fail("WireGuard Endpoint 地址无效");
        plan.endpoints.append(ip);
        const auto endpoint=(ip.protocol()==QAbstractSocket::IPv6Protocol?"["+ip.toString()+"]":ip.toString())
            +":"+QString::number(peer.endpointPort);
        QJsonArray allowed;
        for (const auto& route:peer.allowed) {
            if (!safeRoute(route)) return Result::fail("WireGuard 网段覆盖系统保留地址");
            allowed.append(route.text());
            if (!prefixes.contains(route.text())) { plan.routes.append(route); prefixes.insert(route.text()); }
        }
        QJsonObject entry{{"public_key",QString::fromLatin1(peer.publicKey.toBase64())},{"endpoint",endpoint},
                          {"allowed_ips",allowed},{"keepalive",peer.keepalive}};
        if (!peer.presharedKey.isEmpty()) entry["preshared_key"]=QString::fromLatin1(peer.presharedKey.toBase64());
        peers.append(entry);
    }
    if (std::any_of(plan.endpoints.begin(),plan.endpoints.end(),[&](const auto& ip) { return covers(plan.routes,ip); }))
        return Result::fail("WireGuard Endpoint 落入受管网段，会造成路由递归");
    for (const auto& address:config.addresses) {
        if (!safeIP(address.address) || plan.addresses.contains(address.address))
            return Result::fail("WireGuard 接口地址重复或不可用");
        plan.addresses.append(address.address);
    }
    for (const auto& route:plan.routes)
        if (std::none_of(plan.addresses.begin(),plan.addresses.end(),[&](const auto& ip) { return ip.protocol()==route.address.protocol(); }))
            return Result::fail("WireGuard 受管网段缺少对应地址族的接口地址");
    for (const auto& ip:config.dns) {
        if (!safeIP(ip)||!covers(plan.routes,ip)) return Result::fail("WireGuard 接口 DNS 须在受管网段内");
        if (!plan.dns.contains(ip)) plan.dns.append(ip);
    }
    if (config.mtu && config.mtu<1280 &&
        std::any_of(plan.addresses.begin(),plan.addresses.end(),[](const auto& ip) { return ip.protocol()==QAbstractSocket::IPv6Protocol; }))
        return Result::fail("WireGuard IPv6 接口 MTU 至少为 1280");
    if (config.mtu) plan.mtu=config.mtu;
    plan.device={{"private_key",QString::fromLatin1(config.privateKey.toBase64())},{"listen_port",config.listenPort},
                 {"mtu",config.mtu},{"peers",peers}};
    return {};
}

bool activeRoutes(const Plan& plan,NET_LUID luid,const QList<NET_LUID>& endpoints) {
    if (existingConflict(plan.routes,luid)) return false;
    for (qsizetype index=0;index<plan.endpoints.size();++index) {
        const auto best=bestInterface(plan.endpoints[index]);
        if (!best || best->Value!=endpoints[index].Value || best->Value==luid.Value) return false;
    }
    for (const auto& route:plan.routes) {
        auto row=routeRow(luid,route);
        if (GetIpForwardEntry2(&row)!=NO_ERROR || !matchesRoute(row)) return false;
        const auto best=bestInterface(route.address);
        if (!best || best->Value!=luid.Value) return false;
    }
    for (const auto& ip:plan.addresses) {
        auto row=addressRow(luid,ip);
        if (GetUnicastIpAddressEntry(&row)!=NO_ERROR || !matchesAddress(row) || row.DadState!=IpDadStatePreferred) return false;
    }
    for (const bool ipv6:{false,true}) {
        const bool enabled=std::any_of(plan.addresses.begin(),plan.addresses.end(),
            [ipv6](const auto& ip) { return (ip.protocol()==QAbstractSocket::IPv6Protocol)==ipv6; });
        MIB_IPINTERFACE_ROW row{};
        if (enabled && (!interfaceMtu(luid,ipv6,row) || row.NlMtu!=plan.mtu)) return false;
    }
    return true;
}

bool noOwnedRows(const QJsonObject& marker) {
    bool valid=false;
    const auto stored=marker.value("luid").toString();
    if (stored.isEmpty()) return true; // Only the start intent was committed.
    const auto luid=stored.toULongLong(&valid);
    if (!valid||!luid) return false;
    PMIB_IPFORWARD_TABLE2 routes=nullptr;
    if (GetIpForwardTable2(AF_UNSPEC,&routes)!=NO_ERROR||!routes) return false;
    const auto releaseRoutes=qScopeGuard([&] { FreeMibTable(routes); });
    for (ULONG index=0;index<routes->NumEntries;++index)
        if (routes->Table[index].InterfaceLuid.Value==luid) return false;
    PMIB_UNICASTIPADDRESS_TABLE addresses=nullptr;
    if (GetUnicastIpAddressTable(AF_UNSPEC,&addresses)!=NO_ERROR||!addresses) return false;
    const auto releaseAddresses=qScopeGuard([&] { FreeMibTable(addresses); });
    for (ULONG index=0;index<addresses->NumEntries;++index)
        if (addresses->Table[index].InterfaceLuid.Value==luid) return false;
    return true;
}
}

bool recoverWireGuard(const QString& root) {
    if (!QFileInfo::exists(root+markerName)) return true;
    QByteArray bytes;
    if (!readFile(root+markerName,bytes,32768).ok) return false;
    const auto marker=QJsonDocument::fromJson(bytes).object();
    NET_LUID observed{};
    if (marker.value("version").toInt()!=1 || !adapter(observed)) return false;
    if (observed.Value) {
        if (marker.value("luid").toString()!=QString::number(observed.Value)) return false;
        removeOwned(marker,observed);
        // An adapter still present may have a live owner. Do not delete/reuse it.
        return false;
    }
    return noOwnedRows(marker)&&QFile::remove(root+markerName);
}

Result runWireGuard(const QString& root, QByteArray payload,const std::function<bool()>& connected,
                    const std::function<bool(Result)>& reply,const std::function<QString()>& next) {
    Plan plan;
    auto result=parsePlan(payload,plan);
    if (!result.ok) return result;
    NET_LUID luid{};
    NET_LUID named{};
    if (!recoverWireGuard(root)||!adapter(luid)||luid.Value ||
        ConvertInterfaceAliasToLuid(adapterName,&named)==NO_ERROR || existingConflict(plan.routes))
        return Result::fail("WireGuard 接口待清理或受管网段与 Windows 路由冲突");
    QList<NET_LUID> endpoints;
    for (const auto& ip:plan.endpoints) {
        const auto best=bestInterface(ip);
        if (!best) return Result::fail("WireGuard Endpoint 没有可验证的出口");
        endpoints.append(*best);
    }
    const DnsApi dns;
    if (!plan.dns.isEmpty() && !dns.set) return Result::fail("此 Windows 版本不支持接口 DNS 设置");
    QJsonObject marker{{"version",1},{"luid",""},{"routes",QJsonArray{}},{"addresses",QJsonArray{}}};
    if (!connected()||!saveRecovery(root,marker)) return Result::fail("无法登记 WireGuard 恢复意图");
    Helper helper;
    bool clean=true, replied=false, stop=false;
    const auto prepare=[&]() -> Result {
        if (!helper.launch(root)||helper.command("start",plan.device).isEmpty())
            return Result::fail("WireGuard helper 或 Wintun 启动失败，请检查服务资产");
        plan.device={};
        const auto device=helper.command("status").value("observation").toObject();
        QElapsedTimer timer; timer.start();
        while (connected() && timer.elapsed()<3500 && adapter(luid) && !luid.Value) Sleep(50);
        if (!connected() || !luid.Value || device.value("adapter_luid").toString()!=QString::number(luid.Value))
            return Result::fail("WireGuard 网卡身份无法确认");
        marker["luid"]=QString::number(luid.Value);
        QJsonArray addrs,routes;
        for (const auto& ip:plan.addresses) {
            auto row=addressRow(luid,ip);
            if (!absence(GetUnicastIpAddressEntry(&row))) return Result::fail("WireGuard 接口地址已存在");
            addrs.append(ip.toString());
        }
        for (const auto& prefix:plan.routes) {
            auto row=routeRow(luid,prefix);
            if (!absence(GetIpForwardEntry2(&row))) return Result::fail("WireGuard 网段路由已存在");
            routes.append(prefix.text());
        }
        marker["addresses"]=addrs; marker["routes"]=routes;
        for (const bool ipv6:{false,true}) {
            const bool enabled=std::any_of(plan.addresses.begin(),plan.addresses.end(),
                [ipv6](const auto& ip) { return (ip.protocol()==QAbstractSocket::IPv6Protocol)==ipv6; });
            if (enabled) {
                MIB_IPINTERFACE_ROW row{};
                if (!interfaceMtu(luid,ipv6,row) || !row.NlMtu)
                    return Result::fail("WireGuard 接口 MTU 无法读回");
                marker[ipv6?"mtu6":"mtu4"]=QJsonObject{{"before",double(row.NlMtu)},{"after",double(plan.mtu)}};
            }
            QStringList target;
            for (const auto& ip:plan.dns)
                if ((ip.protocol()==QAbstractSocket::IPv6Protocol)==ipv6) target.append(ip.toString());
            if (target.isEmpty()) continue;
            const auto before=dnsServers(ipv6);
            if (!before) return Result::fail("WireGuard 接口 DNS 无法读回");
            marker[ipv6?"dns6":"dns4"]=QJsonObject{{"before",QJsonArray::fromStringList(*before)},
                                                    {"after",QJsonArray::fromStringList(target)}};
        }
        if (!connected()||!saveRecovery(root,marker)) return Result::fail("无法登记 WireGuard 路由和 DNS 恢复意图");
        for (const bool ipv6:{false,true}) {
            const auto mtu=marker.value(ipv6?"mtu6":"mtu4").toObject();
            if (!mtu.isEmpty() && (!connected()||!writeMtu(luid,ipv6,ULONG(mtu.value("before").toInt()),plan.mtu)))
                return Result::fail("WireGuard 接口 MTU 写入或读回失败");
        }
        for (const auto& ip:plan.addresses) {
            auto row=addressRow(luid,ip);
            if (!connected()||CreateUnicastIpAddressEntry(&row)!=NO_ERROR) return Result::fail("WireGuard 接口地址写入失败");
        }
        for (const auto& prefix:plan.routes) {
            auto row=routeRow(luid,prefix);
            if (!connected()||CreateIpForwardEntry2(&row)!=NO_ERROR) return Result::fail("WireGuard 路由写入失败");
        }
        for (const bool ipv6:{false,true}) {
            const auto target=marker.value(ipv6?"dns6":"dns4").toObject().value("after").toArray();
            if (target.isEmpty()) continue;
            QStringList expected;
            for (const auto& ip:target) expected.append(ip.toString());
            if (!connected()||!setDns(dns,ipv6,expected)||!waitForDns(ipv6,expected))
                return Result::fail("WireGuard 接口 DNS 写入或读回失败");
        }
        timer.restart();
        do {
            if (activeRoutes(plan,luid,endpoints)) return {};
            Sleep(50);
        } while (connected()&&timer.elapsed()<3500);
        return Result::fail("WireGuard 接口或分流路由未达到预期");
    };
    result=prepare();
    while (result.ok && connected()) {
        const auto observed=helper.command("status").value("observation").toObject();
        if (observed.value("adapter_luid").toString()!=QString::number(luid.Value)||!activeRoutes(plan,luid,endpoints)) {
            result=Result::fail("WireGuard 设备或出口路径已改变，正在清理");
            break;
        }
        for (const bool ipv6:{false,true}) {
            const auto target=marker.value(ipv6?"dns6":"dns4").toObject().value("after").toArray();
            if (target.isEmpty()) continue;
            QStringList expected;
            for (const auto& ip:target) expected.append(ip.toString());
            if (dnsServers(ipv6)!=std::optional<QStringList>(expected)) {
                result=Result::fail("WireGuard 接口 DNS 已改变，正在清理");
                break;
            }
        }
        if (!result.ok) break;
        QJsonObject status{{"op",replied?"wireguard-status":"wireguard-started"},{"observation",observed},{"routes",marker.value("routes")},
                           {"dns_configured",!plan.dns.isEmpty()},{"mtu",double(plan.mtu)}};
        if (!reply({true,{},status})) break;
        replied=true;
        const auto op=next();
        if (op=="stop") { stop=true; break; }
        if (op!="status") break;
    }
    NET_LUID observed{};
    clean=adapter(observed);
    if (clean&&observed.Value&&marker.value("luid").toString()==QString::number(observed.Value))
        clean=removeOwned(marker,observed);
    else if (clean&&observed.Value) clean=false;
    const auto stopped=helper.stop();
    QElapsedTimer finish; finish.start();
    while (finish.elapsed()<2000 && adapter(observed) && observed.Value) Sleep(50);
    if (!stopped || !adapter(observed) || observed.Value || !clean || !noOwnedRows(marker) || !QFile::remove(root+markerName))
        result=Result::fail("WireGuard 清理尚未确认，已保留恢复标记");
    if (stop && result.ok) return {true,{},{{"op","wireguard-stopped"},{"cleanup_confirmed",true}}};
    if (replied && result.ok) return {};
    return result;
}
}
