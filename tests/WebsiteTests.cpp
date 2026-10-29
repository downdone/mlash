#include "TestPorts.h"
#include "app/WebsiteTests.h"
#include "core/CoreManager.h"
#include "profile/ConfigBuilder.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/MainWindow.h"
#include "ui/GuiHost.h"
#include "ui/WebsiteTargetsDialog.h"
#include "ui/UiIcons.h"
#include "ui/WebsiteTestsPage.h"
#include <QToolButton>
#include <QPlainTextEdit>
#include "MockHttp.h"
#include <QtTest>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QDialogButtonBox>

using namespace mlash;
namespace {
int freePort() { return isolatedListenerPort(); }
QList<WebsiteTarget> targets(int count=7) { QList<WebsiteTarget> list; for (int i=0;i<count;++i) list.append({QString::number(i),QString("Target %1").arg(i),QString("http://target-%1.invalid/check?a=b").arg(i),"T"}); return list; }
QUrl requestUrl(const QByteArray& request) { return QUrl::fromEncoded("http://localhost"+request.split(' ').value(1)); }
}
class WebsiteTestCases : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QApplication::setFont(QFont("Microsoft YaHei UI",9)); }
    void resultDetailsAreFrozenAndDoNotStartRequests() {
        QTemporaryDir data; AppController c(data.path(),true); QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1); MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); int delay=42; server.handler=[&](const QByteArray&) { return HttpResponse{200,QJsonDocument(QJsonObject{{"delay",delay}}).toJson()}; }; c.findChild<MihomoClient*>()->configure(server.serverPort(),"fixture"); auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.core=CoreState::Running; const_cast<QJsonObject&>(c.proxies())={{"Leaf",QJsonObject{{"type","Http"}}}};
        WebsiteTestsPage page(&c); page.resize(900,600); page.show(); page.setActive(true); QToolButton* button=nullptr; for (auto* item:page.findChildren<QToolButton*>("websiteResultDetails")) if (item->property("targetId").toString()=="github") button=item; QVERIFY(button); QVERIFY(!button->icon().isNull()); QVERIFY(button->accessibleName().contains("GitHub")); button->click(); QPointer<QDialog> dialog=page.findChild<QDialog*>("websiteResultSnapshot"); QVERIFY(dialog); auto* evidence=dialog->findChild<QPlainTextEdit*>("websiteResultEvidence"); QVERIFY(evidence->isReadOnly()); QVERIFY(evidence->toPlainText().contains("尚未开始")); QVERIFY(evidence->toPlainText().contains("https://github.com")); QVERIFY(server.requests.isEmpty()); dialog->reject(); QTRY_VERIFY(dialog.isNull());
        c.startWebsiteTests("Leaf","github"); QTRY_VERIFY(!c.websiteTests()->running()); QCOMPARE(c.websiteTests()->result("github").status,WebsiteResult::Responded); button->click(); dialog=page.findChild<QDialog*>("websiteResultSnapshot"); QVERIFY(dialog); evidence=dialog->findChild<QPlainTextEdit*>("websiteResultEvidence"); const auto frozen=evidence->toPlainText(); QVERIFY(frozen.contains("指定节点：Leaf")); QVERIFY(frozen.contains("42 ms")); QVERIFY(frozen.contains("HTTP 状态：未知")); QVERIFY(frozen.contains("不证明解锁")); QVERIFY(frozen.contains("测试时间：")); const auto reads=server.requests.size(); QTest::qWait(60); QCOMPARE(server.requests.size(),reads);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(dialog->grab().save(output+"/website-result-details.png")); QVERIFY(page.grab().save(output+"/website-result-cards.png")); }
        delay=91; c.startWebsiteTests("Leaf","github"); QTRY_VERIFY(!c.websiteTests()->running()); QCOMPARE(evidence->toPlainText(),frozen); c.clearWebsiteTests(); QTRY_VERIFY(dialog.isNull()); button->click(); dialog=page.findChild<QDialog*>("websiteResultSnapshot"); QVERIFY(dialog); page.setActive(false); QTRY_VERIFY(dialog.isNull()); for (const auto& request:server.requests) QVERIFY(request.startsWith("GET /proxies/Leaf/delay?"));
    }
    void customTargetValidationAndCancellation() {
        QJsonArray definitions{WebsiteTarget{"custom-check","自定义服务","https://example.test/check?sample=1",""}.toJson()}; QVERIFY(WebsiteTests::validateTargets(definitions).ok);
        auto invalid = definitions; auto item = invalid[0].toObject(); item["url"] = "http://example.test/"; invalid[0] = item; QVERIFY(!WebsiteTests::validateTargets(invalid).ok); item["url"] = "https://user:secret@example.test/"; invalid[0] = item; QVERIFY(!WebsiteTests::validateTargets(invalid).ok); invalid = definitions; invalid.append(definitions[0]); QVERIFY(!WebsiteTests::validateTargets(invalid).ok);
        invalid = {}; for (int i=0;i<25;++i) { auto entry = definitions[0].toObject(); entry["id"] = "target-"+QString::number(i); invalid.append(entry); } QVERIFY(!WebsiteTests::validateTargets(invalid).ok); QVERIFY(!UiIcons::service("custom-check").isNull());
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost,0)); server.handler=[](const QByteArray&) { return HttpResponse{200,"{\"delay\":42}",250}; }; MihomoClient api; api.configure(server.serverPort(),"isolated"); WebsiteTests tests(&api);
        tests.configureTargets(definitions); QCOMPARE(tests.targets().size(),1); QVERIFY(server.requests.isEmpty()); tests.start("Leaf"); QTRY_COMPARE(server.requests.size(),1); QCOMPARE(QUrlQuery(requestUrl(server.requests[0])).queryItemValue("url"),QString("https://example.test/check?sample=1")); QVERIFY(tests.running());
        tests.configureTargets({}); QVERIFY(!tests.running()); QCOMPARE(tests.targets().size(),12); QVERIFY(tests.node().isEmpty()); QTest::qWait(300); QCOMPARE(server.requests.size(),1); QCOMPARE(tests.result("custom-check").status,WebsiteResult::Untested);
    }
    void customTargetEditorPersistenceAndReset() {
        QTemporaryDir data; ProfileStore store(data.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; QVERIFY(store.save(settings,{}).ok);
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&controller); window.show(); window.setPage(7);
        auto* edit = window.findChild<QPushButton*>("editWebsiteTargets"); QVERIFY(edit); edit->click(); QPointer<WebsiteTargetsDialog> dialog = window.findChild<WebsiteTargetsDialog*>(); QVERIFY(dialog); auto* table = dialog->findChild<QTableWidget*>("websiteTargetsTable"); QCOMPARE(table->rowCount(),12); table->item(0,0)->setText("Cancelled"); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QVERIFY(controller.settings().websiteTargets.isEmpty()); QCOMPARE(finished.size(),0);
        edit->click(); dialog = window.findChild<WebsiteTargetsDialog*>(); QVERIFY(dialog); table = dialog->findChild<QTableWidget*>("websiteTargetsTable"); dialog->findChild<QPushButton*>("addWebsiteTarget")->click(); QCOMPARE(table->rowCount(),13); table->closePersistentEditor(table->item(12,0)); table->setCurrentCell(0,0); table->item(12,0)->setText("Custom service"); table->item(12,1)->setText("https://example.test/check"); dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click(); QTRY_VERIFY(dialog.isNull()); QTRY_COMPARE(finished.size(),1); QVERIFY(finished[0][0].toBool()); QCOMPARE(controller.websiteTests()->targets().size(),13); QCOMPARE(window.findChildren<QFrame*>("websiteCard").size(),13);
        const auto loaded = store.load(); QVERIFY(loaded.result.ok); QCOMPARE(loaded.settings.websiteTargets.size(),13); settings = loaded.settings; QByteArray before; QVERIFY(readFile(data.path()+"/state.dat",before).ok); auto malformed = settings.websiteTargets; auto bad = malformed[0].toObject(); bad["url"] = "not-a-url"; malformed[0] = bad; settings.websiteTargets=malformed; QVERIFY(!store.save(settings,{}).ok); QByteArray after; QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,before);
        AppController reopened(data.path(),true); QSignalSpy again(&reopened,&AppController::initialized); reopened.initialize(); QTRY_COMPARE(again.size(),1); QCOMPARE(reopened.websiteTests()->targets().size(),13); QVERIFY(!reopened.websiteTests()->running());
        edit->click(); dialog = window.findChild<WebsiteTargetsDialog*>(); QVERIFY(dialog); dialog->findChild<QPushButton*>("defaultWebsiteTargets")->click(); dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click(); QTRY_VERIFY(dialog.isNull()); QTRY_COMPARE(finished.size(),2); QVERIFY(controller.settings().websiteTargets.isEmpty()); QCOMPARE(controller.websiteTests()->targets().size(),12); QCOMPARE(window.findChildren<QFrame*>("websiteCard").size(),12);
    }
    void boundedRequestsResultsAndRetest() {
        const auto defaults=WebsiteTests::defaults(); QCOMPARE(defaults.size(),12); QSet<QString> ids;
        for (const auto& item:defaults) { QVERIFY(!ids.contains(item.id)); ids.insert(item.id); QCOMPARE(QUrl(item.url).scheme(),QString("https")); QVERIFY(QUrl(item.url).userInfo().isEmpty()); }
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost,0));
        server.handler=[](const QByteArray& request) {
            const auto url=QUrlQuery(requestUrl(request)).queryItemValue("url");
            if (url.contains("target-2.")) return HttpResponse{504,"{\"message\":\"timeout\"}",50};
            if (url.contains("target-3.")) return HttpResponse{200,"{\"delay\":1.5}",50};
            if (url.contains("target-4.")) return HttpResponse{200,"bad-json",50};
            if (url.contains("target-5.")) return HttpResponse{200,"{\"delay\":65536}",50};
            return HttpResponse{200,url.contains("target-0.")?QByteArray("{\"delay\":0}"):QByteArray("{\"delay\":123}"),50};
        };
        MihomoClient api; api.configure(server.serverPort(),"isolated-secret"); WebsiteTests tests(&api,nullptr,targets());
        tests.start("香港/节点 &A"); QVERIFY(tests.running()); QTRY_COMPARE(server.requests.size(),3); QVERIFY(server.activeConnections()<=3);
        tests.start("ignored"); QTRY_VERIFY(!tests.running()); QCOMPARE(server.requests.size(),7); QCOMPARE(tests.node(),QString("香港/节点 &A"));
        QCOMPARE(tests.result("0").status,WebsiteResult::Responded); QCOMPARE(tests.result("0").delay,0);
        QCOMPARE(tests.result("1").delay,123); for (const auto& id:{"2","3","4","5"}) QCOMPARE(tests.result(id).status,WebsiteResult::Failed);
        for (const auto& request:server.requests) { QVERIFY(request.startsWith("GET /proxies/")); const auto url=requestUrl(request); QCOMPARE(url.path(),QString("/proxies/香港/节点 &A/delay")); QCOMPARE(QUrlQuery(url).queryItemValue("timeout"),QString("5000")); QVERIFY(QUrlQuery(url).queryItemValue("url").contains("?a=b")); }
        const auto preserved=tests.result("1").measured; tests.start("香港/节点 &A","0"); QTRY_VERIFY(!tests.running()); QCOMPARE(server.requests.size(),8); QCOMPARE(tests.result("1").measured,preserved);
        tests.start("different","unknown"); tests.start(""); QCOMPARE(server.requests.size(),8); QCOMPARE(tests.node(),QString("香港/节点 &A"));
        tests.start("different","1"); QCOMPARE(tests.result("0").status,WebsiteResult::Untested); QTRY_VERIFY(!tests.running()); QCOMPARE(tests.node(),QString("different")); QCOMPARE(server.requests.size(),9);
        tests.clear(); QVERIFY(tests.node().isEmpty()); QCOMPARE(tests.result("1").status,WebsiteResult::Untested);
    }
    void cancellationGenerationAndDestruction() {
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost,0)); int latency=250;
        server.handler=[&](const QByteArray&) { return HttpResponse{200,"{\"delay\":42}",latency}; };
        MihomoClient api; api.configure(server.serverPort(),"test"); WebsiteTests tests(&api,nullptr,targets(30)); QCOMPARE(tests.targets().size(),24);
        tests.start("old"); QTRY_COMPARE(server.requests.size(),3); tests.cancel(); QVERIFY(!tests.running()); for (int i=0;i<24;++i) QCOMPARE(tests.result(QString::number(i)).status,WebsiteResult::Cancelled);
        latency=20; tests.start("new","0"); QTRY_VERIFY(!tests.running()); QCOMPARE(tests.node(),QString("new")); QCOMPARE(tests.result("0").delay,42); QTest::qWait(300); QCOMPARE(server.requests.size(),4); QCOMPARE(tests.result("1").status,WebsiteResult::Untested);
        auto* temporary=new WebsiteTests(&api,nullptr,targets()); latency=100; temporary->start("destroyed"); QTRY_COMPARE(server.requests.size(),7); delete temporary; QTest::qWait(150); QCOMPARE(server.requests.size(),7);
        MihomoClient unconfigured; WebsiteTests synchronous(&unconfigured,nullptr,targets(1)); synchronous.start("node"); QTRY_VERIFY(!synchronous.running()); QCOMPARE(synchronous.result("0").status,WebsiteResult::Failed);
    }
    void realCoreFixedNodeAndHttpStatusMeaning() {
        const auto executable=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; QVERIFY(QFile::exists(executable));
        MockHttp first,second; QVERIFY(first.listen(QHostAddress::LocalHost,0)); QVERIFY(second.listen(QHostAddress::LocalHost,0));
        const auto handler=[](const QByteArray& request) { if (request.startsWith("CONNECT")) { HttpResponse r; r.tunnel=true; return r; } return HttpResponse{request.contains("blocked")?403:204,{},40}; };
        first.handler=handler; second.handler=handler; QTemporaryDir dir; Settings settings; settings.mixedPort=freePort(); settings.unifiedDelayMode="disabled"; const int port=freePort();
        const auto yaml=QString("proxies:\n  - {name: n1, type: http, server: 127.0.0.1, port: %1}\n  - {name: n2, type: http, server: 127.0.0.1, port: %2}\nproxy-groups:\n  - {name: A, type: select, proxies: [n1, n2]}\nrules: [\"MATCH,DIRECT\"]\n").arg(first.serverPort()).arg(second.serverPort()).toUtf8();
        const auto built=ConfigBuilder::build(yaml,settings,port,QString(64,'a')); QVERIFY2(built.result.ok,qUtf8Printable(built.result.error)); const auto config=dir.path()+"/config.yaml"; QVERIFY(writeAtomic(config,built.yaml).ok);
        MihomoClient api; CoreManager core(&api,createPlatformServices()); bool done=false; Result response;
        core.start(executable,config,dir.path(),settings.mixedPort,port,QString(64,'a'),[&](Result r) { response=r; done=true; }); QTRY_VERIFY_WITH_TIMEOUT(done,15000); QVERIFY2(response.ok,qUtf8Printable(response.error));
        const auto pid=core.processId(); WebsiteTests tests(&api,nullptr,{{"ok","OK","http://one.invalid/ok","O"},{"blocked","Blocked","http://two.invalid/blocked","B"}}); tests.start("n2"); QTRY_VERIFY_WITH_TIMEOUT(!tests.running(),3000);
        QCOMPARE(tests.result("ok").status,WebsiteResult::Responded); QCOMPARE(tests.result("blocked").status,WebsiteResult::Responded); QVERIFY(tests.result("ok").delay>=30); QVERIFY(first.requests.isEmpty());
        int heads=0; for (const auto& request:second.requests) if (request.startsWith("HEAD")) ++heads; QCOMPARE(heads,2);
        done=false; api.request("GET","/proxies/A",{},[&](Result r) { response=r; done=true; }); QTRY_VERIFY(done); QVERIFY(response.ok); QCOMPARE(response.data.value("now").toString(),QString("n1")); QCOMPARE(core.processId(),pid);
        done=false; core.stop([&](Result r) { response=r; done=true; }); QTRY_VERIFY(done); QVERIFY(response.ok);
    }
    void controllerNavigationAndGuiLifetime() {
        QTemporaryDir dir; MockHttp first,second; QVERIFY(first.listen(QHostAddress::LocalHost,0)); QVERIFY(second.listen(QHostAddress::LocalHost,0)); int latency=20;
        const auto handler=[&](const QByteArray&) { return HttpResponse{503,{},latency}; }; first.handler=handler; second.handler=handler;
        ProfileStore store(dir.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; settings.desiredProxy=false; settings.mixedPort=freePort(); settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; settings.startupPage=7; QVERIFY(store.save(settings,{}).ok);
        const auto yaml=QString("proxies:\n  - {name: n1, type: http, server: 127.0.0.1, port: %1}\n  - {name: n2, type: http, server: 127.0.0.1, port: %2}\nproxy-groups:\n  - {name: A, type: select, proxies: [n1, n2, DIRECT, REJECT]}\n  - {name: Nested, type: select, proxies: [A]}\n  - {name: Cycle1, type: select, proxies: [Cycle2]}\n  - {name: Cycle2, type: select, proxies: [Cycle1]}\nrules: [\"MATCH,DIRECT\"]\n").arg(first.serverPort()).arg(second.serverPort()).toUtf8();
        // Core forbids cycles: cycle handling remains a bounded defensive resolver, not a runnable fixture.
        auto runnable=yaml; runnable.replace("  - {name: Cycle1, type: select, proxies: [Cycle2]}\n  - {name: Cycle2, type: select, proxies: [Cycle1]}\n","");
        const auto path=dir.path()+"/source.yaml"; QVERIFY(writeAtomic(path,runnable).ok); AppController controller(dir.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished),delays(&controller,&AppController::nodeDelay); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        GuiHost host(&controller,true); host.show(); auto* window=host.window(); QCOMPARE(window->page(),7); QVERIFY(!window->findChild<QPushButton*>("startWebsiteTests")->isEnabled());
        controller.importFile("Website fixture",path); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); controller.activateProfile(controller.profiles().first().id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_VERIFY(controller.state().ready);
        auto* nodes=window->findChild<QComboBox*>("websiteNode"); QTRY_COMPARE(nodes->currentData().toString(),QString("n1")); QVERIFY(window->findChild<QPushButton*>("startWebsiteTests")->isEnabled()); QVERIFY(first.requests.isEmpty() && second.requests.isEmpty());
        QCOMPARE(controller.websiteNode("Nested"),QString("n1")); QVERIFY(controller.websiteNode("REJECT").isEmpty()); QVERIFY(controller.websiteNode("missing").isEmpty());
        auto* navigation=window->findChild<QListWidget*>("navigation"); QCOMPARE(navigation->count(),4); QCOMPARE(navigation->item(1)->text(),QString("配置")); window->setPage(3); QCOMPARE(navigation->currentRow(),1); navigation->setCurrentRow(3); QCOMPARE(window->page(),6);
        auto* startup=window->findChild<QComboBox*>("startupPage"); QCOMPARE(startup->itemData(3).toInt(),7); QCOMPARE(startup->itemData(4).toInt(),3); QCOMPARE(startup->itemData(7).toInt(),6); QCOMPARE(startup->currentData().toInt(),7);
        controller.startWebsiteTests("n1","github"); QCOMPARE(finished.size(),3); QVERIFY(first.requests.isEmpty());
        controller.selectMember("A","n2"); QTRY_COMPARE(finished.size(),4); QVERIFY(finished.last()[0].toBool()); window->setPage(7); QTRY_COMPARE(nodes->currentData().toString(),QString("n2")); const auto pid=controller.coreProcessId();
        controller.startWebsiteTests("n2","github"); QVERIFY(controller.websiteTests()->running()); QVERIFY(!controller.state().busy); QTRY_VERIFY_WITH_TIMEOUT(!controller.websiteTests()->running(),3000); QCOMPARE(controller.websiteTests()->result("github").status,WebsiteResult::Failed); QVERIFY(first.requests.isEmpty()); QVERIFY(!second.requests.isEmpty()); QCOMPARE(delays.size(),0); QCOMPARE(controller.proxies().value("A").toObject().value("now").toString(),QString("n2")); QCOMPARE(controller.coreProcessId(),pid);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(window->grab().save(output+"/website-result.png")); }
        latency=800; controller.startWebsiteTests("n2","github"); QTRY_VERIFY(second.requests.size()>=2); window->setPage(0); QVERIFY(!controller.websiteTests()->running()); QCOMPARE(controller.websiteTests()->result("github").status,WebsiteResult::Cancelled); QTest::qWait(850); QCOMPARE(controller.websiteTests()->result("github").status,WebsiteResult::Cancelled);
        window->setPage(7); controller.startWebsiteTests("n2","google"); window->hide(); QVERIFY(!controller.websiteTests()->running()); QCOMPARE(controller.websiteTests()->result("google").status,WebsiteResult::Cancelled); window->show(); QVERIFY(!controller.websiteTests()->running());
        controller.startWebsiteTests("n2","google"); controller.selectMember("A","n1"); QTRY_COMPARE(finished.size(),5); QVERIFY(finished.last()[0].toBool()); QVERIFY(!controller.websiteTests()->running()); QVERIFY(controller.websiteTests()->node().isEmpty());
        controller.startWebsiteTests("n1","youtube"); QPointer<MainWindow> old=window; host.dispose(); QVERIFY(old.isNull()); QVERIFY(controller.websiteTests()->node().isEmpty()); QVERIFY(!controller.websiteTests()->running()); host.show(7); QCOMPARE(host.window()->page(),7); QVERIFY(!controller.websiteTests()->running()); QCOMPARE(host.window()->findChild<QComboBox*>("websiteNode")->currentData().toString(),QString("n1")); host.dispose();
        for (int page:{3,6,7}) { auto desired=controller.settings(); desired.startupPage=page; controller.updateSettings(desired); QTRY_VERIFY(!controller.state().busy); QCOMPARE(store.load().settings.startupPage,page); }
        QSignalSpy exit(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exit.size(),1);
    }
    void cardLayoutAtSupportedSizes() {
        QTemporaryDir dir; AppController controller(dir.path(),true); QSignalSpy init(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&controller); window.show(); window.setPage(7);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(writeAtomic(output+"/website-render-context.json",QJsonDocument(QJsonObject{{"device_pixel_ratio",window.devicePixelRatioF()},{"platform",QGuiApplication::platformName()}}).toJson()).ok); }
        QCOMPARE(window.findChildren<QFrame*>("websiteCard").size(),12);
        for (const auto size:{QSize(840,560),QSize(940,620),QSize(1180,810)}) {
            window.resize(size); QCoreApplication::processEvents(); auto* scroll=window.findChild<QScrollArea*>("websiteScroll"); QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
            for (auto* card:window.findChildren<QFrame*>("websiteCard")) { QVERIFY(card->width()>=180); for (auto* child:card->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly)) QVERIFY2(card->rect().contains(child->geometry()),qUtf8Printable(child->objectName())); }
            for (auto name:{"websiteNode","startWebsiteTests","clearWebsiteTests"}) { auto* control=window.findChild<QWidget*>(name); QVERIFY(window.rect().contains(control->mapTo(&window,control->rect().bottomRight()))); }
            if (!output.isEmpty()) QVERIFY(window.grab().save(output+QString("/website-%1x%2.png").arg(size.width()).arg(size.height())));
        }
        QSignalSpy exit(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exit.size(),1);
    }
};
QTEST_MAIN(WebsiteTestCases)
#include "WebsiteTests.moc"
