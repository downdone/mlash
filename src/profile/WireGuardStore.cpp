#include "WireGuardStore.h"
#include "storage/Storage.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>

namespace mlash {
namespace {
constexpr qsizetype catalogLimit = 4 * 1024 * 1024;
constexpr qsizetype versionLimit = 2 * WireGuardConfig::sourceLimit;
constexpr qsizetype maxProfiles = 128;

bool safeEntry(const QString& path) {
    const QFileInfo info(path);
    return !info.isSymLink() && !info.isJunction();
}

bool validName(const QString& name) {
    return !name.isEmpty() && name.size() <= 120 && std::none_of(name.begin(), name.end(),
        [](QChar ch) { return ch.category() == QChar::Other_Control; });
}

bool validSummary(const QJsonObject& summary) {
    const auto strings = [&](const char* field, qsizetype limit) {
        const auto value = summary.value(field);
        if (!value.isArray()) return false;
        const auto array = value.toArray();
        return !array.isEmpty() && array.size() <= limit && std::all_of(array.begin(), array.end(),
            [](const auto& item) { return item.isString() && item.toString().size() <= 55; });
    };
    const int peers = summary.value("peer_count").toInt();
    const int dns = summary.value("dns_count").toInt(-1);
    return summary.size() == 7 && summary.value("kind").toString() == "wireguard" &&
        strings("addresses", 16) && strings("routes", 256) && peers >= 1 && peers <= 32 &&
        dns >= 0 && dns <= 8 && summary.value("full_v4").isBool() && summary.value("full_v6").isBool();
}

QJsonObject toJson(const WireGuardProfile& profile) {
    return {{"id",profile.id}, {"name",profile.name}, {"candidate",profile.candidate},
            {"active",profile.active}, {"last_good",profile.lastGood}, {"updated",profile.updated},
            {"summary",profile.summary}};
}

Result readSealed(PlatformServices& platform, const QString& path, qsizetype limit, QJsonObject& object) {
    const QFileInfo info(path);
    if (!safeEntry(path) || !info.isFile()) return Result::fail("WireGuard 数据文件不可用");
    QByteArray encrypted, plain;
    auto read = readFile(path, encrypted, limit);
    if (read.ok) read = platform.open(encrypted, plain);
    if (!read.ok || plain.size() > limit) return Result::fail("WireGuard 数据无法读取或解密");
    const auto document = QJsonDocument::fromJson(plain);
    plain.fill('\0');
    if (!document.isObject()) return Result::fail("WireGuard 数据格式损坏");
    object = document.object();
    return {};
}

Result writeSealed(PlatformServices& platform, const QString& path, const QJsonObject& object, qsizetype limit) {
    if (!safeEntry(path)) return Result::fail("WireGuard 数据路径不可用");
    QByteArray encrypted;
    auto plain = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (plain.size() > limit) return Result::fail("WireGuard 数据超过大小限制");
    const auto seal = platform.seal(plain, encrypted);
    plain.fill('\0');
    if (!seal.ok || encrypted.isEmpty() || encrypted.size() > limit)
        return Result::fail("WireGuard 数据加密失败，保留原状态");
    const auto result = writeAtomic(path, encrypted);
    return result.ok ? Result{} : Result::fail("WireGuard 数据写入失败，保留原状态");
}
}

WireGuardStore::WireGuardStore(const QString& dataRoot, std::shared_ptr<PlatformServices> platform)
    : root_(QDir(dataRoot).filePath("wireguard")), platform_(std::move(platform)) {}

QString WireGuardStore::versionPath(const QString& id, const QString& version) const {
    return root_ + "/versions/" + id + '_' + version + ".dat";
}

WireGuardCatalog WireGuardStore::load() const {
    const auto corrupt = Result::fail("WireGuard 索引损坏或无法解密，保留原数据并停止写入");
    const auto path = root_ + "/catalog.dat";
    if (!safeEntry(root_) || !safeEntry(path)) return {corrupt, {}};
    if (!QFileInfo::exists(path)) return {};

    QJsonObject object;
    const auto result = readSealed(*platform_, path, catalogLimit, object);
    if (!result.ok || object.value("schema").toInt() != 1 || !object.value("profiles").isArray())
        return {corrupt, {}};
    const auto records = object.value("profiles").toArray();
    if (records.size() > maxProfiles) return {corrupt, {}};

    WireGuardCatalog catalog;
    QSet<QString> ids;
    for (const auto& record : records) {
        const auto fields = record.toObject();
        if (fields.size() != 7) return {corrupt, {}};
        for (const auto* key : {"id","name","candidate","active","last_good","updated"})
            if (!fields.value(key).isString()) return {corrupt, {}};
        WireGuardProfile row{fields.value("id").toString(), fields.value("name").toString(),
                             fields.value("candidate").toString(), fields.value("active").toString(),
                             fields.value("last_good").toString(), fields.value("updated").toString(),
                             fields.value("summary").toObject()};
        if (!isId(row.id) || ids.contains(row.id) || !validName(row.name) || !isId(row.candidate) ||
            (!row.active.isEmpty() && !isId(row.active)) || (!row.lastGood.isEmpty() && !isId(row.lastGood)) ||
            !QDateTime::fromString(row.updated, Qt::ISODate).isValid() || !validSummary(row.summary))
            return {corrupt, {}};
        ids.insert(row.id);
        catalog.profiles.append(std::move(row));
    }
    return catalog;
}

WireGuardCatalog WireGuardStore::saveCandidate(const QString& name, const QByteArray& source,
                                              const QString& id, const QString& expectedCandidate) const {
    const auto normalizedName = name.trimmed();
    if (!validName(normalizedName)) return {Result::fail("WireGuard 配置名称无效"), {}};

    WireGuardConfig parsed;
    const auto valid = parseWireGuard(source, parsed);
    if (!valid.ok) return {valid, {}};

    auto next = load();
    if (!next.result.ok) return next;
    auto row = std::find_if(next.profiles.begin(), next.profiles.end(),
        [&](const auto& profile) { return profile.id == id; });
    if ((!id.isEmpty() && (row == next.profiles.end() || row->candidate != expectedCandidate)) ||
        (id.isEmpty() && (!expectedCandidate.isEmpty() || next.profiles.size() >= maxProfiles)))
        return {Result::fail("WireGuard 配置数量已满、已被删除或版本已改变"), {}};

    auto updated = row == next.profiles.end() ? WireGuardProfile{} : *row;
    const auto previous = updated.candidate;
    if (updated.id.isEmpty()) updated.id = newId();
    updated.name = normalizedName;
    updated.candidate = newId();
    updated.updated = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    updated.summary = valid.data;

    const auto versions = root_ + "/versions";
    if (!safeEntry(root_) || !safeEntry(versions) ||
        !platform_->secureDirectory(root_).ok || !platform_->secureDirectory(versions).ok)
        return {Result::fail("无法准备 WireGuard 私有数据目录"), {}};
    const auto path = versionPath(updated.id, updated.candidate);
    if (QFileInfo::exists(path)) return {Result::fail("WireGuard 版本已存在"), {}};
    const QJsonObject snapshot{{"schema",1}, {"id",updated.id}, {"version",updated.candidate},
                               {"source",QString::fromLatin1(source.toBase64())}};
    auto result = writeSealed(*platform_, path, snapshot, versionLimit);
    if (!result.ok) return {result, {}};

    WireGuardConfig checked;
    result = readVersion(updated.id, updated.candidate, checked);
    if (result.ok && checked.summary() != updated.summary)
        result = Result::fail("WireGuard 版本读回与候选不一致");
    if (result.ok) {
        if (row == next.profiles.end()) next.profiles.append(updated);
        else *row = updated;
        QJsonArray records;
        for (const auto& profile : next.profiles) records.append(toJson(profile));
        result = writeSealed(*platform_, root_ + "/catalog.dat", {{"schema",1},{"profiles",records}}, catalogLimit);
    }
    if (!result.ok) {
        QFile::remove(path);
        return {result, {}};
    }
    if (!previous.isEmpty() && previous != updated.active && previous != updated.lastGood) {
        const auto old = versionPath(updated.id, previous);
        if (safeEntry(old)) QFile::remove(old);
    }
    return next;
}

Result WireGuardStore::readVersion(const QString& id, const QString& version, WireGuardConfig& config,
                                   QByteArray* original) const {
    if (!isId(id) || !isId(version) || !safeEntry(root_) || !safeEntry(root_+"/versions"))
        return Result::fail("WireGuard 版本路径无效");
    QJsonObject snapshot;
    const auto read = readSealed(*platform_, versionPath(id,version), versionLimit, snapshot);
    if (!read.ok) return read;
    if (snapshot.size() != 4 || snapshot.value("schema").toInt() != 1 ||
        snapshot.value("id").toString() != id || snapshot.value("version").toString() != version ||
        !snapshot.value("source").isString())
        return Result::fail("WireGuard 版本身份或格式损坏");
    auto decoded = QByteArray::fromBase64Encoding(snapshot.value("source").toString().toLatin1(),
                                                  QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) return Result::fail("WireGuard 版本来源损坏");
    auto result = parseWireGuard(decoded.decoded, config);
    if (result.ok && original) *original=std::move(decoded.decoded);
    else decoded.decoded.fill('\0');
    return result;
}

WireGuardCatalog WireGuardStore::markStarted(const QString& id,const QString& version) const {
    auto catalog=load();
    if (!catalog.result.ok) return catalog;
    auto row=std::find_if(catalog.profiles.begin(),catalog.profiles.end(),[&](const auto& profile) { return profile.id==id; });
    if (row==catalog.profiles.end() || (row->candidate!=version && row->active!=version && row->lastGood!=version))
        return {Result::fail("WireGuard 配置版本已改变"),{}};
    WireGuardConfig config;
    const auto read=readVersion(id,version,config);
    if (!read.ok) return {read,{}};
    if (row->active==version) return catalog;
    row->lastGood=row->active.isEmpty()?version:row->active;
    row->active=version;
    QJsonArray records;
    for (const auto& profile:catalog.profiles) records.append(toJson(profile));
    const auto write=writeSealed(*platform_,root_+"/catalog.dat",{{"schema",1},{"profiles",records}},catalogLimit);
    return write.ok?catalog:WireGuardCatalog{write,{}};
}

WireGuardCatalog WireGuardStore::updateProfile(const QString& id,const std::optional<QString>& name) const {
    auto catalog=load();
    if (!catalog.result.ok) return catalog;
    auto row=std::find_if(catalog.profiles.begin(),catalog.profiles.end(),[&](const auto& profile) { return profile.id==id; });
    if (row==catalog.profiles.end() || (name && !validName(name->trimmed())))
        return {Result::fail("WireGuard 配置或名称无效"),{}};
    const auto previous=*row;
    if (name) row->name=name->trimmed();
    else catalog.profiles.erase(row);
    QJsonArray records;
    for (const auto& profile:catalog.profiles) records.append(toJson(profile));
    const auto write=writeSealed(*platform_,root_+"/catalog.dat",{{"schema",1},{"profiles",records}},catalogLimit);
    if (!write.ok) return {write,{}};
    if (!name && safeEntry(root_+"/versions")) {
        for (const auto& version:QSet<QString>{previous.candidate,previous.active,previous.lastGood}) {
            if (!isId(version)) continue;
            const auto path=versionPath(id,version);
            if (safeEntry(path)) QFile::remove(path);
        }
    }
    return catalog;
}
}
