#pragma once
#include "app/Types.h"
#include <QByteArray>
#include <memory>
#include <functional>

namespace mlash {
struct TunPipeHandle {
    qintptr value=-1;
    std::function<void(qintptr)> close;
    ~TunPipeHandle() { if (value!=-1&&close) close(value); }
    qintptr take() { const auto handle=value; value=-1; return handle; }
};
struct TunPipeOpen { Result result; std::shared_ptr<TunPipeHandle> pipe; bool native=false; };
class PlatformServices {
public:
    virtual ~PlatformServices() = default;
    virtual Result readProxy() = 0;
    virtual Result writeProxy(const QJsonObject& snapshot) = 0;
    virtual QJsonObject proxyTarget(int port) const = 0;
    virtual bool proxyEnabled(const QJsonObject& snapshot) const = 0;
    virtual Result autoStart(bool enabled, const QString& executable) = 0;
    virtual Result readAutoStart(const QString& executable) = 0;
    virtual Result readUrlProtocol(const QString&) { return Result::fail("此平台不支持协议关联"); }
    virtual Result setUrlProtocol(bool,const QString&) { return Result::fail("此平台不支持协议关联"); }
    virtual Result seal(const QByteArray& plain, QByteArray& encrypted) = 0;
    virtual Result open(const QByteArray& encrypted, QByteArray& plain) = 0;
    virtual Result secureDirectory(const QString& path) = 0;
    virtual Result attachChild(qint64 pid) = 0;
    virtual Result ownsTcpPort(qint64 pid, int port) = 0;
    virtual Result tunServiceInfo(bool start = false) { Q_UNUSED(start); return Result::fail("此平台不支持 TUN 服务"); }
    virtual Result manageTunService(bool install, const QString& helper) { Q_UNUSED(install); Q_UNUSED(helper); return Result::fail("此平台不支持 TUN 服务"); }
    virtual Result repairTunService(const QString& helper) { Q_UNUSED(helper); return Result::fail("此平台不支持 TUN 服务修复"); }
    virtual Result networkInterfaces() { return Result::fail("此平台不支持网络接口观察"); }
    virtual Result verifyTunServer(qintptr handle) { Q_UNUSED(handle); return Result::fail("此平台不支持 TUN 服务身份核验"); }
    virtual TunPipeOpen openTunPipe(const QString& name) { Q_UNUSED(name); return {}; }
};
std::shared_ptr<PlatformServices> createPlatformServices();
}
