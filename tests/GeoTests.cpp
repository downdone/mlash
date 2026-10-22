#include "profile/GeoData.h"
#include "profile/SubscriptionDownloader.h"
#include "storage/Storage.h"
#include "ui/GeoDataDialog.h"
#include "MockHttp.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QTableWidget>
#include <QJsonDocument>
using namespace mlash;
class GeoPlatform final : public PlatformServices {
public:
    int writes=0;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
class GeoTests : public QObject {
    Q_OBJECT
    QByteArray field(char tag,const QByteArray& value) { return QByteArray(1,tag)+QByteArray(1,char(value.size()))+value; }
    QByteArray geoip() { return field(10,field(10,"CN")+field(18,field(10,QByteArray::fromHex("cb007100"))+QByteArray::fromHex("1018"))); }
    QByteArray geosite() { return field(10,field(10,"CN")+field(18,QByteArray::fromHex("0803")+field(18,"example.test"))); }
    bool validate(const QString& stage) { QProcess process; process.start(QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe",{"-t","-d",stage,"-f",stage+"/config.yaml"}); if (!process.waitForFinished(5000)) return false; return process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0; }
private slots:
    void realDataCommitHashAndVersionIsolation() {
        QTemporaryDir root,local; auto platform=std::make_shared<GeoPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); QVERIFY(writeAtomic(local.path()+"/geoip.dat",geoip()).ok); QVERIFY(store.importGeoData(local.path()).ok);
        Settings settings; Profile p; p.id=newId(); p.sourceType="file"; p.name="Geo"; const auto old=store.prepare(p,"rules: ['MATCH,DIRECT']\n",settings,19090,QString(64,'a')); QVERIFY(old.result.ok); QByteArray frozen; QVERIFY(readFile(old.workPath+"/geoip.dat",frozen).ok); QCOMPARE(frozen,geoip());
        const auto staged=GeoData::stage(store,"geosite.dat",geosite()); QVERIFY(staged.ok); QVERIFY(validate(staged.data.value("stage").toString())); QVERIFY(GeoData::commit(store,staged).ok); QVERIFY(!QFileInfo::exists(staged.data.value("stage").toString())); QByteArray bytes; QVERIFY(readFile(root.path()+"/geodata/geoip.dat",bytes).ok); QCOMPARE(bytes,geoip()); QVERIFY(readFile(root.path()+"/geodata/geosite.dat",bytes).ok); QCOMPARE(bytes,geosite()); QVERIFY(readFile(old.workPath+"/geoip.dat",bytes).ok); QCOMPARE(bytes,frozen); QVERIFY(!QFileInfo::exists(old.workPath+"/geosite.dat"));
        const auto prepared=store.prepareExisting(old.profile,old.version,settings,19091,QString(64,'b')); QVERIFY(prepared.result.ok); QVERIFY(readFile(prepared.workPath+"/geosite.dat",bytes).ok); QCOMPARE(bytes,geosite());
        const auto metadata=GeoData::inspect(root.path(),old.workPath); QVERIFY(metadata.ok); QCOMPARE(metadata.data.value("files").toArray().size(),3); bool found=false; for (const auto& value:metadata.data.value("files").toArray()) { const auto row=value.toObject(); if (row["name"]=="geosite.dat") { QVERIFY(row["hash_matches"].toBool()); QVERIFY(row["origin"].toString().contains("MetaCubeX")); found=true; } } QVERIFY(found);
        const auto ip=GeoData::stage(store,"geoip.dat",geoip()); QVERIFY(ip.ok); QVERIFY(validate(ip.data.value("stage").toString())); QVERIFY(GeoData::commit(store,ip).ok); QVERIFY(readFile(root.path()+"/geodata.previous/geosite.dat",bytes).ok); QCOMPARE(bytes,geosite()); QCOMPARE(platform->writes,0);
        QVERIFY(writeAtomic(root.path()+"/geodata/geosite.dat",geosite()+"tampered").ok); const auto newer=GeoData::stage(store,"geoip.dat",geoip()); QVERIFY(newer.ok); QVERIFY(!GeoData::commit(store,newer).ok); QVERIFY(readFile(root.path()+"/geodata/geoip.dat",bytes).ok); QCOMPARE(bytes,geoip());
    }
    void failurePreservesAndInspectionUi() {
        QTemporaryDir root,local; auto platform=std::make_shared<GeoPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); QVERIFY(writeAtomic(local.path()+"/geosite.dat",geosite()).ok); QVERIFY(store.importGeoData(local.path()).ok); QByteArray original; QVERIFY(readFile(root.path()+"/geodata/manifest.json",original).ok);
        const auto bad=GeoData::stage(store,"geosite.dat","invalid data"); QVERIFY(bad.ok); QVERIFY(!validate(bad.data.value("stage").toString())); GeoData::discard(root.path(),bad.data.value("stage").toString()); QByteArray after; QVERIFY(readFile(root.path()+"/geodata/manifest.json",after).ok); QCOMPARE(after,original);
        const auto tampered=GeoData::stage(store,"geosite.dat",geosite()); QVERIFY(tampered.ok); QVERIFY(writeAtomic(tampered.data.value("stage").toString()+"/geosite.dat",geosite()+"x").ok); QVERIFY(!GeoData::commit(store,tampered).ok); QVERIFY(readFile(root.path()+"/geodata/manifest.json",after).ok); QCOMPARE(after,original); QVERIFY(!GeoData::stage(store,"../escape",geosite()).ok); QVERIFY(GeoData::source("unknown").isEmpty()); QVERIFY(GeoData::source("asn.mmdb").path().endsWith("GeoLite2-ASN.mmdb"));
        AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1); QPointer<GeoDataDialog> dialog=new GeoDataDialog(&c); dialog->show(); auto* table=dialog->findChild<QTableWidget*>("geoDataTable"); QTRY_COMPARE(table->rowCount(),1); QVERIFY(table->item(0,4)->text().contains("一致")); QVERIFY(table->item(0,0)->toolTip().contains("SHA256")); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(platform->writes,0);
    }
    void downloadBudgetIsExplicitAndBounded() {
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); server.handler=[](const QByteArray&) { return HttpResponse{200,QByteArray(9*1024*1024,'x')}; }; const auto url=QUrl("http://127.0.0.1:"+QString::number(server.serverPort())+"/data"); SubscriptionDownloader downloader(nullptr,true); bool done=false; Result result; QByteArray received;
        downloader.download(url,[&](Result r,QByteArray b){result=r;received=b;done=true;}); QTRY_VERIFY_WITH_TIMEOUT(done,10000); QVERIFY(!result.ok); QVERIFY(received.isEmpty()); done=false; downloader.download(url,[&](Result r,QByteArray b){result=r;received=b;done=true;},{},0,16*1024*1024); QTRY_VERIFY_WITH_TIMEOUT(done,10000); QVERIFY2(result.ok,qUtf8Printable(result.error)); QCOMPARE(received.size(),9*1024*1024); done=false; downloader.download(url,[&](Result r,QByteArray){result=r;done=true;},{},0,129*1024*1024); QVERIFY(done); QVERIFY(!result.ok); QCOMPARE(server.requests.size(),2);
    }
};
QTEST_MAIN(GeoTests)
#include "GeoTests.moc"
