#include "platform/PlatformServices.h"
#include "ProtocolRegistry.h"
#include "TunNative.h"
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>
#include <sddl.h>
#include <aclapi.h>
#include <vector>
#include <iphlpapi.h>

namespace mlash {
namespace {
Result winError(const QString& action) {
    return Result::fail(action + QString(" (Windows %1)").arg(GetLastError()));
}
const wchar_t* wide(const QString& s) { return reinterpret_cast<const wchar_t*>(s.utf16()); }
class WindowsServices final : public PlatformServices {
    HANDLE job_ = nullptr;
    QMutex jobMutex_;
public:
    Result tunServiceInfo(bool start) override { return tunNative::info(start); }
    Result manageTunService(bool install,const QString& helper) override { return tunNative::manage(install,helper); }
    Result repairTunService(const QString& helper) override { return tunNative::repair(helper); }
    Result networkInterfaces() override { return tunNative::networkInterfaces(); }
    Result verifyTunServer(qintptr handle) override { return tunNative::verifyServer(handle); }
    TunPipeOpen openTunPipe(const QString& name) override { return tunNative::openPipe(name); }
    Result readUrlProtocol(const QString& exe) override { return readProtocolRegistry("Software\\Classes\\mlash",exe); }
    Result setUrlProtocol(bool enabled,const QString& exe) override { return writeProtocolRegistry("Software\\Classes\\mlash",exe,enabled); }
    ~WindowsServices() override { if (job_) CloseHandle(job_); }
    Result readProxy() override {
        INTERNET_PER_CONN_OPTIONW options[4]{};
        options[0].dwOption = INTERNET_PER_CONN_FLAGS;
        options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
        options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
        options[3].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
        INTERNET_PER_CONN_OPTION_LISTW list{};
        list.dwSize = sizeof(list); list.dwOptionCount = 4; list.pOptions = options;
        DWORD size = sizeof(list);
        if (!InternetQueryOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size)) return winError("读取系统代理失败");
        QJsonObject snapshot{{"flags", static_cast<int>(options[0].Value.dwValue)}};
        const char* keys[] = {"", "server", "bypass", "pac"};
        for (int i = 1; i < 4; ++i) {
            snapshot[keys[i]] = options[i].Value.pszValue ? QString::fromWCharArray(options[i].Value.pszValue) : QString();
            if (options[i].Value.pszValue) GlobalFree(options[i].Value.pszValue);
        }
        return {true, {}, snapshot};
    }
    Result writeProxy(const QJsonObject& snapshot) override {
        if (!snapshot.contains("flags") || !snapshot.contains("server") || !snapshot.contains("pac") || !snapshot.contains("bypass"))
            return Result::fail("代理快照字段不完整");
        QString server = snapshot.value("server").toString(), bypass = snapshot.value("bypass").toString(), pac = snapshot.value("pac").toString();
        INTERNET_PER_CONN_OPTIONW options[4]{};
        options[0].dwOption = INTERNET_PER_CONN_FLAGS; options[0].Value.dwValue = snapshot.value("flags").toInt();
        options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER; options[1].Value.pszValue = const_cast<wchar_t*>(wide(server));
        options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS; options[2].Value.pszValue = const_cast<wchar_t*>(wide(bypass));
        options[3].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL; options[3].Value.pszValue = const_cast<wchar_t*>(wide(pac));
        INTERNET_PER_CONN_OPTION_LISTW list{};
        list.dwSize = sizeof(list); list.dwOptionCount = 4; list.pOptions = options;
        if (!InternetSetOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, sizeof(list))) return winError("写入系统代理失败");
        if (!InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0) ||
            !InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0)) return winError("系统代理刷新失败");
        return {};
    }
    QJsonObject proxyTarget(int port) const override {
        return {{"flags", PROXY_TYPE_DIRECT | PROXY_TYPE_PROXY}, {"server", QString("127.0.0.1:%1").arg(port)},
            {"bypass", "localhost;127.*;[::1];<local>"}, {"pac", ""}};
    }
    bool proxyEnabled(const QJsonObject& o) const override {
        return (o.value("flags").toInt() & (PROXY_TYPE_PROXY | PROXY_TYPE_AUTO_PROXY_URL | PROXY_TYPE_AUTO_DETECT)) != 0;
    }
    Result readAutoStart(const QString& executable) override {
        HKEY key = nullptr;
        LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key);
        if (status == ERROR_FILE_NOT_FOUND) return {true, {}, {{"enabled", false}}};
        if (status != ERROR_SUCCESS) return Result::fail(QString("读取自启失败 (%1)").arg(status));
        DWORD type = 0, size = 0;
        status = RegQueryValueExW(key, L"Mlash", nullptr, &type, nullptr, &size);
        if (status == ERROR_FILE_NOT_FOUND) { RegCloseKey(key); return {true, {}, {{"enabled", false}}}; }
        if (status != ERROR_SUCCESS || type != REG_SZ || size > 32768) { RegCloseKey(key); return Result::fail("自启项不可读"); }
        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1);
        status = RegQueryValueExW(key, L"Mlash", nullptr, &type, reinterpret_cast<BYTE*>(buffer.data()), &size);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) return Result::fail("读取自启值失败");
        const QString expected = QString("\"%1\" --silent").arg(QDir::toNativeSeparators(executable));
        const QString actual = QString::fromWCharArray(buffer.data());
        return {true, {}, {{"enabled", actual.compare(expected, Qt::CaseInsensitive) == 0}, {"conflict", actual.compare(expected, Qt::CaseInsensitive) != 0}}};
    }
    Result autoStart(bool enabled, const QString& executable) override {
        auto existing = readAutoStart(executable);
        if (!existing.ok || existing.data.value("conflict").toBool()) return Result::fail("自启项存在冲突，未覆盖");
        HKEY key = nullptr;
        LONG status = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
        if (status != ERROR_SUCCESS) return Result::fail(QString("打开自启项失败 (%1)").arg(status));
        QString command = QString("\"%1\" --silent").arg(QDir::toNativeSeparators(executable));
        status = enabled ? RegSetValueExW(key, L"Mlash", 0, REG_SZ, reinterpret_cast<const BYTE*>(command.utf16()), (command.size() + 1) * sizeof(wchar_t))
                         : RegDeleteValueW(key, L"Mlash");
        RegCloseKey(key);
        if (status != ERROR_SUCCESS && !(status == ERROR_FILE_NOT_FOUND && !enabled)) return Result::fail(QString("设置自启失败 (%1)").arg(status));
        const auto observed = readAutoStart(executable);
        if (!observed.ok || observed.data.value("enabled").toBool() != enabled) return Result::fail("自启状态回读不一致");
        return {};
    }
    Result seal(const QByteArray& plain, QByteArray& encrypted) override {
        DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))}, output{};
        if (!CryptProtectData(&input, L"Mlash", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return winError("机密加密失败");
        encrypted = QByteArray(reinterpret_cast<char*>(output.pbData), output.cbData); LocalFree(output.pbData); return {};
    }
    Result open(const QByteArray& encrypted, QByteArray& plain) override {
        DATA_BLOB input{static_cast<DWORD>(encrypted.size()), reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()))}, output{};
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return winError("机密解密失败");
        plain = QByteArray(reinterpret_cast<char*>(output.pbData), output.cbData); LocalFree(output.pbData); return {};
    }
    Result secureDirectory(const QString& path) override {
        if (!QDir().mkpath(path)) return Result::fail("无法创建私有数据目录");
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return winError("读取用户身份失败");
        DWORD size = 0; GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<BYTE> buffer(size);
        const bool queried = GetTokenInformation(token, TokenUser, buffer.data(), size, &size);
        CloseHandle(token);
        if (!queried) return winError("读取用户 SID 失败");
        LPWSTR sid = nullptr;
        if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) return winError("用户 SID 转换失败");
        const QString descriptor = QString("D:P(A;OICI;FA;;;%1)(A;OICI;FA;;;SY)").arg(QString::fromWCharArray(sid));
        LocalFree(sid);
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(wide(descriptor), SDDL_REVISION_1, &sd, nullptr)) return winError("创建目录 ACL 失败");
        PACL dacl = nullptr; BOOL present = false, defaulted = false;
        GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted);
        const DWORD status = SetNamedSecurityInfoW(const_cast<wchar_t*>(wide(QDir::toNativeSeparators(path))), SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, dacl, nullptr);
        LocalFree(sd);
        return status == ERROR_SUCCESS ? Result{} : Result::fail(QString("目录 ACL 设置失败 (%1)").arg(status));
    }
    Result attachChild(qint64 pid) override {
        QMutexLocker locker(&jobMutex_);
        if (!job_) {
            job_ = CreateJobObjectW(nullptr, nullptr);
            if (!job_) return winError("创建子进程 Job 失败");
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return winError("设置 Job 失败");
        }
        HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (!child) return winError("打开内核进程失败");
        const BOOL assigned = AssignProcessToJobObject(job_, child);
        const DWORD error = GetLastError(); CloseHandle(child);
        return assigned ? Result{} : Result::fail(QString("内核 Job 关联失败 (%1)").arg(error));
    }
    Result ownsTcpPort(qint64 pid, int port) override {
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
        std::vector<BYTE> buffer(size);
        const DWORD status = GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (status != NO_ERROR) return Result::fail("无法核对内核监听端口身份");
        const auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
        bool owned = false;
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto& row = table->table[i];
            const int rowPort = ((row.dwLocalPort & 0xff) << 8) | ((row.dwLocalPort >> 8) & 0xff);
            if (rowPort == port && row.dwOwningPid == static_cast<DWORD>(pid)) { owned = true; break; }
        }
        return {true, {}, {{"owned", owned}}};
    }
};
}
std::shared_ptr<PlatformServices> createPlatformServices() { return std::make_shared<WindowsServices>(); }
}
