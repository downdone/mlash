#include "ui/SettingsSearch.h"
#include "app/AppController.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "ui/MainWindow.h"
#include "ui/ProfileCardWidget.h"
#include "ui/DialogDraftGuard.h"
#include "ui/ProfileChangesDialog.h"
#include "profile/VersionChanges.h"
#include "ui/UiTheme.h"
#include "ui/WidgetStyle.h"
#include "ui/GuiHost.h"
#include "ui/ViewIds.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include "MockHttp.h"
#include <QtTest>
#include <QListView>
#include <QTemporaryDir>
#include <QLabel>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QTableWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QJsonDocument>
#include <QComboBox>
#include <QTableView>
#include <QUrlQuery>
#include <QCryptographicHash>
using namespace mlash;
class DepthPlatform final : public PlatformServices {
public:
    bool failSeal=false; int writes=0;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { if (failSeal) return Result::fail("injected save failure"); out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {true,{},{{"owned",true}}}; }
};
class WorkflowDepthTests final : public QObject {
    Q_OBJECT
    static void answer(QMessageBox::StandardButton button) {
        QTimer::singleShot(0,[button] { for (auto* widget:QApplication::topLevelWidgets()) if (auto* box=qobject_cast<QMessageBox*>(widget)) box->button(button)->click(); });
    }
    static QString summary(MainWindow& window,int page) {
        for (auto* label:window.findChildren<QLabel*>("statValue")) if (label->property("summaryPage").toInt()==page) return label->toolTip();
        return {};
    }
private slots:
    void initTestCase() { WidgetStyle::install(); UiTheme::install(); }
    void settingsSearchLocatesWithoutEditingOrDiscardingDrafts() {
        QTemporaryDir root;
        auto platform=std::make_shared<DepthPlatform>();
        AppController controller(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize();
        QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller);
        window.resize(840,560);
        window.setPage(6);
        window.show();
        auto* stack=window.findChild<QStackedWidget*>("settingsTaskStack");
        auto* search=window.findChild<QLineEdit*>("settingsSearch");
        auto* results=window.findChild<QListWidget*>("settingsInlineResults");
        auto* count=window.findChild<QPushButton*>("settingsSearchResults");
        auto* locator=window.findChild<SettingsSearch*>();
        QCOMPARE(stack->currentIndex(),0);
        QVERIFY(search->isVisible());
        window.activateWindow();
        QApplication::setActiveWindow(&window);
        auto* network=window.findChild<QPushButton*>("settingsNetworkRow");
        network->setFocus();
        QTRY_VERIFY(network->hasFocus());
        QTest::keyClick(network,Qt::Key_F,Qt::ControlModifier);
        QTRY_VERIFY(search->hasFocus());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
            QVERIFY(window.grab().save(output+"/settings-directory-search.png"));
        }

        search->setText("private-search-fixture");
        QCOMPARE(count->text(),QString("0 项"));
        QVERIFY(results->isVisible());
        QVERIFY(results->item(0)->text().contains("未找到"));
        QTest::keyClick(search,Qt::Key_Escape);
        QVERIFY(search->text().isEmpty());

        search->setText("外观");
        QVERIFY(results->isVisible());
        QVERIFY(results->item(0)->text().contains("常用 › 外观"));
        QVERIFY(results->item(0)->data(Qt::UserRole).toBool());
        if (!output.isEmpty()) QVERIFY(window.grab().save(output+"/settings-search-appearance.png"));
        search->setText("规则 自定义");
        QVERIFY(results->isVisible());
        QVERIFY(results->item(0)->text().contains("自定义规则"));
        if (!output.isEmpty()) QVERIFY(window.grab().save(output+"/settings-search.png"));
        QTest::mouseClick(results->viewport(),Qt::LeftButton,{},results->visualItemRect(results->item(0)).center());
        QCOMPARE(window.currentRoute().view,int(ViewIds::Rules));
        QCOMPARE(window.findChild<QListWidget*>("navigation")->currentRow(),1);

        window.setPage(6);
        search->setText("本地端口");
        QCOMPARE(count->text(),QString("1 项"));
        QTest::keyClick(search,Qt::Key_Return);
        QCOMPARE(stack->currentIndex(),1);
        QVERIFY(!search->isVisible());
        auto* port=window.findChild<QSpinBox*>("mixedPort");
        auto* target=window.findChild<QLineEdit*>("connectivityTestUrl");
        auto* scroll=window.findChild<QScrollArea*>("settingsScroll");
        QVERIFY(port->isVisible());
        const auto saved=controller.settings().mixedPort;
        port->setValue(saved+1);
        target->setText("https://private-search-fixture.test/");
        const auto geometry=scroll->geometry();
        port->setFocus();
        QTest::keyClick(port,Qt::Key_F,Qt::ControlModifier);
        QVERIFY(!search->hasFocus());
        emit locator->anchorRequested(window.findChild<QWidget*>("settingsAutostartRow"));
        QCOMPARE(stack->currentIndex(),1);
        QCOMPARE(scroll->geometry(),geometry);
        window.setPage(7);
        window.setPage(6);
        QVERIFY(port->isVisible());
        QCOMPARE(port->value(),saved+1);
        QCOMPARE(target->text(),QString("https://private-search-fixture.test/"));
        QCOMPARE(controller.settings().mixedPort,saved);
        QCOMPARE(finished.size(),0);
        QCOMPARE(platform->writes,0);
        QVERIFY(!QFile::exists(root.path()+"/state.dat"));
    }
    void unchangedFileWatchDoesNotResaveItsOwnIndexForever() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; settings.corePath=QCoreApplication::applicationDirPath()+"/fake_core.exe"; Profile p; p.id=newId(); p.name="Watch"; p.sourceType="file"; p.autoUpdate=true; p.source=root.path()+"/watched.yaml"; const QByteArray source="proxies: [{name: A, type: direct}]\nrules: [\"MATCH,DIRECT\"]\n"; QVERIFY(writeAtomic(p.source,source).ok); const auto prepared=store.prepare(p,source,settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); p=prepared.profile; p.sourceHash=QString::fromLatin1(QCryptographicHash::hash(source,QCryptographicHash::Sha256).toHex()); QVERIFY(store.save(settings,{p}).ok);
        AppController c(root.path(),false,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),changed(&c,&AppController::profilesChanged),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); QTRY_VERIFY(!c.state().busy); QTest::qWait(1300); QCOMPARE(changed.size(),1); c.updateProfile(p.id); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); const auto committed=changed.size(); QTest::qWait(2300); QVERIFY(!c.state().busy); QCOMPARE(changed.size(),committed); QCOMPARE(platform->writes,0);
    }
    void perGroupDelayUrlsDoNotShareCachedResults() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; Profile p; p.id=newId(); p.name="URLs"; p.sourceType="file"; const auto prepared=store.prepare(p,"proxies: [{name: A, type: direct}]\nrules: [\"MATCH,DIRECT\"]\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); p=prepared.profile; settings.activeProfile=p.id; p.nodePreferences={{"groups",QJsonObject{{"G",QJsonObject{{"test_url","https://example.test/one"}}},{"H",QJsonObject{{"test_url","https://example.test/two"}}}}}}; QVERIFY(store.save(settings,{p}).ok);
        AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); const_cast<AppState&>(c.state()).ready=true; MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); QStringList targets;
        server.handler=[&](const QByteArray& request) { const auto path=request.split(' ').value(1); const QUrlQuery query(QUrl::fromEncoded(path)); targets.append(query.queryItemValue("url")); return HttpResponse{200,QJsonDocument(QJsonObject{{"delay",targets.size()==1?51:92}}).toJson()}; }; c.findChild<MihomoClient*>()->configure(server.serverPort(),"secret"); c.testNodes({"A"},"G"); QTRY_COMPARE(finished.size(),1); QVERIFY(c.trayNodeDelay("A","G").value("known").toBool()); QVERIFY(!c.trayNodeDelay("A","H").value("known").toBool()); QCOMPARE(c.trayNodeDelay("A","G").value("delay").toInt(),51); QVERIFY(!c.trayNodeDelay("A","G").value("measured_at").toString().isEmpty()); c.testNodes({"A"},"H"); QTRY_COMPARE(finished.size(),2); QCOMPARE(targets,QStringList({"https://example.test/one","https://example.test/two"})); QCOMPARE(c.trayNodeDelay("A","H").value("delay").toInt(),92); QVERIFY(!c.trayNodeDelay("A","G").value("known").toBool()); QCOMPARE(platform->writes,0);
    }
    void groupDelayUsesBoundedParallelRequestsAndCancels() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); AppController c(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&c,&AppController::initialized), finished(&c,&AppController::operationFinished);
        QSignalSpy delays(&c,&AppController::nodeDelay), progress(&c,&AppController::delayProgress);
        c.initialize(); QTRY_COMPARE(initialized.size(),1); const_cast<AppState&>(c.state()).ready=true;
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost));
        server.handler=[](const QByteArray&) { return HttpResponse{200,"{\"delay\":72}",1200}; };
        c.findChild<MihomoClient*>()->configure(server.serverPort(),"secret");
        QStringList names; for (int i=0;i<24;++i) names.append(QString("node-%1").arg(i));
        c.testNodes(names); QTRY_VERIFY_WITH_TIMEOUT(server.requests.size()>=12,1000);
        QCOMPARE(server.requests.size(),12); QCOMPARE(progress.first()[0].toInt(),0); QCOMPARE(progress.first()[1].toInt(),24);
        QTRY_COMPARE_WITH_TIMEOUT(delays.size(),24,6000); QTRY_COMPARE(finished.size(),1);
        QCOMPARE(server.requests.size(),24); QCOMPARE(progress.last()[0].toInt(),24);
        for (const auto& result:delays) QCOMPARE(result[1].toInt(),72);
        server.requests.clear(); server.handler=[](const QByteArray&) { return HttpResponse{200,"{\"delay\":72}",2000}; };
        c.testNodes(names); QTRY_VERIFY_WITH_TIMEOUT(server.requests.size()>=12,1000);
        const int before=delays.size(); c.cancelTests(); QTRY_COMPARE(finished.size(),2);
        QTest::qWait(100); QCOMPARE(delays.size(),before); QCOMPARE(server.requests.size(),12);
        QVERIFY(!c.testingNodes()); QCOMPARE(platform->writes,0);
    }
    void nodeFiltersAndPreferenceValidation() {
        ProxyTableModel model; const QJsonObject proxies{{"HK 香港 A",QJsonObject{{"type","Shadowsocks"}}},{"JP 日本 B",QJsonObject{{"type","Vless"}}},{"Unknown",QJsonObject{{"type","Trojan"}}},{"G",QJsonObject{{"all",QJsonArray{"HK 香港 A","JP 日本 B","Unknown"}}}}}; model.setGroup(proxies,"G"); model.setDelay("HK 香港 A",80,{}); model.setDelay("JP 日本 B",-1,"timeout"); NodeFilter filter; filter.setSourceModel(&model); filter.setFilterKeyColumn(0); filter.setFilterCaseSensitivity(Qt::CaseInsensitive);
        filter.setConditions("HK",{},0,0,false,{}); QCOMPARE(filter.rowCount(),1); filter.setConditions({},"vless",2,0,false,{}); QCOMPARE(filter.rowCount(),1); filter.setConditions({},{},1,100,false,{}); QCOMPARE(filter.rowCount(),1); filter.setConditions({},{},3,0,false,{}); QCOMPARE(filter.rowCount(),1); filter.setConditions({},{},0,0,true,{"Unknown"}); QCOMPARE(filter.rowCount(),1); filter.setFilterFixedString("香港"); QCOMPARE(filter.rowCount(),0);
        model.setDelay("HK 香港 A",80,{},QDateTime::currentDateTimeUtc().addSecs(-61)); QVERIFY(model.data(model.index(0,3)).toString().contains("已过期")); QVERIFY(model.data(model.index(0,0),Qt::ToolTipRole).toString().contains("测于")); model.setTestContext("https://example.test/new"); QCOMPARE(model.data(model.index(0,2)).toString(),QString("—"));
        const QJsonObject valid{{"favorites",QJsonArray{"HK 香港 A"}},{"common_groups",QJsonArray{"G"}},{"groups",QJsonObject{{"G",QJsonObject{{"test_url","https://example.test/check"},{"view",1},{"sort",2}}}}},{"last_group","G"},{"disconnect_affected",false}}; QVERIFY(validateNodePreferences(valid).ok); auto invalid=valid; invalid["favorites"]=QJsonArray{"A","A"}; QVERIFY(!validateNodePreferences(invalid).ok); invalid=valid; invalid["groups"]=QJsonObject{{"G",QJsonObject{{"test_url","file:///etc/config"}}}}; QVERIFY(!validateNodePreferences(invalid).ok); invalid=valid; invalid["groups"]=QJsonObject{{"G",QJsonObject{{"sort",0.5}}}}; QVERIFY(!validateNodePreferences(invalid).ok);
    }
    void nodePreferencesPersistAndLightweightRestoresContext() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; Profile p; p.id=newId(); p.name="Nodes"; p.sourceType="file"; const QByteArray source="proxies: [{name: Leaf, type: direct}]\nrules: [\"MATCH,DIRECT\"]\n"; const auto prepared=store.prepare(p,source,settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); p=prepared.profile; settings.activeProfile=p.id; QVERIFY(store.save(settings,{p}).ok); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        const QJsonObject prefs{{"favorites",QJsonArray{"HK 香港 A"}},{"common_groups",QJsonArray{"G"}},{"groups",QJsonObject{{"G",QJsonObject{{"test_url","https://example.test/other"},{"view",1},{"sort",1}}}}},{"last_group","G"}}; platform->failSeal=true; c.updateNodePreferences(p.id,prefs,{}); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(c.nodePreferences(p.id).isEmpty()); platform->failSeal=false; c.updateNodePreferences(p.id,prefs,{}); QTRY_COMPARE(finished.size(),2); QVERIFY(finished.last()[0].toBool()); QCOMPARE(store.load().profiles[0].nodePreferences,prefs); QCOMPARE(c.groupTestUrl("G"),QString("https://example.test/other")); c.updateNodePreferences(p.id,{},{}); QCOMPARE(finished.size(),3); QVERIFY(!finished.last()[0].toBool());
        auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.core=CoreState::Running; const_cast<QJsonObject&>(c.proxies())={{"HK 香港 A",QJsonObject{{"type","Shadowsocks"}}},{"JP 日本 B",QJsonObject{{"type","Vless"}}},{"G",QJsonObject{{"type","Selector"},{"now","JP 日本 B"},{"all",QJsonArray{"HK 香港 A","JP 日本 B"}}}}};
        GuiHost host(&c,true); host.show(2); auto* window=host.window(); QVERIFY(window); QCOMPARE(window->memberView(),1);
        // H3：节点区是行列表；筛选、显示方式、节点选项都在「更多」菜单里。
        window->setMemberView(1); window->findChild<QLineEdit*>("nodeSearch")->setText("香港"); window->findChild<QCheckBox*>("nodeFavoritesOnly")->setChecked(true);
        auto* rows=window->findChild<QListView*>("nodeList"); QVERIFY(rows); auto* filtered=rows->model();
        QTRY_COMPARE(filtered->rowCount(),2);   // 组头 + 1 个匹配成员
        // 卡片视图开启时，书签取当前可见卡片，不读取隐藏行列表的旧选中。
        auto* cards=window->findChild<QListView*>("nodeCards"); QVERIFY(cards && cards->isVisible());
        QModelIndex selectedCard; for (int row=0;row<cards->model()->rowCount();++row) {
            const auto index=cards->model()->index(row,0);
            if (index.data(Qt::UserRole).toString()==QString("HK 香港 A")) { selectedCard=index; break; }
        }
        QVERIFY(selectedCard.isValid()); cards->setCurrentIndex(selectedCard); const auto bookmark=window->nodeBookmark();
        QCOMPARE(bookmark.value("member").toString(),QString("HK 香港 A")); QCOMPARE(bookmark.value("search").toString(),QString("香港"));
        QPointer<MainWindow> old=window; QVERIFY(window->close()); QTRY_VERIFY(old.isNull());
        host.show(2); QTRY_VERIFY(host.window());
        QCOMPARE(host.window()->nodeBookmark().value("group").toString(),QString("G"));
        QCOMPARE(host.window()->findChild<QLineEdit*>("nodeSearch")->text(),QString("香港"));
        QCOMPARE(host.window()->memberView(),1);
        QVERIFY(host.window()->findChild<QCheckBox*>("nodeFavoritesOnly")->isChecked());
        QCOMPARE(host.window()->nodeBookmark().value("member").toString(),QString("HK 香港 A"));
        QCOMPARE(host.window()->nodeBookmark().value("profile"),bookmark.value("profile"));
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(host.window()->grab().save(output+"/home-nodes.png")); }
        auto* preferences=host.window()->findChild<QAction*>("nodePreferences"); QVERIFY(preferences); preferences->trigger();
        auto* dialog=host.window()->findChild<QDialog*>("nodePreferencesDialog"); QVERIFY(dialog);
        auto* url=dialog->findChild<QLineEdit*>("groupTestUrl"); url->setText("file:///not-a-test-url"); dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        QTRY_COMPARE(finished.size(),4); QVERIFY(dialog->isVisible()); QCOMPARE(url->text(),QString("file:///not-a-test-url"));
        url->setText("https://example.test/edited"); platform->failSeal=true; dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        QTRY_COMPARE(finished.size(),5); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog->isVisible()); QCOMPARE(c.nodePreferences(p.id),prefs); platform->failSeal=false;
        if (!output.isEmpty()) { QCoreApplication::processEvents(); QVERIFY(dialog->grab().save(output+"/node-preferences-failure.png")); }
        answer(QMessageBox::Yes); dialog->reject(); host.dispose(); QCOMPARE(platform->writes,0);
    }
    void optInSelectionOnlyClosesPreexistingAffectedConnections() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; Profile p; p.id=newId(); p.name="Connections"; p.sourceType="file"; const auto prepared=store.prepare(p,"proxies: [{name: A, type: direct}, {name: B, type: direct}]\nproxy-groups: [{name: G, type: select, proxies: [A, B]}]\nrules: [\"MATCH,G\"]\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); p=prepared.profile; settings.activeProfile=p.id; QVERIFY(store.save(settings,{p}).ok); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.core=CoreState::Running;
        QString now="A",denied; bool invalidSnapshot=false; const auto affected=newId(),unrelated=newId(),fresh=newId(); QJsonArray connections{QJsonObject{{"id",affected},{"chains",QJsonArray{"A","G"}}},QJsonObject{{"id",unrelated},{"chains",QJsonArray{"A","Other"}}}}; MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost));
        server.handler=[&](const QByteArray& request) { const auto path=request.split(' ').value(1); if (request.startsWith("GET /proxies/G ")) return HttpResponse{200,QJsonDocument(QJsonObject{{"type","Selector"},{"now",now},{"all",QJsonArray{"A","B"}}}).toJson()}; if (request.startsWith("PUT /proxies/G ")) { now=QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n")+4)).object().value("name").toString(); if (now=="A") connections.append(QJsonObject{{"id",fresh},{"chains",QJsonArray{"A","G"}}}); return HttpResponse{204,{}}; } if (request.startsWith("GET /connections ")) return invalidSnapshot?HttpResponse{200,"{\"connections\":\"invalid\"}"}:HttpResponse{200,QJsonDocument(QJsonObject{{"connections",connections}}).toJson()}; if (request.startsWith("DELETE /connections/")) { if (!denied.isEmpty()&&path.endsWith(denied.toLatin1())) return HttpResponse{500,{}}; for (int i=connections.size()-1;i>=0;--i) if (path.endsWith(connections[i].toObject().value("id").toString().toLatin1())) connections.removeAt(i); return HttpResponse{204,{}}; } return HttpResponse{404,{}}; };
        c.findChild<MihomoClient*>()->configure(server.serverPort(),"secret"); const_cast<QJsonObject&>(c.proxies())={{"G",QJsonObject{{"type","Selector"},{"now",now},{"all",QJsonArray{"A","B"}}}}}; MainWindow home(&c);
        // H3：首页节点区是行列表，单击成员即生效（ADR 0048）；没有提交按钮。
        auto* rows=home.findChild<QListView*>("nodeList"); QVERIFY(rows); auto* model=rows->model(); QTRY_VERIFY(model->rowCount()>=3);
        int target=-1; for (int row=0;row<model->rowCount();++row) if (model->data(model->index(row,0),Qt::DisplayRole).toString()=="B") target=row;
        QVERIFY(target>0); QCOMPARE(finished.size(),0);
        QTest::mouseClick(rows->viewport(),Qt::LeftButton,{},rows->visualRect(model->index(target,0)).center());
        QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); QCOMPARE(connections.size(),2);
        QVERIFY(!home.findChild<QPushButton*>("homeSwitchNode"));
        for (const auto& request:server.requests) QVERIFY(!request.startsWith("GET /connections "));
        c.updateNodePreferences(p.id,{{"disconnect_affected",true}},{}); QTRY_COMPARE(finished.size(),2); QVERIFY(finished.last()[0].toBool()); c.selectMember("G","A"); QTRY_COMPARE(finished.size(),3); QVERIFY(finished.last()[0].toBool()); QVERIFY(finished.last()[1].toString().contains("已断开 1")); QCOMPARE(connections.size(),2); QVERIFY(connections[0].toObject().value("id").toString()==unrelated); QVERIFY(connections[1].toObject().value("id").toString()==fresh);
        connections=QJsonArray{QJsonObject{{"id",unrelated},{"chains",QJsonArray{"A","Other"}}}}; denied=newId(); for (int i=0;i<40;++i) connections.append(QJsonObject{{"id",i==0?denied:newId()},{"chains",QJsonArray{"A","G"}}}); server.requests.clear(); c.selectMember("G","B"); QTRY_COMPARE(finished.size(),4); QVERIFY(finished.last()[0].toBool()); QCOMPARE(connections.size(),10); QVERIFY(finished.last()[1].toString().contains("已断开 31")); QVERIFY(finished.last()[1].toString().contains("9 条保持")); int deletes=0; for (const auto& request:server.requests) if (request.startsWith("DELETE /connections/")) ++deletes; QCOMPARE(deletes,32);
        invalidSnapshot=true; server.requests.clear(); c.selectMember("G","A"); QTRY_COMPARE(finished.size(),5); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(now,QString("B")); for (const auto& request:server.requests) QVERIFY(!request.startsWith("PUT ")); QCOMPARE(platform->writes,0);
    }
    void maintenanceReportsActualRetryBeforeRegularInterval() {
        Profile p; p.id=newId(); p.sourceType="url"; p.autoUpdate=true; p.updateIntervalHours=12; const auto now=QDateTime::currentDateTimeUtc(); p.updated=now.toString(Qt::ISODate);
        ProfileMaintenance maintenance; maintenance.setProfiles({p}); maintenance.setEnabled(true); QVERIFY(maintenance.takeDue(now).isEmpty()); maintenance.completed(p.id,false,now);
        QCOMPARE(maintenance.status(p.id).value("failures").toInt(),1); QCOMPARE(maintenance.status(p.id).value("next").toString(),now.addSecs(300).toString(Qt::ISODate)); QVERIFY(maintenance.takeDue(now.addSecs(299)).isEmpty()); QCOMPARE(maintenance.takeDue(now.addSecs(300)),p.id);
        maintenance.completed(p.id,true,now.addSecs(300)); QCOMPARE(maintenance.status(p.id).value("failures").toInt(),0); QVERIFY(maintenance.takeDue(now.addSecs(301)).isEmpty());
    }
    void batchUpdatesPreserveVersionsContinueFailuresAndStopQueue() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore=false; settings.corePath=QCoreApplication::applicationDirPath()+"/fake_core.exe";
        const QByteArray source="proxies: [{name: Leaf, type: direct}]\nproxy-groups: [{name: Test, type: select, proxies: [Leaf]}]\nrules: [\"MATCH,Test\"]\n";
        QVector<Profile> profiles;
        for (int i=0;i<3;++i) { Profile p; p.id=newId(); p.name=QString("Batch %1").arg(i); p.sourceType="file"; p.source=root.path()+QString("/source%1.yaml").arg(i); QVERIFY(writeAtomic(p.source,source).ok); const auto prepared=store.prepare(p,source,settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); p=prepared.profile; p.active=p.candidate; profiles.append(p); }
        QVERIFY(store.save(settings,profiles).ok); QVERIFY(QFile::remove(profiles[0].source)); QVERIFY(writeAtomic(profiles[1].source,source+"# updated\n").ok);
        AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        const QStringList ids{profiles[0].id,profiles[1].id,profiles[2].id}; c.updateProfilesBatch(ids); QVERIFY(c.state().busy); c.renameProfile(ids[1],"must not interleave"); QCOMPARE(c.profiles()[1].name,profiles[1].name); finished.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!c.state().busy,15000); QCOMPARE(finished.size(),1); const auto rows=c.profileBatch().value("results").toArray(); QCOMPARE(rows.size(),3); QVERIFY(!rows[0].toObject().value("ok").toBool()); QVERIFY(rows[1].toObject().value("ok").toBool()); QVERIFY(rows[2].toObject().value("ok").toBool());
        QCOMPARE(c.profiles()[0].candidate,profiles[0].candidate); QCOMPARE(c.profiles()[1].active,profiles[1].active); QVERIFY(c.profiles()[1].candidate!=profiles[1].candidate); QVERIFY(!c.profiles()[0].updateError.isEmpty());
        c.updateProfilesBatch(ids); c.cancelProfileBatch(); QTRY_VERIFY(!c.state().busy); QCOMPARE(c.profileBatch().value("skipped").toInt(),3); QCOMPARE(c.profileBatch().value("results").toArray().size(),0);
        const auto connection=connect(&c,&AppController::profileBatchChanged,&c,[&] { if (c.profileBatch().value("results").toArray().size()==1) c.cancelProfileBatch(); });
        c.updateProfilesBatch(ids); QTRY_VERIFY(!c.state().busy); disconnect(connection); QCOMPARE(c.profileBatch().value("results").toArray().size(),1); QCOMPARE(c.profileBatch().value("skipped").toInt(),2);
        c.moveProfile(ids[2],-1); QTRY_VERIFY(!c.state().busy); QCOMPARE(c.profiles()[1].id,ids[2]); QCOMPARE(store.load().profiles[1].id,ids[2]);
        auto& current=const_cast<QVector<Profile>&>(c.profiles());
        for (int i=0;i<2;++i) {
            Profile snapshot; snapshot.id=newId(); snapshot.name=QString("快照 %1").arg(i);
            snapshot.sourceType="file";
            const auto prepared=store.prepare(snapshot,source,settings,19090,QString(64,'a'));
            QVERIFY(prepared.result.ok);
            current.append(prepared.profile);
        }
        MainWindow window(&c); window.show(); window.setPage(1);
        auto* batch=window.findChild<QPushButton*>("batchProfiles"); QVERIFY(batch && batch->isVisible());
        auto* list=window.findChild<QListView*>("profileList"); QVERIFY(list);
        auto* selection=list->selectionModel();
        const auto item=[list](int row) { return list->model()->index(row,0); };
        selection->clearSelection();
        QCOMPARE(batch->text(),QString("更新全部…"));
        selection->select(item(0),QItemSelectionModel::Select);
        QCOMPARE(batch->text(),QString("更新全部…"));
        batch->click();
        QPointer<QDialog> all=window.findChild<QDialog*>("profileBatchDialog"); QVERIFY(all);
        QCOMPARE(all->findChild<QTableWidget*>("profileBatchResults")->rowCount(),1);
        all->close(); QTRY_VERIFY(all.isNull());
        selection->select(item(3),QItemSelectionModel::Select);
        QCOMPARE(batch->text(),QString("更新所选 1 项…"));
        batch->click();
        QPointer<QDialog> selected=window.findChild<QDialog*>("profileBatchDialog"); QVERIFY(selected);
        auto* status=selected->findChild<QLabel*>("profileBatchStatus"); QVERIFY(status);
        QVERIFY(status->text().contains("跳过 1 项无来源快照"));
        auto* plan=selected->findChild<QTableWidget*>("profileBatchResults"); QVERIFY(plan);
        QCOMPARE(plan->rowCount(),1);
        QCOMPARE(plan->item(0,0)->text(),profiles[0].name);
        QCOMPARE(plan->item(0,1)->text(),QString("待更新"));
        QVERIFY(plan->height()<100);
        QVERIFY(selected->findChild<QPushButton*>("startProfileBatch")->property("primary").toBool());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QCoreApplication::processEvents();
            QVERIFY(QDir().mkpath(output));
            QVERIFY(selected->grab().save(output+"/subscription-batch.png"));
        }
        selected->findChild<QPushButton*>("startProfileBatch")->click();
        QTRY_VERIFY(!c.state().busy);
        QCOMPARE(c.profileBatch().value("ids").toArray(),QJsonArray{ids[0]});
        QCOMPARE(c.profiles()[0].active,profiles[0].active);
        selected->close(); QTRY_VERIFY(selected.isNull());
        selection->select(item(3),QItemSelectionModel::ClearAndSelect);
        selection->select(item(4),QItemSelectionModel::Select);
        QVERIFY(!batch->isEnabled());
        QCOMPARE(platform->writes,0);
    }
    void sourceEditValidatesBeforeCommitAndRejectsStaleDraft() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; settings.corePath=QCoreApplication::applicationDirPath()+"/fake_core.exe";
        const QByteArray source="proxies: [{name: Leaf, type: direct}]\nrules: [\"MATCH,DIRECT\"]\n"; Profile p; p.id=newId(); p.name="Source"; p.sourceType="file"; p.source=root.path()+"/old.yaml"; QVERIFY(writeAtomic(p.source,source).ok); const auto prepared=store.prepare(p,source,settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); p=prepared.profile; p.active=p.candidate; QVERIFY(store.save(settings,{p}).ok);
        AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); const auto expected=c.profileMaintenance(p.id);
        const auto bad=root.path()+"/bad.yaml"; QVERIFY(writeAtomic(bad,"proxies: [\n").ok); c.editProfileSource(p.id,"New",bad,true,4,{},expected); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.profiles()[0].source,p.source); QCOMPARE(c.profiles()[0].candidate,p.candidate);
        const auto good=root.path()+"/new.yaml"; QVERIFY(writeAtomic(good,source+"# new\n").ok); platform->failSeal=true; c.editProfileSource(p.id,"New",good,true,4,{},expected); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,10000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.profiles()[0].source,p.source); platform->failSeal=false;
        c.editProfileSource(p.id,"New",good,true,4,{},expected); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,10000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(c.profiles()[0].source,good); QCOMPARE(c.profiles()[0].active,p.active); QCOMPARE(c.profiles()[0].name,QString("New")); QVERIFY(c.profiles()[0].autoUpdate);
        c.editProfileSource(p.id,"stale",p.source,false,24,{},expected); QCOMPARE(finished.size(),4); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.profiles()[0].source,good); QCOMPARE(store.load().profiles[0].source,good); QCOMPARE(platform->writes,0);
    }
    void droppedImportsAndQrShareRequireExplicitAction() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&c); window.show();
        QMimeData mime; mime.setUrls({QUrl("https://example.test/sub?token=PRIVATE")}); QDragEnterEvent enter(QPoint(20,20),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(&window,&enter); QVERIFY(enter.isAccepted()); QDropEvent drop(QPointF(20,20),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(&window,&drop);
        auto* input=window.findChild<QLineEdit*>("profileImportInput"); QVERIFY(input&&input->isVisible());
        QCOMPARE(input->text(),QString("https://example.test/sub?token=PRIVATE"));
        QCOMPARE(input->echoMode(),QLineEdit::Password);
        QVERIFY(!window.findChild<QDialog*>("importDialog"));
        QVERIFY(!c.state().busy); QCOMPARE(c.profiles().size(),0);
        Profile p; p.id=newId(); p.name="Share"; p.sourceType="url"; p.source="https://example.test/sub?token=PRIVATE"; const_cast<QVector<Profile>&>(c.profiles()).append(p); emit c.profilesChanged(); window.setPage(1);
        auto* remote=window.findChild<QListView*>("profileList"); QVERIFY(remote); QTRY_COMPARE(remote->model()->rowCount(),1);
        QTest::mouseClick(qobject_cast<ProfileCardWidget*>(remote->indexWidget(remote->model()->index(0,0)))->button(ProfileCardWidget::Action::Menu),Qt::LeftButton);
        auto* detailsMenu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(detailsMenu);
        QAction* shareAction=nullptr;
        for (auto* action:detailsMenu->actions()) if (action->text()=="分享配置…") shareAction=action;
        QVERIFY(shareAction);
        detailsMenu->hide(); shareAction->trigger();
        auto* share=window.findChild<QDialog*>("profileShareDialog"); QVERIFY(share); auto* image=share->findChild<QLabel*>("profileQrImage"); QVERIFY(image->pixmap().isNull()); share->findChild<QPushButton*>("revealProfileQr")->click(); QCoreApplication::processEvents(); QVERIFY(!image->pixmap().isNull()); const auto pixels=image->pixmap().toImage(); QCOMPARE(pixels.pixelColor(0,0),QColor(Qt::white));
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(image->pixmap().save(output+"/subscription-qr.png")); QVERIFY(share->grab().save(output+"/subscription-share.png")); } share->close(); QCOMPARE(platform->writes,0);
    }
    void versionChangesAreBoundedPrivateAndStaleSafe() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        Profile p; p.id=newId(); p.name="Changes"; p.sourceType="file";
        const QByteArray oldSource="proxies: [{name: Old, type: direct}, {name: Keep, type: http, server: example.test, port: 80, password: OLD_CREDENTIAL}]\nproxy-groups: [{name: Test, type: select, proxies: [Old, Keep]}]\nproxy-providers: {Feed: {type: http, url: 'https://example.test/feed?token=PRIVATE_SOURCE'}}\nrules: [\"DOMAIN,old.example,Test\", \"MATCH,DIRECT\"]\n";
        const QByteArray newSource="proxies: [{name: New, type: direct}, {name: Keep, type: http, server: example.test, port: 80, password: NEW_CREDENTIAL}]\nproxy-groups: [{name: Test, type: select, proxies: [New, Keep]}]\nproxy-providers: {Feed: {type: http, url: 'https://example.test/feed?token=PRIVATE_SOURCE'}}\nrules: [\"DOMAIN,new.example,Test\", \"MATCH,DIRECT\"]\n";
        const auto first=store.prepare(p,oldSource,settings,19090,QString(64,'a')); QVERIFY(first.result.ok); p=first.profile; p.active=first.version; p.selections={{"Test","Old"},{"GLOBAL","DIRECT"}};
        const auto next=store.prepare(p,newSource,settings,19090,QString(64,'b')); QVERIFY(next.result.ok); p=next.profile;
        const auto provider=first.result.data.value("providers").toArray().first().toObject(); QVERIFY(writeAtomic(first.workPath+'/'+provider.value("path").toString(),"proxies: [{name: A, type: direct}]").ok); QVERIFY(writeAtomic(next.workPath+'/'+provider.value("path").toString(),"proxies: [{name: B, type: direct}, {name: C, type: direct}]").ok); QVERIFY(store.recordProviderSnapshot(first.workPath,provider,"proxies: [{name: A, type: direct}]").ok); QVERIFY(store.recordProviderSnapshot(next.workPath,provider,"proxies: [{name: B, type: direct}, {name: C, type: direct}]").ok);
        const auto report=VersionChanges::inspect(store,p); QVERIFY2(report.ok,qUtf8Printable(report.error)); const auto sections=report.data.value("sections").toArray(); QCOMPARE(sections.size(),5); QCOMPARE(sections[0].toObject().value("added_count").toInt(),1); QCOMPARE(sections[0].toObject().value("removed_count").toInt(),1); QCOMPARE(sections[0].toObject().value("changed_count").toInt(),1); QCOMPARE(sections[2].toObject().value("changed_count").toInt(),1); QCOMPARE(sections[3].toObject().value("added_count").toInt(),1); QCOMPARE(sections[3].toObject().value("removed_count").toInt(),1); QVERIFY(report.data.value("lost_selections").toArray().contains("Test → Old")); QCOMPARE(report.data.value("lost_selections").toArray().size(),1);
        const auto serialized=QJsonDocument(report.data).toJson(); QVERIFY(!serialized.contains("CREDENTIAL")); QVERIFY(!serialized.contains("PRIVATE_SOURCE")); QVERIFY(!serialized.contains(QByteArray(64,'a'))); QVERIFY(!serialized.contains("old.example"));
        auto firstUse=p; firstUse.active.clear(); const auto initial=VersionChanges::inspect(store,firstUse); QVERIFY(initial.ok); QVERIFY(initial.data.value("first_use").toBool()); QCOMPARE(initial.data.value("sections").toArray()[0].toObject().value("added_count").toInt(),2);
        QVERIFY(store.save(settings,{p}).ok); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1); bool inspected=false; Result providerInfo; c.inspectProviders(p.id,[&](Result result) { providerInfo=result; inspected=true; }); QTRY_VERIFY(inspected); QVERIFY(providerInfo.ok); QCOMPARE(providerInfo.data.value("scope").toString(),QString("candidate")); QCOMPARE(providerInfo.data.value("providers").toArray().first().toObject().value("count").toInt(),2); inspected=false; c.inspectProviders(p.id,[&](Result result) { providerInfo=result; inspected=true; },true); QTRY_VERIFY(inspected); QVERIFY(providerInfo.ok); QCOMPARE(providerInfo.data.value("scope").toString(),QString("active")); QCOMPARE(providerInfo.data.value("providers").toArray().first().toObject().value("count").toInt(),1);
        QSignalSpy submitted(&c,&AppController::operationFinished);
        auto* dialog=new ProfileChangesDialog(&c,p.id); dialog->show();
        QTRY_COMPARE(dialog->findChild<QTableWidget*>("versionChangesTable")->rowCount(),5);
        auto* apply=dialog->findChild<QPushButton*>("applyComparedVersion"); QVERIFY(apply && apply->isEnabled());
        auto* hint=dialog->findChild<QLabel*>("versionChangesHint"); QVERIFY(hint);
        auto* summary=dialog->findChild<QLabel*>("versionChangesSummary"); QVERIFY(summary);
        QVERIFY(summary->text().contains(p.name));
        QVERIFY(summary->text().contains("有 1 项已保存选择在候选中不可用"));
        QVERIFY(!summary->text().contains(p.candidate.left(12)));
        auto* metadata=dialog->findChild<QLabel*>("versionChangesMetadata"); QVERIFY(metadata);
        QVERIFY(metadata->toolTip().contains(p.candidate));
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
            QCoreApplication::processEvents();
            QVERIFY(dialog->grab().save(output+"/version-changes.png"));
        }
        apply->click();
        QPointer<QMessageBox> prompt=dialog->findChild<QMessageBox*>("profileVersionConfirmation"); QVERIFY(prompt);
        QVERIFY(prompt->text().contains(p.candidate));
        QVERIFY(prompt->informativeText().contains("中断连接"));
        QCOMPARE(prompt->defaultButton(),prompt->button(QMessageBox::No));
        QTest::keyClick(prompt,Qt::Key_Escape);
        QTRY_VERIFY(prompt.isNull());
        QCOMPARE(submitted.size(),0);
        apply->click();
        prompt=dialog->findChild<QMessageBox*>("profileVersionConfirmation"); QVERIFY(prompt);
        const_cast<QVector<Profile>&>(c.profiles())[0].candidate=newId();
        emit c.profilesChanged();
        QVERIFY(!apply->isEnabled());
        QVERIFY(hint->text().contains("比较已失效"));
        prompt->button(QMessageBox::Yes)->click();
        QTRY_VERIFY(prompt.isNull());
        QCOMPARE(submitted.size(),0);
        c.activateProfile(p.id,false,p.candidate);
        QCOMPARE(submitted.size(),1);
        QVERIFY(!submitted.last()[0].toBool());
        QCOMPARE(c.profiles().first().active,p.active);
        dialog->close();
        const auto malformed=writeAtomic(next.configPath,"proxies: [ PRIVATE_YAML_SECRET"); QVERIFY(malformed.ok); const auto failed=VersionChanges::inspect(store,p); QVERIFY(!failed.ok); QVERIFY(!failed.error.contains("PRIVATE_YAML_SECRET")); auto invalid=p; invalid.candidate="../outside"; QVERIFY(!VersionChanges::inspect(store,invalid).ok); QCOMPARE(platform->writes,0);
    }
    void homeModeRulesMetricsAndTunMeaning() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>();
        AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1);
        auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.busy=false; state.core=CoreState::Running; state.activeProfile="p";
        Profile profile; profile.id="p"; profile.name="Test"; profile.appendRules={"DOMAIN,example.com,DIRECT"}; profile.disabledRules={"MATCH,DIRECT"}; const_cast<QVector<Profile>&>(c.profiles()).append(profile);
        const_cast<QJsonObject&>(c.proxies())={{"DIRECT",QJsonObject{{"type","Direct"}}},{"Leaf",QJsonObject{{"type","Direct"}}},{"Other",QJsonObject{{"type","Direct"}}},{"Inner",QJsonObject{{"type","Selector"},{"now","Leaf"},{"all",QJsonArray{"Leaf"}}}},{"Test",QJsonObject{{"type","Selector"},{"now","Inner"},{"all",QJsonArray{"Inner"}}}},{"GLOBAL",QJsonObject{{"type","Selector"},{"now","Other"},{"all",QJsonArray{"Other"}}}}};
        // H2/H3：六卡摘要与组卡片删除后，这些事实分别由控制区第一行（当前代理）与
        // 列表底部的 inline-stat 表达；homeNode() 的选路语义本身不变。
        // H2 删掉了首页的组卡片，探测组改由外部入口（深链/记住的浏览组）设置；
        // 选路语义本身不变：规则模式按探测组逐层解析到唯一叶子。
        c.setHomeProbeGroup("Test");
        MainWindow window(&c); window.show(); QCoreApplication::processEvents();
        auto* currentMember=window.findChild<QLabel*>("homeCurrentNode"); QVERIFY(currentMember);
        auto* takeover=window.findChild<QLabel*>("homeTakeoverStatus"); QVERIFY(takeover);
        auto* inlineStat=window.findChild<QLabel*>("homeInlineStatText"); QVERIFY(inlineStat);
        QCOMPARE(c.homeNode(),QString("Leaf")); QTRY_COMPARE(currentMember->text(),QString("Leaf"));
        QCOMPARE(takeover->text(),QString("未接管流量"));
        state.mode="global"; emit c.stateChanged(); QCOMPARE(c.homeNode(),QString("Other")); QCOMPARE(currentMember->text(),QString("Other"));
        state.mode="direct"; emit c.stateChanged(); QCOMPARE(c.homeNode(),QString("DIRECT")); QCOMPARE(currentMember->text(),QString("DIRECT"));
        auto& metrics=const_cast<QJsonObject&>(c.homeMetrics()->snapshot()); metrics={{"active_count",7}}; emit const_cast<HomeMetrics*>(c.homeMetrics())->changed();
        QTRY_VERIFY(inlineStat->text().contains("7 条连接"));
        state.proxy=ProxyState::Unknown; emit c.stateChanged();
        QCOMPARE(takeover->text(),QString("系统代理状态未知"));
        state.proxy=ProxyState::Owned; emit c.stateChanged();
        QCOMPARE(takeover->text(),QString("系统代理已接管"));
        state.tunObserved=true; state.proxy=ProxyState::External; emit c.stateChanged();
        auto* proxySwitch=window.findChild<QCheckBox*>("systemProxy"); QVERIFY(proxySwitch);
        QTRY_VERIFY(proxySwitch->isChecked());
        QCOMPARE(takeover->text(),QString("系统代理由其他程序管理 · TUN 已接管"));
        QCOMPARE(proxySwitch->accessibleDescription(),takeover->text());
        QCOMPARE(currentMember->text(),QString("DIRECT"));
        QVERIFY(!window.findChild<QLabel*>("homeStateLine"));
        auto* control=window.findChild<QWidget*>("homeControlCard"); QVERIFY(control);
        const auto rows=control->findChildren<QWidget*>("homeControlRow"); QCOMPARE(rows.size(),2);
        auto* routeRow=control->findChild<QWidget*>("homeRouteRow"); QVERIFY(routeRow);
        QCOMPARE(rows[0]->height(),routeRow->height()); QCOMPARE(rows[1]->height(),routeRow->height());
        state.tunObserved=false; state.proxy=ProxyState::Disabled; state.ready=false; state.core=CoreState::Stopped;
        emit c.stateChanged(); QTRY_VERIFY(proxySwitch->isEnabled());
        metrics={}; emit const_cast<HomeMetrics*>(c.homeMetrics())->changed(); QVERIFY(!inlineStat->text().contains("7 条连接")); QCOMPARE(platform->writes,0);
    }
    void profilePolicyFailureRetainsEditorAndOldIndex() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        Profile p; p.id=newId(); p.name="Policy"; p.sourceType="url"; p.source="https://example.test/subscription";
        const auto prepared=store.prepare(p,"proxies: [{name: Leaf, type: direct}]\nproxy-groups: [{name: Test, type: select, proxies: [Leaf]}]\nrules: [\"MATCH,Test\"]\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings,{prepared.profile}).ok);
        AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&c); window.show(); window.setPage(1);
        auto* list=window.findChild<QListView*>("profileList"); QVERIFY(list); QTRY_COMPARE(list->model()->rowCount(),1);
        QTest::mouseClick(qobject_cast<ProfileCardWidget*>(list->indexWidget(list->model()->index(0,0)))->button(ProfileCardWidget::Action::Menu),Qt::LeftButton);
        auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu); menu->hide(); menu->actions().at(1)->trigger();
        auto* dialog=window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(dialog); auto* automatic=dialog->findChild<QCheckBox*>("profileAutoUpdate"); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QCoreApplication::processEvents(); QVERIFY(dialog->grab().save(output+"/subscription-details.png")); } automatic->setChecked(true); dialog->findChild<QSpinBox*>("profileUpdateHours")->setValue(6); platform->failSeal=true; dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog->isVisible()); QVERIFY(automatic->isChecked()); QVERIFY(!c.profiles()[0].autoUpdate); QVERIFY(!store.load().profiles[0].autoUpdate);
        platform->failSeal=false; dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click(); QTRY_COMPARE(finished.size(),2); QVERIFY(finished.last()[0].toBool()); QVERIFY(c.profiles()[0].autoUpdate); QCOMPARE(c.profiles()[0].updateIntervalHours,6); QCOMPARE(platform->writes,0);
    }
    void childDraftAndPendingPreventSilentGuiRelease() {
        QTemporaryDir root; auto platform=std::make_shared<DepthPlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1); MainWindow window(&c); window.setTrayAvailable(true); window.show(); QSignalSpy release(&window,&MainWindow::guiReleaseRequested);
        bool dirty=true,pending=true; auto* editor=new QDialog(&window); editor->setWindowTitle("Draft fixture"); DialogDraftGuard::protect(editor,[&] { return dirty; },[&] { return pending; }); editor->show(); QVERIFY(DialogDraftGuard::hasDrafts(&window)); QVERIFY(!window.close()); QCOMPARE(release.size(),0);
        // 生命周期契约：窗口「关闭」直接丢弃草稿、不询问，只有「取消」/Esc 才询问。
        QVERIFY(editor->isVisible()); editor->setAttribute(Qt::WA_DeleteOnClose,false); QVERIFY(editor->close()); QVERIFY(!editor->isVisible()); editor->show();
        pending=false;
        // Esc 是「取消」：有改动时先询问，答 No 则留在窗口。
        answer(QMessageBox::No); QTest::keyClick(editor,Qt::Key_Escape); QVERIFY(editor->isVisible());
        answer(QMessageBox::Yes); QVERIFY(editor->close()); QVERIFY(!editor->isVisible()); editor->show(); answer(QMessageBox::No); QVERIFY(!window.close()); QCOMPARE(release.size(),0); answer(QMessageBox::Yes); QVERIFY(window.close()); QCOMPARE(release.size(),1); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(WorkflowDepthTests)
#include "WorkflowDepthTests.moc"
