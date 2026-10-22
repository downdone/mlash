#include "BackupArchive.h"
#include "ProviderEdits.h"
#include "platform/PortableCrypto.h"
#include "storage/Storage.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>

namespace mlash {
namespace {
constexpr qint64 ArchiveLimit = 64 * 1024 * 1024;
QString hash(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
Result readArchive(const QString& path, const QString& password, QJsonObject& archive, QString& digest) {
    QByteArray encrypted, plain; auto result = readFile(path, encrypted, ArchiveLimit + 52); if (!result.ok) return result;
    digest = hash(encrypted); result = openPortable(encrypted, password, plain); if (!result.ok) return result;
    if (plain.size() > ArchiveLimit) return Result::fail("备份内容超过 64 MiB");
    const auto document = QJsonDocument::fromJson(plain);
    if (!document.isObject()) return Result::fail("备份内容不是有效对象"); archive = document.object();
    if (archive.value("schema").toDouble() != 1 || !archive.value("settings").isObject() || !archive.value("profiles").isArray() || archive.value("profiles").toArray().size() > 256) return Result::fail("备份版本或结构不受支持");
    return {};
}
QByteArray decode(const QJsonValue& value) {
    if (!value.isString()) return {};
    const auto result = QByteArray::fromBase64Encoding(value.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    return result ? result.decoded : QByteArray{};
}
}
Result BackupArchive::create(ProfileStore& store, const Settings& settings, const QVector<Profile>& profiles, const QString& path, const QString& password, bool includeProfiles) {
    if (QFileInfo(path).suffix().toLower() != "mlashbackup") return Result::fail("备份文件请使用 .mlashbackup 扩展名");
    QJsonArray bundles; qint64 bytesRead = 0;
    if (includeProfiles) for (const auto& profile : profiles) {
        QJsonArray versions; QSet<QString> included;
        for (const auto& version : {profile.candidate, profile.active, profile.lastGood}) {
            if (version.isEmpty() || included.contains(version)) continue; included.insert(version);
            const auto root = store.versionRoot(profile.id, version); QByteArray source; auto result = readFile(root + "/source.yaml", source); if (!result.ok) return Result::fail("备份无法读取某个订阅版本，未生成不完整备份");
            auto frozen=profile; result=store.versionProfile(profile.id,version,frozen); if (!result.ok) return result; const auto overrides=ProfileStore::overrideDocument(frozen);
            auto built = ConfigBuilder::buildProfile(source, settings, settings.mixedPort == 19090 ? 19091 : 19090, QString(64,'x'), frozen); if (!built.result.ok) return built.result;
            QJsonObject providerFiles;
            QJsonObject snapshotRecords; QByteArray snapshotBytes;
            if (QFileInfo::exists(root + "/work/provider-snapshots.json")) {
                result = readFile(root + "/work/provider-snapshots.json", snapshotBytes, 1024 * 1024); const auto parsed = QJsonDocument::fromJson(snapshotBytes);
                if (!result.ok || !parsed.isObject()) return Result::fail("提供者清单损坏，未生成不完整备份"); snapshotRecords = parsed.object();
            }
            bytesRead += source.size()+QJsonDocument(overrides).toJson(QJsonDocument::Compact).size();
            if (bytesRead > ArchiveLimit / 2) return Result::fail("备份源数据超过 32 MiB，请分批备份");
            for (const auto& value : built.result.data.value("providers").toArray()) {
                const auto provider = value.toObject(); QByteArray payload; result = readFile(root + "/work/" + provider.value("path").toString(), payload); if (!result.ok) return Result::fail("备份缺少提供者快照，未生成不完整备份");
                const auto checked = ConfigBuilder::checkProvider(payload, provider.value("kind").toString(), provider.value("format").toString(), provider.value("behavior").toString()); if (!checked.result.ok) return checked.result;
                bytesRead += payload.size(); if (bytesRead > ArchiveLimit / 2) return Result::fail("备份源数据超过 32 MiB，请分批备份");
                const auto relative = provider.value("path").toString(); const auto record = snapshotRecords.value(relative).toObject();
                if ((record.contains("source_sha256")&&record.value("source_sha256").toString()!=providerSourceSignature(provider))||(!record.contains("source_sha256")&&provider.value("kind")=="rule-providers"&&frozen.ruleProviderEdits.contains(provider.value("name").toString()))) return Result::fail("提供者声明与快照来源不一致，未生成不完整备份");
                if (record.contains("sha256") && record.value("sha256").toString() != hash(payload)) return Result::fail("提供者快照哈希不一致，未生成不完整备份");
                providerFiles[relative] = QJsonObject{{"payload", QString::fromLatin1(payload.toBase64())}, {"updated_utc", record.value("updated_utc").toString(QFileInfo(root + "/work/" + relative).lastModified().toUTC().toString(Qt::ISODate))}};
            }
            versions.append(QJsonObject{{"id", version}, {"source", QString::fromLatin1(source.toBase64())}, {"providers", providerFiles},{"overrides",overrides}});
        }
        bundles.append(QJsonObject{{"profile", profile.toJson()}, {"versions", versions}});
    }
    auto preferences = settings; preferences.tunEnabled=false; preferences.desiredProxy = false; preferences.restoreProxy = false; preferences.autoStart = false; preferences.startCore = false; preferences.activeProfile.clear(); preferences.corePath.clear(); preferences.previousCorePath.clear(); preferences.remoteBackup={}; preferences.localBackup={};
    const auto plain = QJsonDocument(QJsonObject{{"schema", 1}, {"created_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"settings", preferences.toJson()}, {"profiles", bundles}}).toJson(QJsonDocument::Compact);
    if (plain.size() > ArchiveLimit) return Result::fail("备份内容超过 64 MiB，未覆盖现有文件");
    QByteArray encrypted; auto result = sealPortable(plain, password, encrypted); if (!result.ok) return result;
    return writeAtomic(path, encrypted);
}
QStringList restoreProfileNames(const QStringList& names, const QVector<Profile>& current) {
    QSet<QString> occupied; for (const auto& profile:current) occupied.insert(profile.name.toCaseFolded());
    QStringList result; for (const auto& original:names) { const auto base=original.left(120); auto name=base; int index=2; while (occupied.contains(name.toCaseFolded())) { const auto suffix=QString("（恢复 %1）").arg(index++); name=base.left(120-suffix.size())+suffix; } occupied.insert(name.toCaseFolded()); result.append(name); } return result;
}
Result BackupArchive::inspect(const QString& path, const QString& password) {
    QJsonObject archive; QString digest; auto result = readArchive(path, password, archive, digest); if (!result.ok) return result;
    QJsonArray names; for (const auto& value : archive.value("profiles").toArray()) {
        if (!value.isObject() || !value.toObject().value("profile").isObject() || !value.toObject().value("versions").isArray()) return Result::fail("备份订阅结构损坏");
        names.append(value.toObject().value("profile").toObject().value("name").toString().left(120));
    }
    return {true, {}, {{"profiles", names}, {"created_utc", archive.value("created_utc")}, {"sha256", digest}}};
}
BackupRestore BackupArchive::prepareRestore(ProfileStore& store, const std::shared_ptr<PlatformServices>& platform, const Settings& current, const QVector<Profile>& profiles, const QString& path, const QString& password, const QString& expectedHash, bool restoreSettings, const QJsonValue& selectedProfiles) {
    BackupRestore restored; restored.snapshot.settings = current; restored.snapshot.profiles = profiles;
    QJsonObject archive; QString digest; auto result = readArchive(path, password, archive, digest);
    const auto failed = [&](const QString& error) { for (const auto& id : restored.createdProfiles) store.removeProfile(id); restored.createdProfiles.clear(); restored.snapshot.result = Result::fail(error); return restored; };
    if (!result.ok) return failed(result.error);
    if (expectedHash.isEmpty() || digest != expectedHash) return failed("备份文件在预览后发生变化，请重新预览");
    const auto bundles = archive.value("profiles").toArray(); QSet<int> selected;
    if (selectedProfiles.isUndefined()) { for (int i=0;i<bundles.size();++i) selected.insert(i); }
    else { if (!selectedProfiles.isArray()||selectedProfiles.toArray().size()>256) return failed("恢复选择无效，请重新预览"); for (const auto& value:selectedProfiles.toArray()) { if (!value.isDouble()||value.toDouble()<0||value.toDouble()>=bundles.size()||value.toDouble()!=value.toInt()||selected.contains(value.toInt())) return failed("恢复选择无效，请重新预览"); selected.insert(value.toInt()); } }
    if (selected.isEmpty()&&!restoreSettings) return failed("请选择订阅或恢复偏好");
    if (selected.size() + profiles.size() > 256) return failed("恢复后订阅数量超过 256");
    QJsonArray metadata;
    for (const auto& value : bundles) { if (!value.isObject() || !value.toObject().value("profile").isObject() || !value.toObject().value("versions").isArray()) return failed("备份订阅结构损坏"); metadata.append(value.toObject().value("profile")); }
    QTemporaryDir staging(store.root() + "/restore-check-XXXXXX"); if (!staging.isValid()) return failed("无法创建恢复校验目录");
    result = platform->secureDirectory(staging.path()); if (!result.ok) return failed(result.error);
    QByteArray index; result = platform->seal(QJsonDocument(QJsonObject{{"schema", 1}, {"settings", archive.value("settings")}, {"profiles", metadata}}).toJson(), index); if (!result.ok) return failed(result.error);
    result = writeAtomic(staging.path() + "/state.dat", index); if (!result.ok) return failed(result.error);
    ProfileStore checker(staging.path(), platform); auto validated = checker.load(); if (!validated.result.ok) return failed("备份设置或订阅元数据无效，未修改当前数据");
    if (restoreSettings) {
        restored.snapshot.settings = validated.settings; restored.snapshot.settings.autoStart = current.autoStart; restored.snapshot.settings.corePath = current.corePath;
        restored.snapshot.settings.hotkeys=current.hotkeys;
        restored.snapshot.settings.previousCorePath=current.previousCorePath; restored.snapshot.settings.tunEnabled=false;
        restored.snapshot.settings.remoteBackup=current.remoteBackup;
        restored.snapshot.settings.localBackup=current.localBackup;
        restored.snapshot.settings.desiredProxy = false; restored.snapshot.settings.restoreProxy = false; restored.snapshot.settings.startCore = false; restored.snapshot.settings.activeProfile.clear();
    }
    QStringList selectedNames; for (int i=0;i<bundles.size();++i) if (selected.contains(i)) selectedNames.append(validated.profiles[i].name); const auto restoredNames=restoreProfileNames(selectedNames,profiles); int nameIndex=0;
    for (int i = 0; i < bundles.size(); ++i) {
        if (!selected.contains(i)) continue;
        auto profile = validated.profiles[i]; profile.name=restoredNames[nameIndex++]; const auto candidate = profile.candidate, active = profile.active, lastGood = profile.lastGood;
        profile.id = newId(); profile.candidate.clear(); profile.active.clear(); profile.lastGood.clear(); profile.updateError.clear(); profile.lastUpdateAttempt.clear(); profile.autoUpdate = false;
        for (auto it=profile.providerPolicies.begin();it!=profile.providerPolicies.end();++it) { auto policy=it.value().toObject(); policy["automatic"]=false; it.value()=policy; }
        restored.createdProfiles.append(profile.id);
        const auto versions = bundles[i].toObject().value("versions").toArray(); if (versions.isEmpty() || versions.size() > 3) return failed("备份版本数无效");
        QHash<QString, QString> mapped; QByteArray candidateSource;
        for (const auto& value : versions) {
            const auto record = value.toObject(); const auto old = record.value("id").toString(); const auto source = decode(record.value("source"));
            if (!value.isObject() || !isId(old) || mapped.contains(old) || !record.value("providers").isObject() || source.isEmpty() || source.size() > 8 * 1024 * 1024) return failed("备份配置版本损坏");
            auto frozen=profile; if (record.contains("overrides")) { if (!record.value("overrides").isObject()) return failed("备份覆盖快照损坏"); const auto applied=ProfileStore::applyOverrides(frozen,record.value("overrides").toObject()); if (!applied.ok) return failed(applied.error); }
            auto prepared = store.prepare(frozen, source, restored.snapshot.settings, restored.snapshot.settings.mixedPort == 19090 ? 19091 : 19090, QString(64,'x'), false); if (!prepared.result.ok) return failed(prepared.result.error);
            const auto files = record.value("providers").toObject(); const auto expected = prepared.result.data.value("providers").toArray(); if (files.size() != expected.size()) return failed("备份提供者集合不完整或含未知文件");
            for (const auto& item : expected) {
                const auto provider = item.toObject(); const auto saved = files.value(provider.value("path").toString()).toObject(); const auto payload = decode(saved.value("payload")); const auto stamp = saved.value("updated_utc").toString();
                if (!QDateTime::fromString(stamp, Qt::ISODate).isValid()) return failed("备份提供者时间无效");
                const auto checked = ConfigBuilder::checkProvider(payload, provider.value("kind").toString(), provider.value("format").toString(), provider.value("behavior").toString()); if (!checked.result.ok) return failed(checked.result.error);
                result = writeAtomic(prepared.workPath + '/' + provider.value("path").toString(), payload); if (!result.ok) return failed(result.error);
                result = store.recordProviderSnapshot(prepared.workPath, provider, payload); if (!result.ok) return failed(result.error);
                QByteArray manifest; result = readFile(prepared.workPath + "/provider-snapshots.json", manifest, 1024 * 1024); if (!result.ok) return failed(result.error);
                auto records = QJsonDocument::fromJson(manifest).object(); auto metadata = records.value(provider.value("path").toString()).toObject(); metadata["updated_utc"] = stamp; records[provider.value("path").toString()] = metadata;
                result = writeAtomic(prepared.workPath + "/provider-snapshots.json", QJsonDocument(records).toJson(QJsonDocument::Compact)); if (!result.ok) return failed(result.error);
            }
            mapped.insert(old, prepared.version); if (old == candidate) candidateSource = source;
        }
        if (!mapped.contains(candidate) || (!active.isEmpty() && !mapped.contains(active)) || (!lastGood.isEmpty() && !mapped.contains(lastGood))) return failed("备份缺少引用的配置版本");
        profile.candidate = mapped.value(candidate); profile.active = mapped.value(active); profile.lastGood = mapped.value(lastGood);
        if (profile.sourceType == "file") { profile.source = store.root() + "/profiles/" + profile.id + "/restored-source.yaml"; result = writeAtomic(profile.source, candidateSource); if (!result.ok) return failed(result.error); }
        restored.snapshot.profiles.append(profile);
    }
    restored.snapshot.result = {}; return restored;
}
}
