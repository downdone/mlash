#include "profile/MrsSnapshot.h"
#include "profile/ConfigBuilder.h"
#include "profile/ProfileStore.h"
#include "profile/BackupArchive.h"
#include "storage/Storage.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QJsonArray>
#include <zstd.h>
#include <QtEndian>

using namespace mlash;
class MrsPlatform final : public PlatformServices {
public:
    Result readProxy() override { return {}; }
    Result writeProxy(const QJsonObject&) override { return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {}; }
    Result seal(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {}; }
};
class MrsTests final : public QObject {
    Q_OBJECT
    QString core() const { return QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; }
    QByteArray compress(const QByteArray& plain) {
        QByteArray encoded(qsizetype(ZSTD_compressBound(size_t(plain.size()))),Qt::Uninitialized);
        const auto size=ZSTD_compress(encoded.data(),size_t(encoded.size()),plain.constData(),size_t(plain.size()),1); if (ZSTD_isError(size)) return {}; encoded.resize(qsizetype(size)); return encoded;
    }
    QByteArray fixture(const QString& directory,const QString& behavior) {
        const auto source=directory+'/'+behavior+".txt",target=directory+'/'+behavior+".mrs";
        if (!writeAtomic(source,behavior=="domain"?QByteArray("+.example.com\ngithub.com\n"):QByteArray("192.0.2.0/24\n2001:db8::/32\n")).ok) return {};
        QProcess process; process.start(core(),{"convert-ruleset",behavior,"text",source,target}); if (!process.waitForFinished(5000)||process.exitCode()) return {};
        QByteArray bytes; readFile(target,bytes); return bytes;
    }
private slots:
    void realCoreSnapshotsAndReuse() {
        QTemporaryDir temp;
        for (const auto& behavior : QStringList{"domain","ipcidr"}) {
            const auto bytes=fixture(temp.path(),behavior); QVERIFY(!bytes.isEmpty()); const auto inspected=inspectMrs(bytes,behavior); QVERIFY2(inspected.ok,qUtf8Printable(inspected.error)); QVERIFY(inspected.data.value("count").toInt()>=2); QVERIFY(!inspectMrs(bytes,behavior=="domain"?"ipcidr":"domain").ok);
            const QByteArray source="proxies: []\nrule-providers: {frozen: {type: http, behavior: "+behavior.toUtf8()+", format: mrs, url: 'https://example.test/rules?token=private'}}\nrules: ['RULE-SET,frozen,DIRECT','MATCH,DIRECT']\n";
            Settings settings; settings.startCore=false; Profile profile; profile.id=newId(); profile.sourceType="file"; profile.name=behavior; auto platform=std::make_shared<MrsPlatform>(); ProfileStore store(temp.path()+"/data-"+behavior,platform); QVERIFY(store.load().result.ok); auto prepared=store.prepare(profile,source,settings,19090,QString(64,'a')); QVERIFY2(prepared.result.ok,qUtf8Printable(prepared.result.error)); const auto provider=prepared.result.data.value("providers").toArray()[0].toObject(); QCOMPARE(provider.value("behavior").toString(),behavior); QVERIFY(ConfigBuilder::checkProvider(bytes,"rule-providers","mrs",behavior).result.ok); QVERIFY(writeAtomic(prepared.workPath+'/'+provider.value("path").toString(),bytes).ok); QVERIFY(store.recordProviderSnapshot(prepared.workPath,provider,bytes).ok);
            QProcess process; process.start(core(),{"-t","-d",prepared.workPath,"-f",prepared.configPath}); QVERIFY(process.waitForFinished(5000)); QVERIFY2(process.exitCode()==0,process.readAllStandardOutput().constData()); auto rows=store.inspectProviders(prepared.profile,prepared.version,settings); QVERIFY(rows.ok); QCOMPARE(rows.data.value("providers").toArray()[0].toObject().value("count").toInt(),inspected.data.value("count").toInt());
            const auto copied=store.prepareExisting(prepared.profile,prepared.version,settings,19091,QString(64,'b')); QVERIFY2(copied.result.ok,qUtf8Printable(copied.result.error)); QVERIFY(copied.result.data.value("providers").toArray().isEmpty()); QByteArray copy; QVERIFY(readFile(copied.workPath+'/'+provider.value("path").toString(),copy).ok); QCOMPARE(copy,bytes);
            const auto archive=temp.path()+'/'+behavior+".mlashbackup"; QVERIFY(BackupArchive::create(store,settings,{prepared.profile},archive,"mrs-backup-password").ok); const auto preview=BackupArchive::inspect(archive,"mrs-backup-password"); QVERIFY(preview.ok); ProfileStore destination(temp.path()+"/restore-"+behavior,platform); QVERIFY(destination.load().result.ok); const auto restored=BackupArchive::prepareRestore(destination,platform,settings,{},archive,"mrs-backup-password",preview.data.value("sha256").toString(),false); QVERIFY2(restored.snapshot.result.ok,qUtf8Printable(restored.snapshot.result.error)); QCOMPARE(restored.snapshot.profiles.size(),1); const auto restoredProfile=restored.snapshot.profiles[0]; QVERIFY(readFile(destination.versionRoot(restoredProfile.id,restoredProfile.candidate)+"/work/"+provider.value("path").toString(),copy).ok); QCOMPARE(copy,bytes);
            QVERIFY(writeAtomic(prepared.workPath+'/'+provider.value("path").toString(),bytes+"x").ok); QVERIFY(!store.prepareExisting(prepared.profile,prepared.version,settings,19091,QString(64,'b')).result.ok); const auto repair=store.prepareExisting(prepared.profile,prepared.version,settings,19091,QString(64,'b'),true,{"*"}); QVERIFY(repair.result.ok); QCOMPARE(repair.result.data.value("providers").toArray().size(),1);
        }
    }
    void malformedAndBoundedDecode() {
        QTemporaryDir temp; const auto bytes=fixture(temp.path(),"domain"); QVERIFY(!bytes.isEmpty()); QVERIFY(!inspectMrs(bytes.left(bytes.size()-1)).ok); QVERIFY(!inspectMrs(bytes+bytes).ok); QVERIFY(!inspectMrs("not zstd").ok); QVERIFY(!ConfigBuilder::checkProvider(bytes,"proxy-providers","mrs").result.ok);
        QByteArray decoded(1024*1024,Qt::Uninitialized); const auto size=ZSTD_decompress(decoded.data(),size_t(decoded.size()),bytes.constData(),size_t(bytes.size())); QVERIFY(!ZSTD_isError(size)); decoded.resize(qsizetype(size));
        auto bad=decoded; bad[0]='X'; QVERIFY(!inspectMrs(compress(bad)).ok); bad=decoded; bad[4]=2; QVERIFY(!inspectMrs(compress(bad)).ok); bad=decoded; qToBigEndian<quint64>(quint64(1)<<62,bad.data()+13); QVERIFY(!inspectMrs(compress(bad)).ok); bad=decoded; qToBigEndian<quint64>(quint64(1)<<62,bad.data()+22); QVERIFY(!inspectMrs(compress(bad)).ok); bad=decoded; bad.append('x'); QVERIFY(!inspectMrs(compress(bad)).ok);
        QVERIFY(!inspectMrs(compress(QByteArray(64*1024*1024+1,'x'))).ok);
        const auto invalid=ConfigBuilder::build("rule-providers: {bad: {type: http, behavior: classical, format: mrs, url: 'https://example.test/x'}}\n",Settings{},19090,QString(64,'a')); QVERIFY(!invalid.result.ok); QCOMPARE(invalid.result.data.value("code").toString(),QString("MRS_BEHAVIOR"));
    }
};
QTEST_GUILESS_MAIN(MrsTests)
#include "MrsTests.moc"
