#include "TestPorts.h"
#include <QTcpServer>
#include "app/AppController.h"
#include "profile/ConfigBuilder.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/DnsDialog.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <yaml-cpp/yaml.h>

using namespace mlash;
class DnsPlatform final : public PlatformServices {
public:
    int writes=0;
    bool owned=true;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("proxy write prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("Run write prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { out="sealed:"+in.toBase64(); return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { if (!in.startsWith("sealed:")) return Result::fail("bad index"); out=QByteArray::fromBase64(in.mid(7)); return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",owned}}}; }
};
class DnsFixture final : public QUdpSocket {
public:
    int requests=0;
    bool respond=true;
    explicit DnsFixture(char last) {
        bind(QHostAddress(QHostAddress::LocalHost),quint16(0));
        connect(this,&QUdpSocket::readyRead,this,[this,last] {
            while (hasPendingDatagrams()) {
                const auto packet=receiveDatagram(); const auto data=packet.data(); ++requests;
                if (!respond || data.size()<17) continue;
                int end=12; while (end<data.size() && data[end]) end+=1+quint8(data[end]); end+=5;
                if (end>data.size()) continue;
                QByteArray reply=data.left(2)+QByteArray::fromHex("81800001000100000000")+data.mid(12,end-12);
                reply+=QByteArray::fromHex("c00c000100010000003c0004"); reply.append(char(203)).append(char(0)).append(char(113)).append(last);
                writeDatagram(reply,packet.senderAddress(),packet.senderPort());
            }
        });
    }
};
class DnsTests final : public QObject {
    Q_OBJECT
    const QByteArray base="proxies: []\nproxy-groups: [{name: Test, type: select, proxies: [DIRECT, REJECT]}]\nrules: ['MATCH,DIRECT']\n";
    Settings settings() const { static const int isolatedPort=isolatedListenerPort(); Settings s; s.mixedPort=isolatedPort; s.startCore=false; s.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; return s; }
    QByteArray source(int port) const { return base+"dns:\n  enable: true\n  enhanced-mode: normal\n  use-hosts: false\n  use-system-hosts: false\n  nameserver: ['127.0.0.1:"+QByteArray::number(port)+"']\n  nameserver-policy: {example.test: '127.0.0.1:"+QByteArray::number(port)+"'}\n"; }
private slots:
    void mergeBoundsAndPersistence() {
        Profile p; p.id=newId(); p.name="DNS"; p.sourceType="file"; p.dnsOverrideEnabled=true; p.dnsOverride="ipv6: false\nnameserver-policy: {added.test: 1.1.1.1}\n";
        const auto original=source(5353); const auto built=ConfigBuilder::buildProfile(original,settings(),19090,QString(64,'a'),p); QVERIFY2(built.result.ok,qUtf8Printable(built.result.error));
        const auto dns=YAML::Load(built.yaml.toStdString())["dns"]; QVERIFY(dns["enable"].as<bool>()); QVERIFY(!dns["ipv6"].as<bool>()); QCOMPARE(dns["nameserver-policy"].size(),size_t(2)); QCOMPARE(dns["nameserver"][0].as<std::string>(),std::string("127.0.0.1:5353"));
        for (const auto& text : QStringList{"listen: 0.0.0.0:53","dns: {enable: true}","nameserver: [","[]","enable: true\n---\nipv6: false",QString(65537,'x')}) { p.dnsOverride=text; QVERIFY(!ConfigBuilder::buildProfile(original,settings(),19090,QString(64,'a'),p).result.ok); }
        p.dnsOverride="listen: 0.0.0.0:53"; p.dnsOverrideEnabled=false; QVERIFY(ConfigBuilder::buildProfile(original,settings(),19090,QString(64,'a'),p).result.ok);
        p.dnsOverride="ipv6: false"; p.dnsOverrideEnabled=true; QTemporaryDir data; auto platform=std::make_shared<DnsPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); const auto prepared=store.prepare(p,original,settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok); const auto loaded=store.load(); QVERIFY(loaded.result.ok); QCOMPARE(loaded.profiles[0].dnsOverride,p.dnsOverride); QVERIFY(loaded.profiles[0].dnsOverrideEnabled); QByteArray bytes; QVERIFY(readFile(store.versionRoot(p.id,prepared.version)+"/source.yaml",bytes).ok); QCOMPARE(bytes,original);
        auto legacy=p.toJson(); legacy.remove("dns_override"); legacy.remove("dns_override_enabled"); QVERIFY(!Profile::fromJson(legacy).dnsOverrideEnabled); QCOMPARE(platform->writes,0);
    }
    void liveResolverSwitchFailureAndCancellation() {
        DnsFixture first(9),second(10); QVERIFY(first.localPort()); QVERIFY(second.localPort()); QTemporaryDir data; auto platform=std::make_shared<DnsPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.name="Resolver"; p.sourceType="file"; auto prepared=store.prepare(p,source(first.localPort()),settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1); c.activateProfile(p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished[0][0].toBool(),qUtf8Printable(finished[0][1].toString()));
        c.setHomeVisible(true); QTRY_VERIFY_WITH_TIMEOUT(c.homeMetrics()->snapshot().contains("active_count"),4000); QVERIFY(c.homeMetrics()->snapshot().value("core_working_set").toDouble()>0); c.setHomeVisible(false); QVERIFY(c.homeMetrics()->snapshot().isEmpty());
        platform->owned=false; c.startIpProbe(true); QTRY_VERIFY(!c.outboundDiagnostics()->busy()); QVERIFY(c.outboundDiagnostics()->result().value("error").toString().contains("端口")); c.cancelOutboundDiagnostics(); platform->owned=true;
        bool done=false; Result result; c.queryDns("before.example","A",[&](Result r){result=r;done=true;}); QTRY_VERIFY_WITH_TIMEOUT(done,6000); QVERIFY2(result.ok,qUtf8Printable(result.error)); QCOMPARE(result.data["Answer"].toArray()[0].toObject()["data"].toString(),QString("203.0.113.9")); QVERIFY(first.requests>0);
        const auto pendingPath=data.path()+"/pending.yaml"; QVERIFY(writeAtomic(pendingPath,source(first.localPort())+"# pending\n").ok); c.importFile("",pendingPath,p.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished[1][0].toBool()); const auto candidate=c.profiles()[0].candidate;
        c.updateDnsOverride(p.id,"nameserver: ['127.0.0.1:"+QString::number(second.localPort())+"']",true); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY2(finished[2][0].toBool(),qUtf8Printable(finished[2][1].toString())); QCOMPARE(c.profiles()[0].candidate,candidate);
        done=false; c.queryDns("after.example","A",[&](Result r){result=r;done=true;}); QTRY_VERIFY_WITH_TIMEOUT(done,6000); QVERIFY(result.ok); QCOMPARE(result.data["Answer"].toArray()[0].toObject()["data"].toString(),QString("203.0.113.10")); QVERIFY(second.requests>0);
        const auto pid=c.coreProcessId(); c.updateDnsOverride(p.id,"listen: :53",true); QTRY_COMPARE(finished.size(),4); QVERIFY(!finished[3][0].toBool()); QCOMPARE(c.coreProcessId(),pid); QCOMPARE(c.profiles()[0].candidate,candidate);
        second.respond=false; done=false; const auto token=c.queryDns("cancel.example","A",[&](Result r){result=r;done=true;}); QVERIFY(token); c.cancelDnsQuery(token); QTRY_VERIFY(done); QVERIFY(!result.ok); QVERIFY(result.error.contains("取消"));
        done=false; QCOMPARE(c.queryDns("a..b","A",[&](Result r){result=r;done=true;}),quint64(0)); QVERIFY(done); QVERIFY(!result.ok);
        c.updateDnsOverride(p.id,{},false); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),5,15000); QVERIFY(finished[4][0].toBool()); done=false; c.queryDns("inherit.example","A",[&](Result r){result=r;done=true;}); QTRY_VERIFY_WITH_TIMEOUT(done,6000); QVERIFY(result.ok); QCOMPARE(result.data["Answer"].toArray()[0].toObject()["data"].toString(),QString("203.0.113.9"));
        QSignalSpy exited(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.size(),1,10000); QCOMPARE(platform->writes,0);
    }
    void previewPrivacyAndDraft() {
        QTemporaryDir data; auto platform=std::make_shared<DnsPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok); Profile p; p.id=newId(); p.sourceType="file"; p.name="Private"; auto prepared=store.prepare(p,base+"dns: {enable: true, nameserver: ['https://user:password@dns.example/query?token=secret']}\n",settings(),19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings(),{prepared.profile}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1); QPointer<DnsDialog> dialog=new DnsDialog(&c,p.id); dialog->show(); auto* preview=dialog->findChild<QPlainTextEdit*>("sourceDnsPreview"); QTRY_VERIFY(preview->toPlainText().contains("dns.example")); QVERIFY(!preview->toPlainText().contains("password")); QVERIFY(!preview->toPlainText().contains("secret")); auto* save=dialog->findChild<QPushButton*>("saveDnsOverride"); QVERIFY(!save->isEnabled()); QVERIFY(!dialog->findChild<QPushButton*>("queryDns")->isEnabled()); auto* editor=dialog->findChild<QPlainTextEdit*>("dnsOverrideEditor"); dialog->findChild<QCheckBox*>("dnsOverrideEnabled")->setChecked(true); editor->setPlainText("listen: :53"); QTRY_VERIFY(save->isEnabled()); save->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished[0][0].toBool()); QVERIFY(dialog); QCOMPARE(editor->toPlainText(),QString("listen: :53")); dialog->findChild<QPushButton*>("resetDnsOverride")->click(); QVERIFY(!save->isEnabled()); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QVERIFY(!c.profiles()[0].dnsOverrideEnabled); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(DnsTests)
#include "DnsTests.moc"
