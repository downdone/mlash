#include "app/AppController.h"
#include "ui/MainWindow.h"
#include "ui/NodeRowDelegate.h"
#include "ui/NodeRowModel.h"
#include "ui/NodeRowView.h"
#include "ui/ProfileRowDelegate.h"
#include "ui/ProfileCardWidget.h"
#include "ui/TaskWorkspace.h"
#include "ui/TaskDialog.h"
#include "ui/YamlFormEditor.h"
#include "ui/DialogDraftGuard.h"
#include "ui/ViewIds.h"
#include "ui/ImportDialog.h"
#include "ui/CoreUpdateDialog.h"
#include "ui/ProxyBypassDialog.h"
#include "ui/BackupDialog.h"
#include "ui/OutboundDialog.h"
#include "app/OutboundDiagnostics.h"
#include "ui/UiTheme.h"
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <QToolButton>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QTabBar>
#include <QTabWidget>
#include <QCheckBox>
#include <QMessageBox>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardItemModel>
#include "platform/GlobalHotkeys.h"
#include "profile/WebDavClient.h"
#include "profile/SubscriptionDownloader.h"
using namespace mlash;
class WorkspacePlatform final:public PlatformServices {
public:
    Result readProxy() override { return {true,{},{{"flags",1},{"server",""},{"bypass",""},{"pac",""}}}; }
    Result writeProxy(const QJsonObject&) override { return Result::fail("prohibited"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool,const QString&) override { return Result::fail("prohibited"); }
    Result readAutoStart(const QString&) override { return {true,{},{{"enabled",false}}}; }
    Result seal(const QByteArray& bytes,QByteArray& out) override { out=bytes; return {}; }
    Result open(const QByteArray& bytes,QByteArray& out) override { out=bytes; return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path)?Result{}:Result::fail("mkdir"); }
    Result attachChild(qint64) override { return Result::fail("prohibited"); }
    Result ownsTcpPort(qint64,int) override { return Result::fail("prohibited"); }
};
class FlatWorkspaceTests final:public QObject {
    Q_OBJECT
    static void answer(QMessageBox::StandardButton button) { QTimer::singleShot(0,[button] { if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(button)->click(); }); }
    static void snapshot(QWidget* widget,const QString& name) { const auto out=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!out.isEmpty()) { QDir().mkpath(out); QVERIFY(widget->grab().save(out+"/u12-"+name+".png")); } }
private slots:
    void cleanup() { UiTheme::setPreferences("system",100); }
    void diagnosticWorkspaceDestructionClearsSnapshots() {
        QTemporaryDir root; auto platform=std::make_shared<WorkspacePlatform>(); AppController controller(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto* window=new MainWindow(&controller); window->show(); window->setPage(7); window->findChild<QPushButton*>("websiteDiagnostics")->click();
        QPointer<OutboundDialog> diagnostic=window->findChild<OutboundDialog*>(); QVERIFY(diagnostic);
        auto* service=diagnostic->findChild<QTabWidget*>("outboundTabs"); service->setCurrentIndex(1);
        auto* state=const_cast<OutboundDiagnostics*>(controller.outboundDiagnostics());
        const_cast<QJsonObject&>(state->result())={{"kind","services"},{"route","direct"},{"services",QJsonArray{QJsonObject{{"id","youtube"},{"name","YouTube"},{"state","unknown"},{"http_status",403},{"target","https://example.test/check"}}}}}; state->changed();
        diagnostic->findChild<QTableWidget*>("serviceDiagnosticsTable")->selectRow(0); diagnostic->findChild<QPushButton*>("viewServiceEvidence")->click();
        QPointer<QDialog> evidence=window->findChild<QDialog*>("serviceEvidenceSnapshot"); QVERIFY(evidence&&evidence->isVisible()&&!evidence->isWindow());
        delete window; QVERIFY(diagnostic.isNull()); QVERIFY(evidence.isNull()); QVERIFY(state->result().isEmpty()); QVERIFY(!state->busy());
    }
    void compactRowsKeepSeparateMouseTargets() {
        for (int scale:{100,120}) {
            UiTheme::setPreferences("dark",scale);
            QStandardItemModel model(1,1); const auto index=model.index(0,0);
            model.setData(index,"日本 · 示例线路"); model.setData(index,1,NodeRowModel::RowKind);
            NodeRowView view; view.setModel(&model); view.setItemDelegate(new NodeRowDelegate(&view)); view.setTestEnabled(true); view.resize(500,120); view.show(); QCoreApplication::processEvents();
            QSignalSpy selections(&view,&QListView::clicked); int tests=0,details=0;
            view.delayClicked=[&](const QModelIndex&) { ++tests; }; view.infoClicked=[&](const QModelIndex&) { ++details; };
            const auto row=view.visualRect(index);
            QTest::mouseClick(view.viewport(),Qt::LeftButton,{},NodeRowDelegate::delayRect(row).center()); QCOMPARE(tests,1); QCOMPARE(details,0); QCOMPARE(selections.size(),0);
            QTest::mouseClick(view.viewport(),Qt::LeftButton,{},NodeRowDelegate::infoRect(row).center()); QCOMPARE(tests,1); QCOMPARE(details,1); QCOMPARE(selections.size(),0);
            QTest::mouseClick(view.viewport(),Qt::LeftButton,{},QPoint(row.left()+70,row.center().y())); QCOMPARE(selections.size(),1); QCOMPARE(tests,1); QCOMPARE(details,1);
        }
    }
    void logHistoryKeepsDirectReturnAction() {
        QTemporaryDir root; auto platform=std::make_shared<WorkspacePlatform>(); AppController controller(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(5);
        auto* logs=window.findChild<QPlainTextEdit*>("logView"); auto* follow=window.findChild<QPushButton*>("followLatestLogs");
        QVERIFY(!follow->isVisible()); for (int i=0;i<160;++i) controller.diagnostics()->append("info",QString("record-%1").arg(i));
        auto* scroll=logs->verticalScrollBar(); QTRY_VERIFY(scroll->maximum()>100); scroll->setValue(20);
        QVERIFY(follow->isVisible()); const auto position=scroll->value(); controller.diagnostics()->append("info","new-while-reading"); QCOMPARE(scroll->value(),position);
        QTest::mouseClick(follow,Qt::LeftButton); QCOMPARE(scroll->value(),scroll->maximum()); QVERIFY(!follow->isVisible());
    }
    void prefilledImportsAndInlineEvidenceDoNotAddNavigation() {
        QTemporaryDir root; auto platform=std::make_shared<WorkspacePlatform>(); AppController controller(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(1);
        auto* import=new ImportDialog(&window); import->setPrefill("https://example.test/sub",true);
        window.presentTask(import,"import"); QVERIFY(!DialogDraftGuard::hasDrafts(import));
        import->findChild<QLineEdit*>("subscriptionName")->setText("我的名称");
        QVERIFY(DialogDraftGuard::hasDrafts(import)); answer(QMessageBox::No); import->reject(); QVERIFY(import->isVisible());
        answer(QMessageBox::Yes); import->reject(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        window.setPage(7);
        QVERIFY(!window.findChild<QToolButton*>("websiteResultDetails")->isVisible());
        const_cast<AppState&>(controller.state()).activeProfile="layout-fixture";
        emit controller.stateChanged();
        auto* info=window.findChild<QToolButton*>("websiteResultDetails"); QVERIFY(info&&info->isVisible()); info->click();
        QPointer<QDialog> evidence=window.findChild<QDialog*>("websiteResultSnapshot"); QVERIFY(evidence&&evidence->isVisible()); QVERIFY(!evidence->isWindow());
        auto* body=evidence->findChild<QPlainTextEdit*>("websiteResultEvidence"); QVERIFY(body->isReadOnly()); QVERIFY(body->toPlainText().contains("尚未开始"));
        QCOMPARE(window.page(),7); window.setPage(0); QTRY_VERIFY(evidence.isNull());
    }
    void embeddedTasksFitNarrowWindowsAndTextScales() {
        QTemporaryDir root; auto platform=std::make_shared<WorkspacePlatform>(); AppController controller(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(6); window.findChild<QPushButton*>("settingsCoreRow")->click();
        auto* update=window.findChild<QPushButton*>("manageCoreUpdate"); QVERIFY(update&&update->isVisible()); update->click();
        for (const auto& theme:{QString("light"),QString("dark")}) for (int scale:{100,120}) {
            UiTheme::setPreferences(theme,scale);
            for (auto size:{QSize(840,560),QSize(1040,720),QSize(1440,900)}) {
                window.resize(size); QTest::qWait(20);
                TaskWorkspace* workspace=nullptr; for (auto* candidate:window.findChildren<TaskWorkspace*>()) if (candidate->isVisible()) workspace=candidate;
                QVERIFY(workspace); auto* task=window.findChild<CoreUpdateDialog*>(); QVERIFY(task&&task->isVisible()&&!task->isWindow());
                for (auto* scroll:workspace->findChildren<QScrollArea*>()) if (scroll->isVisible()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
                QVERIFY(task->width()<=workspace->width()); QVERIFY(workspace->findChild<QPushButton*>("taskBack")->isVisible());
                snapshot(&window,QString("core-%1-%2-%3").arg(theme).arg(scale).arg(size.width()));
            }
        }
    }
    void nestedTaskAndReplacementKeepSubmissionBoundaries() {
        TaskWorkspace workspace; workspace.configure("one","示例任务",{},[] {}); workspace.show();
        auto* parent=new TaskDialog; parent->setObjectName("parent"); auto* parentLayout=new QVBoxLayout(parent); parentLayout->addWidget(new QLabel("父草稿")); workspace.mount(parent,"parent","父任务");
        bool pending=true,dirty=true;
        auto* child=new TaskDialog(parent); child->setObjectName("child"); auto* childLayout=new QVBoxLayout(child); childLayout->addWidget(new QLabel("子草稿"));
        DialogDraftGuard::protect(child,[&] { return dirty; },[&] { return pending; }); workspace.mount(child,"child","子任务");
        QVERIFY(!parent->isWindow()); QVERIFY(!child->isWindow()); QVERIFY(!workspace.configure("two","另一个对象",{},[] {}));
        parent->reject(); QVERIFY(workspace.editor("parent")); QVERIFY(workspace.editor("child"));
        pending=false; answer(QMessageBox::No); parent->reject(); QVERIFY(workspace.editor("parent")); QVERIFY(workspace.editor("child"));
        dirty=false; parent->reject(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QVERIFY(!workspace.editor("parent")); QVERIFY(!workspace.editor("child"));
        QVERIFY(workspace.configure("two","另一个对象",{},[] {}));
    }
    void sameProfileRefreshKeepsOnlyRealTaskTabs() {
        TaskWorkspace workspace; workspace.show();
        const auto action=[] {};
        QVERIFY(workspace.configure("profile:p","示例",{{"overview","概览",action},{"rules","规则",action}},action));
        auto* tabs=workspace.findChild<QTabBar*>("taskTabs"); QVERIFY(tabs); QCOMPARE(tabs->count(),2); QVERIFY(tabs->isHidden());
        QVERIFY(workspace.configure("profile:p","示例",{{"rules","规则与分流",action},{"dns","DNS",action}},action));
        QCOMPARE(tabs->count(),2); QCOMPARE(tabs->tabText(0),QString("规则与分流")); QCOMPARE(tabs->tabText(1),QString("DNS"));
        QVERIFY(workspace.findChild<QPushButton*>("taskBack"));
    }
    void singleSessionFailureCancelAndHiddenDrafts() {
        QTemporaryDir root; auto platform=std::make_shared<WorkspacePlatform>(); AppController controller(root.path(),true,nullptr,platform);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(6);
        window.findChild<QPushButton*>("settingsCoreRow")->click();
        auto* update=window.findChild<QPushButton*>("manageCoreUpdate"); QVERIFY(update&&update->isVisible()); update->click();
        auto* core=window.findChild<CoreUpdateDialog*>(); QVERIFY(core&&core->isVisible()); QVERIFY(!core->isWindow());
        auto* workspace=window.findChild<TaskWorkspace*>(); QVERIFY(workspace); snapshot(&window,"core");
        auto* path=core->findChild<QLineEdit*>("coreUpdatePath"); path->setText("not-a-real-core");
        for (auto* back:window.findChildren<QPushButton*>("taskBack")) if (back->isVisible()) { back->click(); break; } update->click();
        QCOMPARE(window.findChildren<CoreUpdateDialog*>().size(),1); QCOMPARE(path->text(),QString("not-a-real-core"));
        for (auto* back:window.findChildren<QPushButton*>("taskBack")) if (back->isVisible()) { back->click(); break; } QVERIFY(DialogDraftGuard::hasDrafts(&window));
        answer(QMessageBox::No); QVERIFY(!window.confirmDiscardEdits());
        window.findChild<QPushButton*>("settingsBypassRow")->click(); auto* bypass=window.findChild<ProxyBypassDialog*>(); QVERIFY(bypass&&!bypass->isWindow());
        auto* editor=bypass->findChild<QPlainTextEdit*>("proxyBypassEditor"); editor->setPlainText(QString(5000,'x'));
        auto* save=bypass->findChild<QPushButton*>("saveProxyBypass"); QVERIFY(save->isEnabled()); save->click();
        QTRY_VERIFY(!bypass->findChild<QLabel*>("proxyBypassHint")->text().contains("系统代理已开启"));
        QCOMPARE(editor->toPlainText(),QString(5000,'x')); QVERIFY(bypass->isVisible());
        answer(QMessageBox::No); bypass->reject(); QVERIFY(bypass->isVisible()); QCOMPARE(editor->toPlainText(),QString(5000,'x'));
        answer(QMessageBox::Yes); bypass->reject(); QTRY_VERIFY(!window.findChild<ProxyBypassDialog*>());
        window.setPage(7);
        auto* nav=window.findChild<QListWidget*>("navigation");
        QVERIFY(!nav->selectedItems().isEmpty());
        const_cast<AppState&>(controller.state()).autoStartConflict=true;
        emit controller.stateChanged();
        window.findChild<QToolButton*>("sidebarSettings")->click();
        auto* search=window.findChild<QLineEdit*>("settingsSearch");
        auto* startupHint=window.findChild<QLabel*>("startupLocationHint");
        QVERIFY(search->isVisible());
        QVERIFY(startupHint->isVisible());
        QVERIFY(nav->selectedItems().isEmpty());
        window.findChild<QPushButton*>("settingsToolsRow")->click();
        QVERIFY(!search->isVisible());
        QVERIFY(!startupHint->isVisible());
        auto settingRow=[&window](const QString& name) -> QWidget* {
            for (auto* row:window.findChildren<QWidget*>("settingRow"))
                if (row->property("settingSearchName")==name) return row;
            return nullptr;
        };
        QVERIFY(settingRow("日志目录")->isVisible());
        QVERIFY(!settingRow("代理环境变量")->isVisible());
        QVERIFY(!settingRow("链接导入")->isVisible());
        auto* back=window.findChild<QPushButton*>("settingsBack");
        auto* topbar=window.findChild<QLabel*>("topbarTitle");
        QTRY_VERIFY(back->mapTo(&window,QPoint()).y()-topbar->mapTo(&window,QPoint(0,topbar->height())).y()<28);
        snapshot(&window,"settings-tools");
        back->click();
        QVERIFY(search->isVisible());
        search->setText("代理环境变量");
        QTest::keyClick(search,Qt::Key_Return);
        QVERIFY(settingRow("代理环境变量")->isVisible());
        QVERIFY(!settingRow("日志目录")->isVisible());
        back->click();
        window.findChild<QPushButton*>("settingsBackupRow")->click();
        auto backups=window.findChildren<BackupDialog*>(); QCOMPARE(backups.size(),1); QVERIFY(!backups.first()->isWindow());
        workspace=nullptr; for (auto* candidate:window.findChildren<TaskWorkspace*>()) if (candidate->isVisible()) workspace=candidate; QVERIFY(workspace);
        auto* tabs=workspace->findChild<QTabBar*>("taskTabs"); int restore=-1; for (int i=0;i<tabs->count();++i) if (tabs->tabText(i)=="恢复备份") restore=i; QVERIFY(restore>=0);
        auto* password=backups.first()->findChild<QLineEdit*>("backupPassword"); QVERIFY(password); password->setText("local-test-password");
        auto* heading=workspace->findChild<QLabel*>("taskTitle");
        QCOMPARE(heading->text(),QString("备份与恢复"));
        auto* form=backups.first()->findChild<QWidget*>("backupForm");
        QVERIFY(form && form->isVisible());
        QVERIFY(form->width()<=640);
        auto* pathField=form->findChild<QLineEdit*>("backupPath");
        auto* include=form->findChild<QCheckBox*>("backupIncludeProfiles");
        auto* backupScroll=backups.first()->findChild<QScrollArea*>("backupScroll");
        QTRY_VERIFY(backupScroll->viewport()->rect().contains(QRect(pathField->mapTo(backupScroll->viewport(),QPoint()),pathField->size())));
        QVERIFY(backupScroll->viewport()->rect().contains(QRect(include->mapTo(backupScroll->viewport(),QPoint()),include->size())));
        QVERIFY(password->y()-pathField->mapTo(password->parentWidget(),QPoint()).y()<70);
        QVERIFY(include->mapTo(form,QPoint()).y()<210);
        password->clear();
        snapshot(&window,"backup-create");
        password->setText("local-test-password");
        tabs->setCurrentIndex(restore); QCOMPARE(window.findChildren<BackupDialog*>().size(),2); QVERIFY(DialogDraftGuard::hasDrafts(workspace));
        QCOMPARE(heading->text(),QString("备份与恢复"));
        auto* restoreForm=workspace->editor("backup:restore");
        QVERIFY(restoreForm->findChild<QListWidget*>("backupSelection")->isHidden());
        QVERIFY(restoreForm->findChild<QPushButton*>("previewBackup")->isVisible());
        QVERIFY(!restoreForm->findChild<QPushButton*>("submitBackup")->isEnabled());
        snapshot(&window,"backup");
        workspace->findChild<QPushButton*>("taskBack")->click();
        search->setText("恢复与迁移");
        QTest::keyClick(search,Qt::Key_Return);
        QCOMPARE(tabs->currentIndex(),restore);
        QVERIFY(workspace->isVisible());
        // The isolated window is disposed without issuing an OS exit or real write.
    }
    void profileContentAndInspectorKeepShellGeometry() {
        QTemporaryDir root; auto platform=std::make_shared<WorkspacePlatform>(); ProfileStore store(root.path(),platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore=false; Profile profile; profile.id=newId(); profile.name="本地示例"; profile.sourceType="file";
        auto prepared=store.prepare(profile,"proxies: []\nrules: ['MATCH,DIRECT']\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); profile=prepared.profile; profile.active=profile.candidate; settings.activeProfile=profile.id; QVERIFY(store.save(settings,{profile}).ok);
        AppController controller(root.path(),true,nullptr,platform); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true; state.core=CoreState::Running; state.proxy=ProxyState::Owned; state.mode="rule";
        const_cast<Settings&>(controller.settings()).appearance="dark";
        QJsonArray members{"JP 日本","香港 · 日常线路","新加坡 · 工作","美国 · 备用","德国 · 线路 05","英国 · 线路 06"};
        for (int i=7;i<=30;++i) members.append(QString("日本 · 备用线路 %1").arg(i,2,10,QChar('0')));
        auto& proxies=const_cast<QJsonObject&>(controller.proxies());
        proxies={{"示例组",QJsonObject{{"type","Selector"},{"all",members},{"now","JP 日本"}}}};
        for (const auto& member:members) proxies[member.toString()]=QJsonObject{{"type","Shadowsocks"}};
        MainWindow window(&controller); window.show();
        QVERIFY(!window.findChild<QLineEdit*>("nodeSearch")->isVisible());
        auto* groupPicker=window.findChild<QToolButton*>("browseGroup");
        QCOMPARE(groupPicker->toolButtonStyle(),Qt::ToolButtonTextBesideIcon);
        QVERIFY(groupPicker->text().contains(QStringLiteral("示例组")));
        QTest::qWait(40); snapshot(&window,"home");
        auto* densityNodes=window.findChild<QListView*>("nodeList");
        const auto first=densityNodes->visualRect(densityNodes->model()->index(1,0));
        const auto header=densityNodes->visualRect(densityNodes->model()->index(0,0));
        qInfo()<<"Home density:"<<"control"<<window.findChild<QWidget*>("homeControlCard")->height()<<"headerY"<<densityNodes->viewport()->mapTo(&window,header.topLeft()).y()<<"nodeHeight"<<first.height()<<"viewport"<<densityNodes->viewport()->height()<<"visibleMembers"<<qMax(0,(densityNodes->viewport()->height()-header.height())/first.height());
        QVERIFY(window.findChild<QWidget*>("homeControlCard")->height()<=124);
        QVERIFY(first.height()<=40); QVERIFY(densityNodes->viewport()->mapTo(&window,header.topLeft()).y()<=220);
        QVERIFY((densityNodes->viewport()->height()-header.height())/first.height()>=10);
        for (int scale:{100,120}) {
            UiTheme::setPreferences("dark",scale);
            for (const auto size:{QSize(840,560),QSize(1040,720),QSize(1440,900)}) {
                window.resize(size); QTest::qWait(30); densityNodes->scrollToTop();
                const auto row=densityNodes->visualRect(densityNodes->model()->index(1,0));
                QVERIFY(row.height()<= (scale==100?40:50));
                QVERIFY(!NodeRowDelegate::delayRect(row).intersects(NodeRowDelegate::infoRect(row)));
                QCOMPARE(densityNodes->horizontalScrollBar()->maximum(),0);
                snapshot(&window,QString("density-%1-%2").arg(scale).arg(size.width()));
            }
        }
        UiTheme::setPreferences("dark",100); window.resize(1040,720); QTest::qWait(30);
        QJsonArray shortMembers; for (int i=0;i<6;++i) shortMembers.append(members[i]);
        proxies["示例组"]=QJsonObject{{"type","Selector"},{"all",shortMembers},{"now","JP 日本"}}; controller.proxiesChanged(); QTest::qWait(30);
        QVERIFY(densityNodes->viewport()->mapTo(&window,QPoint()).y()<=220); snapshot(&window,"home-short");
        proxies["示例组"]=QJsonObject{{"type","URLTest"},{"all",QJsonArray{members[0],members[1]}},{"now","JP 日本"}}; controller.proxiesChanged();
        UiTheme::setPreferences("light",120); window.resize(840,560); window.setMemberView(1);
        auto* searchToggle=window.findChild<QToolButton*>("nodeSearchToggle"); searchToggle->click(); QTest::qWait(30);
        auto* cards=window.findChild<QListView*>("nodeCards"); QVERIFY(cards->viewport()->height()>=cards->gridSize().height());
        for (int i=0;i<cards->model()->rowCount();++i) {
            const auto card=cards->visualRect(cards->model()->index(i,0));
            QVERIFY2(cards->viewport()->rect().contains(card),qPrintable(QString("row %1 rect %2,%3 %4x%5 viewport %6x%7 grid %8x%9 scroll %10,%11")
                .arg(i).arg(card.x()).arg(card.y()).arg(card.width()).arg(card.height())
                .arg(cards->viewport()->width()).arg(cards->viewport()->height())
                .arg(cards->gridSize().width()).arg(cards->gridSize().height())
                .arg(cards->horizontalScrollBar()->value()).arg(cards->verticalScrollBar()->value())));
        }
        snapshot(&window,"compact-card-search"); searchToggle->click(); window.setMemberView(0); UiTheme::setPreferences("dark",100); window.resize(1040,720);
        proxies["示例组"]=QJsonObject{{"type","Selector"},{"all",members},{"now","JP 日本"}}; controller.proxiesChanged();
        window.setPage(1);
        auto* list=window.findChild<QListView*>("profileList"); QVERIFY(list);
        const auto index=list->model()->index(0,0); QVERIFY(index.isValid());
        QVERIFY(!list->findChild<QPushButton*>("emptyStateAction")->isVisible()); snapshot(&window,"profiles");
        QTest::mouseClick(qobject_cast<ProfileCardWidget*>(list->indexWidget(index))->button(ProfileCardWidget::Action::Menu),Qt::LeftButton);
        auto* profileMenu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(profileMenu);
        profileMenu->hide(); profileMenu->actions().first()->trigger();
        auto* profileWorkspace=window.findChild<TaskWorkspace*>("taskWorkspace"); QVERIFY(profileWorkspace);
        QTRY_VERIFY(profileWorkspace->isVisible()); QVERIFY(profileWorkspace->context().endsWith(":content"));
        auto* profileTabs=profileWorkspace->findChild<QTabBar*>("taskTabs"); QVERIFY(profileTabs&&profileTabs->isVisible());
        QCOMPARE(profileTabs->tabText(0),QString("节点与组"));
        profileTabs->setCurrentIndex(2); auto* dns=window.findChild<QDialog*>("dnsDialog"); QVERIFY(dns&&!dns->isWindow());
        auto* dnsFields=dns->findChild<YamlFormEditor*>(); QVERIFY(dnsFields); QVERIFY(dnsFields->isHidden());
        auto* dnsToggle=dns->findChild<QCheckBox*>("dnsOverrideEnabled"); QVERIFY(dnsToggle);
        dnsToggle->setChecked(true); QVERIFY(dnsFields->isVisible());
        auto* dnsEditor=dns->findChild<QPlainTextEdit*>("dnsOverrideEditor"); QVERIFY(dnsEditor);
        auto* dnsMode=dns->findChild<QPushButton*>("dnsEditMode"); QVERIFY(dnsMode&&dnsMode->isVisible());
        dnsMode->click(); QVERIFY(dnsEditor->isVisible()); QCOMPARE(dnsMode->text(),QString("常用字段"));
        dnsMode->click(); QVERIFY(!dnsEditor->isVisible());
        dnsEditor->setPlainText("enable: true\n"); snapshot(&window,"profile-dns-editing");
        profileTabs->setCurrentIndex(1); profileTabs->setCurrentIndex(2);
        QCOMPARE(dnsEditor->toPlainText(),QString("enable: true\n"));
        window.setPage(0); QVERIFY(!profileWorkspace->isVisible());
        window.setPage(1); QVERIFY(profileWorkspace->isVisible());
        QCOMPARE(dnsEditor->toPlainText(),QString("enable: true\n"));
        window.resize(840,560); QCoreApplication::processEvents();
        QCOMPARE(window.findChild<QWidget*>("sidebar")->width(),144);
        for (auto* parent=dns->parentWidget();parent;parent=parent->parentWidget())
            QVERIFY(parent->objectName()!="taskScroll");
        snapshot(&window,"profile-dns-compact");
        dnsEditor->clear(); dnsToggle->setChecked(false);
        QTimer::singleShot(0,[] { if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); });
        profileWorkspace->findChild<QPushButton*>("taskBack")->click();
        QTRY_VERIFY(list->isVisible());
        window.setPage(0);
        QVERIFY(!profileWorkspace->isVisible());
        auto* nodes=window.findChild<QListView*>("nodeList"); QVERIFY(nodes->model()->rowCount()>1);
        for (const auto size:{QSize(840,560),QSize(1040,720),QSize(1440,900)}) {
            window.resize(size); QTest::qWait(40); const auto item=nodes->model()->index(1,0); nodes->scrollTo(item); QCoreApplication::processEvents();
            QTest::mouseClick(nodes->viewport(),Qt::LeftButton,{},NodeRowDelegate::infoRect(nodes->visualRect(item)).center()); QTest::qWait(60);
            auto* top=window.findChild<QWidget*>("topbar"); auto* status=window.findChild<QWidget*>("operationStatusBar"); auto* host=window.findChild<QWidget*>("contentSurface");
            QCOMPARE(top->mapTo(host,QPoint()).y(),0); QCOMPARE(status->mapTo(host,QPoint()).y()+status->height(),host->height());
            snapshot(&window,QString("inspector-%1").arg(size.width())); window.findChild<QToolButton*>("drawerClose")->click();
        }
        auto* toggle=window.findChild<QToolButton*>("nodeSearchToggle"); toggle->click();
        auto* search=window.findChild<QLineEdit*>("nodeSearch"); QVERIFY(search->isVisible()); search->setText("no-result"); QCoreApplication::processEvents();
        window.findChild<QAction*>("locateCurrent")->trigger(); QCOMPARE(search->text(),QString());
        auto* restore=window.findChild<QPushButton*>("restoreNodeFilters"); QVERIFY(restore->isVisible()); restore->click(); QCOMPARE(search->text(),QString("no-result"));
        window.findChild<QAction*>("nodePreferences")->trigger(); QPointer<QDialog> groupEditor=window.findChild<QDialog*>("nodePreferencesDialog"); QVERIFY(groupEditor&&groupEditor->isVisible()&&groupEditor->isWindow());
        snapshot(&window,"group-settings-background");
        if (const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); !output.isEmpty()) QVERIFY(groupEditor->grab().save(output+"/group-settings-window.png"));
        auto* groupUrl=groupEditor->findChild<QLineEdit*>("groupTestUrl"); groupUrl->setText("https://example.test/check");
        proxies["另一组"]=QJsonObject{{"type","Selector"},{"all",members},{"now","JP 日本"}}; controller.proxiesChanged();
        auto* menu=window.findChild<QMenu*>("nodeGroupMenu"); QVERIFY(menu);
        for (auto* action:menu->actions()) if (action->text()=="另一组") { action->trigger(); break; }
        QVERIFY(!groupEditor->isVisible()); QVERIFY(DialogDraftGuard::hasDrafts(&window));
        answer(QMessageBox::No); window.findChild<QAction*>("nodePreferences")->trigger(); QVERIFY(groupEditor); QVERIFY(!groupEditor->isVisible());
        for (auto* action:menu->actions()) if (action->text()=="示例组") { action->trigger(); break; }
        QVERIFY(groupEditor->isVisible()); QCOMPARE(groupUrl->text(),QString("https://example.test/check"));
        window.setPage(1); QVERIFY(!groupEditor->isVisible()); QVERIFY(DialogDraftGuard::hasDrafts(&window));
        window.setPage(0); QVERIFY(groupEditor->isVisible()); QCOMPARE(groupUrl->text(),QString("https://example.test/check"));
        answer(QMessageBox::Yes); groupEditor->reject(); QTRY_VERIFY(groupEditor.isNull());
    }
};
QTEST_MAIN(FlatWorkspaceTests)
#include "FlatWorkspaceTests.moc"
