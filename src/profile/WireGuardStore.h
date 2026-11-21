#pragma once

#include "WireGuardConfig.h"
#include "platform/PlatformServices.h"
#include <optional>

namespace mlash {
struct WireGuardProfile {
    QString id, name, candidate, active, lastGood, updated;
    QJsonObject summary;
};

struct WireGuardCatalog {
    Result result;
    QVector<WireGuardProfile> profiles;
};

// Called on the bounded worker pool, with writes serialized by AppController.
class WireGuardStore {
public:
    WireGuardStore(const QString& dataRoot, std::shared_ptr<PlatformServices> platform);
    WireGuardCatalog load() const;
    WireGuardCatalog saveCandidate(const QString& name, const QByteArray& source,
                                   const QString& id = {}, const QString& expectedCandidate = {}) const;
    Result readVersion(const QString& id, const QString& version, WireGuardConfig& config,
                       QByteArray* original = nullptr) const;
    WireGuardCatalog markStarted(const QString& id,const QString& version) const;
    WireGuardCatalog updateProfile(const QString& id,const std::optional<QString>& name) const;

private:
    QString versionPath(const QString& id, const QString& version) const;
    QString root_;
    std::shared_ptr<PlatformServices> platform_;
};
}
