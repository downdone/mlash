#include "app/AppController.h"
#include "storage/Storage.h"
#include "ui/ConnectionOptions.h"
#include "ui/ConnectionDetailsDialog.h"
#include "ui/RuleOverlayDialog.h"
#include <QClipboard>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QScopeGuard>
#include "ui/ActivityTableModel.h"
#include "ui/MainWindow.h"
#include "ui/WidgetStyle.h"
#include "ui/UiTheme.h"
#include "app/ConnectionSession.h"
#include "profile/ProfileStore.h"
#include "MockHttp.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QCheckBox>
#include <QKeyEvent>
#include <QAction>
#include <QPushButton>
#include <QJsonDocument>
#include <QScrollBar>
#include <QMenu>
#include <QContextMenuEvent>
using namespace mlash;
class ViewPlatform final : public PlatformServices {
public:
    int writes=0; bool failSeal=false;
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { ++writes; return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& in,QByteArray& out) override { if (failSeal) return Result::fail("injected index failure"); out=in; return {}; }
    Result open(const QByteArray& in,QByteArray& out) override { out=in; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64,int) override { return {}; }
};
class ConnectionViewTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WidgetStyle::install(); }
    void returningToConnectionsRestoresFilterSelectionAndPosition() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&c); window.resize(850,540); window.show(); window.setPage(4);
        auto* table=window.findChild<QTableView*>("connectionsTable");
        auto* search=window.findChild<QLineEdit*>("connectionsSearch");
        auto* tabs=window.findChild<QTabBar*>("dataTabs");
        QVERIFY(table && search && tabs && c.connectionsVisible());
        QJsonArray snapshot;
        for (int i=0;i<60;++i)
            snapshot.append(QJsonObject{{"id",QString("connection-%1").arg(i)},
                {"metadata",QJsonObject{{"host",QString("match-%1.example").arg(i)},
                    {"destinationPort",443}}}});
        const auto refresh=[&] { const_cast<QJsonArray&>(c.connections())=snapshot; c.connectionsChanged(); };
        search->setText("match-"); refresh();
        QTRY_COMPARE(table->model()->rowCount(),60);
        auto* scroll=table->verticalScrollBar(); QTRY_VERIFY(scroll->maximum()>0);
        table->selectRow(20);
        const auto selected=table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString();
        scroll->setValue(qMin(250,scroll->maximum())); const int position=scroll->value(); QVERIFY(position>0);
        window.setPage(0); QVERIFY(!c.connectionsVisible());
        window.setPage(4); QVERIFY(c.connectionsVisible()); refresh();
        QCOMPARE(search->text(),QString("match-")); QCOMPARE(scroll->value(),position);
        QCOMPARE(table->selectionModel()->selectedRows().first().data(Qt::UserRole).toString(),selected);
        scroll->setValue(qMin(330,scroll->maximum())); const int tabPosition=scroll->value();
        tabs->setCurrentIndex(3); QVERIFY(!c.connectionsVisible());
        tabs->setCurrentIndex(1); QVERIFY(c.connectionsVisible()); refresh();
        QCOMPARE(scroll->value(),tabPosition);
        QCOMPARE(search->text(),QString("match-"));
        QVERIFY(!window.findChild<QDialog*>("connectionDetails"));
        QCOMPARE(platform->writes,0);
    }
    void logCommandsHaveOneVisibleEntryAndCopySelection() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&c); window.show(); window.setPage(5);
        auto* view=window.findChild<QPlainTextEdit*>("logView");
        auto* menu=window.findChild<QMenu*>("logOptionsMenu");
        auto* exportButton=window.findChild<QToolButton*>("exportDiagnosticsButton");
        QVERIFY(view && menu && exportButton && exportButton->isVisible());
        // The export belongs to the same local toolbar as the log filters.
        QVERIFY(window.findChild<QWidget*>("logToolbar")->isAncestorOf(exportButton));
        c.diagnostics()->append("app", "Log page remains readable before core startup");
        QTRY_VERIFY(view->toPlainText().contains("Log page remains readable"));
        QVERIFY(!window.findChild<QLabel*>("logEmptyState")->isVisible());
        QVERIFY(!window.findChild<QToolButton*>("clearLogViewButton"));
        QAction* copy=nullptr; int clearCount=0, exportCount=0;
        for (auto* action:menu->actions()) {
            if (action->text()=="复制选中") copy=action;
            if (action->text()=="清空视图") ++clearCount;
            if (action->text().contains("导出")) ++exportCount;
        }
        QVERIFY(copy); QCOMPARE(clearCount,1); QCOMPARE(exportCount,0);
        view->setPlainText("line one\nline two");
        auto cursor=view->textCursor(); cursor.movePosition(QTextCursor::Start); cursor.movePosition(QTextCursor::EndOfLine,QTextCursor::KeepAnchor); view->setTextCursor(cursor);
        QVERIFY(copy->isEnabled());
        const auto old=QApplication::clipboard()->text(); const auto restore=qScopeGuard([old] { QApplication::clipboard()->setText(old); });
        copy->trigger(); QCOMPARE(QApplication::clipboard()->text(),QString("line one"));
        QCOMPARE(platform->writes,0);
    }
    void ruleSuggestionsUseOnlySafeObservedFields() {
        const auto suggestions=ConnectionDetailsDialog::ruleSuggestions({{"metadata",QJsonObject{{"host","Example.TEST."},{"destinationIP","2001:db8::1"},{"processPath","C:\\Apps\\browser.exe"}}}});
        QCOMPARE(suggestions.size(),3); QCOMPARE(suggestions[0].toObject().value("payload").toString(),QString("example.test")); QCOMPARE(suggestions[1].toObject().value("type").toString(),QString("IP-CIDR6")); QCOMPARE(suggestions[1].toObject().value("payload").toString(),QString("2001:db8::1/128")); QCOMPARE(suggestions[1].toObject().value("option").toString(),QString("no-resolve")); QCOMPARE(suggestions[2].toObject().value("payload").toString(),QString("browser.exe"));
        for (const auto& host:QStringList{"127.0.0.1","bad,example.test","bad\nexample.test",QString(254,'a')}) QVERIFY(ConnectionDetailsDialog::ruleSuggestions({{"metadata",QJsonObject{{"host",host}}}}).isEmpty());
        for (const auto& process:QStringList{"a,b.exe","a\nb.exe",QString(257,'a')}) QVERIFY(ConnectionDetailsDialog::ruleSuggestions({{"metadata",QJsonObject{{"process",process}}}}).isEmpty());
        QVERIFY(ConnectionDetailsDialog::ruleSuggestions({{"metadata",QJsonObject{{"destinationIP","fe80::1%4"}}}}).isEmpty()); QVERIFY(ConnectionDetailsDialog::ruleSuggestions({}).isEmpty());
        const auto v4=ConnectionDetailsDialog::ruleSuggestions({{"metadata",QJsonObject{{"destinationIP","192.0.2.1"}}}}); QCOMPARE(v4[0].toObject().value("payload").toString(),QString("192.0.2.1/32"));
    }
    void structuredSnapshotCopyAndStaleContext() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1);
        Profile profile; profile.id=newId(); profile.name="Fixture"; profile.active="v1"; const_cast<QVector<Profile>&>(c.profiles()).append(profile); auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.core=CoreState::Running; state.activeProfile=profile.id;
        const_cast<QJsonObject&>(c.proxies())=QJsonObject{{"G",QJsonObject{{"all",QJsonArray{"Leaf"}},{"now","Leaf"}}},{"Leaf",QJsonObject{{"type","Http"}}}};
        const QString path="C:/"+QString(900,'x')+"/browser.exe"; const QJsonObject row{{"id",newId()},{"metadata",QJsonObject{{"host","example.test"},{"destinationIP","2001:db8::1"},{"destinationPort",443},{"processPath",path}}},{"chains",QJsonArray{"Leaf","G","Missing"}},{"_observed_at","2026-10-17T01:02:03.000Z"},{"rule","Domain"},{"rulePayload","example.test"},{"_active",false}};
        ActivityTableModel model(true); model.setRows({row}); QCOMPARE(model.index(0,0).data(Qt::UserRole+3).value<QJsonObject>(),row);
        QPointer<ConnectionDetailsDialog> dialog=new ConnectionDetailsDialog(&c,row,profile.id,profile.active); dialog->show(); QTest::qWait(100); QVERIFY(dialog->height()<=540); auto* groups=dialog->findChild<QComboBox*>("connectionGroups"); QCOMPARE(groups->count(),1); QCOMPARE(groups->currentData().toString(),QString("G")); QVERIFY(dialog->findChild<QPushButton*>("locateConnectionGroup")->isEnabled()); QVERIFY(dialog->findChild<QPushButton*>("createConnectionRule")->isEnabled()); QVERIFY(!dialog->findChild<QPushButton*>("disconnectConnection")->isEnabled()); QCOMPARE(dialog->findChild<QLabel*>("connectionField_destination")->text(),QString("[2001:db8::1]:443"));
        auto* displayed=dialog->findChild<QLabel*>("connectionField_processPath"); QCOMPARE(displayed->text(),path.left(512)+"…");
        dialog->resize(560,400); QTest::qWait(80); QVERIFY(dialog->height()<=430); auto* actions=dialog->findChild<QDialogButtonBox*>(); QVERIFY(actions->geometry().bottom()<dialog->height()); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(dialog->grab().save(output+"/connection-details-small.png")); }
        QSignalSpy suggestion(dialog,&ConnectionDetailsDialog::createRule); dialog->findChild<QAction*>("connectionRule_DOMAIN")->trigger(); QCOMPARE(suggestion.size(),1); QCOMPARE(suggestion[0][0].toJsonObject().value("target").toString(),QString("G")); QCOMPARE(suggestion[0][0].toJsonObject().value("active_version").toString(),profile.active);
        const_cast<QVector<Profile>&>(c.profiles())[0].active="v2"; c.stateChanged(); QVERIFY(!dialog->findChild<QPushButton*>("locateConnectionGroup")->isEnabled()); QVERIFY(!dialog->findChild<QPushButton*>("createConnectionRule")->isEnabled()); dialog->findChild<QAction*>("connectionRule_DOMAIN")->trigger(); QCOMPARE(suggestion.size(),1); QCOMPARE(displayed->text(),path.left(512)+"…"); QVERIFY(dialog->findChild<QLabel*>("connectionDetailsHint")->text().contains("上下文")); dialog->reject(); QTRY_VERIFY(dialog.isNull());
        const_cast<QVector<Profile>&>(c.profiles())[0].active="v1"; dialog=new ConnectionDetailsDialog(&c,row,profile.id,profile.active); QSignalSpy browse(dialog,&ConnectionDetailsDialog::browseGroup); dialog->findChild<QPushButton*>("locateConnectionGroup")->click(); QCOMPARE(browse.size(),1); QCOMPARE(browse[0][0].toString(),QString("G")); QTRY_VERIFY(dialog.isNull()); QCOMPARE(c.proxies().value("G").toObject().value("now").toString(),QString("Leaf")); QCOMPARE(platform->writes,0);
    }
    void connectionGenerationInvalidatesAssociatedCommands() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform);
        QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1);
        Profile profile; profile.id=newId(); profile.name="Context"; profile.active="v1"; const_cast<QVector<Profile>&>(c.profiles()).append(profile);
        auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.activeProfile=profile.id;
        const_cast<QJsonObject&>(c.proxies())={{"G",QJsonObject{{"all",QJsonArray{"Leaf"}},{"now","Leaf"}}},{"Leaf",QJsonObject{{"type","Http"}}}};
        const QJsonObject row{{"id",newId()},{"metadata",QJsonObject{{"host","example.test"}}},{"chains",QJsonArray{"Leaf","G"}},{"_active",false}};
        QPointer<ConnectionDetailsDialog> dialog=new ConnectionDetailsDialog(&c,row,profile.id,"v1");
        QSignalSpy suggestions(dialog,&ConnectionDetailsDialog::createRule); auto* rule=dialog->findChild<QAction*>("connectionRule_DOMAIN"); QVERIFY(rule); rule->trigger(); QCOMPARE(suggestions.size(),1);
        QCOMPARE(suggestions[0][0].toJsonObject().value("generation").toInteger(),qint64(state.generation));
        ++state.generation; c.stateChanged(); QVERIFY(!dialog->findChild<QPushButton*>("createConnectionRule")->isEnabled()); QVERIFY(!dialog->findChild<QPushButton*>("locateConnectionGroup")->isEnabled()); rule->trigger(); QCOMPARE(suggestions.size(),1); QCOMPARE(platform->writes,0);
        dialog->reject(); QTRY_VERIFY(dialog.isNull());
    }
    void fullFieldCopyPreservesUnabridgedText() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1);
        const QString path="C:/"+QString(900,'x')+"/browser.exe"; QPointer<ConnectionDetailsDialog> dialog=new ConnectionDetailsDialog(&c,{{"metadata",QJsonObject{{"processPath",path}}}},{},{}); const auto old=QApplication::clipboard()->text(); const auto restore=qScopeGuard([old] { QApplication::clipboard()->setText(old); }); dialog->findChild<QToolButton*>("copyConnection_processPath")->click(); QCOMPARE(QApplication::clipboard()->text(),path); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(platform->writes,0);
    }
    void connectionContextMenuKeepsItsOriginalIdentity() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(initialized.size(),1);
        const QString first=newId(), second=newId();
        auto row=[](const QString& id,const QString& host) { return QJsonObject{{"id",id},{"metadata",QJsonObject{{"host",host}}},{"_active",true}}; };
        MainWindow window(&c); window.resize(900,600); window.show(); window.setPage(4);
        auto* table=window.findChild<QTableView*>("connectionsTable"); QVERIFY(table);
        auto* filter=qobject_cast<QSortFilterProxyModel*>(table->model()); QVERIFY(filter);
        auto* model=static_cast<ActivityTableModel*>(filter->sourceModel()); QVERIFY(model);
        model->setRows(QJsonArray{row(first,"first.example")}); QTRY_COMPARE(table->model()->rowCount(),1);
        const QPoint at=table->visualRect(table->model()->index(0,0)).center();
        QContextMenuEvent event(QContextMenuEvent::Mouse,at,table->viewport()->mapToGlobal(at)); QApplication::sendEvent(table->viewport(),&event);
        QPointer<QMenu> menu=table->findChild<QMenu*>("connectionContextMenu"); QVERIFY(menu); QTRY_VERIFY(menu->isVisible());
        QCOMPARE(menu->actions().first()->text(),QString("详情"));
        const QString previous=QApplication::clipboard()->text(); QApplication::clipboard()->setText("unchanged");
        model->setRows(QJsonArray{row(second,"second.example")}); QCoreApplication::processEvents();
        menu->actions().at(1)->trigger(); QCOMPARE(QApplication::clipboard()->text(),QString("unchanged"));
        QApplication::clipboard()->setText(previous); if (menu) menu->close(); QCOMPARE(platform->writes,0);
    }
    void connectionRuleCarriesDraftAndCancelKeepsParentText() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; Profile p; p.id=newId(); p.name="Fixture"; p.sourceType="file";
        const auto prepared=store.prepare(p,"proxies: [{name: Leaf, type: http, server: 127.0.0.1, port: 9}]\nproxy-groups: [{name: G, type: select, proxies: [Leaf, DIRECT]}]\nrules: ['MATCH,G']\n",settings,19090,QString(64,'a')); QVERIFY2(prepared.result.ok,qUtf8Printable(prepared.result.error)); auto profile=prepared.profile; profile.active=prepared.version; settings.activeProfile=p.id; QVERIFY(store.save(settings,{profile}).ok);
        AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1);
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); const auto row=QJsonObject{{"id",newId()},{"metadata",QJsonObject{{"host","example.test"}}},{"chains",QJsonArray{"G","Leaf"}},{"upload",0},{"download",0}}; server.handler=[&](const QByteArray& request) { if (request.startsWith("GET /connections ")) return HttpResponse{200,QJsonDocument(QJsonObject{{"connections",QJsonArray{row}},{"uploadTotal",0},{"downloadTotal",0}}).toJson()}; return HttpResponse{200,R"({"proxies":{"G":{"all":["Leaf","DIRECT"],"now":"Leaf","type":"Selector"},"Leaf":{"type":"Http"}}})"}; }; c.findChild<MihomoClient*>()->configure(server.serverPort(),"fixture"); auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.core=CoreState::Running; state.activeProfile=p.id; const_cast<QJsonObject&>(c.proxies())={{"G",QJsonObject{{"all",QJsonArray{"Leaf","DIRECT"}},{"now","Leaf"}}},{"Leaf",QJsonObject{{"type","Http"}}}};
        MainWindow window(&c); window.show(); auto* editor=window.findChild<QPlainTextEdit*>("customRulesEditor"); const QString draft="# keep exact formatting\n DOMAIN,parent.test,DIRECT \n"; editor->setPlainText(draft); window.setPage(4); QTRY_VERIFY(c.connectionsCurrent()); auto* table=window.findChild<QTableView*>("connectionsTable"); table->selectRow(0); window.findChild<QAction*>("connectionDetailsAction")->trigger(); auto* details=window.findChild<ConnectionDetailsDialog*>(); QVERIFY(details);
        const auto screenshot=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!screenshot.isEmpty()) { QDir().mkpath(screenshot); QTest::qWait(40); QVERIFY(window.grab().save(screenshot+"/connection-inspector-page.png")); window.resize(1440,900); QTest::qWait(40); QVERIFY(window.grab().save(screenshot+"/connection-inspector-side.png")); }
        details->findChild<QAction*>("connectionRule_DOMAIN")->trigger(); QPointer<RuleOverlayDialog> overlay=window.findChild<RuleOverlayDialog*>(); QVERIFY(overlay); auto* rules=overlay->findChild<QTableWidget*>("prependRulesTable"); QCOMPARE(rules->rowCount(),2); QCOMPARE(rules->item(0,1)->text(),QString("example.test")); QCOMPARE(rules->item(0,2)->text(),QString("G")); QCOMPARE(rules->item(1,1)->text(),QString("parent.test")); QVERIFY(overlay->findChild<QLabel*>("ruleDraftOrigin")->text().contains("快照")); QTRY_VERIFY(!overlay->findChild<QLabel*>("ruleOverlayHint")->text().contains("正在"));
        // Reject dirty child through the real discard guard, never save or mutate the parent draft.
        QTimer::singleShot(0,[] { if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); }); overlay->close(); QTRY_VERIFY(overlay.isNull()); QCOMPARE(editor->toPlainText(),draft); QCOMPARE(c.profiles()[0].customRules,QStringList{}); QCOMPARE(c.profiles()[0].active,prepared.version); QCOMPARE(finished.size(),0); for (const auto& request:server.requests) QVERIFY(request.startsWith("GET ")); QCOMPARE(platform->writes,0); window.hide();
    }
    void optionalColumnsAndLegacyPreferences() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok);
        QJsonArray legacy; for (int id=0;id<7;++id) legacy.append(QJsonObject{{"id",id},{"width",id==3?231:90},{"visible",id!=4}}); const QJsonValue third=legacy[3]; legacy.removeAt(3); legacy.insert(1,third); Settings settings; settings.connectionView={{"columns",legacy}}; QVERIFY(store.save(settings,{}).ok);
        AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1); QSignalSpy finished(&c,&AppController::operationFinished);
        ActivityTableModel model(true); const auto row=QJsonObject{{"id",newId()},{"metadata",QJsonObject{{"destinationIP","2001:db8::1"},{"destinationPort",443},{"sourceIP","::1"},{"sourcePort",1234},{"processPath","C:\\Apps\\browser.exe"}}},{"upload",2048},{"download",100},{"_upload_rate",1500},{"start","2026-10-17T01:02:03Z"}}; auto unknown=row; unknown["id"]=newId(); unknown["metadata"]=QJsonObject{}; unknown["start"]="bad time"; unknown["upload"]=10; unknown["_upload_rate"]=QJsonValue(); model.setRows({row,unknown}); QCOMPARE(model.columnCount(),int(ConnectionColumns::Count)); QCOMPARE(model.index(0,0).data().toString(),QString("[2001:db8::1]:443")); QCOMPARE(model.index(0,ConnectionColumns::Process).data().toString(),QString("browser.exe")); QCOMPARE(model.index(0,ConnectionColumns::Source).data().toString(),QString("[::1]:1234")); QVERIFY(model.index(0,0).data(Qt::ToolTipRole).toString().contains("来源：[::1]:1234")); QVERIFY(model.index(0,ConnectionColumns::Started).data(Qt::UserRole+1).toLongLong()>0); QCOMPARE(model.index(1,ConnectionColumns::Process).data().toString(),QString("—")); QCOMPARE(model.index(1,ConnectionColumns::Started).data().toString(),QString("—"));
        QSortFilterProxyModel filter; filter.setSourceModel(&model); filter.setSortRole(Qt::UserRole+1); filter.sort(ConnectionColumns::Upload,Qt::AscendingOrder); QCOMPARE(filter.index(0,0).data(Qt::UserRole).toString(),unknown.value("id").toString());
        QWidget host; QTableView table(&host); table.setModel(&filter); QLineEdit search(&host); QLabel error(&host); ConnectionOptions options(&c,&table,&filter,&search,&error,&host); QCOMPARE(table.horizontalHeader()->logicalIndex(1),3); QCOMPARE(table.columnWidth(3),231); QVERIFY(table.isColumnHidden(4)); for (int id=7;id<ConnectionColumns::Count;++id) QVERIFY(table.isColumnHidden(id)); QCOMPARE(options.preferences().value("columns").toArray().size(),int(ConnectionColumns::Count)); QVERIFY(validateConnectionView(options.preferences()).ok); options.findChild<QCheckBox*>("connectionColumn7")->setChecked(true); QVERIFY(!table.isColumnHidden(7)); options.flushPreferences(); QTRY_COMPARE(finished.size(),1); QVERIFY(finished[0][0].toBool()); QCOMPARE(c.settings().connectionView,options.preferences()); const auto reloaded=store.load(); QVERIFY(reloaded.result.ok); QCOMPARE(reloaded.settings.connectionView,options.preferences());
        auto partial=options.preferences(); auto columns=partial.value("columns").toArray(); columns.removeLast(); partial["columns"]=columns; QVERIFY(!validateConnectionView(partial).ok); QCOMPARE(platform->writes,0);
    }
    void suspendedSessionStartsANewRateAndDisappearanceBaseline() {
        ConnectionSession session; auto row=QJsonObject{{"id",newId()},{"metadata",QJsonObject{}},{"upload",100},{"download",100}}; QJsonObject snapshot{{"connections",QJsonArray{row}},{"uploadTotal",100},{"downloadTotal",100}}; QVERIFY(session.update(snapshot,0).ok); row["download"]=500; snapshot["connections"]=QJsonArray{row}; snapshot["downloadTotal"]=500; QVERIFY(session.update(snapshot,2000).ok); QCOMPARE(session.active()[0].toObject().value("_download_rate").toDouble(),200.0); const auto observed=session.totals().value("_observed_at"); session.suspend(); QVERIFY(session.active()[0].toObject().value("_paused").toBool()); QVERIFY(session.active()[0].toObject().value("_download_rate").isNull()); QCOMPARE(session.totals().value("_observed_at"),observed);
        snapshot["connections"]=QJsonArray{}; QVERIFY(session.update(snapshot,10000).ok); QVERIFY(session.closed().isEmpty()); QVERIFY(session.totals().value("_downloadTotal_rate").isNull()); QVERIFY(!session.totals().value("_paused").toBool());
        snapshot["connections"]=QJsonArray{row}; QVERIFY(session.update(snapshot,12000).ok); snapshot["connections"]=QJsonArray{}; QVERIFY(session.update(snapshot,14000).ok); QCOMPARE(session.closed().size(),1); const auto last=session.observedAt(); QVERIFY(!last.isEmpty()); session.invalidate(); QCOMPARE(session.observedAt(),last); session.reset(); QVERIFY(session.observedAt().isEmpty());
    }
    void pauseRejectsLateReadsAndDisablesFrozenCloseCommands() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        const auto first=newId(),next=newId(); int reads=0; bool fail=false; MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); server.handler=[&](const QByteArray& request) { if (!request.startsWith("GET /connections ")) return HttpResponse{200,"{\"proxies\":{}}"}; ++reads; if (fail) return HttpResponse{500,"{}"}; const auto name=reads==1?"first.example":reads==2?"late.example":"next.example"; const auto id=reads<3?first:next; const auto row=QJsonObject{{"id",id},{"metadata",QJsonObject{{"host",name},{"network","tcp"}}},{"upload",100*reads},{"download",200*reads}}; return HttpResponse{200,QJsonDocument(QJsonObject{{"connections",QJsonArray{row}},{"uploadTotal",100*reads},{"downloadTotal",200*reads}}).toJson(),reads==2?350:0}; };
        c.findChild<MihomoClient*>()->configure(server.serverPort(),"fixture"); auto& state=const_cast<AppState&>(c.state()); state.ready=true; state.core=CoreState::Running;
        MainWindow window(&c); window.setPage(4); window.show(); QTRY_COMPARE(c.connections().size(),1); QVERIFY(c.connectionsCurrent()); const auto observed=c.connectionTotals().value("_observed_at"); auto* table=window.findChild<QTableView*>("connectionsTable"); table->selectRow(0); auto* pause=window.findChild<QPushButton*>("pauseConnections"); QVERIFY(pause->isEnabled()); QTRY_COMPARE_WITH_TIMEOUT(reads,2,4000); pause->click(); QVERIFY(c.connectionsPaused()); QVERIFY(!c.connectionsCurrent()); QTest::qWait(450); QCOMPARE(c.connections()[0].toObject().value("metadata").toObject().value("host").toString(),QString("first.example")); QCOMPARE(c.connectionTotals().value("_observed_at"),observed); QCOMPARE(table->model()->index(0,6).data().toString(),QString("暂停快照")); QVERIFY(table->columnWidth(6)>=table->fontMetrics().horizontalAdvance("暂停快照")+12); QVERIFY(!window.findChild<QPushButton*>("closeSelectedConnection")->isEnabled()); QVERIFY(!window.findChild<QAction*>("closeAllConnections")->isEnabled()); QVERIFY(window.findChild<QLabel*>("connectionSamplingStatus")->text().contains("已暂停")); const auto geometry=table->geometry(); auto* search=window.findChild<QLineEdit*>("connectionsSearch"); window.findChild<QToolButton*>("connectionRegex")->click(); search->setText("["); QCoreApplication::processEvents(); QCOMPARE(table->geometry(),geometry); QVERIFY(window.findChild<QLabel*>("connectionSamplingStatus")->text().contains("搜索条件无效")); search->clear(); QVERIFY(window.findChild<QLabel*>("connectionSamplingStatus")->text().contains("已暂停"));
        const auto beforeClose=finished.size(); c.closeConnection(first); c.closeAllConnections(); QCOMPARE(finished.size(),beforeClose+2); QVERIFY(!finished[beforeClose][0].toBool()); QVERIFY(!finished[beforeClose+1][0].toBool()); QTest::qWait(2100); QCOMPARE(reads,2); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(window.grab().save(output+"/connections-paused.png")); }
        pause->click(); QVERIFY(!c.connectionsPaused()); QVERIFY(!c.connectionsCurrent()); QTRY_VERIFY(c.connectionsCurrent()); QCOMPARE(c.connections()[0].toObject().value("id").toString(),next); QVERIFY(c.closedConnections().isEmpty()); QVERIFY(c.connections()[0].toObject().value("_download_rate").isNull()); table->selectRow(0); QVERIFY(window.findChild<QPushButton*>("closeSelectedConnection")->isEnabled()); fail=true; const auto lastSuccess=c.connectionSampleTime(); QTRY_VERIFY_WITH_TIMEOUT(!c.connectionError().isEmpty(),4000);
        // 刷新失败保留上一份快照并标注过期，而不是清空列表（状态呈现规格「通用约束」）。
        QVERIFY(!c.connectionsCurrent());
        QVERIFY(c.connectionsStale());
        QCOMPARE(c.connections().size(),1);
        QCOMPARE(c.connectionSampleTime(),lastSuccess); QVERIFY(window.findChild<QLabel*>("connectionSamplingStatus")->text().contains("失败")); window.hide(); QVERIFY(c.connectionSampleTime().isEmpty()); QVERIFY(!c.connectionsVisible()); QVERIFY(!c.connectionsPaused()); QVERIFY(c.connections().isEmpty()); const int stopped=reads; QTest::qWait(2100); QCOMPARE(reads,stopped); for (const auto& request:server.requests) QVERIFY(!request.startsWith("DELETE")); QByteArray persisted; QVERIFY(readFile(root.path()+"/state.dat",persisted).ok); QVERIFY(!persisted.contains("first.example")); QVERIFY(!persisted.contains("_observed_at")); QCOMPARE(platform->writes,0);
    }
    void searchFlagsErrorsAndColumnPersistence() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1); ActivityTableModel model(true); QJsonArray rows; for (const auto& host:QStringList{"alpha.example","AlphaBeta.example","beta.example"}) rows.append(QJsonObject{{"id",newId()},{"metadata",QJsonObject{{"host",host},{"destinationPort",443},{"process","browser.exe"}}},{"upload",0},{"download",0}}); model.setRows(rows); QSortFilterProxyModel filter; filter.setSourceModel(&model); filter.setFilterKeyColumn(0); filter.setFilterRole(Qt::UserRole+2);
        QWidget host; host.resize(700,400); auto* table=new QTableView(&host); table->setModel(&filter); table->resize(680,240); auto* search=new QLineEdit(&host); auto* error=new QLabel(&host); ConnectionOptions options(&c,table,&filter,search,error,&host); options.show(); host.show(); UiTheme::setPreferences("light",120); QCoreApplication::processEvents(); QVERIFY(options.findChild<QToolButton*>("connectionCase")->isVisible()); QVERIFY(options.findChild<QToolButton*>("connectionWholeWord")->isVisible()); QVERIFY(options.findChild<QToolButton*>("connectionRegex")->isVisible()); auto* chooser=options.findChild<QWidget*>("connectionColumnsPopup"); QVERIFY(chooser); QCOMPARE(chooser->findChildren<QCheckBox*>().size(),int(ConnectionColumns::Count)); QVERIFY(table->columnWidth(6)>=table->fontMetrics().horizontalAdvance("暂停快照")+12); QVERIFY(!options.preferences().contains("columns")); UiTheme::setPreferences("system",100); QCoreApplication::processEvents(); search->setText("alpha"); QCOMPARE(filter.rowCount(),2); options.findChild<QToolButton*>("connectionCase")->click(); QCOMPARE(filter.rowCount(),1); options.findChild<QToolButton*>("connectionCase")->click(); options.findChild<QToolButton*>("connectionWholeWord")->click(); QCOMPARE(filter.rowCount(),1); options.findChild<QToolButton*>("connectionRegex")->click(); search->setText("^beta"); QCOMPARE(filter.rowCount(),1); search->setText("["); QVERIFY(error->isVisible()); QCOMPARE(filter.rowCount(),1); search->setText("(*ACCEPT)"); QVERIFY(error->isVisible()); QCOMPARE(filter.rowCount(),1); search->setText("alpha"); QVERIFY(!error->isVisible()); QCOMPARE(filter.rowCount(),1);
        auto* columnsButton=options.findChild<QToolButton*>("connectionColumns"); QVERIFY(columnsButton); columnsButton->click();
        QTRY_VERIFY(chooser->isVisible()); QVERIFY(chooser->height()<=480);
        QCOMPARE(chooser->grab().toImage().pixelColor(0,0).alpha(),0);
        const auto position=chooser->geometry(); QTest::qWait(60); QCOMPARE(chooser->geometry(),position);
        if (const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); !output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(chooser->grab().save(output+"/connection-columns.png")); }
        auto* column7=options.findChild<QCheckBox*>("connectionColumn7"); auto* column8=options.findChild<QCheckBox*>("connectionColumn8"); QVERIFY(column7 && column8);
        column7->setChecked(true); column8->setChecked(true); QVERIFY(chooser->isVisible()); QVERIFY(!table->isColumnHidden(7) && !table->isColumnHidden(8));
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QCoreApplication::sendEvent(column7,&escape); QVERIFY(!chooser->isVisible());
        columnsButton->click(); QTRY_VERIFY(chooser->isVisible()); QVERIFY(column7->isChecked() && column8->isChecked());
        host.move(host.pos()+QPoint(5,0)); QTRY_VERIFY(!chooser->isVisible());
        auto* header=table->horizontalHeader(); header->moveSection(header->visualIndex(3),1); table->setColumnWidth(3,230); options.findChild<QCheckBox*>("connectionColumn4")->setChecked(false); QVERIFY(table->isColumnHidden(4)); QVERIFY(!options.findChild<QCheckBox*>("connectionColumn0")->isEnabled()); const auto desired=options.preferences(); QVERIFY(validateConnectionView(desired).ok); const auto generation=c.state().generation; options.flushPreferences(); QTRY_COMPARE(finished.size(),1); QVERIFY(finished[0][0].toBool()); QCOMPARE(c.settings().connectionView,desired); QCOMPARE(c.state().generation,generation); QByteArray bytes; QVERIFY(readFile(root.path()+"/state.dat",bytes).ok); QVERIFY(!bytes.contains("alpha.example"));
        auto* restoredTable=new QTableView(&host); restoredTable->setModel(&filter); auto* restoredSearch=new QLineEdit(&host); auto* restoredError=new QLabel(&host); ConnectionOptions restored(&c,restoredTable,&filter,restoredSearch,restoredError,&host); QCOMPARE(restoredTable->horizontalHeader()->logicalIndex(1),3); QCOMPARE(restoredTable->columnWidth(3),230); QVERIFY(restoredTable->isColumnHidden(4)); QVERIFY(restored.findChild<QToolButton*>("connectionRegex")->isChecked()); QVERIFY(restoredSearch->text().isEmpty()); restored.findChild<QPushButton*>("resetConnectionColumns")->click(); QVERIFY(!restored.preferences().contains("columns")); restored.flushPreferences(); QTRY_COMPARE(finished.size(),2); QVERIFY(finished[1][0].toBool()); QVERIFY(!c.settings().connectionView.contains("columns")); QCOMPARE(platform->writes,0);
    }
    void queuedSaveFailureAndSchemaBounds() {
        QTemporaryDir root; auto platform=std::make_shared<ViewPlatform>(); AppController c(root.path(),true,nullptr,platform); QSignalSpy initialized(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(initialized.size(),1); const QJsonObject preferences{{"regex",true},{"whole_word",false},{"case_sensitive",true}}; c.updateWebsiteTargets(QJsonArray{QJsonObject{{"id","fixture"},{"name","Fixture"},{"url","https://example.test/"},{"mark","F"}}}); QVERIFY(c.state().busy); c.updateConnectionView(preferences); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,5000); QVERIFY(finished[0][0].toBool()); QVERIFY(finished[1][0].toBool()); QCOMPARE(c.settings().connectionView,preferences); platform->failSeal=true; auto next=preferences; next["regex"]=false; QByteArray before,after; QVERIFY(readFile(root.path()+"/state.dat",before).ok); c.updateConnectionView(next); QTRY_COMPARE(finished.size(),3); QVERIFY(!finished[2][0].toBool()); QCOMPARE(c.settings().connectionView,preferences); QVERIFY(readFile(root.path()+"/state.dat",after).ok); QCOMPARE(after,before);
        QVERIFY(!validateConnectionView({{"regex","yes"}}).ok); QVERIFY(!validateConnectionView({{"search","private host"}}).ok); QVERIFY(!validateConnectionView({{"columns",QJsonArray{}}}).ok); QJsonArray columns; for (int id=0;id<7;++id) columns.append(QJsonObject{{"id",id},{"width",100},{"visible",true}}); QVERIFY(validateConnectionView({{"columns",columns}}).ok); auto bad=columns[0].toObject(); bad["visible"]=false; columns[0]=bad; QVERIFY(!validateConnectionView({{"columns",columns}}).ok); bad["visible"]=true; bad["width"]=1001; columns[0]=bad; QVERIFY(!validateConnectionView({{"columns",columns}}).ok); QCOMPARE(platform->writes,0);
    }
    void logAndRuleRegionsDistinguishStates() {
        QTemporaryDir root; AppController c(root.path(),true,nullptr,std::make_shared<ViewPlatform>()); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1);
        MainWindow window(&c); window.resize(940,620); window.show(); QCoreApplication::processEvents();
        // 日志页：已有应用日志时正文优先；真正无记录时才展示空态。
        window.setPage(5); QCoreApplication::processEvents();
        auto* logEmpty=window.findChild<QLabel*>("logEmptyState"); QVERIFY(logEmpty);
        auto* logView=window.findChild<QPlainTextEdit*>("logView"); QVERIFY(logView);
        QVERIFY((logEmpty->isVisible() && !logEmpty->text().isEmpty()) || !logView->toPlainText().isEmpty());
        // 无配置时规则页只提供导入路径，不展示不可用的规则编辑器。
        window.setPage(3); QCoreApplication::processEvents();
        auto* rulesNoProfile=window.findChild<QWidget*>("rulesNoProfile"); QVERIFY(rulesNoProfile); QVERIFY(rulesNoProfile->isVisible());
        auto* rulesTabs=window.findChild<QTabWidget*>("rulesTabs"); QVERIFY(rulesTabs); QVERIFY(!rulesTabs->isVisible());
        // 连接页：没有配置时的唯一下一步直达配置。
        window.setPage(4); QCoreApplication::processEvents();
        auto* connectionsEmpty=window.findChild<QTableView*>("connectionsTable"); QVERIFY(connectionsEmpty);
        auto* emptyLabel=connectionsEmpty->viewport()->findChild<QLabel*>("emptyState"); QVERIFY(emptyLabel);
        QTRY_VERIFY(emptyLabel->isVisible()); QVERIFY(emptyLabel->text().contains("选择配置"));
        QSignalSpy exited(&c,&AppController::exitReady); c.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
};
QTEST_MAIN(ConnectionViewTests)
#include "ConnectionViewTests.moc"
