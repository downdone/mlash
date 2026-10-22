#include "app/AppController.h"
#include "core/OfficialCoreSource.h"
#include "core/TunPolicy.h"
#include "storage/Storage.h"
#include <QtTest>
#include <QDataStream>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QtZlib/zlib.h>

using namespace mlash;
namespace {
QString sha(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
QByteArray zip(const QByteArray& payload,const QByteArray& name="mihomo.exe",bool deflate=true) {
    QByteArray compressed=payload;
    if (deflate) {
        compressed.resize(compressBound(payload.size())); z_stream stream{};
        stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(payload.constData())); stream.avail_in=payload.size();
        stream.next_out=reinterpret_cast<Bytef*>(compressed.data()); stream.avail_out=compressed.size();
        if (deflateInit2(&stream,6,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY)!=Z_OK) return {};
        const auto result=::deflate(&stream,Z_FINISH); compressed.resize(stream.total_out); deflateEnd(&stream); if (result!=Z_STREAM_END) return {};
    }
    const auto crc=quint32(crc32(0,reinterpret_cast<const Bytef*>(payload.constData()),payload.size()));
    QByteArray archive; QDataStream out(&archive,QIODevice::WriteOnly); out.setByteOrder(QDataStream::LittleEndian);
    out<<quint32(0x04034b50)<<quint16(20)<<quint16(0)<<quint16(deflate?8:0)<<quint16(0)<<quint16(0)<<crc<<quint32(compressed.size())<<quint32(payload.size())<<quint16(name.size())<<quint16(0);
    out.writeRawData(name.constData(),name.size()); out.writeRawData(compressed.constData(),compressed.size());
    const auto central=archive.size();
    out<<quint32(0x02014b50)<<quint16(20)<<quint16(20)<<quint16(0)<<quint16(deflate?8:0)<<quint16(0)<<quint16(0)<<crc<<quint32(compressed.size())<<quint32(payload.size())<<quint16(name.size())<<quint16(0)<<quint16(0)<<quint16(0)<<quint16(0)<<quint32(0)<<quint32(0);
    out.writeRawData(name.constData(),name.size()); const auto centralSize=archive.size()-central;
    out<<quint32(0x06054b50)<<quint16(0)<<quint16(0)<<quint16(1)<<quint16(1)<<quint32(centralSize)<<quint32(central)<<quint16(0);
    return archive;
}
QJsonObject release(const QByteArray& archive,const QString& version="v9.99.0") {
    const auto asset="mihomo-windows-amd64-v1-"+version+".zip";
    return {{"tag_name",version},{"draft",false},{"prerelease",false},{"assets",QJsonArray{QJsonObject{
        {"name",asset},{"size",archive.size()},{"digest","sha256:"+sha(archive)},
        {"browser_download_url","https://github.com/MetaCubeX/mihomo/releases/download/"+version+'/'+asset}}}}};
}
class SourceFixture final : public SubscriptionDownloader {
public:
    QList<QPair<Result,QByteArray>> responses; QList<QUrl> urls; int cancels=0;
    void download(const QUrl& url,std::function<void(Result,QByteArray)> done,const DownloadPolicy&,int,qint64 maximum,bool) override {
        urls.append(url); const auto response=responses.isEmpty()?QPair<Result,QByteArray>{Result::fail("fixture empty"),{}}:responses.takeFirst();
        QTimer::singleShot(20,this,[response,maximum,done=std::move(done)] { done(response.second.size()>maximum?Result::fail("budget"):response.first,response.second); });
    }
    void cancel() override { ++cancels; }
};
class IsolatedPlatform final : public PlatformServices {
public:
    int writes=0;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; } bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
}
class OfficialCoreTests final : public QObject {
    Q_OBJECT
private slots:
    void metadataRejectsUntrustedOrIncompleteAssets() {
        const auto good=release("archive"); QVERIFY(OfficialCoreSource::parseRelease(QJsonDocument(good).toJson()).ok);
        auto bad=good; bad["prerelease"]=true; QVERIFY(!OfficialCoreSource::parseRelease(QJsonDocument(bad).toJson()).ok);
        bad=good; auto assets=bad.value("assets").toArray(); auto asset=assets[0].toObject(); asset["digest"]=QJsonValue(); assets[0]=asset; bad["assets"]=assets;
        QVERIFY(!OfficialCoreSource::parseRelease(QJsonDocument(bad).toJson()).ok);
        asset=good.value("assets").toArray()[0].toObject(); asset["browser_download_url"]="https://evil.test/mihomo.exe"; assets[0]=asset; bad["assets"]=assets;
        QVERIFY(!OfficialCoreSource::parseRelease(QJsonDocument(bad).toJson()).ok);
        bad=good; assets=good.value("assets").toArray(); assets.append(assets[0]); bad["assets"]=assets; QVERIFY(!OfficialCoreSource::parseRelease(QJsonDocument(bad).toJson()).ok);
        QVERIFY(!OfficialCoreSource::parseRelease("{\"message\":\"API rate limit exceeded\"}").ok);
    }
    void archiveChecksHashPathsBoundsAndCrc() {
        const QByteArray payload(512,'x'); QByteArray output;
        for (bool compressed:{false,true}) {
            const auto archive=zip(payload,"mihomo.exe",compressed); const auto metadata=OfficialCoreSource::parseRelease(QJsonDocument(release(archive)).toJson()); QVERIFY(metadata.ok);
            QVERIFY(OfficialCoreSource::unpack(archive,metadata.data,output).ok); QCOMPARE(output,payload);
            auto tampered=archive; tampered[35]^=1; QVERIFY(!OfficialCoreSource::unpack(tampered,metadata.data,output).ok); QVERIFY(output.isEmpty());
            auto resigned=metadata.data; resigned["sha256"]=sha(tampered); QVERIFY(!OfficialCoreSource::unpack(tampered,resigned,output).ok);
        }
        const auto archive=zip(payload,"../mihomo.exe"); auto record=OfficialCoreSource::parseRelease(QJsonDocument(release(archive)).toJson()); QVERIFY(!OfficialCoreSource::unpack(archive,record.data,output).ok);
        const auto truncated=zip(payload).chopped(3); record=OfficialCoreSource::parseRelease(QJsonDocument(release(truncated)).toJson()); QVERIFY(!OfficialCoreSource::unpack(truncated,record.data,output).ok);
    }
    void cancellationIgnoresLateMetadata() {
        auto fixture=std::make_unique<SourceFixture>(); auto* transport=fixture.get(); fixture->responses.append({{},QJsonDocument(release("test")).toJson()});
        OfficialCoreSource source(nullptr,std::move(fixture)); int calls=0; Result result;
        source.check([&](Result r) { ++calls; result=r; }); QVERIFY(source.busy()); source.cancel(); QCOMPARE(calls,1); QVERIFY(!result.ok);
        QTest::qWait(50); QCOMPARE(calls,1); QVERIFY(source.release().isEmpty()); QVERIFY(!source.busy()); QVERIFY(transport->cancels>0);
    }
    void controllerDownloadPreflightExplicitApplyAndVersionMismatch() {
        QByteArray executable; QVERIFY(readFile(QCoreApplication::applicationDirPath()+"/fake_core_update.exe",executable,128*1024*1024).ok);
        const auto archive=zip(executable);
        QTemporaryDir data; auto platform=std::make_shared<IsolatedPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore=false; settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; QVERIFY(store.save(settings,{}).ok);
        auto fixture=std::make_unique<SourceFixture>(); auto* transport=fixture.get();
        transport->responses={{{},QJsonDocument(release(archive)).toJson()},{{},archive}};
        AppController c(data.path(),true,nullptr,platform,WebsiteTests::defaults(),{},{},{},{},std::move(fixture));
        QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        bool done=false; Result checked; c.checkOfficialCore([&](Result r) { checked=r; done=true; }); QTRY_VERIFY(done); QVERIFY(checked.ok);
        done=false; c.downloadOfficialCore([&](Result r) { checked=r; done=true; }); QTRY_VERIFY_WITH_TIMEOUT(done,10000); QVERIFY2(checked.ok,qUtf8Printable(checked.error));
        QCOMPARE(c.settings().corePath,settings.corePath); QVERIFY(!checked.data.value("token").toString().isEmpty()); QVERIFY(CoreUpdate::verify(data.path(),checked.data).ok);
        const auto token=checked.data.value("token").toString();
        auto& intent=const_cast<Settings&>(c.settings()); intent.tunEnabled=true;
        auto before=finished.size(); c.applyCoreUpdate(token); QCOMPARE(finished.size(),before+1); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.settings().corePath,settings.corePath);
        intent.tunEnabled=false; before=finished.size(); c.applyCoreUpdate(token); QTRY_VERIFY_WITH_TIMEOUT(finished.size()>before,10000); QVERIFY(finished.last()[0].toBool()); QVERIFY(c.settings().corePath.contains("/core-updates/"));
        const auto applied=c.settings().corePath;
        transport->responses={{{},QJsonDocument(release(archive,"v9.99.1")).toJson()},{{},archive}};
        done=false; c.checkOfficialCore([&](Result r) { checked=r; done=true; }); QTRY_VERIFY(done); QVERIFY(checked.ok);
        done=false; c.downloadOfficialCore([&](Result r) { checked=r; done=true; }); QTRY_VERIFY_WITH_TIMEOUT(done,10000); QVERIFY(!checked.ok); QVERIFY(checked.error.contains("版本")); QCOMPARE(c.settings().corePath,applied); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(OfficialCoreTests)
#include "OfficialCoreTests.moc"
