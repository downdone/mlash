#include "AppUpdateSource.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

namespace mlash {
namespace {
constexpr qint64 MetadataLimit=2*1024*1024,ArchiveLimit=128*1024*1024;
const QRegularExpression VersionPattern("^v([0-9]{1,4}\\.[0-9]{1,4}\\.[0-9]{1,4})$");
const QRegularExpression DigestPattern("^sha256:([a-f0-9]{64})$");
}
AppUpdateSource::AppUpdateSource(QObject* parent,std::unique_ptr<SubscriptionDownloader> downloader)
    :QObject(parent),downloader_(downloader?std::move(downloader):std::make_unique<SubscriptionDownloader>()) {}

Result AppUpdateSource::parseReleases(const QByteArray& bytes,const QString& currentVersion) {
    if (bytes.size()>MetadataLimit) return Result::fail("应用版本元数据超过2 MiB");
    const auto document=QJsonDocument::fromJson(bytes);
    if (!document.isArray() || document.array().size()>50) return Result::fail("应用版本列表无效");
    const auto current=QVersionNumber::fromString(currentVersion);
    if (current.segmentCount()!=3) return Result::fail("当前应用版本无效");
    QVersionNumber highest=current;
    QJsonObject selected;
    bool newerRelease=false;
    for (const auto& item:document.array()) {
        if (!item.isObject()) continue;
        const auto release=item.toObject();
        const auto tag=release.value("tag_name").toString();
        const auto match=VersionPattern.match(tag);
        if (!match.hasMatch() || release.value("draft").toBool(true) || !release.value("prerelease").isBool()) continue;
        const auto version=QVersionNumber::fromString(match.captured(1));
        if (version.segmentCount()!=3 || QVersionNumber::compare(version,highest)<=0) continue;
        newerRelease=true;
        const auto versionText=match.captured(1);
        const auto assetName="Mlash-"+versionText+"-windows-x64-portable.zip";
        QJsonObject asset; int matches=0;
        const auto assets=release.value("assets").toArray();
        if (assets.size()>64) continue;
        for (const auto& entry:assets) if (entry.toObject().value("name").toString()==assetName) { asset=entry.toObject(); ++matches; }
        if (matches!=1) continue;
        const QUrl assetUrl(asset.value("browser_download_url").toString(),QUrl::StrictMode);
        const QUrl releaseUrl(release.value("html_url").toString(),QUrl::StrictMode);
        const auto digest=DigestPattern.match(asset.value("digest").toString());
        const double size=asset.value("size").toDouble();
        if (!assetUrl.isValid() || assetUrl.scheme()!="https" || assetUrl.host()!="github.com" || assetUrl.port()!=-1 ||
            !assetUrl.userInfo().isEmpty() || assetUrl.hasQuery() || assetUrl.hasFragment() ||
            assetUrl.path()!="/downdone/mlash/releases/download/"+tag+'/'+assetName ||
            !releaseUrl.isValid() || releaseUrl.scheme()!="https" || releaseUrl.host()!="github.com" || releaseUrl.port()!=-1 ||
            !releaseUrl.userInfo().isEmpty() || releaseUrl.hasQuery() || releaseUrl.hasFragment() ||
            releaseUrl.path()!="/downdone/mlash/releases/tag/"+tag || !digest.hasMatch() ||
            !asset.value("size").isDouble() || size<1 || size>ArchiveLimit || size!=double(qint64(size))) continue;
        highest=version;
        selected={{"version",versionText},{"tag",tag},{"asset",assetName},{"url",assetUrl.toString()},
                  {"sha256",digest.captured(1)},{"bytes",size},{"release_url",releaseUrl.toString()},
                  {"prerelease",release.value("prerelease").toBool()}};
    }
    if (selected.isEmpty() && newerRelease) return Result::fail("新版缺少可验证的 Windows x64 便携包或 SHA-256");
    if (selected.isEmpty()) return {true,{},{{"up_to_date",true},{"version",currentVersion}}};
    return {true,{},selected};
}

Result AppUpdateSource::verifyArchive(const QByteArray& bytes,const QJsonObject& release) {
    const auto expected=release.value("bytes").toDouble();
    if (bytes.isEmpty() || bytes.size()>ArchiveLimit || bytes.size()!=expected ||
        QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())!=release.value("sha256").toString())
        return Result::fail("应用更新包的大小或 SHA-256 不符，未保存候选");
    return {};
}

Result AppUpdateSource::stageArchive(const QByteArray& bytes,const QJsonObject& release,const QString& dataRoot) {
    const auto checked=verifyArchive(bytes,release);
    if (!checked.ok) return checked;
    const auto asset=release.value("asset").toString();
    if (!QRegularExpression("^Mlash-[0-9]{1,4}\\.[0-9]{1,4}\\.[0-9]{1,4}-windows-x64-portable\\.zip$").match(asset).hasMatch())
        return Result::fail("更新包名称无效");
    const QString directory=QDir(dataRoot).filePath("updates");
    if (QFileInfo(directory).isSymLink() || !QDir().mkpath(directory)) return Result::fail("无法创建应用更新目录");
    const QString target=QDir(directory).filePath(asset);
    if (QFileInfo(target).isSymLink()) return Result::fail("更新包目标不能是符号链接");
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())
        return Result::fail("更新包保存失败；当前应用保持不变");
    return Result{true,{},{{"version",release.value("version")},{"path",target},
                            {"sha256",release.value("sha256")},{"bytes",release.value("bytes")},
                            {"release_url",release.value("release_url")},{"prerelease",release.value("prerelease")}}};
}

void AppUpdateSource::check(const QString& currentVersion,Completion done) {
    if (busy_) { done(Result::fail("应用更新任务正在进行")); return; }
    busy_=true; release_={}; const auto token=++generation_;
    done_=[done=std::move(done)](Result result,QByteArray) mutable { done(result); };
    downloader_->download(QUrl("https://api.github.com/repos/downdone/mlash/releases?per_page=20"),
        [this,token,currentVersion](Result result,QByteArray bytes) {
            if (token!=generation_) return;
            if (result.ok) result=parseReleases(bytes,currentVersion);
            else result.error="检查应用版本失败。"+result.error;
            if (result.ok && !result.data.value("up_to_date").toBool()) release_=result.data;
            busy_=false; auto done=std::move(done_); if (done) done(result,{});
        },{},0,MetadataLimit);
}

void AppUpdateSource::download(DownloadDone done) {
    if (busy_ || release_.isEmpty()) { done(Result::fail("请先检查可用的应用版本"),{}); return; }
    busy_=true; done_=std::move(done); const auto token=++generation_;
    const auto release=release_;
    DownloadPolicy policy; policy.timeoutSeconds=120;
    downloader_->download(QUrl(release.value("url").toString()),[this,token,release](Result result,QByteArray bytes) {
        if (token!=generation_) return;
        result.data=release; busy_=false; auto done=std::move(done_);
        if (done) done(result,result.ok?std::move(bytes):QByteArray{});
    },policy,0,ArchiveLimit);
}

void AppUpdateSource::cancel() {
    if (!busy_) return;
    ++generation_; downloader_->cancel(); busy_=false;
    auto done=std::move(done_); if (done) done(Result::fail("应用更新已取消"),{});
}
}
