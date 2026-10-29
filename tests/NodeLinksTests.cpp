#include "TestPorts.h"
#include "profile/NodeLinks.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "app/AppController.h"
#include "ui/ImportDialog.h"
#include "ui/NodeLinksEditor.h"
#include "ui/ProxyEditorDialog.h"
#include "ui/MainWindow.h"
#include "ui/WidgetStyle.h"
#include "ui/UiTheme.h"
#include <QtTest>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <yaml-cpp/yaml.h>

using namespace mlash;
namespace {
const QString uuid="11111111-1111-4111-8111-111111111111";
QString ss(const QString& name="香港") { return "ss://"+QString::fromLatin1(QByteArray("aes-128-gcm:PRIVATE_SECRET").toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals))+"@127.0.0.1:19001#"+QString::fromLatin1(QUrl::toPercentEncoding(name)); }
QString samples() {
    const QJsonObject vmess{{"v","2"},{"ps","VMess"},{"add","127.0.0.1"},{"port","19002"},{"id",uuid},{"aid","0"},{"scy","auto"},{"net","ws"},{"type","none"},{"host","example.com"},{"path","/path?ed=2048"},{"tls","tls"},{"sni","example.com"}};
    return QStringList{ss(),"vmess://"+QString::fromLatin1(QJsonDocument(vmess).toJson(QJsonDocument::Compact).toBase64()),"vless://"+uuid+"@127.0.0.1:19003?security=tls&type=grpc&serviceName=test&sni=example.com#VLESS","trojan://PRIVATE_SECRET@127.0.0.1:19004?sni=example.com#Trojan","hy2://PRIVATE_SECRET@127.0.0.1:19005?sni=example.com&obfs=salamander&obfs-password=OBFS_SECRET#HY2","tuic://"+uuid+":PRIVATE_SECRET@127.0.0.1:19006?congestion_control=bbr&udp_relay_mode=native&sni=example.com#TUIC","anytls://PRIVATE_SECRET@127.0.0.1:19007?sni=example.com#AnyTLS","https://user:PRIVATE_SECRET@127.0.0.1:19008#HTTP","socks5://user:PRIVATE_SECRET@[::1]:19009#SOCKS"}.join('\n');
}
void saveSettings(const QString& root) { auto platform=createPlatformServices(); ProfileStore store(root,platform); Settings settings; settings.startCore=false; settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; settings.mixedPort=isolatedListenerPort(); if (!store.save(settings,{}).ok) qFatal("fixture settings failed"); }
}
class NodeLinksTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WidgetStyle::install(); }
    void commonLinksEncodingAndUniqueNames() {
        const auto parsed=NodeLinks::parse(samples(),{"香港","香港 (2)"}); QVERIFY2(parsed.ok,qUtf8Printable(parsed.error));
        QCOMPARE(parsed.data.value("items").toArray().size(),9); auto definitions=parsed.data.value("definitions").toObject(); QVERIFY(definitions.contains("香港 (3)"));
        const auto ssNode=YAML::Load(definitions.value("香港 (3)").toString().toStdString()); QCOMPARE(QString::fromStdString(ssNode["password"].as<std::string>()),QString("PRIVATE_SECRET"));
        const auto vmess=YAML::Load(definitions.value("VMess").toString().toStdString()); QVERIFY(vmess["tls"].as<bool>()); QCOMPARE(QString::fromStdString(vmess["ws-opts"]["path"].as<std::string>()),QString("/path?ed=2048"));
        const auto vless=YAML::Load(definitions.value("VLESS").toString().toStdString()); QCOMPARE(QString::fromStdString(vless["grpc-opts"]["grpc-service-name"].as<std::string>()),QString("test")); QVERIFY(!vless["skip-cert-verify"]);
        QVERIFY(!QJsonDocument(parsed.data.value("items").toArray()).toJson().contains("PRIVATE_SECRET"));
        const QString password="password:@?#中文"; const auto legacy="ss://"+QString::fromLatin1(("aes-128-gcm:"+password+"@127.0.0.1:19001").toUtf8().toBase64())+"#Legacy";
        const auto legacyParsed=NodeLinks::parse(legacy); QVERIFY2(legacyParsed.ok,qUtf8Printable(legacyParsed.error)); const auto legacyNode=YAML::Load(legacyParsed.data.value("definitions").toObject().value("Legacy").toString().toStdString()); QCOMPARE(QString::fromStdString(legacyNode["password"].as<std::string>()),password);
        const auto reality=NodeLinks::parse("vless://"+uuid+"@[::1]:443?security=reality&pbk=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA&sid=01234567&fp=chrome#Reality"); QVERIFY2(reality.ok,qUtf8Printable(reality.error)); QVERIFY(reality.data.value("definitions").toObject().value("Reality").toString().contains("reality-opts"));
        const auto anytls=YAML::Load(definitions.value("AnyTLS").toString().toStdString()); QVERIFY(anytls["udp"].as<bool>());
        const QJsonObject json{{"add","EXAMPLE.COM"},{"port",443},{"id",uuid},{"tls",true},{"ps","Boolean TLS"}}; const auto boolTls=NodeLinks::parse("vmess://"+QString::fromLatin1(QJsonDocument(json).toJson(QJsonDocument::Compact).toBase64())); QVERIFY2(boolTls.ok,qUtf8Printable(boolTls.error)); const auto boolNode=YAML::Load(boolTls.data.value("definitions").toObject().value("Boolean TLS").toString().toStdString()); QVERIFY(boolNode["tls"].as<bool>());
        const auto udp=NodeLinks::parse("trojan://PRIVATE_SECRET@127.0.0.1:443?udp=0#UDP"); QVERIFY(udp.ok); QVERIFY(!YAML::Load(udp.data.value("definitions").toObject().value("UDP").toString().toStdString())["udp"].as<bool>());
    }
    void invalidBatchIsAtomicAndDoesNotExposeSecrets() {
        const QString secret="NEVER_PRINT_PRIVATE";
        const QStringList invalid{"trojan://"+secret+"@127.0.0.1:443?unknown="+secret,"trojan://"+secret+"@127.0.0.1:443?insecure=1&allowInsecure=0","vless://bad-"+secret+"@127.0.0.1:443","anytls://"+secret+"@127.0.0.1:443?sni=%FF","trojan://"+secret+"@127.0.0.1:443?sni=%0A",ss().section('#',0,0)+"?plugin=UNSUPPORTED",QString(8193,'x'),"vmess://!!!!", "ss://YWJj===@127.0.0.1:443"};
        for (const auto& bad:invalid) { const auto result=NodeLinks::parse(ss()+"\n"+bad); QVERIFY2(!result.ok,qUtf8Printable(QString("invalid fixture %1 was accepted").arg(invalid.indexOf(bad)))); QVERIFY(result.data.value("definitions").isUndefined()); QVERIFY(!result.error.contains(secret)); QVERIFY(!result.error.contains("PRIVATE_SECRET")); QCOMPARE(result.data.value("line").toInt(),2); }
        QVERIFY(!NodeLinks::parse(QString(65537,'x')).ok); QVERIFY(!NodeLinks::parse(QStringList(65,ss()).join('\n')).ok);
        const auto duplicateJson=QString("{\"add\":\"127.0.0.1\",\"add\":\"%1\",\"port\":443,\"id\":\"%2\"}").arg(secret,uuid); const auto result=NodeLinks::parse("vmess://"+QString::fromLatin1(duplicateJson.toUtf8().toBase64())); QVERIFY(!result.ok); QVERIFY(!result.error.contains(secret));
    }
    void newProfileUsesRealCoreAndFailedImportKeepsRunningVersion() {
        QTemporaryDir root; saveSettings(root.path()); AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        controller.importNodeLinks("Links",samples()); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QCOMPARE(controller.profiles().size(),1); const auto profile=controller.profiles().first(); QVERIFY(profile.source.isEmpty()); QCOMPARE(profile.sourceType,QString("file")); QVERIFY(profile.active.isEmpty()); QVERIFY(!controller.state().ready);
        controller.activateProfile(profile.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QVERIFY(controller.state().ready); QTRY_COMPARE_WITH_TIMEOUT(controller.proxies().value("节点选择").toObject().value("all").toArray().size(),9,5000);
        const auto active=controller.profiles().first().active; const auto generation=controller.state().generation;
        controller.importNodeLinks("Broken",ss()+"\ntrojan://PRIVATE_SECRET@127.0.0.1:443?unsupported=PRIVATE_SECRET"); QTRY_COMPARE(finished.size(),3); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(controller.profiles().size(),1); QCOMPARE(controller.profiles().first().active,active); QVERIFY(controller.state().ready); QVERIFY(controller.state().generation>=generation);
        controller.updateProfilePolicy(profile.id,true,12); QTRY_COMPARE(finished.size(),4); QVERIFY(!finished.last()[0].toBool()); QVERIFY(!controller.profiles().first().autoUpdate);
        const auto expected=controller.profileMaintenance(profile.id); controller.editProfileSource(profile.id,"Renamed",{},false,12,profile.download,expected); QTRY_COMPARE(finished.size(),5); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QCOMPARE(controller.profiles().first().name,QString("Renamed")); QVERIFY(controller.state().ready);
        const auto logs=controller.diagnostics()->lines().join('\n'); QVERIFY(!logs.contains("PRIVATE_SECRET")); QVERIFY(!logs.contains("OBFS_SECRET")); QVERIFY(!logs.contains("ss://"));
        auto platform=createPlatformServices(); ProfileStore reloaded(root.path(),platform); const auto loaded=reloaded.load(); QVERIFY(loaded.result.ok); QCOMPARE(loaded.profiles.first().source,QString());
        QSignalSpy exit(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exit.size(),1,10000);
    }
    void clipboardFailureRetainsDialogAndNodePreviewNeverImports() {
        QTemporaryDir root; saveSettings(root.path()); AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&controller); window.show(); window.setPage(1);
        QApplication::clipboard()->setText(root.path()+"/missing.yaml"); window.findChild<QPushButton*>("pasteSubscription")->click(); QPointer<ImportDialog> dialog=window.findChild<ImportDialog*>(); QVERIFY(dialog); dialog->findChild<QLineEdit*>("subscriptionName")->setText("Keep draft"); dialog->findChild<QPushButton*>("importSubmit")->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog); QVERIFY(dialog->isVisible()); QCOMPARE(dialog->profileName(),QString("Keep draft")); QVERIFY(dialog->findChild<QPushButton*>("importSubmit")->isEnabled()); dialog->reject(); QTRY_VERIFY(dialog.isNull());
        QMimeData mime; mime.setUrls({QUrl::fromLocalFile(root.path()+"/also-missing.yaml")}); QDragEnterEvent enter(QPoint(20,20),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(&window,&enter); QVERIFY(enter.isAccepted()); QDropEvent drop(QPointF(20,20),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(&window,&drop); dialog=window.findChild<ImportDialog*>(); QVERIFY(dialog); dialog->findChild<QPushButton*>("importSubmit")->click(); QTRY_COMPARE(finished.size(),2); QVERIFY(dialog); QVERIFY(!finished.last()[0].toBool()); dialog->reject(); QTRY_VERIFY(dialog.isNull());
        QApplication::clipboard()->setText(ss()); window.findChild<QPushButton*>("pasteSubscription")->click(); dialog=window.findChild<ImportDialog*>(); QVERIFY(dialog); QVERIFY(dialog->isNodeLinks()); auto* editor=dialog->findChild<NodeLinksEditor*>(); QVERIFY(editor); QVERIFY(!dialog->findChild<QPushButton*>("importSubmit")->isEnabled()); editor->findChild<QPushButton*>("checkNodeLinks")->click(); QTRY_VERIFY(editor->ready()); QCOMPARE(editor->findChild<QTableWidget*>("nodeLinksPreview")->rowCount(),1); QVERIFY(controller.profiles().isEmpty()); QVERIFY(!controller.state().busy); QVERIFY(dialog->findChild<QPushButton*>("importSubmit")->isEnabled());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); dialog->resize(500,420); QCoreApplication::processEvents(); QVERIFY(dialog->size().width()<=500); QVERIFY(dialog->grab().save(output+"/node-link-import.png")); }
        editor->setText(ss()+"?invalid=PRIVATE_SECRET"); QVERIFY(!editor->ready()); QVERIFY(!dialog->findChild<QPushButton*>("importSubmit")->isEnabled()); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(finished.size(),2);
    }
    void existingNodesAreStagedAndStaleSourceIsRejected() {
        QTemporaryDir root; saveSettings(root.path()); AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1); controller.importNodeLinks("Base",ss("Node")); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(finished.last()[0].toBool()); const auto profile=controller.profiles().first();
        QPointer<ProxyEditorDialog> owner=new ProxyEditorDialog(&controller,profile.id); owner->show(); auto* import=owner->findChild<QPushButton*>("importNodeLinks"); QTRY_VERIFY(import->isEnabled()); import->click(); auto* dialog=owner->findChild<QDialog*>("nodeLinksDialog"); QVERIFY(dialog); auto* editor=dialog->findChild<NodeLinksEditor*>(); editor->setText(ss("Node")); editor->findChild<QPushButton*>("checkNodeLinks")->click(); QTRY_VERIFY(editor->ready()); QVERIFY(editor->definitions().contains("Node (2)")); dialog->findChild<QPushButton*>("stageNodeLinks")->click(); QTRY_VERIFY(owner->findChild<QPushButton*>("saveProxyEdits")->isEnabled()); QVERIFY(controller.profiles().first().proxyEdits.isEmpty());
        owner->findChild<QPushButton*>("saveProxyEdits")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_VERIFY(owner.isNull()); QVERIFY(controller.profiles().first().proxyEdits.value("nodes").toObject().contains("Node (2)"));
        owner=new ProxyEditorDialog(&controller,profile.id); owner->show(); import=owner->findChild<QPushButton*>("importNodeLinks"); QTRY_VERIFY(import->isEnabled()); import->click(); dialog=owner->findChild<QDialog*>("nodeLinksDialog"); editor=dialog->findChild<NodeLinksEditor*>(); editor->setText(ss("New")); editor->findChild<QPushButton*>("checkNodeLinks")->click(); QTRY_VERIFY(editor->ready()); QVERIFY(dialog->findChild<QPushButton*>("stageNodeLinks")->isEnabled());
        auto& profiles=const_cast<QVector<Profile>&>(controller.profiles()); profiles[0].candidate=newId(); emit controller.stateChanged(); QVERIFY(!dialog->findChild<QPushButton*>("stageNodeLinks")->isEnabled()); QVERIFY(dialog->findChild<QLabel*>("stageNodeLinksHint")->text().contains("变化")); dialog->reject(); owner->reject(); QTRY_VERIFY(owner.isNull());
    }
    void importLayoutAndDraftFitWithoutClipboardAccess() {
        QTemporaryDir root; saveSettings(root.path()); AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(init.size(),1);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) QDir().mkpath(output);
        for (const int scale:{100,120}) {
            UiTheme::setPreferences("light",scale); ImportDialog dialog; dialog.bindController(&controller); dialog.resize(500,420); dialog.show(); QCoreApplication::processEvents();
            auto* submit=dialog.findChild<QPushButton*>("importSubmit"); auto* hint=dialog.findChild<QLabel*>("importHint"); const auto position=submit->mapTo(&dialog,QPoint()); const auto feedback=hint->mapTo(&dialog,QPoint());
            for (const bool url:{true,false}) { dialog.setSourceTab(url); QCoreApplication::processEvents(); QCOMPARE(dialog.size(),QSize(500,420)); QCOMPARE(submit->mapTo(&dialog,QPoint()),position); }
            dialog.setNodeLinks(samples()); auto* editor=dialog.findChild<NodeLinksEditor*>(); editor->findChild<QPushButton*>("checkNodeLinks")->click(); QTRY_VERIFY(editor->ready()); QCoreApplication::processEvents();
            QCOMPARE(dialog.size(),QSize(500,420)); QCOMPARE(submit->mapTo(&dialog,QPoint()),position); QCOMPARE(hint->mapTo(&dialog,QPoint()),feedback); QVERIFY(submit->isEnabled()); QVERIFY(controller.profiles().isEmpty()); QVERIFY(dialog.rect().contains(submit->mapTo(&dialog,submit->rect().bottomRight())));
            dialog.resize(500,560); auto* scroll=dialog.findChild<QScrollArea*>("importScroll"); scroll->verticalScrollBar()->setValue(0); QCoreApplication::processEvents(); auto* check=editor->findChild<QPushButton*>("checkNodeLinks"); auto* preview=editor->findChild<QTableWidget*>("nodeLinksPreview"); QVERIFY(scroll->viewport()->rect().contains(check->mapTo(scroll->viewport(),check->rect().bottomRight()))); QVERIFY(preview->mapTo(scroll->viewport(),QPoint()).y()+preview->horizontalHeader()->height()+24<scroll->viewport()->height());
            if (!output.isEmpty()) { QVERIFY(dialog.grab().save(output+QString("/node-import-%1.png").arg(scale))); const QJsonObject context{{"dpr",dialog.devicePixelRatioF()},{"font",dialog.font().family()},{"scale",scale}}; QVERIFY(writeAtomic(output+QString("/node-context-%1.json").arg(scale),QJsonDocument(context).toJson()).ok); }
            dialog.setSourceTab(true); dialog.findChild<QTabWidget*>()->setCurrentIndex(2); QVERIFY(editor->ready()); dialog.reject();
        }
        UiTheme::setPreferences("system",100);
    }
    void changedInputDiscardsPendingPreviewAndCoreFailureKeepsDraft() {
        QTemporaryDir root; saveSettings(root.path()); AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        QPointer<ImportDialog> dialog=new ImportDialog; dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->bindController(&controller); dialog->setNodeLinks(ss("Old")); dialog->show(); auto* editor=dialog->findChild<NodeLinksEditor*>(); auto* check=editor->findChild<QPushButton*>("checkNodeLinks");
        check->click(); editor->setText(ss("New")); QTRY_VERIFY(!editor->checking()); QVERIFY(!editor->ready()); QCOMPARE(editor->findChild<QTableWidget*>("nodeLinksPreview")->rowCount(),0); check->click(); QTRY_VERIFY(editor->ready()); QVERIFY(editor->definitions().contains("New")); QVERIFY(!editor->definitions().contains("Old")); QVERIFY(controller.profiles().isEmpty()); QVERIFY(!controller.state().busy);
        const auto invalid="ss://"+QString::fromLatin1(QByteArray("not-a-cipher:PRIVATE_SECRET").toBase64())+"@127.0.0.1:443#Invalid"; editor->setText(invalid); check->click(); QTRY_VERIFY(editor->ready()); dialog->findChild<QLineEdit*>("subscriptionName")->setText("Keep node draft"); dialog->findChild<QPushButton*>("importSubmit")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog); QVERIFY(dialog->isVisible()); QCOMPARE(dialog->source(),invalid); QCOMPARE(dialog->profileName(),QString("Keep node draft")); QVERIFY(controller.profiles().isEmpty()); QVERIFY(!dialog->findChild<QLabel*>("importHint")->text().contains("PRIVATE_SECRET"));
        editor->setText(ss("Valid")); check->click(); QTRY_VERIFY(editor->ready()); dialog->findChild<QPushButton*>("importSubmit")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,10000); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(dialog.isNull()); QCOMPARE(controller.profiles().size(),1); QVERIFY(controller.profiles().first().active.isEmpty()); QVERIFY(!controller.state().ready);
    }
    void runningEditsPreservePendingCandidateAndStopInvalidatesOldDraft() {
        QTemporaryDir root; saveSettings(root.path()); AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        controller.importNodeLinks("Base",ss("Running")); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(finished.last()[0].toBool()); const auto id=controller.profiles().first().id;
        controller.activateProfile(id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished.last()[0].toBool()); const auto active=controller.profiles().first().active;
        const auto next=NodeLinks::parse(ss("Candidate")); QVERIFY(next.ok); const auto file=root.path()+"/next.yaml"; QVERIFY(writeAtomic(file,next.data.value("yaml").toString().toUtf8()).ok);
        controller.importFile({},file,id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY(finished.last()[0].toBool()); const auto candidate=controller.profiles().first().candidate; QVERIFY(candidate!=active); QCOMPARE(controller.profiles().first().active,active); QVERIFY(controller.state().ready);
        QPointer<ProxyEditorDialog> owner=new ProxyEditorDialog(&controller,id); owner->show(); auto* import=owner->findChild<QPushButton*>("importNodeLinks"); QTRY_VERIFY(import->isEnabled()); QVERIFY(owner->findChild<QLabel*>("proxyEditScope")->text().contains("运行版本")); QVERIFY(owner->findChild<QLabel*>("proxyEditScope")->text().contains("待应用"));
        const auto stage=[&](const QString& name) { import->click(); auto* dialog=owner->findChild<QDialog*>("nodeLinksDialog"); auto* editor=dialog->findChild<NodeLinksEditor*>(); editor->setText(ss(name)); editor->findChild<QPushButton*>("checkNodeLinks")->click(); QTRY_VERIFY(editor->ready()); dialog->findChild<QPushButton*>("stageNodeLinks")->click(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); };
        stage("Extra"); QVERIFY(owner->findChild<QPushButton*>("saveProxyEdits")->isEnabled()); owner->findChild<QPushButton*>("saveProxyEdits")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_VERIFY(owner.isNull()); QCOMPARE(controller.profiles().first().candidate,candidate); QVERIFY(controller.profiles().first().active!=active); QVERIFY(controller.state().ready); QTRY_VERIFY(controller.proxies().contains("Extra"));
        owner=new ProxyEditorDialog(&controller,id); owner->show(); import=owner->findChild<QPushButton*>("importNodeLinks"); QTRY_VERIFY(import->isEnabled()); stage("Unsubmitted"); QVERIFY(owner->findChild<QPushButton*>("saveProxyEdits")->isEnabled());
        const auto saved=controller.profiles().first().proxyEdits; controller.stopCore(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),5,10000); QVERIFY(finished.last()[0].toBool()); QVERIFY(!owner->findChild<QPushButton*>("saveProxyEdits")->isEnabled()); QVERIFY(owner->findChild<QLabel*>("proxyEditorHint")->text().contains("变化")); QCOMPARE(controller.profiles().first().candidate,candidate); QCOMPARE(controller.profiles().first().proxyEdits,saved); owner->reject(); QTRY_VERIFY(owner.isNull());
        owner=new ProxyEditorDialog(&controller,id); owner->show(); QTRY_VERIFY(owner->findChild<QPushButton*>("importNodeLinks")->isEnabled()); QVERIFY(owner->findChild<QLabel*>("proxyEditScope")->text().contains("候选版本")); const auto* nodes=owner->findChild<QListWidget*>("editableNodes"); QVERIFY(nodes->findItems("Candidate",Qt::MatchExactly).size()==1); owner->reject(); QTRY_VERIFY(owner.isNull());
    }
};
QTEST_MAIN(NodeLinksTests)
#include "NodeLinksTests.moc"
