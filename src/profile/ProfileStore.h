#pragma once
#include "ConfigBuilder.h"
#include "platform/PlatformServices.h"

namespace mlash {
struct StoreSnapshot { Result result; Settings settings; QVector<Profile> profiles; };
struct PreparedProfile {
    Result result;
    Profile profile;
    QString version, configPath, workPath;
    QStringList secrets, remoteUrls;
    int apiPort = 0;
    int expectedRuleCount = -1;
    QString secret;
    QString expectedCoreVersion;
};
class ProfileStore {
public:
    ProfileStore(QString root, std::shared_ptr<PlatformServices> platform);
    StoreSnapshot load();
    Result save(const Settings& settings, const QVector<Profile>& profiles);
    PreparedProfile prepare(const Profile& profile, const QByteArray& source, const Settings& settings, int apiPort, const QString& secret, bool useLocalGeo=true);
    PreparedProfile prepareExisting(const Profile& profile, const QString& version, const Settings& settings, int apiPort, const QString& secret, bool useLocalGeo=true, const QStringList& refreshProviders = {},bool allowEmptyRefresh=false);
    Result inspectProviders(const Profile& profile, const QString& version, const Settings& settings);
    Result recordProviderSnapshot(const QString& workPath, const QJsonObject& provider, const QByteArray& bytes);
    Result versionProfile(const QString& id,const QString& version,Profile& profile) const;
    Result ensureVersionSnapshots(const Profile& profile) const;
    static QJsonObject overrideDocument(const Profile& profile);
    static Result applyOverrides(Profile& profile,const QJsonObject& object);
    Result importGeoData(const QString& sourceDirectory, const QJsonObject& provenance = {});
    Result createPrivateDirectory(const QString& path) { return platform_->secureDirectory(path); }
    QString versionRoot(const QString& id, const QString& version) const;
    Result removeProfile(const QString& id);
    const QString& root() const { return root_; }
private:
    QString root_;
    std::shared_ptr<PlatformServices> platform_;
};
}
