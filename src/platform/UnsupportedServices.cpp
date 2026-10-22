#include "PlatformServices.h"
#include <QDir>

namespace mlash {
class UnsupportedServices final : public PlatformServices {
public:
    Result readProxy() override { return Result::fail("此平台的系统代理尚未支持"); }
    Result writeProxy(const QJsonObject&) override { return readProxy(); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool, const QString&) override { return Result::fail("此平台的自启尚未支持"); }
    Result readAutoStart(const QString&) override { return autoStart(false, {}); }
    Result seal(const QByteArray&, QByteArray&) override { return Result::fail("此平台的机密存储尚未支持"); }
    Result open(const QByteArray&, QByteArray&) override { return Result::fail("此平台的机密存储尚未支持"); }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path) ? Result{} : Result::fail("无法创建目录"); }
    Result attachChild(qint64) override { return Result::fail("此平台尚未支持受控内核进程管理"); }
    Result ownsTcpPort(qint64, int) override { return Result::fail("此平台尚未支持内核端口身份核验"); }
};
std::shared_ptr<PlatformServices> createPlatformServices() { return std::make_shared<UnsupportedServices>(); }
}
