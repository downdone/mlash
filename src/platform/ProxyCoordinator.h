#pragma once
#include "PlatformServices.h"
#include <optional>

namespace mlash {
class ProxyCoordinator {
public:
    ProxyCoordinator(QString root, std::shared_ptr<PlatformServices> platform);
    Result inspect();
    Result acquire(int port, bool allowExternal = false,const std::optional<QString>& bypass=std::nullopt);
    Result release();
    bool hasJournal() const;
private:
    QString journal_;
    std::shared_ptr<PlatformServices> platform_;
    Result readJournal(QJsonObject& journal);
    Result saveJournal(const QJsonObject& journal);
    Result replaceOwned(QJsonObject journal,const QJsonObject& current,const QJsonObject& target);
};
}
