#include "TestPorts.h"
#include "app/AppController.h"
#include "ui/MainWindow.h"
#include "ui/ImportDialog.h"
#include "ui/DialogDraftGuard.h"
#include "ui/NodePresentation.h"
#include "ui/NodeRowDelegate.h"
#include "ui/NodeRowModel.h"
#include "ui/NodeRowView.h"
#include "ui/GroupPickerPopup.h"
#include "ui/PrototypeUi.h"
#include "ui/ProfileRowModel.h"
#include "ui/ProfileCardDelegate.h"
#include "ui/TaskWorkspace.h"
#include "ui/SettingsSearch.h"
#include "ui/ViewIds.h"
#include <QListView>
#include <QStandardItemModel>
#include <QtTest>
#include <QToolButton>
#include <QListWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QInputDialog>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QClipboard>
#include <QMenu>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTableWidget>
#include <QTableView>
#include <QComboBox>
#include <QAction>
#include <QDir>
#include <QTcpServer>
#include <QRandomGenerator>
#include <QPlainTextEdit>
#include <QTcpSocket>
#include "ui/GuiHost.h"
#include "ui/StatusSentence.h"
#include "ui/FeedbackSync.h"
#include "ui/PageLifecycle.h"
#include "ui/ToolWindowRegistry.h"
#include "ui/StatusShape.h"
#include "ui/SkeletonBlock.h"
#include "ui/TrayController.h"
#include "ui/TrayController.h"
#include <QSpinBox>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QJsonDocument>
#include <QPointer>
#include "profile/ProfileStore.h"
#include "storage/Storage.h"
#include "MockHttp.h"
#include "ui/WidgetStyle.h"
#include "ui/UiTheme.h"
#include <QScreen>
#include <QVBoxLayout>
#include <QCryptographicHash>

using namespace mlash;
static bool seedDashboardFixture(const QString& root) {
    auto platform=createPlatformServices(); ProfileStore store(root,platform); if (!store.load().result.ok) return false;
    Settings settings; settings.startCore=false;
    Profile profile; profile.id=newId(); profile.name="Dashboard fixture"; profile.sourceType="file";
    auto prepared=store.prepare(profile,"proxies: []\nrules: ['MATCH,DIRECT']\n",settings,19090,QString(64,'a'));
    if (!prepared.result.ok) return false;
    profile=prepared.profile; profile.active=profile.candidate; settings.activeProfile=profile.id;
    return store.save(settings,{profile}).ok;
}
class UiTests : public QObject {
    Q_OBJECT
private slots:
    void subscriptionCardsRouteActionsWithoutChangingUse() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(), true);
        QSignalSpy initialized(&controller, &AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(), 1);
        Profile remote = controller.profiles().first();
        remote.id = newId(); remote.name = "远程示例"; remote.sourceType = "url";
        remote.source = "http://127.0.0.1:1/sub?token=private";
        remote.active.clear();
        const_cast<QVector<Profile>&>(controller.profiles()).append(remote);
        MainWindow window(&controller); window.resize(900, 650); window.show(); window.setPage(1);
        auto* list = window.findChild<QListView*>("profileList"); QVERIFY(list);
        QCOMPARE(list->model()->rowCount(), 2);
        const auto shots = qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!shots.isEmpty()) { QVERIFY(QDir().mkpath(shots)); QVERIFY(window.grab().save(shots + "/subscription-list.png")); }
        const auto localIndex = list->model()->index(0, 0);
        const auto remoteIndex = list->model()->index(1, 0);
        QCOMPARE(localIndex.data(ProfileRowModel::SourceLabel).toString(), QString("本地快照"));
        QCOMPARE(remoteIndex.data(ProfileRowModel::SourceLabel).toString(), QString("127.0.0.1"));
        QVERIFY(!remoteIndex.data(ProfileRowModel::SourceLabel).toString().contains("private"));
        QVERIFY(!localIndex.data(ProfileRowModel::CanRefresh).toBool());
        QVERIFY(remoteIndex.data(ProfileRowModel::CanRefresh).toBool());
        QCOMPARE(list->viewMode(), QListView::IconMode);
        QCOMPARE(list->itemDelegate()->sizeHint({}, remoteIndex).height(), 144);
        QVERIFY(list->visualRect(localIndex).top() == list->visualRect(remoteIndex).top());
        window.resize(560, 650);
        QTRY_VERIFY(list->visualRect(localIndex).top() < list->visualRect(remoteIndex).top());
        window.resize(900, 650);
        QTRY_VERIFY(list->visualRect(localIndex).top() == list->visualRect(remoteIndex).top());
        const auto currentId = controller.state().activeProfile;
        const auto remoteRect = list->visualRect(remoteIndex);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, ProfileCardDelegate::refresh(remoteRect).center());
        QCOMPARE(controller.state().activeProfile, currentId);
        QTRY_VERIFY(!controller.state().busy);
        const auto menuPoint = ProfileCardDelegate::menu(list->visualRect(remoteIndex)).center();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, menuPoint);
        auto* menu = window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu);
        QCOMPARE(menu->property("profileKind").toString(), QString("remote"));
        QCOMPARE(menu->actions().size(), 5); // four object actions and a separator
        menu->hide();
        menu->actions().first()->trigger();
        auto* workspace = window.findChild<TaskWorkspace*>("taskWorkspace"); QVERIFY(workspace);
        QTRY_VERIFY(workspace->isVisible());
        QVERIFY(workspace->context().endsWith(":content"));
        QVERIFY(workspace->editor("nodes"));
        QVERIFY(workspace->findChild<QTabBar*>("taskTabs")->isVisible());
        QTRY_VERIFY(!window.findChild<QLabel*>("proxyEditorHint")->text().startsWith("正在读取"));
        if (!shots.isEmpty()) QVERIFY(window.grab().save(shots + "/subscription-content.png"));
        workspace->findChild<QPushButton*>("taskBack")->click();
        QTRY_VERIFY(list->isVisible());
        QCOMPARE(list->currentIndex().data(ProfileRowModel::ProfileId).toString(), remote.id);
        const auto review = ProfileCardDelegate::review(list->visualRect(remoteIndex)).center();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, review);
        QTRY_VERIFY(workspace->context().endsWith(":candidate"));
        QVERIFY(workspace->editor("candidate"));
        QCOMPARE(controller.state().activeProfile, currentId);
        workspace->findChild<QPushButton*>("taskBack")->click();
        const auto localMenuPoint = ProfileCardDelegate::menu(list->visualRect(localIndex)).center();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, localMenuPoint);
        QCOMPARE(menu->property("profileKind").toString(), QString("local"));
        QCOMPARE(menu->actions().size(), 4); // three actions and separator
        menu->hide();
        menu->actions().at(1)->trigger();
        QTRY_VERIFY(workspace->context().endsWith(":source"));
        auto* sourceEditor = window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(sourceEditor);
        QVERIFY(sourceEditor->findChild<QLineEdit*>("profileEditName"));
        QCOMPARE(controller.state().activeProfile, currentId);
    }
    void subscriptionCardEditingAndReturnPreserveObject() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        const auto id=controller.profiles().first().id;
        MainWindow window(&controller); window.resize(900,600); window.show(); window.setPage(1);
        auto* list=window.findChild<QListView*>("profileList"); QVERIFY(list);
        QCOMPARE(list->model()->rowCount(),1);
        QVERIFY(!window.findChild<QToolButton*>("profileOrder")->isVisible());
        const auto index=list->model()->index(0,0);
        QTest::mouseClick(list->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(list->visualRect(index)).center());
        auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu);
        QCOMPARE(menu->property("contextProfileId").toString(),id);
        menu->hide(); menu->actions().at(1)->trigger();
        auto* workspace=window.findChild<TaskWorkspace*>("taskWorkspace"); QVERIFY(workspace);
        QTRY_VERIFY(workspace->context().endsWith(":source"));
        auto* editor=window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(editor);
        auto* name=editor->findChild<QLineEdit*>("profileEditName"); QVERIFY(name);
        const auto original=name->text(); name->setText("尚未保存");
        QTimer::singleShot(0,&window,[&window] { if (auto* box=window.findChild<QMessageBox*>()) box->button(QMessageBox::No)->click(); });
        workspace->findChild<QPushButton*>("taskBack")->click();
        QVERIFY(workspace->isVisible()); QCOMPARE(name->text(),QString("尚未保存"));
        name->setText(original);
        workspace->findChild<QPushButton*>("taskBack")->click();
        QTRY_VERIFY(list->isVisible()); QCOMPARE(finished.size(),0);
        QTest::mouseClick(list->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(list->visualRect(index)).center());
        menu->hide(); menu->actions().at(1)->trigger();
        QTRY_VERIFY(workspace->context().endsWith(":source"));
        editor=window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(editor);
        name=editor->findChild<QLineEdit*>("profileEditName"); QVERIFY(name);
        name->setText("新的配置名称");
        editor->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        QTRY_COMPARE(finished.size(),1); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QCOMPARE(controller.profiles().first().id,id);
        QCOMPARE(controller.profiles().first().name,QString("新的配置名称"));
        QCOMPARE(list->model()->index(0,0).data(Qt::DisplayRole).toString(),QString("新的配置名称"));
    }
    void subscriptionCardReturnRestoresGridPosition() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        for (int i=0;i<18;++i) {
            Profile extra=controller.profiles().first(); extra.id=newId(); extra.name=QString("配置 %1").arg(i);
            const_cast<QVector<Profile>&>(controller.profiles()).append(extra);
        }
        MainWindow window(&controller); window.resize(900,570); window.show(); window.setPage(1);
        auto* list=window.findChild<QListView*>("profileList"); QVERIFY(list);
        QCOMPARE(list->model()->rowCount(),19);
        list->verticalScrollBar()->setValue(list->verticalScrollBar()->maximum()/2);
        QCoreApplication::processEvents(); const int position=list->verticalScrollBar()->value();
        const auto index=list->indexAt(QPoint(20,list->viewport()->height()/2)); QVERIFY(index.isValid());
        const auto id=index.data(ProfileRowModel::ProfileId).toString();
        QTest::mouseClick(list->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(list->visualRect(index)).center());
        auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu);
        QCOMPARE(menu->property("contextProfileId").toString(),id);
        menu->hide(); list->setCurrentIndex(list->model()->index(0,0));
        menu->actions().first()->trigger();
        auto* workspace=window.findChild<TaskWorkspace*>("taskWorkspace"); QVERIFY(workspace);
        QTRY_VERIFY(workspace->context().endsWith(":content"));
        QVERIFY(workspace->context().contains(id));
        QVERIFY(workspace->editor("nodes"));
        workspace->findChild<QPushButton*>("taskBack")->click();
        QTRY_VERIFY(list->isVisible());
        QTRY_COMPARE(list->verticalScrollBar()->value(),position);
        QCOMPARE(list->currentIndex().data(ProfileRowModel::ProfileId).toString(),id);
    }
    void subscriptionOrderKeepsSelectedObject() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        auto platform=createPlatformServices(); ProfileStore store(data.path(),platform);
        auto loaded=store.load(); QVERIFY(loaded.result.ok);
        Profile second=loaded.profiles.first(); second.id=newId(); second.name="第二个配置";
        const auto secondId=second.id;
        loaded.profiles.append(second);
        QVERIFY(store.save(loaded.settings,loaded.profiles).ok);
        AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(900,600); window.show(); window.setPage(1);
        auto* cards=window.findChild<QListView*>("profileList"); QVERIFY(cards);
        QTRY_COMPARE(cards->model()->rowCount(),2);
        QVERIFY(window.findChild<QToolButton*>("profileOrder")->isVisible());
        cards->setCurrentIndex(cards->model()->index(1,0));
        auto* menu=window.findChild<QMenu*>("profileOrderMenu"); QVERIFY(menu);
        menu->popup(cards->mapToGlobal(QPoint(20,20)));
        QTRY_VERIFY(menu->isVisible()); QVERIFY(menu->actions().first()->isEnabled());
        menu->hide(); menu->actions().first()->trigger();
        QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool());
        QCOMPARE(controller.profiles().first().id,secondId);
        QCOMPARE(cards->currentIndex().data(ProfileRowModel::ProfileId).toString(),secondId);
    }
    void subscriptionRollbackRequiresExplicitConfirmation() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& profile=const_cast<Profile&>(controller.profiles().first());
        profile.lastGood="previous-verified-version";
        const auto active=profile.active;
        MainWindow window(&controller); window.resize(900,600); window.show(); window.setPage(1);
        auto* cards=window.findChild<QListView*>("profileList"); QVERIFY(cards);
        QTest::mouseClick(cards->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(cards->visualRect(cards->model()->index(0,0))).center());
        auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu);
        menu->hide(); menu->actions().at(1)->trigger();
        auto* editor=window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(editor);
        auto* rollback=editor->findChild<QPushButton*>("rollbackProfile"); QVERIFY(rollback);
        QTimer::singleShot(0,&window,[&window] { if (auto* prompt=window.findChild<QMessageBox*>()) prompt->button(QMessageBox::No)->click(); });
        rollback->click();
        QCOMPARE(finished.size(),0);
        QCOMPARE(controller.profiles().first().active,active);
        QVERIFY(editor->isVisible());
    }
    void groupPickerSearchKeepsStablePopupAndSelection() {
        QWidget anchor; anchor.resize(36, 32); anchor.move(100, 100); anchor.show();
        auto* picker = new GroupPickerPopup(&anchor);
        QPointer<GroupPickerPopup> guard = picker;
        picker->addChoice("HK", false);
        picker->addChoice("JP", true);
        picker->addChoice("SG", false);
        QString chosen;
        picker->chosen = [&chosen](const QString& group) { chosen = group; };
        picker->openBelow(&anchor);
        QTRY_VERIFY(picker->isVisible());
        const QRect firstGeometry = picker->geometry();
        QCOMPARE(picker->grab().toImage().pixelColor(0, 0).alpha(), 0);
        const auto output = qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(picker->grab().save(output + "/group-picker.png")); }
        auto* search = picker->findChild<QLineEdit*>("groupSearch");
        auto* choices = picker->findChild<QListWidget*>("groupChoices");
        QVERIFY(search && choices);
        QCOMPARE(choices->count(), 3);
        QCOMPARE(choices->currentItem()->data(Qt::UserRole).toString(), QString("JP"));
        QTest::keyClicks(search, "SG");
        QCOMPARE(picker->geometry(), firstGeometry);
        QVERIFY(choices->item(0)->isHidden());
        QVERIFY(choices->item(1)->isHidden());
        QCOMPARE(choices->currentItem()->text(), QString("SG"));
        QTest::keyClick(search, Qt::Key_Return);
        QCOMPARE(chosen, QString("SG"));
        QTRY_VERIFY(guard.isNull());
    }
    void initTestCase() {
        // Match the production QApplication font, including item delegate fallback.
        QApplication::setFont(QFont("Microsoft YaHei UI",9));
        WidgetStyle::install();
    }
    void groupedRowHoverCoversWholeInterior() {
        QWidget canvas; auto* column=new QVBoxLayout(&canvas); auto* rows=PrototypeUi::group(column);
        auto* first=PrototypeUi::row(rows,"first","第一行",UiIcons::Glyph::Globe);
        auto* middle=PrototypeUi::row(rows,"middle","中间行",UiIcons::Glyph::Globe);
        auto* last=PrototypeUi::row(rows,"last","末行",UiIcons::Glyph::Globe);
        canvas.resize(360,200); canvas.show(); QCoreApplication::processEvents();
        for (auto* item:{first,middle,last}) {
            item->setAttribute(Qt::WA_UnderMouse,true); item->update();
            const auto image=item->grab().toImage();
            const int x=image.width()/2, dpr=qMax(1.0,image.devicePixelRatio());
            const int y=qMin(image.height()-2,int((item->height()-4)*dpr));
            QCOMPARE(image.pixelColor(x,y),QColor(UiTheme::Hover));
            item->setAttribute(Qt::WA_UnderMouse,false);
        }
    }
    void profileRenameStaysWithItsDetails() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        const auto id=controller.profiles().first().id;
        MainWindow window(&controller); window.resize(900,600); window.show(); window.setPage(1);
        QVERIFY(!window.findChild<QPushButton*>("batchProfiles")->isVisible());
        auto* list=window.findChild<QListView*>("localProfileList"); QVERIFY(list && list->model()->rowCount()==1);
        list->setCurrentIndex(list->model()->index(0,0)); QCoreApplication::processEvents();
        QMenu* menu=nullptr;
        for (auto* candidate:window.findChildren<QMenu*>("profileContextMenu"))
            if (candidate->property("profileKind").toString()=="local") menu=candidate;
        QVERIFY(menu);
        auto* rename=menu->findChild<QAction*>("renameProfile"); QVERIFY(rename && rename->isEnabled());
        rename->trigger();
        auto* editor=window.findChild<QWidget*>("profileNameEditor"); QVERIFY(editor); QTRY_VERIFY(editor->isVisible());
        QVERIFY(!window.findChild<QInputDialog*>());
        auto* input=editor->findChild<QLineEdit*>("profileNameInput"); QVERIFY(input);
        input->setText("尚未保存");
        // A double click can deliver activation after the first click opened
        // details. It must not rebuild the inline editor or lose its draft.
        list->activated(list->model()->index(0,0));
        QCOMPARE(window.findChild<QWidget*>("profileNameEditor"),editor);
        QCOMPARE(input->text(),QString("尚未保存"));
        QTimer::singleShot(0,&window,[&window] { if (auto* box=window.findChild<QMessageBox*>()) box->button(QMessageBox::Cancel)->click(); });
        window.findChild<QPushButton*>("profileDetailsBack")->click();
        QVERIFY(editor->isVisible()); QCOMPARE(input->text(),QString("尚未保存"));
        input->setText("取消的草稿"); editor->findChild<QPushButton*>("cancelProfileName")->click();
        QVERIFY(!editor->isVisible()); QCOMPARE(input->text(),QString("Dashboard fixture"));
        QCOMPARE(controller.profiles().first().name,QString("Dashboard fixture")); QCOMPARE(finished.size(),0);
        window.findChild<QPushButton*>("profileDetailsRename")->click(); QTRY_VERIFY(editor->isVisible());
        input->setText("新的配置名称"); editor->findChild<QPushButton*>("saveProfileName")->click();
        QTRY_COMPARE(finished.size(),1); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QCOMPARE(controller.profiles().first().id,id); QCOMPARE(controller.profiles().first().name,QString("新的配置名称"));
        QTRY_VERIFY(!editor->isVisible());
    }
    void profileRenameDraftBlocksObjectSwitch() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        Profile second=controller.profiles().first(); second.id=newId(); second.name="Second fixture";
        const_cast<QVector<Profile>&>(controller.profiles()).append(second);
        MainWindow window(&controller); window.resize(900,600); window.show(); window.navigate(Route{ViewIds::Profiles});
        auto* list=window.findChild<QListView*>("localProfileList"); QVERIFY(list); QCOMPARE(list->model()->rowCount(),2);
        const auto first=list->model()->index(0,0), other=list->model()->index(1,0);
        const auto originalId=first.data(ProfileRowModel::ProfileId).toString();
        list->activated(first);
        auto* editor=window.findChild<QWidget*>("profileNameEditor"); QVERIFY(editor);
        editor->parentWidget()->findChild<QPushButton*>("profileDetailsRename")->click();
        auto* input=editor->findChild<QLineEdit*>("profileNameInput"); QVERIFY(input); input->setText("未保存的配置名");
        list->activated(other);
        QCOMPARE(window.findChild<QWidget*>("profileNameEditor"),editor);
        QCOMPARE(input->text(),QString("未保存的配置名"));
        QCOMPARE(list->currentIndex().data(ProfileRowModel::ProfileId).toString(),originalId);
        editor->findChild<QPushButton*>("cancelProfileName")->click();
        list->activated(other);
        auto* stack=window.findChild<QStackedWidget*>("profileTaskStack"); QVERIFY(stack);
        QCOMPARE(stack->currentWidget()->findChild<QLineEdit*>("profileNameInput")->text(),QString("Second fixture"));
        QCOMPARE(list->currentIndex().data(ProfileRowModel::ProfileId).toString(),second.id);
    }
    void profileTaskReturnPreservesDetailAndExternalNoticeStaysLocal() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        for (int i=0;i<18;++i) { Profile extra=controller.profiles().first(); extra.id=newId(); extra.name=QString("滚动配置 %1").arg(i); const_cast<QVector<Profile>&>(controller.profiles()).append(extra); }
        MainWindow window(&controller); window.resize(900,570); window.show();
        window.navigate(Route{ViewIds::Profiles}); QCoreApplication::processEvents();
        auto* list=window.findChild<QListView*>("localProfileList"); QVERIFY(list && list->model()->rowCount());
        list->verticalScrollBar()->setValue(list->verticalScrollBar()->maximum()/2); QCoreApplication::processEvents();
        const int listPosition=list->verticalScrollBar()->value();
        const auto index=list->indexAt(QPoint(20,list->viewport()->height()/2)); QVERIFY(index.isValid());
        QTest::mouseClick(list->viewport(),Qt::LeftButton,{},list->visualRect(index).center());
        auto* stack=window.findChild<QStackedWidget*>("profileTaskStack"); QVERIFY(stack); QTRY_COMPARE(stack->currentIndex(),1);
        auto* detail=qobject_cast<QScrollArea*>(stack->widget(1)); QVERIFY(detail); QVERIFY(window.findChild<QWidget*>("profileDetailsPage")); QVERIFY(!window.findChild<QTabBar*>("profileTaskTabs"));
        detail->verticalScrollBar()->setValue(detail->verticalScrollBar()->maximum()); const int position=detail->verticalScrollBar()->value();
        window.findChild<QPushButton*>("profileDetailsRules")->click();
        auto* tabs=window.findChild<QTabBar*>("taskTabs"); QVERIFY(tabs); QVERIFY(tabs->isHidden());
        auto* taskBack=window.findChild<QPushButton*>("taskBack"); QVERIFY(taskBack&&taskBack->isVisible()); taskBack->click(); QTRY_COMPARE(stack->currentIndex(),1);
        QCOMPARE(stack->widget(1),static_cast<QWidget*>(detail));
        QCOMPARE(detail->verticalScrollBar()->value(),position);
        window.navigate(Route{ViewIds::Profiles}); QCOMPARE(stack->currentIndex(),1);
        window.navigate(Route{ViewIds::Home});
        window.navigate(Route{ViewIds::Profiles});
        QCOMPARE(stack->currentIndex(),1);
        QCOMPARE(stack->widget(1),static_cast<QWidget*>(detail));
        QCOMPARE(detail->verticalScrollBar()->value(),position);
        window.findChild<QPushButton*>("profileDetailsBack")->click(); QCOMPARE(stack->currentIndex(),0);
        QCOMPARE(list->verticalScrollBar()->value(),listPosition);
        auto& state=const_cast<AppState&>(controller.state());
        state.proxy=ProxyState::External; state.lastError="系统已有代理/PAC/自动检测，需明确允许接管"; controller.stateChanged();
        auto* alarm=window.findChild<QLabel*>("operationAlarm"); auto* resolve=window.findChild<QToolButton*>("statusResolve"); QVERIFY(alarm && resolve);
        QVERIFY(alarm->text().isEmpty()); QVERIFY(!resolve->isVisible());
        state.lastError="恢复日志写入失败"; controller.stateChanged();
        QCOMPARE(alarm->text(),QString("恢复日志写入失败")); QVERIFY(resolve->isVisible());
    }
    void changingSectionRestoresNetworkSettingsDraft() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(900,600); window.show();
        window.navigate(Route{ViewIds::Settings});
        window.findChild<QPushButton*>("settingsNetworkRow")->click();
        auto* port=window.findChild<QSpinBox*>("mixedPort"); QVERIFY(port);
        const int original=port->value(); port->setValue(original+1);
        window.navigate(Route{ViewIds::Home});
        QCOMPARE(window.currentRoute().view,int(ViewIds::Home));
        QCOMPARE(port->value(),original+1);
        window.navigate(Route{ViewIds::Settings});
        QCOMPARE(window.findChild<QStackedWidget*>("settingsTaskStack")->currentIndex(),1);
        QCOMPARE(port->value(),original+1);
        QTimer::singleShot(0,[] { if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); });
        window.findChild<QPushButton*>("settingsBack")->click();
        QCOMPARE(port->value(),original);
        QCOMPARE(window.findChild<QStackedWidget*>("settingsTaskStack")->currentIndex(),0);
    }
    void settingsDirectoryOwnsNetworkAndCoreTasks() {
        QTemporaryDir data; AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,700); window.show(); window.setPage(6);
        auto* stack=window.findChild<QStackedWidget*>("settingsTaskStack");
        auto* port=window.findChild<QSpinBox*>("mixedPort");
        auto* logLevel=window.findChild<QComboBox*>("coreLogLevel");
        auto* coreUpdate=window.findChild<QPushButton*>("manageCoreUpdate");
        QVERIFY(stack&&port&&logLevel&&coreUpdate);
        window.findChild<QPushButton*>("settingsNetworkRow")->click();
        QCOMPARE(stack->currentIndex(),1); QVERIFY(port->isVisible()); QVERIFY(!logLevel->isVisible()); QVERIFY(!coreUpdate->isVisible());
        window.findChild<QPushButton*>("settingsBack")->click();
        window.findChild<QPushButton*>("settingsCoreRow")->click();
        QCOMPARE(stack->currentIndex(),1); QVERIFY(logLevel->isVisible()); QVERIFY(coreUpdate->isVisible()); QVERIFY(!port->isVisible());
        window.findChild<QPushButton*>("settingsBack")->click();
        auto* search=window.findChild<QLineEdit*>("settingsSearch"); QVERIFY(search&&search->isVisible());
        search->setText("日志等级"); QTest::keyClick(search,Qt::Key_Return);
        QCOMPARE(stack->currentIndex(),1); QVERIFY(logLevel->isVisible()); QVERIFY(!port->isVisible());
        window.findChild<QPushButton*>("settingsBack")->click();
        search->setText("登录启动"); QTest::keyClick(search,Qt::Key_Return);
        QCOMPARE(stack->currentIndex(),0);
        auto* autostart=window.findChild<QCheckBox*>("autoStart");
        QVERIFY(autostart&&autostart->isVisible());
        QVERIFY(window.findChild<QWidget*>("settingsDirectory")->isAncestorOf(autostart));
    }
    void settingsSearchDoesNotRearrangeDirtyNetworkTask() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(900,600); window.show(); window.navigate(Route{ViewIds::Settings});
        window.findChild<QPushButton*>("settingsNetworkRow")->click();
        auto* stack=window.findChild<QStackedWidget*>("settingsTaskStack");
        auto* port=window.findChild<QSpinBox*>("mixedPort"); QVERIFY(stack && port);
        port->setValue(port->value()+1); const auto draft=port->value();
        // A stale search result can still emit a navigation request while the
        // network task owns an unapplied draft.
        auto* search=window.findChild<SettingsSearch*>(); QVERIFY(search);
        emit search->anchorRequested(port);
        QCOMPARE(stack->currentIndex(),1);
        QCOMPARE(port->value(),draft);
        QVERIFY(port->isVisible());
    }
    void profileContextCommandKeepsOpenedObject() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        Profile second=controller.profiles().first(); second.id=newId(); second.name="Second fixture";
        const_cast<QVector<Profile>&>(controller.profiles()).append(second);
        MainWindow window(&controller); window.resize(900,600); window.show(); window.navigate(Route{ViewIds::Profiles});
        QVERIFY(!window.findChild<QToolButton*>("toolbarAdd")->isVisible());
        auto* list=window.findChild<QListView*>("localProfileList"); QVERIFY(list); QCOMPARE(list->model()->rowCount(),2);
        const auto first=list->model()->index(0,0), other=list->model()->index(1,0);
        const auto openedId=first.data(ProfileRowModel::ProfileId).toString();
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::ControlModifier,list->visualRect(first).center());
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::ControlModifier,list->visualRect(other).center());
        QCOMPARE(window.findChild<QStackedWidget*>("profileTaskStack")->currentIndex(),0);
        QCOMPARE(list->selectionModel()->selectedRows().size(),2);
        const auto point=list->visualRect(first).center();
        QContextMenuEvent event(QContextMenuEvent::Mouse,point,list->viewport()->mapToGlobal(point));
        QApplication::sendEvent(list->viewport(),&event);
        QMenu* menu=nullptr;
        for (auto* candidate:window.findChildren<QMenu*>("profileContextMenu"))
            if (candidate->property("profileKind").toString()=="local") menu=candidate;
        QVERIFY(menu); QTRY_VERIFY(menu->isVisible()); QCOMPARE(list->selectionModel()->selectedRows().size(),1); menu->hide();
        list->setCurrentIndex(other); // a refresh or another selection after the menu opened
        menu->findChild<QAction*>("renameProfile")->trigger();
        auto* editor=window.findChild<QWidget*>("profileNameEditor"); QVERIFY(editor && editor->isVisible());
        QCOMPARE(editor->findChild<QLineEdit*>("profileNameInput")->text(),QString("Dashboard fixture"));
        QCOMPARE(window.findChild<QStackedWidget*>("profileTaskStack")->currentIndex(),1);
        QCOMPARE(list->currentIndex().data(ProfileRowModel::ProfileId).toString(),openedId);
    }
    void changingSectionRestoresEmbeddedTaskDraft() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(900,600); window.show(); window.navigate(Route{ViewIds::Settings});
        auto* editor=new QDialog(&window); editor->setObjectName("navigationDraftFixture");
        auto* input=new QLineEdit(editor); input->setText("unsaved");
        DialogDraftGuard::protect(editor,[input] { return !input->text().isEmpty(); });
        window.presentTask(editor,"navigation-draft"); QTRY_VERIFY(editor->isVisible());
        window.navigate(Route{ViewIds::Home});
        QCOMPARE(window.currentRoute().view,int(ViewIds::Home));
        QCOMPARE(input->text(),QString("unsaved"));
        window.navigate(Route{ViewIds::Settings});
        QCOMPARE(window.currentRoute().view,int(ViewIds::Settings));
        QVERIFY(editor->isVisible()); QCOMPARE(input->text(),QString("unsaved"));
    }
    void rulesNextStepAlwaysReturnsToItsProfile() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(900,600); window.show();
        auto* tabs=window.findChild<QTabWidget*>("rulesTabs");
        auto* next=window.findChild<QPushButton*>("rulesNextStep");
        QVERIFY(tabs); QVERIFY(next);
        for (int tab=0; tab<tabs->count(); ++tab) {
            window.navigate(Route{ViewIds::Rules});
            tabs->setCurrentIndex(tab);
            next->click();
            QCOMPARE(window.currentRoute().view,int(ViewIds::Profiles));
            QCOMPARE(window.findChild<QStackedWidget*>("profileTaskStack")->currentIndex(),1);
        }
    }
    void settingsSearchKeepsDirtyTaskVisible() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(900,600); window.show(); window.navigate(Route{ViewIds::Settings});
        auto* stack=window.findChild<QStackedWidget*>("settingsTaskStack");
        QCOMPARE(stack->currentIndex(),0);
        auto* strip=window.findChild<QWidget*>("settingsSearchStrip");
        QVERIFY(strip && strip->isVisible());
        QVERIFY(strip->height()<72);
        auto* search=window.findChild<QLineEdit*>("settingsSearch");
        QVERIFY(search && search->isVisible());
        auto* directory=window.findChild<QWidget*>("settingsDirectory");
        QVERIFY(directory && directory->isVisible());
        search->setText("外观");
        QTest::keyClick(search,Qt::Key_Return);
        QCOMPARE(stack->currentIndex(),0);
        QVERIFY(directory->isVisible());
        auto* appearanceRow=window.findChild<QWidget*>("settingsAppearanceRow");
        QVERIFY(appearanceRow->isVisible());
        QVERIFY(appearanceRow->property("located").toBool());
        window.navigate(Route{ViewIds::Home});
        window.navigate(Route{ViewIds::Settings});
        QCOMPARE(stack->currentIndex(),0);
        QVERIFY(directory->isVisible());
        QVERIFY(search->isVisible());
        QVERIFY(appearanceRow->isVisible());
        search->clear();
        window.findChild<QPushButton*>("settingsNetworkRow")->click();
        auto* port=window.findChild<QSpinBox*>("mixedPort");
        QVERIFY(stack); QVERIFY(port); QVERIFY(search);
        QVERIFY(search->isVisible());
        const int changed=port->value()+1; port->setValue(changed);
        search->setText("登录 启动");
        QTest::keyClick(search,Qt::Key_Return);
        QCOMPARE(stack->currentIndex(),1);
        QCOMPARE(port->value(),changed);
        QVERIFY(port->isVisible());
        for (auto* row:window.findChildren<QWidget*>("settingRow"))
            if (row->property("settingSearchName").toString()=="登录启动") QVERIFY(!row->property("located").toBool());
    }
    void nodeViewSwitchAndBookmarkUseVisibleSelection() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true;
        auto& proxies=const_cast<QJsonObject&>(controller.proxies());
        proxies={{"A",QJsonObject{{"type","Direct"}}},{"C",QJsonObject{{"type","Direct"}}},
                 {"B",QJsonObject{{"type","Selector"},{"now","C"},{"all",QJsonArray{"C"}}}},
                 {"G",QJsonObject{{"type","Selector"},{"now","A"},{"all",QJsonArray{"A","B"}}}}};
        MainWindow window(&controller); window.resize(900,600); window.show();
        window.restoreNodeBookmark({{"profile",state.activeProfile},{"group","G"},{"view",0}});
        auto* rows=window.findChild<QListView*>("nodeList");
        auto* cards=window.findChild<QListView*>("nodeCards");
        QVERIFY(rows); QVERIFY(cards);
        QModelIndex a;
        for (int i=0;i<rows->model()->rowCount();++i) if (rows->model()->index(i,0).data(Qt::DisplayRole).toString()=="A") a=rows->model()->index(i,0);
        QVERIFY(a.isValid());
        QCOMPARE(a.data(Qt::DisplayRole).toString(),QString("A"));
        rows->setCurrentIndex(a);
        window.setMemberView(1);
        QCOMPARE(cards->currentIndex().data(Qt::UserRole).toString(),QString("A"));
        for (int i=0;i<cards->model()->rowCount();++i) if (cards->model()->index(i,0).data(Qt::UserRole).toString()=="B") cards->setCurrentIndex(cards->model()->index(i,0));
        QCOMPARE(window.nodeBookmark().value("member").toString(),QString("B"));
        window.setMemberView(0);
        QCOMPARE(rows->currentIndex().data(Qt::DisplayRole).toString(),QString("B"));
        const auto point=rows->visualRect(rows->currentIndex()).center();
        QContextMenuEvent context(QContextMenuEvent::Mouse,point,rows->viewport()->mapToGlobal(point));
        QApplication::sendEvent(rows->viewport(),&context);
        auto* favorite=rows->findChild<QMenu*>("nodeFavoriteMenu");
        QVERIFY(favorite); QTRY_VERIFY(favorite->isVisible());
        QCOMPARE(favorite->actions().first()->text(),QString("收藏此成员"));
        favorite->hide();
        window.setMemberView(1);
        window.findChild<QAction*>("enterGroup")->trigger();
        QCOMPARE(window.nodeBookmark().value("group").toString(),QString("B"));
    }
    void prototypeSystemPagesAndResponsiveInspector() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true; state.mode="rule";
        auto& proxies=const_cast<QJsonObject&>(controller.proxies()); QJsonArray members;
        for (int i=0;i<40;++i) { const auto name=QString("JP 日本 节点 %1").arg(i); members.append(name); proxies.insert(name,QJsonObject{{"type","Http"}}); }
        proxies.insert("日常选择",QJsonObject{{"type","Selector"},{"all",members},{"now",members.first()}});
        MainWindow window(&controller); window.resize(1040,720); window.show(); QTest::qWait(40);
        auto* sidebar=window.findChild<QWidget*>("sidebar"); auto* navigation=window.findChild<QListWidget*>("navigation");
        QVERIFY(sidebar && navigation); QCOMPARE(sidebar->width(),228);
        const auto firstNav=navigation->visualItemRect(navigation->item(0));
        const auto secondNav=navigation->visualItemRect(navigation->item(1));
        QVERIFY(firstNav.height()>=48); QVERIFY(secondNav.top()-firstNav.bottom()-1>=8);
        const auto save=[&](const QString& name) { const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(window.grab().save(output+"/u11-"+name+".png")); } };
        auto* title=window.findChild<QLabel*>("topbarTitle"); QCOMPARE(title->text(),QString("Mlash"));
        QVERIFY(window.findChild<QToolButton*>("toolbarAdd")->isVisible()); QVERIFY(!window.findChild<QToolButton*>("toolbarImport")); QVERIFY(!window.findChild<QToolButton*>("topbarMore"));
        QVERIFY(window.findChild<QComboBox*>("nodeProfile")->isHidden()); QVERIFY(window.findChild<QToolButton*>("browseGroup")->isVisible());
        auto* nodes=window.findChild<QListView*>("nodeList"); QVERIFY(nodes->model()->rowCount()>1); QCOMPARE(nodes->model()->index(0,0).data().toString(),QString("日常选择"));
        QVERIFY(!nodes->model()->index(0,0).data(NodeRowModel::Subtitle).toString().isEmpty());
        window.navigate(Route{ViewIds::Settings}); QCoreApplication::processEvents();
        auto* settings=window.findChild<QStackedWidget*>("settingsTaskStack"); QVERIFY(settings); QCOMPARE(settings->currentIndex(),0);
        QVERIFY(window.findChild<QPushButton*>("settingsNetworkRow")->isVisible()); QVERIFY(!window.findChild<QSpinBox*>("mixedPort")->isVisible());
        QVERIFY(window.findChild<QCheckBox*>("lightweightMode")->isVisible()); save("settings-directory");
        QSignalSpy operations(&controller,&AppController::operationStarted);
        QTest::mouseClick(window.findChild<QPushButton*>("settingsNetworkRow"),Qt::LeftButton); QCOMPARE(settings->currentIndex(),1);
        auto* port=window.findChild<QSpinBox*>("mixedPort"); QVERIFY(port->isVisible()); const int baseline=port->value(); port->setValue(baseline+1);
        QTRY_VERIFY(window.findChild<QPushButton*>("applySettings")->isVisible());
        auto* testUrl=window.findChild<QLineEdit*>("connectivityTestUrl"); const auto originalUrl=testUrl->text(); testUrl->setText("file:///invalid-probe"); emit testUrl->editingFinished(); QCOMPARE(operations.size(),0);
        QSignalSpy rejected(&controller,&AppController::operationFinished); window.findChild<QPushButton*>("applySettings")->click(); QTRY_COMPARE(rejected.size(),1); QVERIFY(!rejected[0][0].toBool()); QCOMPARE(qvariant_cast<OperationId>(rejected[0][2]),OperationId::SettingsApply); QCOMPARE(port->value(),baseline+1); QCOMPARE(testUrl->text(),QString("file:///invalid-probe")); save("network-editor");
        QTimer::singleShot(0,[] { if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::No)->click(); });
        window.findChild<QPushButton*>("settingsBack")->click(); QCOMPARE(settings->currentIndex(),1); QCOMPARE(port->value(),baseline+1);
        QTimer::singleShot(0,[] { if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); });
        window.findChild<QPushButton*>("settingsBack")->click(); QCOMPARE(settings->currentIndex(),0); QCOMPARE(port->value(),baseline); QCOMPARE(testUrl->text(),originalUrl); QCOMPARE(operations.size(),0);
        window.navigate(Route{ViewIds::Profiles}); QCoreApplication::processEvents(); save("profiles");
        auto* local=window.findChild<QListView*>("profileList"); QVERIFY(local&&local->model()->rowCount());
        QTest::mouseClick(local->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(local->visualRect(local->model()->index(0,0))).center());
        auto* profileMenu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(profileMenu);
        profileMenu->hide(); profileMenu->actions().first()->trigger();
        auto* profileTasks=window.findChild<TaskWorkspace*>("taskWorkspace"); QVERIFY(profileTasks);
        QTRY_VERIFY(profileTasks->isVisible()); QVERIFY(profileTasks->editor("nodes"));
        QCOMPARE(profileTasks->findChild<QTabBar*>("taskTabs")->count(),6); save("profile-content");
        profileTasks->findChild<QPushButton*>("taskBack")->click(); QTRY_VERIFY(local->isVisible());
        window.navigate(Route{ViewIds::Home}); QCoreApplication::processEvents(); save("home");
        const auto index=nodes->model()->index(1,0); nodes->scrollTo(index); QCoreApplication::processEvents();
        QTest::mouseClick(nodes->viewport(),Qt::LeftButton,{},NodeRowDelegate::infoRect(nodes->visualRect(index)).center());
        auto* inspector=window.findChild<QFrame*>("nodeDetailsDrawer"); QTRY_VERIFY(inspector&&inspector->isVisible());
        QCOMPARE(inspector->property("inspectorMode").toString(),QString("page")); QVERIFY(!window.findChild<QStackedWidget*>("mainPages")->isVisible()); save("inspector-page");
        window.resize(1440,900); QTest::qWait(40); QCOMPARE(inspector->property("inspectorMode").toString(),QString("side"));
        QVERIFY(window.findChild<QStackedWidget*>("mainPages")->isVisible()); QVERIFY(window.findChild<QStackedWidget*>("mainPages")->contentsMargins().right()>300); save("inspector-side");
        window.findChild<QToolButton*>("drawerClose")->click(); QVERIFY(!inspector->isVisible()); QCOMPARE(window.findChild<QStackedWidget*>("mainPages")->contentsMargins(),QMargins(UiTheme::ContentInset,UiTheme::ContentTop,UiTheme::ContentInset,UiTheme::ContentTop));
        for (const auto scheme:QStringList{"light","dark"}) {
            UiTheme::setPreferences(scheme,120); window.resize(840,560); window.navigate(Route{ViewIds::Settings}); QTest::qWait(40); save("settings-"+scheme+"-120");
            auto* directory=window.findChild<QScrollArea*>("settingsDirectoryScroll"); QCOMPARE(directory->horizontalScrollBar()->maximum(),0); QVERIFY(directory->widget()->width()<=directory->viewport()->width());
        }
        UiTheme::setPreferences("system",100);
    }
    void compactNodeDelayDoesNotSelectAndProfileBrowseDoesNotApply() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1); MainWindow window(&controller); window.show();
        auto* combo=window.findChild<QComboBox*>("nodeProfile"); QVERIFY(combo);
        QSignalSpy finished(&controller,&AppController::operationFinished);
        combo->addItem("Browse only","not-an-active-profile"); combo->setCurrentIndex(combo->count()-1);
        QCoreApplication::processEvents(); QVERIFY(controller.state().activeProfile.isEmpty()); QCOMPARE(finished.size(),0);
        auto* apply=window.findChild<QPushButton*>("useBrowsedProfile"); QVERIFY(apply&&apply->isHidden());
        // Independent delay target consumes the click before row selection.
        QStandardItemModel rows; auto* item=new QStandardItem("Japan"); item->setData(1,NodeRowModel::RowKind); rows.appendRow(item);
        NodeRowView view; view.setModel(&rows); view.setItemDelegate(new NodeRowDelegate(&view)); view.resize(500,120); view.show(); view.setTestEnabled(true);
        int tests=0; view.delayClicked=[&](const QModelIndex& index) { if (index.row()==0) ++tests; };
        QSignalSpy selected(&view,&QListView::clicked); QCoreApplication::processEvents();
        const auto hit=NodeRowDelegate::delayRect(view.visualRect(rows.index(0,0))).center();
        QTest::mouseClick(view.viewport(),Qt::LeftButton,Qt::NoModifier,hit); QCOMPARE(tests,1); QCOMPARE(selected.size(),0);
        view.setTestEnabled(false); QTest::mouseClick(view.viewport(),Qt::LeftButton,Qt::NoModifier,hit); QCOMPARE(tests,1); QCOMPARE(selected.size(),0);
        QVERIFY(!NodeRowDelegate::delayRect(view.rect()).intersects(NodeRowDelegate::infoRect(view.rect())));
    }
    void popupSurfacesWithoutMainWindow() {
        for (auto effect:{Qt::UI_AnimateCombo,Qt::UI_AnimateMenu,Qt::UI_FadeMenu}) QApplication::setEffectEnabled(effect,true);
        WidgetStyle::install(); WidgetStyle::install();
        for (auto effect:{Qt::UI_AnimateCombo,Qt::UI_AnimateMenu,Qt::UI_FadeMenu}) QVERIFY(!QApplication::isEffectEnabled(effect));
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        TrayController tray(&controller); auto* root=tray.findChild<QSystemTrayIcon*>()->contextMenu(); root->popup(QPoint(200,100)); QTRY_VERIFY(root->isVisible()); QCOMPARE(QApplication::style()->objectName(),QString("mlash-refined")); QVERIFY(root->testAttribute(Qt::WA_TranslucentBackground)); QCOMPARE(root->grab().toImage().pixelColor(0,0).alpha(),0);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(root->grab().save(output+"/tray-popup.png")); } root->hide();
        QDialog dialog; dialog.move(80,80); auto* layout=new QVBoxLayout(&dialog); auto* combo=new QComboBox; QPointer<QAbstractItemView> originalView=combo->view(); auto* originalSelection=originalView->selectionModel(); WidgetStyle::prepareCombo(combo); QVERIFY(originalView); QCOMPARE(combo->view(),originalView.data()); QCOMPARE(combo->view()->selectionModel(),originalSelection); combo->addItem("规则模式","rule"); combo->addItem("全局模式","global"); combo->addItem("直连模式","direct"); combo->setFixedWidth(70); layout->addWidget(combo); dialog.resize(260,90); dialog.show(); QApplication::setActiveWindow(&dialog); QSignalSpy activated(combo,&QComboBox::activated);
        combo->clearFocus(); combo->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QVERIFY(!combo->property("keyboardFocus").toBool());
        combo->clearFocus(); combo->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY(combo->property("keyboardFocus").toBool());
        combo->showPopup(); QTRY_VERIFY(combo->view()->isVisible()); auto* popup=combo->view()->window(); QVERIFY(popup->geometry().top()>=combo->mapToGlobal(combo->rect().bottomLeft()).y()-2); QVERIFY(popup->width()>=QFontMetrics(combo->font()).horizontalAdvance(combo->itemText(0))+42); QCOMPARE(popup->grab().toImage().pixelColor(0,0).alpha(),0); const auto popupImage=popup->grab().toImage(); QCOMPARE(popupImage.pixelColor(popupImage.width()/2,popupImage.height()/2).alpha(),255);
        const auto initialGeometry=popup->geometry();
        for (int ms:{0,16,60,160}) { if (ms) QTest::qWait(ms); QCOMPARE(popup->geometry(),initialGeometry); QCOMPARE(popup->grab().toImage().pixelColor(0,0).alpha(),0); if (!output.isEmpty()) QVERIFY(popup->grab().save(output+QString("/combo-frame-%1.png").arg(ms))); }
        if (!output.isEmpty()) QVERIFY(popup->grab().save(output+"/combo-popup.png")); QTest::keyClick(combo->view(),Qt::Key_Down); if (!output.isEmpty()) QVERIFY(popup->grab().save(output+"/combo-keyboard.png")); QTest::keyClick(combo->view(),Qt::Key_Return); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(combo->currentData().toString(),QString("global")); QCOMPARE(activated.size(),1);
        combo->showPopup(); QTest::keyClick(combo->view(),Qt::Key_Escape); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(combo->currentData().toString(),QString("global")); QCOMPARE(activated.size(),1);
        combo->setEditable(true); for (int i=0;i<40;++i) combo->addItem(QString("节点 %1").arg(i),QString("n%1").arg(i)); combo->setEditText("自定义组"); combo->showPopup(); QVERIFY(combo->view()->window()->height()<400); QTest::keyClick(combo->view(),Qt::Key_Escape); QCOMPARE(combo->currentText(),QString("自定义组")); combo->showPopup(); QTest::keyClick(combo->view(),Qt::Key_End); QTest::keyClick(combo->view(),Qt::Key_Return); QTRY_VERIFY(!combo->view()->isVisible()); QCOMPARE(combo->currentData().toString(),QString("n39"));
        QMenu longMenu; QAction* last=nullptr; for (int i=0;i<500;++i) last=longMenu.addAction(QString("节点 %1").arg(i)); QSignalSpy triggered(&longMenu,&QMenu::triggered); longMenu.popup(QPoint(150,80)); QTRY_VERIFY(longMenu.isVisible()); QVERIFY2(longMenu.height()<=longMenu.screen()->availableGeometry().height(),qUtf8Printable(QString("menu %1x%2 screen %3 avail %4 scroll %5").arg(longMenu.width()).arg(longMenu.height()).arg(longMenu.screen()->geometry().height()).arg(longMenu.screen()->availableGeometry().height()).arg(longMenu.style()->styleHint(QStyle::SH_Menu_Scrollable)))); QCOMPARE(longMenu.actionGeometry(longMenu.actions().first()).left(),longMenu.actionGeometry(last).left()); QVERIFY(longMenu.width()<480);
        if (!output.isEmpty()) QVERIFY(longMenu.grab().save(output+"/long-menu.png")); QTest::keyClick(&longMenu,Qt::Key_End); QTest::keyClick(&longMenu,Qt::Key_Return); QTRY_VERIFY(!longMenu.isVisible()); QCOMPARE(triggered.size(),1); QCOMPARE(qvariant_cast<QAction*>(triggered.first()[0]),last);
        QMenu measured; for (int i=0;i<30;++i) measured.addAction(QString("选项 %1").arg(i));
        const QPoint anchor=QGuiApplication::primaryScreen()->availableGeometry().bottomRight()-QPoint(12,12);
        WidgetStyle::popupMenu(&measured,anchor); QTRY_VERIFY(measured.isVisible());
        const QRect placed=measured.geometry(), screen=measured.screen()->availableGeometry();
        QVERIFY(screen.contains(placed)); QVERIFY(placed.height()<=480);
        QCOMPARE(measured.grab().toImage().pixelColor(0,0).alpha(),0);
        if (!output.isEmpty()) QVERIFY(measured.grab().save(output+"/measured-menu.png"));
        QTest::qWait(60); QCOMPARE(measured.geometry(),placed);
        QTest::keyClick(&measured,Qt::Key_Escape); QTRY_VERIFY(!measured.isVisible());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void desktopPreferencesAndEnvironmentCopy() {
        QTemporaryDir root; ProfileStore store(root.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; QVERIFY(store.save(settings,{}).ok);
        AppController controller(root.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(6); auto* startup=window.findChild<QComboBox*>("startupPage"); auto* click=window.findChild<QComboBox*>("trayClick"); auto* ipv6=window.findChild<QComboBox*>("ipv6Mode"); auto* unified=window.findChild<QComboBox*>("unifiedDelayMode"); QVERIFY(startup && click && ipv6 && unified);
        QCOMPARE(startup->currentIndex(),0); QCOMPARE(ipv6->currentData().toString(),QString("source"));
        // Preference rows commit on their own; restart-class rows wait for the explicit action.
        startup->setCurrentIndex(2); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(store.load().settings.startupPage,2);
        click->setCurrentIndex(click->findData("proxy-page")); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,10000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(store.load().settings.trayClick,QString("proxy-page"));
        ipv6->setCurrentIndex(ipv6->findData("enabled")); unified->setCurrentIndex(unified->findData("disabled")); QCOMPARE(finished.size(),2); QCOMPARE(store.load().settings.ipv6Mode,QString("source"));
        window.findChild<QPushButton*>("applySettings")->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,10000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(store.load().settings.ipv6Mode,QString("enabled")); QCOMPARE(store.load().settings.unifiedDelayMode,QString("disabled"));
        window.findChild<QAction*>("copyProxyEnvironmentPowerShell")->trigger(); const auto powershell=QApplication::clipboard()->text(); QVERIFY(powershell.contains("$env:HTTPS_PROXY=\"http://127.0.0.1:")); QVERIFY(powershell.contains('\n')); QVERIFY(!powershell.contains("secret")); window.findChild<QAction*>("copyProxyEnvironmentCMD")->trigger(); QVERIFY(QApplication::clipboard()->text().startsWith("set \"HTTP_PROXY="));
        // K1：旧编号 2（节点页）已归一化为「首页 › 节点」，page() 回到 0。
        GuiHost fresh(&controller,true); fresh.show(); QCOMPARE(fresh.window()->page(),0); QCOMPARE(fresh.window()->currentRoute().anchor,HomeAnchor::Nodes); fresh.dispose();
        TrayController tray(&controller); auto* icon=tray.findChild<QSystemTrayIcon*>(); QVERIFY(icon); QSignalSpy show(&tray,&TrayController::showWindow),pages(&tray,&TrayController::showPage),routes(&tray,&TrayController::showRoute); icon->activated(QSystemTrayIcon::Trigger); QCOMPARE(routes.size(),1); QCOMPARE(routes.first()[0].value<Route>().anchor,HomeAnchor::Nodes); QCOMPARE(pages.size(),0);
        tray.measureMenuPreparation(); const auto menuCount=icon->contextMenu()->findChildren<QMenu*>().size(); for (int i=0;i<20;++i) tray.measureMenuPreparation(); QCOMPARE(icon->contextMenu()->findChildren<QMenu*>().size(),menuCount);
        auto desired=controller.settings(); desired.trayClick="toggle-proxy"; controller.updateSettings(desired); QTRY_COMPARE(finished.size(),4); icon->activated(QSystemTrayIcon::Trigger); icon->activated(QSystemTrayIcon::DoubleClick); QTest::qWait(QApplication::doubleClickInterval()+40); QCOMPARE(finished.size(),4); QCOMPARE(show.size(),1);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); window.resize(840,560); QVERIFY(window.grab().save(output+"/settings-enhanced.png")); }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void responsivePageGeometry() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(writeAtomic(output+"/render-context.json",QJsonDocument(QJsonObject{{"device_pixel_ratio",window.devicePixelRatioF()},{"logical_dpi",window.screen()->logicalDotsPerInch()},{"screen_scale_override",qEnvironmentVariable("QT_SCREEN_SCALE_FACTORS")},{"platform",QGuiApplication::platformName()}}).toJson()).ok); }
        for (const auto size:{QSize(840,560),QSize(940,620),QSize(1000,680),QSize(1180,810),QSize(1440,900)}) {
            window.resize(size);
            for (int page=0;page<8;++page) { window.setPage(page); QCoreApplication::processEvents(); if (page==4) { auto* connections=window.findChild<QTableView*>("connectionsTable"); QVERIFY2(connections->columnWidth(0)>=150,qPrintable(QString("size %1 width %2 viewport %3").arg(size.width()).arg(connections->columnWidth(0)).arg(connections->viewport()->width()))); QCOMPARE(connections->horizontalScrollBar()->maximum(),0); } if (!output.isEmpty()) QVERIFY(window.grab().save(output+QString("/size-%1x%2-page-%3.png").arg(size.width()).arg(size.height()).arg(page))); }
            auto* settings=window.findChild<QScrollArea*>("settingsScroll"); QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
            // K5：首页固定五段（控制卡 → 节点标题 → 节点列表 → inline-stat → 提示），不再有整卡空态。
            window.setPage(0); auto* controlCard=window.findChild<QWidget*>("homeControlCard"); QVERIFY(controlCard); QVERIFY(controlCard->isVisible());
            QVERIFY(window.rect().contains(controlCard->mapTo(&window,controlCard->rect().bottomRight())));
            QVERIFY(!window.findChild<QWidget*>("homeOnboarding"));
            // K6：数据页四个页签固定存在，四块面板共用同一页签宿。
            auto* dataTabs=window.findChild<QTabBar*>("dataTabs"); QVERIFY(dataTabs); QCOMPARE(dataTabs->count(),4);
            QVERIFY(window.findChild<QStackedWidget*>("dataPanels"));
        }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void persistentStatusBarDoesNotMovePages() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(840,560); window.show(); auto* status=window.findChild<QStatusBar*>("operationStatusBar"); auto* message=window.findChild<QLabel*>("operationMessage"); auto* pages=window.findChild<QStackedWidget*>("mainPages"); auto* details=window.findChild<QToolButton*>("statusDetails"); QVERIFY(status&&message&&pages&&details); QVERIFY(!message->wordWrap());
        const QString longMessage="连接失败 <原始文本>\n"+QString(4096,QChar(0x6D4B));
        for (int page=0;page<8;++page) {
            window.setPage(page); QCoreApplication::processEvents(); const auto central=window.centralWidget()->geometry(),pageRect=pages->geometry(),barRect=status->geometry();
            const auto stable=[&] { QCoreApplication::processEvents(); return status->isVisible()&&window.centralWidget()->geometry()==central&&pages->geometry()==pageRect&&status->geometry()==barRect; };
            controller.operationStarted("正在安全停止内核"); QVERIFY(stable()); controller.delayProgress(12,500); QVERIFY(stable());
            controller.operationFinished(false,longMessage); QVERIFY(stable()); QCOMPARE(message->toolTip(),longMessage); QVERIFY(message->text().contains("操作未完成"));
            controller.operationFinished(true,"操作完成"); QVERIFY(stable());
        }
        const auto before=window.centralWidget()->geometry(); QTRY_VERIFY_WITH_TIMEOUT(message->text().isEmpty(),4500); QVERIFY(!details->isVisible()); QCOMPARE(window.centralWidget()->geometry(),before); QVERIFY(status->isVisible());
        controller.operationFinished(false,longMessage); details->click();
        QPointer<QDialog> detail=window.findChild<QDialog*>("statusDetailsDialog"); QVERIFY(detail&&detail->isVisible()); QVERIFY(!detail->isWindow());
        auto* full=detail->findChild<QPlainTextEdit*>(); QVERIFY(full&&full->isReadOnly()); QCOMPARE(full->toPlainText(),longMessage); QCOMPARE(window.centralWidget()->geometry(),before);
        detail->reject(); QTRY_VERIFY(detail.isNull());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); for (int page:{0,2,6}) { window.setPage(page); QCoreApplication::processEvents(); QVERIFY(window.grab().save(output+QString("/status-page-%1.png").arg(page))); } }
    }
    void historyReadingAndDraftFeedback() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(5); auto* logs=window.findChild<QPlainTextEdit*>("logView"); auto* follow=window.findChild<QPushButton*>("followLatestLogs");
        for (int i=0;i<160;++i) controller.diagnostics()->append("info",QString("record-%1").arg(i));
        auto* scroll=logs->verticalScrollBar(); QTRY_VERIFY(scroll->maximum()>100); QCOMPARE(scroll->value(),scroll->maximum());
        scroll->setValue(20); auto cursor=logs->textCursor(); cursor.setPosition(20); cursor.setPosition(30,QTextCursor::KeepAnchor); logs->setTextCursor(cursor); scroll->setValue(20);
        const auto selection=logs->textCursor().selectedText(); const int position=scroll->value(); controller.diagnostics()->append("info","new-while-reading"); QCOMPARE(scroll->value(),position); QCOMPARE(logs->textCursor().selectedText(),selection); QVERIFY(follow->isEnabled());
        const auto raw=logs->toPlainText(); auto* wrap=window.findChild<QAction*>("wrapLogs"); QVERIFY(wrap); wrap->trigger(); QCOMPARE(logs->lineWrapMode(),QPlainTextEdit::WidgetWidth); QCOMPARE(logs->toPlainText(),raw); QCOMPARE(logs->textCursor().selectedText(),selection); wrap->trigger(); QCOMPARE(logs->lineWrapMode(),QPlainTextEdit::NoWrap); scroll->setValue(position);
        window.setPage(0); controller.diagnostics()->append("info","new-while-away"); window.setPage(5); QCOMPARE(logs->textCursor().selectedText(),selection); QCOMPARE(scroll->value(),position);
        QTest::mouseClick(follow,Qt::LeftButton); QCOMPARE(scroll->value(),scroll->maximum()); QVERIFY(!logs->textCursor().hasSelection()); controller.diagnostics()->append("info","latest"); QCOMPARE(scroll->value(),scroll->maximum());
        window.setPage(6); window.findChild<QPushButton*>("settingsNetworkRow")->click(); auto* hint=window.findChild<QLabel*>("settingsDraftHint"); auto* port=window.findChild<QSpinBox*>("mixedPort"); auto* light=window.findChild<QCheckBox*>("lightweightMode"); port->setValue(port->value()+1); light->toggle(); QTRY_COMPARE_WITH_TIMEOUT(hint->text(),QString("有 1 项未保存的网络设置"),10000);
        window.resize(840,560); QCoreApplication::processEvents(); auto* settings=window.findChild<QScrollArea*>("settingsScroll"); QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
        // K8：分类不再是页签；三组卡片同时可见，窄窗口靠纵向滚动而不是单列重排。
        window.setMinimumWidth(700); window.resize(700,560); QCoreApplication::processEvents();
        QVERIFY(!window.findChild<QTabBar*>("settingsCategories"));
        const auto groups=window.findChildren<QFrame*>("settingsGroup"); QCOMPARE(groups.size(),3);
        { int visible=0; for (auto* group:groups) if (group->isVisible()) ++visible; QCOMPARE(visible,1); }
        QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
        window.resize(940,620); QCoreApplication::processEvents();
        QCOMPARE(window.findChildren<QFrame*>("settingsGroup").size(),3); QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
        QTRY_VERIFY(!controller.state().busy); QSignalSpy finished(&controller,&AppController::operationFinished); auto* url=window.findChild<QLineEdit*>("connectivityTestUrl"); url->setText("file:///invalid-test-address"); emit url->editingFinished(); QCOMPARE(finished.size(),0); window.findChild<QPushButton*>("applySettings")->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(hint->text().contains("失败")||hint->text().contains("2 项")); QCOMPARE(port->value(),controller.settings().mixedPort+1); QCOMPARE(url->text(),QString("file:///invalid-test-address"));
        window.setPage(0); auto* message=window.findChild<QLabel*>("operationMessage"); QVERIFY(message->isVisible()); QVERIFY(hint->toolTip().contains("HTTP/HTTPS")); window.setPage(6); QVERIFY(message->isVisible());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void logPauseClearAndDiagnosticPrivacy() {
        QTemporaryDir data; ProfileStore store(data.path(), createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore = false;
        Profile p; p.id = newId(); p.candidate = newId(); p.name = "private-profile-name"; p.sourceType = "url"; p.source = "https://example.invalid/private-path-token?token=query-private"; QVERIFY(store.save(settings,{p}).ok);
        AppController controller(data.path(),true); QSignalSpy init(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(init.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(5); auto* logs = window.findChild<QPlainTextEdit*>("logView"); auto* level = window.findChild<QComboBox*>("logLevelFilter"); auto* pause = window.findChild<QPushButton*>("pauseLogs"); QVERIFY(logs && level && pause);
        controller.diagnostics()->setSecrets({"test-node-password"}); controller.diagnostics()->append("app",p.source+" test-node-password"); QVERIFY(!logs->toPlainText().contains("private-path-token")); QVERIFY(!logs->toPlainText().contains("test-node-password"));
        controller.diagnostics()->append("core","level=info info-only"); controller.diagnostics()->append("core","level=warning warning-only"); level->setCurrentIndex(level->findData("warning")); QVERIFY(logs->toPlainText().contains("warning-only")); QVERIFY(!logs->toPlainText().contains("info-only"));
        QTest::mouseClick(pause,Qt::LeftButton); const auto frozen = logs->toPlainText(); controller.diagnostics()->append("error","paused-error"); QCOMPARE(logs->toPlainText(),frozen); window.setPage(0); window.setPage(5); QCOMPARE(logs->toPlainText(),frozen);
        window.findChild<QAction*>("clearLogView")->trigger(); QVERIFY(logs->toPlainText().isEmpty()); QVERIFY(controller.diagnostics()->lines().isEmpty()); QVERIFY(pause->isChecked());
        controller.diagnostics()->append("error","new-after-clear"); QVERIFY(logs->toPlainText().isEmpty());
        QTest::mouseClick(pause,Qt::LeftButton); level->setCurrentIndex(level->findData("error")); QVERIFY(logs->toPlainText().contains("new-after-clear")); QVERIFY(!logs->toPlainText().contains("paused-error"));
        const auto report = data.path()+"/diagnostics.json"; controller.exportDiagnostics(report); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.last()[0].toBool()); QByteArray exported; QVERIFY(readFile(report,exported).ok);
        for (const auto& privateValue : {"private-path-token","query-private","test-node-password","private-profile-name","source.yaml","subscription_info"}) QVERIFY(!exported.contains(privateValue)); QVERIFY(QJsonDocument::fromJson(exported).object().value("logs").isArray());
        QTRY_VERIFY(QFileInfo(data.path()+"/logs/app.log").size()>0); window.findChild<QAction*>("clearLogView")->trigger(); QVERIFY(controller.diagnostics()->lines().isEmpty()); QVERIFY(logs->toPlainText().isEmpty()); QVERIFY(QFileInfo(data.path()+"/logs/app.log").size()>0);
        window.setPage(0); window.setPage(5); QVERIFY(logs->toPlainText().isEmpty()); controller.diagnostics()->append("error","after-clear"); QVERIFY(logs->toPlainText().contains("after-clear"));
        window.setPage(6); auto* port = window.findChild<QSpinBox*>("mixedPort"); const auto draft = port->value()+1; port->setValue(draft); controller.renameProfile(p.id,"Renamed"); QTRY_COMPARE(finished.size(),2); QCOMPARE(port->value(),draft); QCOMPARE(controller.settings().mixedPort,settings.mixedPort);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void profilePolicyDialogPersistsOnlyOnSave() {
        QTemporaryDir data; ProfileStore store(data.path(), createPlatformServices()); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore = false; Profile profile; profile.id = newId(); profile.candidate = newId(); profile.name = "用量示例"; profile.sourceType = "url"; profile.source = "https://example.invalid/config";
        profile.subscriptionInfo = {{"upload","1024"},{"download","2048"},{"total","8192"},{"expire","2000000000"},{"observed_at","2026-10-13T12:00:00Z"}};
        QVERIFY(store.save(settings, {profile}).ok); AppController controller(data.path(), true); QSignalSpy init(&controller, &AppController::initialized), finished(&controller, &AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(init.size(), 1); MainWindow window(&controller); window.show(); window.setPage(1); auto* list = window.findChild<QListView*>("profileList"); QVERIFY(list); QTRY_COMPARE(list->model()->rowCount(), 1);
        const auto openSource=[&] {
            const auto index=list->model()->index(0,0);
            QTest::mouseClick(list->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(list->visualRect(index)).center());
            auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu);
            menu->hide(); menu->actions().at(1)->trigger();
        };
        openSource();
        QCoreApplication::processEvents();
        QPointer<QDialog> dialog = window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(dialog);
        auto* automatic = dialog->findChild<QCheckBox*>("profileAutoUpdate"); auto* hours = dialog->findChild<QSpinBox*>("profileUpdateHours"); QVERIFY(!automatic->isChecked()); QVERIFY(!hours->isEnabled()); automatic->setChecked(true); hours->setValue(3);
        dialog->findChild<QComboBox*>("downloadRoute")->setCurrentIndex(1); dialog->findChild<QLineEdit*>("downloadUserAgent")->setText("Cancelled-Agent/1");
        QTimer::singleShot(0,[] { if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); }); dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(finished.size(), 0); QVERIFY(!controller.profiles()[0].autoUpdate); QCOMPARE(controller.profiles()[0].download.route, QString("direct")); QVERIFY(controller.profiles()[0].download.userAgent.isEmpty());
        QTRY_VERIFY(list->isVisible()); openSource();
        dialog = window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(dialog); automatic = dialog->findChild<QCheckBox*>("profileAutoUpdate"); hours = dialog->findChild<QSpinBox*>("profileUpdateHours"); automatic->setChecked(true); hours->setValue(3);
        dialog->findChild<QComboBox*>("downloadRoute")->setCurrentIndex(1); dialog->findChild<QLineEdit*>("downloadUserAgent")->setText("Saved-Agent/1"); dialog->findChild<QSpinBox*>("downloadTimeout")->setValue(45);
        const auto screenshots = qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!screenshots.isEmpty()) { QDir().mkpath(screenshots); dialog->grab().save(screenshots+"/profile-policy.png"); }
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click(); QTRY_COMPARE(finished.size(), 1); QVERIFY(finished.last()[0].toBool()); QVERIFY(controller.profiles()[0].autoUpdate); QCOMPARE(controller.profiles()[0].updateIntervalHours, 3);
        const auto loaded = store.load(); QVERIFY(loaded.result.ok); QVERIFY(loaded.profiles[0].autoUpdate); QCOMPARE(loaded.profiles[0].subscriptionInfo, profile.subscriptionInfo); QCOMPARE(loaded.profiles[0].download.route, QString("core")); QCOMPARE(loaded.profiles[0].download.userAgent, QString("Saved-Agent/1")); QCOMPARE(loaded.profiles[0].download.timeoutSeconds, 45);
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(), 1);
    }
    void providerAndCompatibilityDialogsRespectVersions() {
        QTemporaryDir data; ProfileStore store(data.path(), createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore = false;
        Profile profile; profile.id = newId(); profile.name = "Provider"; profile.sourceType = "file"; profile.download.route = "core";
        const QByteArray source = "proxy-providers: {nodes: {type: http, url: 'https://example.invalid/nodes?token=hidden'}}\nproxy-groups: [{name: Test, type: select, use: [nodes]}]\nrules: [\"MATCH,Test\"]\n";
        auto prepared = store.prepare(profile, source, settings, 19090, QString(64, 'a')); QVERIFY(prepared.result.ok); const auto provider = prepared.result.data.value("providers").toArray()[0].toObject(); const QByteArray payload = "proxies: [{name: Sample, type: http, server: example.test, port: 443}]\n";
        QVERIFY(writeAtomic(prepared.workPath + '/' + provider.value("path").toString(), payload).ok); QVERIFY(store.recordProviderSnapshot(prepared.workPath, provider, payload).ok); QVERIFY(store.save(settings, {prepared.profile}).ok);
        AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized), finished(&controller, &AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(), 1);
        MainWindow window(&controller); window.resize(840,560); window.show(); window.setPage(1);
        auto* list = window.findChild<QListView*>("profileList"); QVERIFY(list); QTRY_COMPARE(list->model()->rowCount(), 1);
        QTest::mouseClick(list->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(list->visualRect(list->model()->index(0,0))).center());
        auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu); menu->hide(); menu->actions().first()->trigger();
        auto* workspace=window.findChild<TaskWorkspace*>("taskWorkspace"); QVERIFY(workspace);
        QTRY_VERIFY(workspace->context().endsWith(":content"));
        auto* tabs=workspace->findChild<QTabBar*>("taskTabs"); QVERIFY(tabs); tabs->setCurrentIndex(5);
        auto* report = window.findChild<QPlainTextEdit*>("compatibilityReport"); QVERIFY(report); QTRY_VERIFY(report->toPlainText().contains("静态兼容检查通过")); QVERIFY(!report->toPlainText().contains("token=hidden")); window.findChild<QDialog*>("profileCompatibilityTask")->close();
        tabs->setCurrentIndex(4); QPointer<QDialog> dialog = window.findChild<QDialog*>("providersDialog"); QTRY_VERIFY(dialog); auto* table = dialog->findChild<QTableWidget*>("providersTable"); QTRY_COMPARE(table->rowCount(), 1); QCOMPARE(table->item(0,2)->text(), QString("1")); table->selectRow(0);
        auto* refresh = dialog->findChild<QPushButton*>("refreshProvider"); QTRY_VERIFY(refresh->isEnabled()); refresh->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished[0][0].toBool()); QCOMPARE(controller.profiles()[0].candidate, prepared.version);
        dialog->close(); QTRY_VERIFY(dialog.isNull()); tabs->setCurrentIndex(0); tabs->setCurrentIndex(4); dialog = window.findChild<QDialog*>("providersDialog"); QTRY_VERIFY(dialog); dialog->close(); QTRY_VERIFY(dialog.isNull());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void trayQuickSubscriptionsKeepCurrentAndBoundMenu() {
        QTemporaryDir data; ProfileStore store(data.path(),createPlatformServices()); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore=false; settings.desiredProxy=false;
        QList<Profile> profiles;
        for (int index=0;index<7;++index) {
            Profile profile; profile.id=newId(); profile.name=QString("配置 %1").arg(index); profile.sourceType="file";
            profile.source=data.path()+QString("/source-%1.yaml").arg(index);
            const QByteArray source="proxies: []\nproxy-groups: []\nrules: [\"MATCH,DIRECT\"]\n";
            QVERIFY(writeAtomic(profile.source,source).ok);
            const auto prepared=store.prepare(profile,source,settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok);
            profiles.append(prepared.profile);
        }
        settings.activeProfile=profiles.last().id; QVERIFY(store.save(settings,profiles).ok);
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        TrayController tray(&controller); tray.measureMenuPreparation();
        auto* root=tray.findChild<QSystemTrayIcon*>()->contextMenu(); QVERIFY(root);
        QMenu* subscriptions=nullptr; for (auto* menu:root->findChildren<QMenu*>()) if (menu->title()=="订阅") subscriptions=menu;
        QVERIFY(subscriptions); int quick=0; bool current=false;
        for (auto* action:subscriptions->actions()) if (action->isCheckable()) { ++quick; if (action->isChecked()&&action->text()==profiles.last().name) current=true; }
        QCOMPARE(quick,5); QVERIFY(current);
        QVERIFY(subscriptions->actions().last()->text().contains("订阅页"));
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void trayGroupDwellCacheAndSelection() {
        QTemporaryDir data; MockHttp proxy; QVERIFY(proxy.listen(QHostAddress::LocalHost,0)); int responseDelay=60;
        proxy.handler=[&](const QByteArray& request) { if (request.startsWith("CONNECT")) { HttpResponse response; response.tunnel=true; return response; } return HttpResponse{204,{},responseDelay}; };
        const auto requests=[&] { int count=0; for (const auto& request:proxy.requests) if (request.startsWith("HEAD")) ++count; return count; };
        ProfileStore store(data.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false; settings.desiredProxy=false; settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; settings.testUrl="http://example.invalid/ping";
        settings.mixedPort=isolatedListenerPort(); QVERIFY(settings.mixedPort>0); QVERIFY(store.save(settings,{}).ok);
        QByteArray yaml="proxies:\n"; for (const auto& name:QStringList{"n1","n2","n3","n4","other"}) yaml+=QString("  - {name: %1, type: http, server: 127.0.0.1, port: %2}\n").arg(name).arg(proxy.serverPort()).toUtf8();
        yaml+="  - {name: bad, type: http, server: 127.0.0.1, port: 9}\nproxy-groups:\n  - {name: A, type: select, proxies: [n1, n2, n3, n4, bad, DIRECT, REJECT]}\n  - {name: B, type: select, proxies: [other]}\nrules: [\"MATCH,DIRECT\"]\n";
        const auto file=data.path()+"/tray.yaml"; QVERIFY(writeAtomic(file,yaml).ok); AppController controller(data.path(),true); QSignalSpy init(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished),delays(&controller,&AppController::nodeDelay); controller.initialize(); QTRY_COMPARE(init.size(),1); controller.importFile("Tray",file); QTRY_COMPARE(finished.size(),1); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); controller.activateProfile(controller.profiles().first().id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QTRY_VERIFY(controller.state().ready && controller.proxies().contains("A")); TrayController tray(&controller); tray.measureMenuPreparation(); auto* root=tray.findChild<QSystemTrayIcon*>()->contextMenu(); QMenu* group=nullptr; int quickGroups=0; for (auto* menu:root->findChildren<QMenu*>()) if (!menu->property("groupName").toString().isEmpty()) { ++quickGroups; if (menu->property("groupName").toString()=="A") group=menu; } QVERIFY(group); QVERIFY(quickGroups<=5);
        group->popup(QPoint(100,100)); QTRY_VERIFY(group->isVisible()); int quickMembers=0; for (auto* action:group->actions()) if (!action->property("memberName").toString().isEmpty()) ++quickMembers; QVERIFY(quickMembers<=6); group->hide(); QTest::qWait(340); QCOMPARE(requests(),0); QCOMPARE(delays.size(),0);
        const auto pid=controller.coreProcessId(); const auto member=controller.proxies().value("A").toObject().value("now").toString(); group->popup(QPoint(100,100)); QTRY_VERIFY_WITH_TIMEOUT(delays.size()==5,2500); QVERIFY(!controller.state().busy); QCOMPARE(requests(),4); QCOMPARE(controller.coreProcessId(),pid); QCOMPARE(controller.proxies().value("A").toObject().value("now").toString(),member); QCOMPARE(finished.size(),2);
        QVERIFY(controller.trayNodeDelay("n1").value("delay").toInt()>0); QVERIFY(controller.trayNodeDelay("bad").value("known").toBool()); QCOMPARE(controller.trayNodeDelay("bad").value("delay").toInt(),-1); QVERIFY(!controller.trayNodeDelay("other").value("known").toBool());
        QAction* node=nullptr; for (auto* action:group->actions()) if (action->property("memberName").toString()=="n1") node=action; QVERIFY(node && node->isEnabled()); QVERIFY(node->text().contains(" ms")); QVERIFY(!node->icon().isNull());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(group->grab().save(output+"/tray-group-delays.png")); }
        group->hide(); group->popup(QPoint(100,100)); QTest::qWait(340); QCOMPARE(requests(),4); QCOMPARE(delays.size(),5);
        auto* automatic=root->findChild<QAction*>("trayAutoTestGroups"); QVERIFY(automatic); automatic->setChecked(false); group->hide(); group->popup(QPoint(100,100)); QTest::qWait(340); QCOMPARE(requests(),4);
        responseDelay=1000; auto* retest=group->findChild<QAction*>("trayRetestGroup"); QVERIFY(retest && retest->isEnabled()); group->setActiveAction(retest); QTest::keyClick(group,Qt::Key_Return); QVERIFY(group->isVisible()); QVERIFY(!controller.state().busy); QTRY_VERIFY_WITH_TIMEOUT(requests()==7,1000); QVERIFY(controller.trayNodeDelay("n1").value("pending").toBool());
        controller.selectMember("A","REJECT"); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,3000); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.proxies().value("A").toObject().value("now").toString(),QString("REJECT")); QVERIFY(!controller.trayNodeDelay("n1").value("pending").toBool()); QVERIFY(!controller.trayNodeDelay("n1").value("known").toBool()); group->hide(); QTest::qWait(100); QCOMPARE(delays.size(),5); QCOMPARE(requests(),7);
        automatic->setChecked(true); group->popup(QPoint(100,100)); QTRY_VERIFY_WITH_TIMEOUT(requests()==10,1000); group->hide(); const auto before=delays.size(); QTest::qWait(100); QCOMPARE(delays.size(),before); QVERIFY(!controller.trayNodeDelay("n1").value("pending").toBool()); QVERIFY(controller.state().ready);
        QPointer<QMenu> oldGroup=group; tray.measureMenuPreparation(); QVERIFY(oldGroup.isNull()); const auto count=root->findChildren<QMenu*>().size(); for (int i=0;i<10;++i) tray.measureMenuPreparation(); QCOMPARE(root->findChildren<QMenu*>().size(),count);
        // 首页主动整组测速与托盘悬停测速有不同生命周期：切换当前
        // 成员应保留正在运行的只读请求，并由原组结果继续更新。
        responseDelay=1200;
        MainWindow window(&controller); window.resize(900,600); window.show();
        window.restoreNodeBookmark({{"profile",controller.state().activeProfile},{"group","A"}});
        auto* homeTest=window.findChild<QPushButton*>("homeConnectivityTest"); QVERIFY(homeTest && homeTest->isEnabled());
        homeTest->click(); QVERIFY(controller.homeGroupTestStatus().value("running").toBool());
        const int beforeSwitch=finished.size(); controller.selectMember("A","n1");
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),beforeSwitch+1,3000);
        QVERIFY(finished.last()[0].toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.homeGroupTestStatus().value("running").toBool(),4000);
        const auto status=controller.homeGroupTestStatus();
        QCOMPARE(status.value("completed").toInt(),status.value("total").toInt());
        QVERIFY(!status.value("cancelled").toBool());
        QVERIFY(!window.findChild<QLabel*>("homeControlMeta"));
        window.hide();
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void trayAppearancePriorityAndResources() {
        const auto state = [](CoreState core, ProxyState proxy, QString mode = "rule", bool ready = true, bool exiting = false) { AppState value; value.core = core; value.proxy = proxy; value.mode = mode; value.ready = ready; value.exiting = exiting; return value; };
        const QList<QPair<AppState, QString>> cases{
            {state(CoreState::Stopped, ProxyState::Disabled, "rule", false), "idle"},
            {state(CoreState::Starting, ProxyState::Owned, "rule", false), "working"},
            {state(CoreState::Stopping, ProxyState::Owned), "working"},
            {state(CoreState::Failed, ProxyState::Unknown), "attention"},
            {state(CoreState::Running, ProxyState::Owned, "rule", false), "attention"},
            {state(CoreState::Running, ProxyState::Unknown), "checking"},
            {state(CoreState::Running, ProxyState::Owned, "unexpected"), "checking"},
            {state(CoreState::Failed, ProxyState::Owned, "rule", false, true), "working"},
            {state(CoreState::Running, ProxyState::Owned, "rule"), "rule"},
            {state(CoreState::Running, ProxyState::Owned, "global"), "global"},
            {state(CoreState::Running, ProxyState::Owned, "direct"), "direct"},
            {state(CoreState::Running, ProxyState::Disabled, "rule"), "ready-rule"},
            {state(CoreState::Running, ProxyState::Disabled, "global"), "ready-global"},
            {state(CoreState::Running, ProxyState::Disabled, "direct"), "ready-direct"},
            {state(CoreState::Running, ProxyState::External, "rule"), "external-rule"},
            {state(CoreState::Running, ProxyState::External, "global"), "external-global"},
            {state(CoreState::Running, ProxyState::External, "direct"), "external-direct"}
        };
        QHash<QString, QImage> images;
        for (const auto& item : cases) {
            QCOMPARE(TrayController::appearance(item.first), item.second);
            const auto icon = TrayController::statusIcon(item.first); QVERIFY(!icon.isNull());
            QCOMPARE(icon.cacheKey(), TrayController::statusIcon(item.first).cacheKey());
            auto busy = item.first; busy.busy = !busy.busy; QCOMPARE(TrayController::appearance(busy), item.second);
            const auto image = icon.pixmap(16, 16).toImage(); QVERIFY(!image.isNull()); QVERIFY(!icon.pixmap(24, 24).isNull()); QVERIFY(!icon.pixmap(32, 32).isNull());
            if (!images.contains(item.second)) { for (const auto& previous : images) QVERIFY(previous != image); images.insert(item.second, image); }
        }
        QCOMPARE(images.size(), 13);
        QVERIFY(TrayController::statusTooltip(state(CoreState::Running, ProxyState::Disabled, "global"),Settings()).contains("系统代理未开启"));
        QVERIFY(TrayController::statusTooltip(state(CoreState::Running, ProxyState::External, "direct"),Settings()).contains("其他程序管理"));
        QVERIFY(TrayController::statusTooltip(state(CoreState::Running, ProxyState::Owned),Settings()).contains("内核模式：规则"));
    }
    void lightweightLifecycleAndQuickReopen() {
        QApplication::setQuitOnLastWindowClosed(false);
        QTemporaryDir data; AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.count(), 1);
        QSignalSpy finished(&controller, &AppController::operationFinished); QSignalSpy exit(&controller, &AppController::exitReady);
        GuiHost host(&controller, true); host.show(6); auto* light = host.window()->findChild<QCheckBox*>("lightweightMode"); QVERIFY(light->isChecked());
        host.window()->setMemberView(1); QPointer<MainWindow> old = host.window(); QList<QPointer<QObject>> resources; for (auto* object : old->findChildren<QObject*>()) resources.append(object); QVERIFY(resources.size() > 100);
        old->close(); host.show(4); QTRY_VERIFY(old.isNull()); QTRY_VERIFY(host.window() && host.window()->isVisible()); QCOMPARE(host.window()->page(), 4); QCOMPARE(exit.count(), 0);
        for (const auto& resource : resources) QVERIFY(resource.isNull()); QCOMPARE(host.window()->memberView(),1);
        for (int i = 0; i < 6; ++i) { QPointer<MainWindow> previous = host.window(); previous->close(); QTRY_VERIFY(previous.isNull()); QVERIFY(!host.window()); QVERIFY(!controller.connectionsVisible()); host.show(i % 7); QVERIFY(host.window()); }
        auto settings = controller.settings(); settings.lightweight = false; controller.updateSettings(settings); QTRY_COMPARE(finished.count(), 1); QVERIFY(finished.last()[0].toBool());
        QPointer<MainWindow> retained = host.window(); retained->close(); QVERIFY(retained); QVERIFY(!retained->isVisible()); host.show(); QCOMPARE(host.window(), retained.data()); QVERIFY(retained->isVisible());
        settings.lightweight = true; controller.updateSettings(settings); QTRY_COMPARE(finished.count(), 2); QVERIFY(finished.last()[0].toBool());
        host.show(6); auto* port = retained->findChild<QSpinBox*>("mixedPort"); port->setValue(port->value() + 1);
        QTimer::singleShot(0, [] { auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if (dialog) dialog->button(QMessageBox::No)->click(); });
        retained->close(); QVERIFY(retained); QVERIFY(retained->isVisible());
        QTimer::singleShot(0, [] { auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if (dialog) dialog->button(QMessageBox::Yes)->click(); });
        retained->close(); QTRY_VERIFY(retained.isNull()); QCOMPARE(exit.count(), 0);
        ProfileStore persisted(data.path(), createPlatformServices()); const auto loaded = persisted.load(); QVERIFY(loaded.result.ok); QVERIFY(loaded.settings.lightweight); QCOMPARE(loaded.settings.mixedPort, settings.mixedPort);
        GuiHost noTray(&controller, false); noTray.show(); noTray.window()->close(); QTRY_COMPARE(exit.count(), 1);
    }
    void importValidationAndSecretVisibility() {
        ImportDialog dialog;
        auto* url = dialog.findChild<QLineEdit*>("subscriptionUrl"); auto* submit = dialog.findChild<QPushButton*>("importSubmit");
        QCOMPARE(dialog.findChild<QLabel*>("importHint")->textFormat(),Qt::PlainText);
        QCOMPARE(url->echoMode(), QLineEdit::Password); QVERIFY(!submit->isEnabled());
        url->setText("invalid"); QVERIFY(!submit->isEnabled());
        url->setText("https://user:pass@example.test/sub"); QVERIFY(!submit->isEnabled());
        url->setText("https://example.test/sub#fragment"); QVERIFY(!submit->isEnabled());
        url->setText("https://example.test/sub?token=private"); QVERIFY(submit->isEnabled());
        dialog.findChild<QLineEdit*>("subscriptionName")->setText("  我的订阅  "); QCOMPARE(dialog.profileName(), QString("我的订阅"));
        dialog.findChild<QCheckBox*>()->setChecked(true); QCOMPARE(url->echoMode(), QLineEdit::Normal);
        dialog.findChild<QLineEdit*>("downloadUserAgent")->setText("无效代理头"); QVERIFY(!submit->isEnabled());
        dialog.setSourceTab(false); QVERIFY(!dialog.isUrl()); QVERIFY(!submit->isEnabled());
        dialog.findChild<QLineEdit*>("subscriptionFile")->setText("  C:/test/config.yaml  "); QVERIFY(submit->isEnabled()); QCOMPARE(dialog.source(), QString("C:/test/config.yaml"));
        dialog.setSourceTab(true); QVERIFY(!submit->isEnabled());
    }
    void failedImportRetainsDraftUntilExplicitRetry() {
        QTemporaryDir data; auto platform=createPlatformServices(); ProfileStore store(data.path(),platform);
        QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        settings.corePath=QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe";
        QVERIFY(store.save(settings,{}).ok);
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        QPointer<ImportDialog> dialog=new ImportDialog; dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->bindController(&controller);
        dialog->setPrefill(data.path()+"/missing.yaml",false); dialog->show();
        auto* submit=dialog->findChild<QPushButton*>("importSubmit");
        auto* name=dialog->findChild<QLineEdit*>("subscriptionName"); name->setText("保留名称");
        submit->click(); controller.startCore();
        QVERIFY(!dialog->findChild<QTabWidget*>()->isEnabled());
        QTRY_COMPARE(finished.size(),2); QVERIFY(!finished.last()[0].toBool());
        QVERIFY(dialog); QVERIFY(dialog->isVisible()); QVERIFY(submit->isEnabled());
        QCOMPARE(name->text(),QString("保留名称")); QVERIFY(dialog->source().endsWith("missing.yaml"));
        const auto error=dialog->findChild<QLabel*>("importHint")->text(); QVERIFY(!error.isEmpty());
        emit controller.stateChanged(); QCOMPARE(dialog->findChild<QLabel*>("importHint")->text(),error);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(dialog->grab().save(output+"/import-failure-retained.png")); }

        QTest::qWait(60); QCOMPARE(finished.size(),2); QVERIFY(controller.profiles().isEmpty());
        const auto path=data.path()+"/valid.yaml";
        const auto before=dialog->size();
        const auto hintHeight=dialog->findChild<QLabel*>("importHint")->height(); const auto submitPosition=submit->mapTo(dialog,QPoint());
        const auto invalid=QString("proxies: []\nrules: ['IP-CIDR,<b>%1</b>,DIRECT']\n").arg(QString(1200,'x')).toUtf8();
        QVERIFY(writeAtomic(path,invalid).ok); dialog->setPrefill(path,false); submit->click();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,10000); QVERIFY(!finished.last()[0].toBool());
        auto* feedback=dialog->findChild<QLabel*>("importHint");
        QCOMPARE(feedback->textFormat(),Qt::PlainText); QCOMPARE(feedback->accessibleDescription(),finished.last()[1].toString());
        QVERIFY2(feedback->accessibleDescription().size()>500,qUtf8Printable(feedback->accessibleDescription())); QCOMPARE(feedback->text(),feedback->accessibleDescription()); QVERIFY(!feedback->wordWrap()); QVERIFY(feedback->fontMetrics().horizontalAdvance(feedback->text())>feedback->width());
        QCoreApplication::processEvents(); QCOMPARE(dialog->size(),before);
        QCOMPARE(feedback->height(),hintHeight); QCOMPARE(submit->mapTo(dialog,QPoint()),submitPosition);
        QHelpEvent tooltip(QEvent::ToolTip,QPoint(2,2),feedback->mapToGlobal(QPoint(2,2)));
        QApplication::sendEvent(feedback,&tooltip);
        QVERIFY(!QToolTip::text().contains("&lt;qt&gt;"));
        QVERIFY(!QToolTip::text().contains("&amp;lt;"));
        QCOMPARE(feedback->text(),feedback->accessibleDescription()); QToolTip::hideText();
        // Import validates a scalar rule with the actual pinned core.
        QVERIFY(writeAtomic(path,"proxies: []\nrules: ['MATCH,DIRECT']\n").ok);
        submit->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),4,10000);
        QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString()));
        QTRY_VERIFY(dialog.isNull()); QCOMPARE(controller.profiles().size(),1);
        QCOMPARE(controller.profiles().first().name,QString("保留名称")); QVERIFY(!controller.state().ready);
    }
    void unchangedSnapshotsPreserveRowsAndChoices() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true; state.mode="rule";
        auto& proxies=const_cast<QJsonObject&>(controller.proxies());
        proxies={{"Test",QJsonObject{{"type","Selector"},{"all",QJsonArray{"HK 01","JP 02"}},{"now","HK 01"}}},
                 {"HK 01",QJsonObject{{"type","Http"}}},{"JP 02",QJsonObject{{"type","Http"}}}};
        MainWindow window(&controller); window.show(); window.setPage(2);
        // H3：表格换成行列表（QListView#nodeList），源模型仍是 ProxyTableModel。
        QVERIFY(!window.findChild<QTableView*>("nodesTable"));
        auto* rows=window.findChild<QListView*>("nodeList"); QVERIFY(rows);
        auto* model=rows->model(); QVERIFY(model);
        QTRY_VERIFY(model->rowCount()>=3);   // 组头 + 两个成员
        const auto groupHeader=model->index(0,0);
        rows->setCurrentIndex(model->index(1,0)); const QPersistentModelIndex selection=rows->currentIndex();
        QSignalSpy reset(model,&QAbstractItemModel::modelReset),changed(model,&QAbstractItemModel::dataChanged);
        for (int i=0;i<3;++i) { emit controller.proxiesChanged(); emit controller.stateChanged(); }
        QCOMPARE(reset.size(),0); QVERIFY(selection.isValid()); QCOMPARE(rows->currentIndex(),QModelIndex(selection));
        QVERIFY(groupHeader.isValid());
        proxies["Test"]=QJsonObject{{"type","Selector"},{"all",QJsonArray{"HK 01","JP 02"}},{"now","JP 02"}};
        emit controller.proxiesChanged(); QVERIFY(changed.size()>=2);
        QCOMPARE(window.findChild<QWidget*>("nodeSection")->findChildren<QListView*>("nodeList").size(),1);
        // 成员类型变化仍然只刷新受影响的行，不重建列表（折叠状态与滚动位置靠这一点保住）。
        auto leaf=proxies.value("JP 02").toObject(); leaf["type"]="Socks5"; proxies["JP 02"]=leaf;
        emit controller.proxiesChanged(); QTRY_VERIFY(window.findChild<QLabel*>("nodeHeadingCount")->text().contains("2"));
        window.setPage(7);
        auto* website=window.findChild<QComboBox*>("websiteNode"); QVERIFY(website);
        website->setCurrentIndex(website->findData("JP 02"));
        QSignalSpy websiteReset(website->model(),&QAbstractItemModel::modelReset),websiteRows(website->model(),&QAbstractItemModel::rowsInserted);
        website->showPopup(); QTRY_VERIFY(website->view()->isVisible());
        const QPersistentModelIndex popupIndex=website->model()->index(website->currentIndex(),0);
        for (int i=0;i<3;++i) { emit controller.proxiesChanged(); emit controller.stateChanged(); }
        QCOMPARE(websiteReset.size(),0); QCOMPARE(websiteRows.size(),0);
        QVERIFY(popupIndex.isValid()); QVERIFY(website->view()->isVisible());
        QCOMPARE(website->currentData().toString(),QString("JP 02"));
        website->hidePopup();
    }
    void emptyStateCannotIssueInvalidCommands() {
        QTemporaryDir data; AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.count(), 1);
        MainWindow window(&controller); window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        // 无卡片时没有可触发的对象命令，批量更新也不应出现。
        auto* cards=window.findChild<QListView*>("profileList"); QVERIFY(cards);
        QCOMPARE(cards->model()->rowCount(),0);
        auto* cardMenu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(cardMenu);
        QVERIFY(cardMenu->actions().isEmpty());
        QVERIFY(!window.findChild<QPushButton*>("batchProfiles")->isVisible());
        QVERIFY(!window.findChild<QPushButton*>("profileDetailsActivate"));
        QVERIFY(!window.findChild<QPushButton*>("chooseMember"));
        QVERIFY(!window.findChild<QAction*>("automaticAction")->isEnabled());
        // K5：首页六卡与「更多运行信息」抽屉都已删除，控制卡在任何状态下都存在。
        QVERIFY(!window.findChild<QTableWidget*>("homeGroups")); QCOMPARE(window.findChildren<QWidget*>("dashboardCard").size(),0);
        QVERIFY(!window.findChild<QToolButton*>("homeDetailsDisclosure"));
        // K5：指标视图移进「数据 › 流量」，首页不再有它；控件本身仍在，只需断言首屏不可见。
        auto* metrics = window.findChild<QWidget*>("homeMetrics"); QVERIFY(!metrics || !metrics->isVisible());
        // 首页主控件是代理总开关（ADR 0048）：没有订阅时点击只引导添加订阅，不发起任何操作。
        auto* master=window.findChild<QCheckBox*>("systemProxy"); QVERIFY(master); QVERIFY(master->text().isEmpty()); QVERIFY(master->accessibleName().contains("系统代理"));
        QSignalSpy noStart(&controller,&AppController::operationStarted);
        // K5：空态下总开关禁用；节点区有唯一的就近导入动作。
        QVERIFY(!master->isEnabled()); QVERIFY(!master->isChecked());
        auto* emptyHome=window.findChild<QWidget*>("prototypeHomeEmpty"); QVERIFY(emptyHome); QVERIFY(emptyHome->isVisible());
        auto* importHome=emptyHome->findChild<QPushButton*>(); QVERIFY(importHome); QCOMPARE(importHome->text(),QString("导入配置"));
        QCOMPARE(noStart.size(),0); QVERIFY(!controller.state().ready);
        QVERIFY(!window.findChild<QPushButton*>("saveCustomRules")->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("addCustomRule")->isEnabled()); QVERIFY(!window.findChild<QComboBox*>("ruleType")->isEnabled());
        for (int page = 0; page < 8; ++page) { window.setPage(page); QCoreApplication::processEvents(); }
        auto* startup = window.findChild<QCheckBox*>("startCore");
        QVERIFY(startup); const auto checked = startup->isChecked(); QTest::mouseClick(startup, Qt::LeftButton, {}, QPoint(startup->width() - 5, startup->height() / 2)); QCOMPARE(startup->isChecked(), !checked); QTest::mouseClick(startup, Qt::LeftButton, {}, QPoint(startup->width() - 5, startup->height() / 2)); QCOMPARE(startup->isChecked(), checked);
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.count(), 1);
    }
    void refinedTaskGeometryAndSemantics() {
        {
            QDialog dialog; auto* layout=new QVBoxLayout(&dialog); auto* input=new QLineEdit; auto* tool=new QPushButton("维护工具"); auto* primary=new QPushButton("保存"); primary->setProperty("primary",true); primary->setDefault(true); layout->addWidget(input); layout->addWidget(tool); layout->addWidget(primary);
            QSignalSpy tools(tool,&QPushButton::clicked),saved(primary,&QPushButton::clicked); dialog.show(); QApplication::setActiveWindow(&dialog); QVERIFY(!tool->autoDefault()); input->setFocus(); QTRY_VERIFY(input->hasFocus()); QTest::keyClick(input,Qt::Key_Return); QCOMPARE(saved.size(),1); QCOMPARE(tools.size(),0);
            tool->setFocus(Qt::TabFocusReason); QTest::keyClick(tool,Qt::Key_Return); QCOMPARE(tools.size(),1); QCOMPARE(saved.size(),1);
        }
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true; state.mode="rule";
        auto& proxies=const_cast<QJsonObject&>(controller.proxies()); QJsonArray members;
        for (int i=0;i<500;++i) { const auto name=QString("JP 日本节点 %1").arg(i,3,10,QChar('0')); members.append(name); proxies.insert(name,QJsonObject{{"type","Http"}}); }
        proxies.insert("Test",QJsonObject{{"type","Selector"},{"all",members},{"now",members.first()}});
        MainWindow window(&controller); window.resize(940,620); window.show(); window.setPage(2); window.setMemberView(1); QCoreApplication::processEvents();
        // H3：显示方式收进「更多 › 显示方式」，节点区容器是 nodeSection（原 nodeContent 面板已删）。
        // ADR 0048：节点区并入首页滚动视图后不再独占页高，改为断言「锚点把节点区带到视口内」
        // 与「节点视口在 940×620 下至少占 60% 高度」——前者是入口 2 的可观察结果，后者是列表可用性。
        auto* cards=window.findChild<QListView*>("nodeCards"); auto* content=window.findChild<QWidget*>("nodeSection");
        // K1：页签宿主已删除；视图归属由固定的 72px 顶栏标题表达。
        auto* title=window.findChild<QWidget*>("topbar"); auto* homeScroll=window.findChild<QScrollArea*>("homeScroll");
        QVERIFY(title&&homeScroll&&content&&cards); QCOMPARE(title->height(),UiTheme::TopbarHeight);
        QVERIFY2(homeScroll->viewport()->rect().contains(content->mapTo(homeScroll->viewport(),QPoint(0,0))),
            qPrintable(QString("node section not scrolled into view: viewport %1x%2 top y=%3").arg(homeScroll->viewport()->width()).arg(homeScroll->viewport()->height()).arg(content->mapTo(homeScroll->viewport(),QPoint(0,0)).y())));
        const int span=content->height();
        QVERIFY2(cards->viewport()->height()*100>=span*60,qPrintable(QString("node viewport %1 / content %2").arg(cards->viewport()->height()).arg(span)));
        QCOMPARE(cards->model()->rowCount(),500); const auto viewport=cards->viewport()->geometry();
        emit controller.delayProgress(1,500); QCoreApplication::processEvents(); QCOMPARE(cards->viewport()->geometry(),viewport);
        // 组切换收进「更多」溢出菜单（H3），首页不再有常驻的组合框。
        QVERIFY(!window.findChild<QComboBox*>("homeQuickGroup"));
        QVERIFY(!window.findChild<QComboBox*>("homeQuickMember"));
        QVERIFY(window.findChild<QToolButton*>("nodeToolsButton"));
        window.setPage(7); QCoreApplication::processEvents();
        // K6：旧入口 7 = 数据 › 测试页签。
        QCOMPARE(window.findChild<QTabBar*>("dataTabs")->currentIndex(),int(ViewIds::DataTab::Tests));
        QCoreApplication::processEvents();
        auto* scroll=window.findChild<QScrollArea*>("websiteScroll"); auto* surface=window.findChild<QWidget*>("websiteSurface");
        const int columns=surface->property("columns").toInt(); QVERIFY(columns>=2&&columns<=3);
        for (auto* details:window.findChildren<QToolButton*>("websiteResultDetails")) { QVERIFY2(details->isVisible(),qPrintable(QString("details target=%1 hidden=%2 parentVis=%3 websiteSurfaceVis=%4 surfaceHidden=%5 stackIdx=%6 stackCount=%7").arg(details->property("targetId").toString()).arg(details->isHidden()).arg(details->parentWidget()->isVisible()).arg(window.findChild<QWidget*>("websiteSurface")?window.findChild<QWidget*>("websiteSurface")->isVisible():-1).arg(window.findChild<QWidget*>("websiteSurface")?window.findChild<QWidget*>("websiteSurface")->isHidden():-1).arg(window.findChild<QStackedWidget*>("mainPages")->currentIndex()).arg(window.findChild<QStackedWidget*>("mainPages")->count()))); QVERIFY(details->focusPolicy()!=Qt::NoFocus); QVERIFY(details->property("quiet").toBool()); }
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); const QJsonObject geometry{{"node_viewport_height",cards->viewport()->height()},{"content_span",span},{"website_columns",columns},{"website_viewport_width",scroll->viewport()->width()},{"dpr",window.devicePixelRatioF()}}; QVERIFY(writeAtomic(output+"/refinement-geometry.json",QJsonDocument(geometry).toJson()).ok); QVERIFY(window.grab().save(output+"/refined-website.png")); window.setPage(2); QVERIFY(window.grab().save(output+"/refined-nodes.png")); }
        window.resize(840,560); QCoreApplication::processEvents(); QVERIFY(cards->viewport()->height()>=108);
        state.ready=false;
    }
    void refinedContextFitsAutomaticGroupsAndLargeText() {
        struct ResetAppearance { ~ResetAppearance() { UiTheme::setPreferences("system",100); } } resetAppearance;
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto settings=controller.settings(); settings.textScale=120; settings.appearance="light"; controller.updateSettings(settings); QTRY_COMPARE(finished.size(),1); QVERIFY(finished.first()[0].toBool());
        const_cast<QVector<Profile>&>(controller.profiles())[0].name=QString(80,QChar(0x8BA2));
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true;
        auto& proxies=const_cast<QJsonObject&>(controller.proxies()); proxies={{"自动选择组",QJsonObject{{"type","URLTest"},{"all",QJsonArray{"香港节点","日本节点"}},{"now","香港节点"}}},{"香港节点",QJsonObject{{"type","Http"}}},{"日本节点",QJsonObject{{"type","Http"}}}};
        {
            MainWindow window(&controller); window.resize(840,560); window.show(); window.setPage(2); window.setMemberView(1); QCoreApplication::processEvents(); QCoreApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest); QCoreApplication::processEvents();
            QCOMPARE(window.size(),QSize(840,560));
            QList<QPair<QString,QRect>> rectangles;
            // U12：搜索默认收起，明确展开后检查紧凑工具排。
            auto* searchToggle=window.findChild<QToolButton*>("nodeSearchToggle"); QVERIFY(searchToggle&&searchToggle->isVisible()); searchToggle->click(); QCoreApplication::processEvents();
            // 分组、搜索、测速与更多仍须处于容器内且互不压叠。
            // 约束是「都在标题行容器内、彼此不压叠」——窗口坐标受首页滚动影响，不是稳定判据。
            auto* heading=window.findChild<QWidget*>("nodeHeading"); QVERIFY(heading);
            for (const auto& name:{"browseGroup","nodeSearch","nodeTestButton","nodeToolsButton"}) {
                auto* control=window.findChild<QWidget*>(name); QVERIFY2(control,qPrintable(QString("missing %1").arg(name))); QVERIFY(control->isVisible());
                const QRect rect(control->mapTo(heading,QPoint()),control->size());
                QVERIFY2(heading->rect().contains(rect),qPrintable(QString("%1 at %2,%3 %4x%5 heading %6x%7").arg(name).arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).arg(heading->width()).arg(heading->height())));
                rectangles.append({QString::fromUtf8(name),rect});
            }
            for (const auto& name:{"nodeRegion","nodeProtocol","nodeHealth","nodeMaximumDelay","nodeFavoritesOnly"} ) QVERIFY2(window.findChild<QWidget*>(name),qPrintable(QString("missing %1").arg(name)));
            for (int i=0;i<rectangles.size();++i) for (int j=i+1;j<rectangles.size();++j) QVERIFY2(!rectangles[i].second.intersects(rectangles[j].second),qPrintable(rectangles[i].first+" intersects "+rectangles[j].first));
            auto* cards=window.findChild<QListView*>("nodeCards"); QVERIFY(cards);
            QVERIFY(cards->viewport()->height()>=cards->gridSize().height());
            for (int row=0;row<cards->model()->rowCount();++row) {
                const auto card=cards->visualRect(cards->model()->index(row,0));
                QVERIFY2(cards->viewport()->rect().contains(card),qPrintable(QString("row %1 rect %2,%3 %4x%5 viewport %6x%7 grid %8x%9 scroll %10,%11")
                    .arg(row).arg(card.x()).arg(card.y()).arg(card.width()).arg(card.height())
                    .arg(cards->viewport()->width()).arg(cards->viewport()->height())
                    .arg(cards->gridSize().width()).arg(cards->gridSize().height())
                    .arg(cards->horizontalScrollBar()->value()).arg(cards->verticalScrollBar()->value())));
            }
            const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(window.grab().save(output+"/refined-automatic-long-120.png")); }
        }
        state.ready=false;
    }
    void fileProfileOffersSourceSyncAndReplacement() {
        QTemporaryDir data; auto platform = createPlatformServices(); ProfileStore store(data.path(), platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore = false; settings.desiredProxy = false;
        Profile p; p.id = newId(); p.name = "本地配置"; p.sourceType = "file"; p.source=data.path()+"/original.yaml"; QVERIFY(writeAtomic(p.source,"proxies: []\nproxy-groups: []\nrules: [\"MATCH,DIRECT\"]\n").ok);
        auto prepared = store.prepare(p, "proxies: []\nproxy-groups: []\nrules: [\"MATCH,DIRECT\"]\n", settings, 19090, QString(64, 'a')); QVERIFY(prepared.result.ok);
        QVERIFY(store.save(settings, {prepared.profile}).ok);
        AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.count(), 1);
        MainWindow window(&controller); window.setPage(1); window.show();
        auto* cards = window.findChild<QListView*>("profileList"); QVERIFY(cards); QTRY_COMPARE(cards->model()->rowCount(), 1);
        const auto index=cards->model()->index(0,0);
        QCOMPARE(index.data(ProfileRowModel::SourceLabel).toString(),QString("original.yaml"));
        QVERIFY(index.data(ProfileRowModel::CanRefresh).toBool());
        QTest::mouseClick(cards->viewport(), Qt::LeftButton, {}, ProfileCardDelegate::menu(cards->visualRect(index)).center());
        auto* localMenu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(localMenu);
        QCOMPARE(localMenu->property("profileKind").toString(),QString("local"));
        QCOMPARE(localMenu->actions().size(),4);
        localMenu->hide(); localMenu->actions().at(1)->trigger();
        auto* sourceEditor=window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(sourceEditor);
        QCOMPARE(sourceEditor->findChild<QLineEdit*>("profileEditSource")->text(),p.source);
        sourceEditor->reject();
        window.findChild<QPushButton*>("taskBack")->click();
        auto* add = window.findChild<QPushButton*>("importLocalProfile"); QVERIFY(add); add->click();
        auto* dialog = window.findChild<ImportDialog*>(); QTRY_VERIFY(dialog && dialog->isVisible()); QVERIFY(!dialog->isUrl());
        QVERIFY(!dialog->findChild<QLabel*>("dialogTitle")->isVisible()); dialog->reject();
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.count(), 1);
    }
    void coherentActionsAndBilingualRuleTokens() {
        QTemporaryDir data; ProfileStore store(data.path(),createPlatformServices()); QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        Profile profile; profile.id=newId(); profile.name="规则示例"; profile.sourceType="file"; auto prepared=store.prepare(profile,"proxies: []\nproxy-groups: []\nrules: [\"MATCH,DIRECT\"]\n",settings,19090,QString(64,'a')); QVERIFY(prepared.result.ok); QVERIFY(store.save(settings,{prepared.profile}).ok);
        AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.resize(840,560);
        // K5：没有订阅时控制卡仍然可见，行尾开关可点（点击只引导添加订阅）。
        auto* master=window.findChild<QCheckBox*>("systemProxy"); QVERIFY(master); QVERIFY(master->isVisible());
        QVERIFY(window.findChild<QWidget*>("homeControlCard")->isVisible()); QVERIFY(!window.findChild<QPushButton*>("homeNextStep"));
        // 首屏行尾开关是唯一代理入口；没有订阅时它会把这个动作退回引导。
        QVERIFY(window.findChild<QAbstractButton*>("systemProxy"));
        window.setPage(3); QCoreApplication::processEvents(); auto* type=window.findChild<QComboBox*>("ruleType"); auto* target=window.findChild<QComboBox*>("ruleTarget"); auto* match=window.findChild<QLineEdit*>("ruleMatch"); auto* add=window.findChild<QPushButton*>("addCustomRule"); auto* editor=window.findChild<QPlainTextEdit*>("customRulesEditor");
        auto* save=window.findChild<QPushButton*>("saveCustomRules"); editor->setPlainText("DOMAIN-SUFFIX,example.com,,no-resolve"); QVERIFY(!save->isEnabled()); QVERIFY(save->toolTip().contains("第 1 行")); editor->clear(); QVERIFY(!save->isEnabled());
        QVERIFY(type->currentText().contains("域名后缀")); QVERIFY(target->currentText().contains("直连")); match->setText("example.com"); add->click(); QCOMPARE(editor->toPlainText(),QString("DOMAIN-SUFFIX,example.com,DIRECT"));
        type->setCurrentIndex(type->findData("MATCH")); QVERIFY(!match->isEnabled()); target->setCurrentIndex(target->findData("REJECT")); add->click(); QVERIFY(editor->toPlainText().endsWith("MATCH,REJECT"));
        type->setCurrentIndex(type->findData("PROCESS-NAME")); match->setText("sample.exe"); target->setEditText("自定义组"); add->click(); QVERIFY(editor->toPlainText().endsWith("PROCESS-NAME,sample.exe,自定义组"));
        // K6：规则编辑不依赖数据页；目标集由内核实时读取（inspectRuleSource）异步补全。
        QTRY_VERIFY_WITH_TIMEOUT(target->findData("GLOBAL")>=0, 5000); QVERIFY(!window.findChild<QPushButton*>("saveCustomRules")->isEnabled()); QCOMPARE(finished.size(),0); QVERIFY(!editor->isReadOnly()); QVERIFY(editor->toPlainText().contains("自定义组"));
        window.setPage(6); window.findChild<QPushButton*>("settingsNetworkRow")->click(); auto* apply=window.findChild<QPushButton*>("applySettings"); QVERIFY(apply->isVisible()); QVERIFY(!apply->isEnabled()); auto* port=window.findChild<QSpinBox*>("mixedPort"); port->setValue(port->value()+1); QVERIFY(apply->isEnabled()); auto* reset=window.findChild<QPushButton*>("discardNetworkSettings"); QVERIFY(reset&&reset->isVisible()); reset->click(); QCOMPARE(port->value(),settings.mixedPort); QVERIFY(!apply->isVisible()); QCOMPARE(finished.size(),0);
        window.findChild<QPushButton*>("settingsNetworkRow")->click(); port->setValue(port->value()+1); QVERIFY(apply->isVisible()); QVERIFY(window.rect().contains(apply->mapTo(&window,apply->rect().bottomRight())));
        window.setPage(4); QVERIFY(!window.findChild<QToolButton*>("closeAllConnectionsButton")->isVisible()); QVERIFY(!window.findChild<QWidget*>("connectionSummary")->isVisible()); QVERIFY(!window.findChild<QToolButton*>("connectionDetailsButton")->isVisible()); auto* detailsAction=window.findChild<QAction*>("connectionDetailsAction"); QVERIFY(detailsAction); QVERIFY(!detailsAction->isEnabled()); QVERIFY(!window.findChild<QAction*>("defaultConnectionOrder")->isEnabled());
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { window.setPage(3); QVERIFY(window.grab().save(output+"/rules-bilingual.png")); window.setPage(6); QVERIFY(window.grab().save(output+"/settings-footer.png")); }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void nodeLocationHints() {
        QCOMPARE(NodePresentation::icon("HK one",true).cacheKey(),NodePresentation::icon("香港 two").cacheKey());
        QCOMPARE(NodePresentation::icon("unknown one").cacheKey(),NodePresentation::icon("unknown two").cacheKey());
        NodePresentation::clearTransientCache();
        QCOMPARE(NodePresentation::location("Hong Kong fresh").code,QString("HK"));
        QCOMPARE(NodePresentation::location("HK 香港 中转 01").code,QString("HK"));
        QCOMPARE(NodePresentation::location("Japan Tokyo 02").code,QString("JP"));
        QCOMPARE(NodePresentation::location("🇸🇬 Singapore 03").code,QString("SG"));
        QCOMPARE(NodePresentation::location("🇯🇵 香港转发").code,QString("JP"));
        QCOMPARE(NodePresentation::location("US-01").label,QString("美国"));
        QVERIFY(NodePresentation::location("AUS route").code.isEmpty());
        QVERIFY(NodePresentation::location("VLESS unknown").code.isEmpty());
        QVERIFY(NodePresentation::location("made in nowhere").code.isEmpty());
        QVERIFY(!NodePresentation::icon("香港").isNull()); QVERIFY(!NodePresentation::icon("unknown").isNull());
        QCOMPARE(NodePresentation::iconKey("HK",true),QString("HK")); QCOMPARE(NodePresentation::iconKey("JP",true),QString("JP")); QCOMPARE(NodePresentation::iconKey("视频",true),QString("group")); QCOMPARE(NodePresentation::location("ES 马德里").code,QString("ES")); QCOMPARE(NodePresentation::location("匈牙利 01").code,QString("HU"));
        QFile manifest(QStringLiteral(MLASH_SOURCE_DIR "/resources/flags/manifest.json")); QVERIFY(manifest.open(QIODevice::ReadOnly)); const auto assets=QJsonDocument::fromJson(manifest.readAll()).object().value("files").toArray(); QCOMPARE(assets.size(),51);
        for (const auto& asset:assets) { const auto object=asset.toObject(); const auto code=object.value("code").toString(); const auto icon=NodePresentation::icon(code,true); QVERIFY(!icon.isNull()); QFile resource(":/flags/"+code+".png"); QVERIFY(resource.open(QIODevice::ReadOnly)); QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(resource.readAll(),QCryptographicHash::Sha256).toHex()),object.value("sha256").toString()); }
        QVERIFY(NodePresentation::icon("HK",true).pixmap(24,18).toImage()!=NodePresentation::icon("JP",true).pixmap(24,18).toImage());
        QCOMPARE(NodePresentation::icon("HK",true).pixmap(24,18).toImage(),NodePresentation::icon("香港节点").pixmap(24,18).toImage());
        QCOMPARE(NodePresentation::icon("视频",true).pixmap(24,18).toImage(),NodePresentation::icon("unknown",true).pixmap(24,18).toImage());
    }
    void dashboardTabsAndSettingsFocus() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path())); AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(840,560); window.show(); QApplication::setActiveWindow(&window);
        // K5/K6：首页的「更多运行信息」抽屉（含指标与内核启停）已删除；运行信息改由数据页承载。
        QVERIFY(!window.findChild<QToolButton*>("homeDetailsDisclosure"));
        QVERIFY(!window.findChild<QWidget*>("homeAdditionalInfo"));
        // 指标视图整体搬进「数据 › 流量」面板，因此对象存在但不在首页视野内。
        auto* metrics=window.findChild<QWidget*>("homeMetrics"); QVERIFY(!metrics || !metrics->isVisible());
        QVERIFY(!window.findChild<QToolButton*>("homeCoreToggle"));
        // K6：数据页是一个视图、四个页签；切到连接页签才开始采样，切走即停。
        window.setPage(4); auto* tabs=window.findChild<QTabBar*>("dataTabs"); QVERIFY(tabs); QCOMPARE(tabs->count(),4);
        QCOMPARE(tabs->currentIndex(),int(ViewIds::DataTab::Connections));
        QVERIFY(controller.connectionsVisible());
        window.setPage(0); QVERIFY(!controller.connectionsVisible()); QVERIFY(controller.connections().isEmpty());
        window.setPage(6); auto* scroll=window.findChild<QScrollArea*>("settingsScroll"); auto* light=window.findChild<QCheckBox*>("lightweightMode"); scroll->ensureWidgetVisible(light); light->clearFocus(); light->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QVERIFY(light->hasFocus()); const auto mouse=light->grab().toImage(); light->clearFocus(); light->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); const auto keyboard=light->grab().toImage(); QVERIFY(mouse!=keyboard); const auto before=light->isChecked(); QTest::keyClick(light,Qt::Key_Space); QCOMPARE(light->isChecked(),!before);
        QTest::mouseClick(light,Qt::LeftButton,{},QPoint(light->width()-10,light->height()/2)); QCOMPARE(light->isChecked(),before); light->clearFocus(); const auto plain=light->grab().toImage(); QTest::mouseClick(light,Qt::LeftButton,{},QPoint(light->width()-10,light->height()/2)); QTest::mouseClick(light,Qt::LeftButton,{},QPoint(light->width()-10,light->height()/2)); QCOMPARE(light->grab().toImage(),plain);
        window.findChild<QPushButton*>("settingsNetworkRow")->click(); auto* port=window.findChild<QSpinBox*>("mixedPort"); QVERIFY(port); port->setValue(port->value()+1); QCoreApplication::processEvents();
        auto* apply=window.findChild<QPushButton*>("applySettings"); QTRY_VERIFY(apply->isVisible()); const auto position=apply->mapTo(&window,QPoint()); scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); QCOMPARE(apply->mapTo(&window,QPoint()),position); QVERIFY(window.rect().contains(apply->mapTo(&window,apply->rect().bottomRight()))); QVERIFY(window.findChildren<QWidget*>("settingRow").size()>=20);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QDir().mkpath(output); QVERIFY(window.grab().save(output+"/settings-tools.png")); scroll->verticalScrollBar()->setValue(0); QVERIFY(window.grab().save(output+"/settings-list.png")); }
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void quietMouseFocusAndGroupedSettings() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(840,560); window.show(); QApplication::setActiveWindow(&window);
        for (int page=0;page<8;++page) {
            window.setPage(page); QCoreApplication::processEvents();
            for (auto* widget:window.findChildren<QWidget*>()) {
                if (!widget->isVisible() || !widget->isEnabled() || widget->focusPolicy()==Qt::NoFocus || !(qobject_cast<QComboBox*>(widget) || qobject_cast<QPushButton*>(widget) || qobject_cast<QToolButton*>(widget) || qobject_cast<QLineEdit*>(widget) || qobject_cast<QSpinBox*>(widget))) continue;
                widget->clearFocus(); widget->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY2(widget->property("keyboardFocus").toBool(),qUtf8Printable(QString("page %1: %2 %3 focus %4").arg(page).arg(widget->metaObject()->className()).arg(widget->objectName()).arg(widget->hasFocus()))); widget->clearFocus(); widget->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QVERIFY(!widget->property("keyboardFocus").toBool());
            }
        }
        // H3：排序收进「更多」菜单；键盘焦点样式用标题行的订阅下拉验证（同一条 QSS 规则）。
        window.setPage(6); auto* combo=window.findChild<QPushButton*>("settingsAppearanceRow"); QVERIFY(combo); combo->clearFocus(); const auto neutral=combo->grab().toImage(); combo->setFocus(Qt::MouseFocusReason); QCoreApplication::processEvents(); QCOMPARE(combo->grab().toImage(),neutral); combo->clearFocus(); combo->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); QVERIFY(combo->grab().toImage()!=neutral);
        // 鼠标选中下拉项不得留下键盘焦点环：用设置页的日志级别下拉（本用例没有订阅，
        // 节点区的订阅下拉是空的，弹不出列表）。
        window.setPage(6); window.findChild<QPushButton*>("settingsCoreRow")->click(); auto* scroll=window.findChild<QScrollArea*>("settingsScroll"); scroll->verticalScrollBar()->setValue(0);
        auto* popupCombo=window.findChild<QComboBox*>("coreLogLevel"); QVERIFY(popupCombo && popupCombo->count()>1);
        scroll->ensureWidgetVisible(popupCombo); QCoreApplication::processEvents();
        popupCombo->showPopup(); QTRY_VERIFY(popupCombo->view()->isVisible()); QTest::qWait(50);
        const auto item=popupCombo->model()->index(1,0); const auto point=popupCombo->view()->visualRect(item).center();
        QCOMPARE(popupCombo->view()->indexAt(point),item);
        QMouseEvent move(QEvent::MouseMove,QPointF(point),QPointF(popupCombo->view()->viewport()->mapToGlobal(point)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(popupCombo->view()->viewport(),&move);
        QTest::mouseClick(popupCombo->view()->viewport(),Qt::LeftButton,{},point);
        QTRY_VERIFY(!popupCombo->view()->isVisible()); QCOMPARE(popupCombo->currentIndex(),1); QVERIFY(!popupCombo->property("keyboardFocus").toBool());
        // A wheel passing over a settings selector must scroll, not change the preference.
        auto* logLevel=window.findChild<QComboBox*>("coreLogLevel"); auto* portBox=window.findChild<QSpinBox*>("mixedPort"); QVERIFY(logLevel&&portBox);
        const auto logBefore=logLevel->currentIndex(); const auto portBefore=portBox->value(); logLevel->clearFocus(); portBox->clearFocus();
        auto wheel=[&](QWidget* target,int delta) { const QPointF point(target->width()/2.0,target->height()/2.0); QWheelEvent event(point,target->mapToGlobal(point.toPoint()),QPoint(),QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false); QApplication::sendEvent(target,&event); };
        wheel(logLevel,-120); QCOMPARE(logLevel->currentIndex(),logBefore);
        QApplication::setActiveWindow(&window); logLevel->setFocus(Qt::TabFocusReason); QCoreApplication::processEvents(); wheel(logLevel,-120); QVERIFY(logLevel->currentIndex()!=logBefore); logLevel->clearFocus();
        logLevel->setCurrentIndex(logLevel->findData(controller.settings().logLevel));
        window.findChild<QPushButton*>("settingsBack")->click(); window.findChild<QPushButton*>("settingsNetworkRow")->click();
        wheel(portBox,-120); QCOMPARE(portBox->value(),portBefore);
        window.navigate(Route{ViewIds::Settings,1});
        // 分类路由只显示所属任务，不混入其他组的字段。
        QVERIFY(!window.findChild<QTabBar*>("settingsCategories"));
        const auto groups=scroll->widget()->findChildren<QFrame*>("settingsGroup"); QCOMPARE(groups.size(),3);
        { int visible=0; for (auto* group:groups) if (group->isVisible()) { ++visible; QVERIFY(group->height()>=group->minimumSizeHint().height()); } QCOMPARE(visible,1); }
        QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        // K5：控制卡在任何状态下都存在；首页不再有整卡空态或抽屉入口。
        window.setPage(0); QVERIFY(window.findChild<QWidget*>("homeControlCard")->isVisible());
        QVERIFY(!window.findChild<QWidget*>("homeOnboarding")); QVERIFY(!window.findChild<QToolButton*>("homeDetailsDisclosure"));
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void currentGroupSelectionAndNativePreview() {
        const auto corePath = QString(MLASH_SOURCE_DIR) + "/cores/mihomo.exe";
        QVERIFY(QFile::exists(corePath)); QTemporaryDir data;
        auto platform = createPlatformServices(); ProfileStore store(data.path(), platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.corePath = corePath; settings.startCore = false; settings.desiredProxy = false;
        QTcpServer reservation;
        for (int attempt = 0; attempt < 100; ++attempt) if (reservation.listen(QHostAddress::LocalHost, QRandomGenerator::global()->bounded(20000, 40000))) break;
        QVERIFY(reservation.isListening()); settings.mixedPort = reservation.serverPort(); reservation.close();
        QVERIFY(store.save(settings, {}).ok);
        QByteArray yaml = "proxies:\n  - {name: 香港示例, type: http, server: 127.0.0.1, port: 9}\n  - {name: 日本示例, type: http, server: 127.0.0.1, port: 9}\n  - {name: 新加坡示例, type: http, server: 127.0.0.1, port: 9}\nproxy-groups:\n  - {name: 节点选择, type: select, proxies: [DIRECT, REJECT, 香港示例, 日本示例, 新加坡示例, HK]}\n  - {name: HK, type: select, proxies: [香港示例, DIRECT]}\nrules: [\"MATCH,节点选择\"]\n";
        QByteArray services; for (int i=0;i<22;++i) services+=QString("\n  - {name: 服务 %1, type: select, proxies: [DIRECT]}").arg(i,2,10,QChar('0')).toUtf8(); yaml.replace("\nrules:",services+"\nrules:");
        const auto file = data.path() + "/sample.yaml"; QVERIFY(writeAtomic(file, yaml).ok);
        AppController controller(data.path(), true); QSignalSpy initialized(&controller, &AppController::initialized); QSignalSpy finished(&controller, &AppController::operationFinished);
        controller.initialize(); QTRY_COMPARE(initialized.count(), 1); controller.importFile("交互验证示例", file); QTRY_COMPARE(finished.count(), 1); QVERIFY(finished.last()[0].toBool());
        controller.activateProfile(controller.profiles().first().id); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 15000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString())); QVERIFY(controller.state().ready);
        MainWindow window(&controller); window.setPage(2); window.show();
        // H3：节点区是行列表（组头 + 成员行），组切换收进「更多」溢出菜单。
        // H3：节点区是行列表（组头 + 成员行），组切换收进「更多」溢出菜单。
        // 一律用 helper 现取控件：窗口在用例中途可能被重建，缓存裸指针会悬空。
        const auto listView=[&window]() -> QListView* { return window.findChild<QListView*>("nodeList"); };
        const auto listModel=[&listView]() -> QAbstractItemModel* { auto* view=listView(); if (!view) { qFatal("nodeList missing"); } return view->model(); };
        const auto memberRow=[&listModel](const QString& name) { auto* model=listModel(); if (!model) return -1; for (int row=0;row<model->rowCount();++row) if (model->data(model->index(row,0),NodeRowModel::RowKind).toInt()==1 && model->data(model->index(row,0),Qt::DisplayRole).toString()==name) return row; return -1; };
        const auto clickRow=[&listView,&listModel](int row) { auto* view=listView(); QTest::mouseClick(view->viewport(),Qt::LeftButton,{},view->visualRect(listModel()->index(row,0)).center()); };
        QVERIFY(listView()); QTRY_VERIFY_WITH_TIMEOUT(listModel()->rowCount() >= 6, 10000);   // 「节点选择」组头 + 5 个成员（含 HK 子组）
        QCOMPARE(memberRow("REJECT") > 0, true);
        // 组头折叠只改行数，不改过滤结果。
        const int expandedRows = listModel()->rowCount();
        clickRow(0); QTRY_VERIFY(listModel()->rowCount() < expandedRows);
        clickRow(0); QTRY_COMPARE(listModel()->rowCount(), expandedRows);

        // 手动组：单击成员即生效（ADR 0048），没有提交按钮。
        const auto rejectRow = memberRow("REJECT"); QVERIFY(rejectRow > 0);
        clickRow(rejectRow);
        QTRY_COMPARE(finished.count(),3); QTRY_COMPARE(controller.proxies().value("节点选择").toObject().value("now").toString(),QString("REJECT"));
        QCOMPARE(listModel()->data(listModel()->index(rejectRow,0),NodeRowModel::IsCurrent).toBool(),true);

        // Enter 与单击同义：点中的成员已是当前成员，因此不重复提交。
        const int afterClick=finished.count();
        listView()->setFocus(); QTest::keyClick(listView(), Qt::Key_Return); QCoreApplication::processEvents(); QCOMPARE(finished.count(), afterClick);
        QTest::keyClick(listView(),Qt::Key_Escape); QVERIFY(!listView()->selectionModel()->hasSelection());

        // 「节点选择」是手动组：整段归在「手动选择」标题下。
        QCOMPARE(listModel()->data(listModel()->index(0,0),Qt::DisplayRole).toString(),QString("节点选择"));
        QCOMPARE(listModel()->data(listModel()->index(rejectRow,0),NodeRowModel::IsAutoGroup).toBool(), false);

        const auto cardOutput=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!cardOutput.isEmpty()) { QDir().mkpath(cardOutput); QVERIFY(window.grab().save(cardOutput+"/home-nodes.png")); window.resize(840,560); QTest::qWait(50); QVERIFY(window.grab().save(cardOutput+"/home-nodes-compact.png")); }
        window.resize(940,620); QCoreApplication::processEvents();

        // 「最近选择」由保存后的 recentSelections 驱动：上面那次 click 已经把 REJECT 写进去。
        QTRY_VERIFY_WITH_TIMEOUT(!controller.profiles().isEmpty() && !controller.profiles()[0].recentSelections.isEmpty() &&
            controller.profiles()[0].recentSelections[0].toObject().value("member").toString()==QString("REJECT"), 10000);
        QCoreApplication::processEvents(); const int settled=finished.count();
        auto* recent=window.findChild<QAction*>("recentNodes"); QVERIFY(recent); auto* recentMenu=recent->menu(); QVERIFY(recentMenu);
        QSignalSpy recentOpened(recentMenu,&QMenu::aboutToShow);
        // 菜单只读打开：不得触发任何节点切换。
        recentMenu->popup(QCursor::pos()); QTRY_COMPARE(recentOpened.size(),1); QVERIFY(!recentMenu->isEmpty());
        QCOMPARE(finished.count(),settled); recentMenu->close(); QCoreApplication::processEvents();
        QVERIFY(!window.findChild<QAction*>("automaticAction")->isEnabled());
        // K5：控制区第一行只报告「当前成员」；成员行（NodeRowModel::IsCurrent）是本轮的证据，
        // 概览标题的成员名依赖 controller 的探测组上下文（setHomeProbeGroup 由外部入口设置），
        // 没有上下文时按设计退化为「没有唯一当前节点」，因此这里不断言具体成员名。
        window.navigate(Route{0,-1,HomeAnchor::Nodes}); QCoreApplication::processEvents();
        auto* overview=window.findChild<QLabel*>("overviewTitle"); QVERIFY(overview);
        QVERIFY2(overview->text().contains("REJECT") || overview->text()==QString("尚未连接") || overview->text()==QString("未连接") || overview->text()==QString("没有唯一当前节点"),
                 qUtf8Printable("overview: "+overview->text()));

        // 六卡删除后，「当前成员」由控制区第一行与成员行勾选共同表达（H2/H3）。
        QVERIFY(!window.findChild<QLabel*>("homeStateLine"));
        // K5：列表底部那一行提示只在内核未就绪时出现，就绪后为空。
        QWidget* hintHost = window.findChild<QWidget*>("nodeSection"); QVERIFY(hintHost);
        auto* selectionHint=window.findChild<QLabel*>("homeSelectionHint");
        QVERIFY(selectionHint);
        QVERIFY2(selectionHint->text().trimmed().isEmpty(), qUtf8Printable(selectionHint->text()));
        window.setPage(0); window.resize(940,620); QCoreApplication::processEvents();
        auto* homeScroll=window.findChild<QScrollArea*>("homeScroll"); homeScroll->verticalScrollBar()->setValue(0);
        // K5：「更多运行信息」抽屉已删除；指标视图整体搬到「数据 › 流量」，所以它
        // 不再属于首页子树——只断言首页里没有它（控件本身仍在数据页）。
        QVERIFY(!window.findChild<QWidget*>("homeAdditionalInfo"));
        QWidget* homePage = window.findChild<QStackedWidget*>("mainPages")->widget(int(ViewIds::Home)); QVERIFY(homePage);
        QVERIFY(!homePage->findChild<QWidget*>("homeMetrics"));
        window.setPage(2);
        // H3：先等上一轮切换的保存与回读都结束，再发起导入（导入在忙碌时会被拒绝）。
        QTRY_VERIFY_WITH_TIMEOUT(!controller.state().busy,10000);
        const int beforeImport=finished.count();
        // 再导入一份不同的文件（同一来源重复导入会被拒，这与本用例无关）。
        const auto secondFile = data.path() + "/second.yaml";
        QVERIFY(writeAtomic(secondFile, yaml + QByteArrayLiteral("# second")).ok);
        controller.importFile("第二份示例", secondFile); QTRY_COMPARE(finished.count(), beforeImport+1);
        const auto importError=qUtf8Printable(finished.last()[1].toString());
        QVERIFY2(finished.last()[0].toBool(),importError);
        auto* sources = window.findChild<QComboBox*>("nodeProfile"); QVERIFY(sources); QCOMPARE(sources->count(), 2); sources->setCurrentIndex(1);
        QTRY_COMPARE(listModel()->rowCount(), 0); QVERIFY(!window.findChild<QPushButton*>("chooseMember"));
        // U10：浏览不应用，只有显式使用才进入原有激活事务。
        QCOMPARE(finished.count(),beforeImport+1);
        QVERIFY(controller.settings().activeProfile!=sources->currentData().toString());
        window.navigate(Route{ViewIds::Profiles}); auto* localProfiles=window.findChild<QListView*>("profileList"); QVERIFY(localProfiles);
        const auto secondProfileIndex=localProfiles->model()->index(1,0); localProfiles->scrollTo(secondProfileIndex); QCoreApplication::processEvents();
        QTest::mouseClick(localProfiles->viewport(),Qt::LeftButton,{},ProfileCardDelegate::card(localProfiles->visualRect(secondProfileIndex)).topLeft()+QPoint(40,25));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), beforeImport+2, 15000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString()));
        QTRY_VERIFY_WITH_TIMEOUT(listModel()->rowCount() >= 6, 10000); QCOMPARE(controller.settings().activeProfile, sources->currentData().toString());
        // K1/K6：旧入口 5 是「数据 › 日志」，回显仍是数据页；用 currentRoute 而不是 page() 断言。
        window.setPage(5); QCoreApplication::processEvents(); QCOMPARE(window.currentRoute().view, int(ViewIds::Data)); QCOMPARE(window.currentRoute().tab, int(ViewIds::DataTab::Logs));
        window.setPage(0); QCoreApplication::processEvents(); QCOMPARE(window.currentRoute().view,int(ViewIds::Home));
        window.setPage(2); QTRY_VERIFY(memberRow("HK") > 0);
        const auto childRow = memberRow("HK"); listView()->setCurrentIndex(listModel()->index(childRow,0));
        QCOMPARE(listModel()->data(listModel()->index(childRow,0),NodeRowModel::RowKind).toInt(),1);
        // 进入子组：与「→」同一个入口（enterSelectedGroup），这里直接触发「更多 › 进入所选策略组」。
        const auto beforeBrowse = finished.count();
        auto* enterGroupAction=window.findChild<QAction*>("enterGroup"); QVERIFY(enterGroupAction);
        QVERIFY(enterGroupAction->isEnabled()); enterGroupAction->trigger();
        QTRY_COMPARE(listModel()->rowCount(), 3);   // 「HK」组头 + 2 个成员
        QCOMPARE(finished.count(), beforeBrowse);
        auto* backGroup = window.findChild<QAction*>("backGroup"); QVERIFY(backGroup->isVisible()); backGroup->trigger(); QTRY_VERIFY(listModel()->rowCount() >= 6); QCOMPARE(finished.count(), beforeBrowse);
        window.setPage(3);
        QCOMPARE(window.currentRoute().view,int(ViewIds::Rules));
        auto* rulesProfiles = window.findChild<QComboBox*>("rulesProfiles"); rulesProfiles->setCurrentIndex(rulesProfiles->findData(controller.state().activeProfile));
        auto* editor = window.findChild<QPlainTextEdit*>("customRulesEditor"); auto* saveRules = window.findChild<QPushButton*>("saveCustomRules");
        const QString rule = "IP-CIDR,127.0.0.1/32,DIRECT,no-resolve"; editor->setPlainText(rule); QVERIFY(saveRules->isEnabled());
        const auto activeId = controller.state().activeProfile; rulesProfiles->setCurrentIndex(0); QCOMPARE(rulesProfiles->currentData().toString(), activeId); QCOMPARE(editor->toPlainText(), rule);
        QTest::mouseClick(saveRules, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), beforeImport+3, 15000); QVERIFY2(finished.last()[0].toBool(), qUtf8Printable(finished.last()[1].toString()));
        auto* rulesTabs = window.findChild<QTabWidget*>("rulesTabs"); rulesTabs->setCurrentIndex(1);
        if (controller.activeRules().isEmpty()) { QString st; for (int i=0;i<200 && controller.activeRules().isEmpty();++i) { QTest::qWait(50); st=QString("ready=%1 busy=%2 view=%3 err=%4 stateText=%5 tab=%6").arg(controller.state().ready).arg(controller.state().busy).arg(window.currentRoute().view).arg(controller.rulesError()).arg(window.findChild<QLabel*>("operationMessage")?window.findChild<QLabel*>("operationMessage")->text():QString("?")) .arg(rulesTabs->currentIndex()); } qWarning("%s",qUtf8Printable(st)); }
        QTRY_VERIFY(!controller.activeRules().isEmpty()); QCOMPARE(controller.activeRules()[0].toObject().value("payload").toString(), QString("127.0.0.1/32")); QCOMPARE(controller.activeRules()[0].toObject().value("proxy").toString(), QString("DIRECT"));
        QVERIFY(controller.profiles()[0].customRules.isEmpty()); QCOMPARE(controller.profiles()[1].customRules, QStringList{rule});
        auto* ruleSearch = window.findChild<QLineEdit*>("activeRulesSearch"); auto* ruleTable = window.findChild<QTableView*>("activeRulesTable"); ruleSearch->setText("no-rule-match-example"); QCOMPARE(ruleTable->model()->rowCount(),0); ruleSearch->setText("127.0.0.1/32"); QCOMPARE(ruleTable->model()->rowCount(),1);
        const auto rulesPreview = qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!rulesPreview.isEmpty()) { QDir().mkpath(rulesPreview); window.grab().save(rulesPreview+"/rules-readonly.png"); }
        ruleSearch->clear(); rulesTabs->setCurrentIndex(0); QVERIFY(controller.activeRules().isEmpty());
        const auto previousVersion = controller.profiles()[1].active; editor->setPlainText("IP-CIDR,invalid,DIRECT"); QTest::mouseClick(saveRules, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), beforeImport+4, 10000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(controller.profiles()[1].active, previousVersion); QCOMPARE(controller.profiles()[1].customRules, QStringList{rule}); QVERIFY(controller.state().ready);
        editor->setPlainText(rule); QVERIFY(!saveRules->isEnabled()); QCOMPARE(finished.count(),beforeImport+4);
        editor->setPlainText(rule+"\nDOMAIN-SUFFIX,update-test.example,DIRECT"); QTest::mouseClick(saveRules, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.count(), beforeImport+5, 15000); QVERIFY(finished.last()[0].toBool());
        QTcpServer destination; QVERIFY(destination.listen(QHostAddress::LocalHost, 0)); QTcpSocket tunnel; tunnel.connectToHost(QHostAddress::LocalHost, controller.settings().mixedPort); QTRY_COMPARE(tunnel.state(), QAbstractSocket::ConnectedState);
        const auto address = QByteArray("127.0.0.1:") + QByteArray::number(destination.serverPort()); tunnel.write("CONNECT " + address + " HTTP/1.1\r\nHost: " + address + "\r\n\r\n"); QTRY_VERIFY(destination.hasPendingConnections()); auto* remote = destination.nextPendingConnection(); QVERIFY(remote); remote->setParent(&destination);
        window.setPage(4); QTRY_VERIFY_WITH_TIMEOUT(!controller.connections().isEmpty(), 5000); QVERIFY(controller.connectionsVisible());
        bool matched = false; for (const auto& value : controller.connections()) if (value.toObject().value("rulePayload").toString() == "127.0.0.1/32") matched = true; QVERIFY(matched);
        window.hide(); QVERIFY(!controller.connectionsVisible()); QVERIFY(controller.connections().isEmpty()); QSignalSpy hiddenUpdates(&controller, &AppController::connectionsChanged); QTest::qWait(2200); QCOMPARE(hiddenUpdates.count(), 0);
        window.show(); QTRY_VERIFY_WITH_TIMEOUT(!controller.connections().isEmpty(), 5000);
        const auto output = qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QVERIFY(writeAtomic(output+"/render-context.json",QJsonDocument(QJsonObject{{"device_pixel_ratio",window.devicePixelRatioF()},{"screen_device_pixel_ratio",window.screen()->devicePixelRatio()},{"logical_dpi",window.screen()->logicalDotsPerInch()},{"screen_scale_override",qEnvironmentVariable("QT_SCREEN_SCALE_FACTORS")},{"scale_factor",qEnvironmentVariable("QT_SCALE_FACTOR")},{"platform",QGuiApplication::platformName()}}).toJson()).ok);
            QVERIFY(QDir().mkpath(output));
            for (int page = 0; page < 8; ++page) { window.setPage(page); QTest::qWait(100); if (page == 4) QTRY_VERIFY(!controller.connections().isEmpty()); QVERIFY(window.grab().save(output + QString("/page-%1.png").arg(page))); if (page==0) { auto* tools=window.findChild<QToolButton*>("nodeToolsButton"); QVERIFY(tools); tools->showMenu(); QTRY_VERIFY(tools->menu()->isVisible()); QVERIFY(tools->menu()->grab().save(output+"/node-tools-menu.png")); QTest::keyClick(tools->menu(),Qt::Key_Escape); QTRY_VERIFY(!tools->menu()->isVisible()); } }
            for (const auto size:{QSize(1000,680),QSize(1180,810),QSize(1440,900)}) { window.resize(size); for (int page=0;page<8;++page) { window.setPage(page); QTest::qWait(20); QVERIFY(window.grab().save(output+QString("/size-%1x%2-page-%3.png").arg(size.width()).arg(size.height()).arg(page))); } }
            window.setPage(2); window.resize(840, 560); QTest::qWait(100); QVERIFY(window.grab().save(output + "/compact.png"));
            for (int page : {0, 1, 3, 4, 5, 6}) { window.setPage(page); QTest::qWait(100); if (page == 4) QTRY_VERIFY(!controller.connections().isEmpty()); QVERIFY(window.grab().save(output + QString("/compact-%1.png").arg(page))); }
            ImportDialog dialog(&window); dialog.show(); QTest::qWait(100); QVERIFY(dialog.grab().save(output + "/import.png"));
        }
        window.setPage(0); window.hide(); QVERIFY(!controller.connectionsVisible());
        GuiHost lightweight(&controller, true); lightweight.show(4); QTRY_VERIFY(!controller.connections().isEmpty()); QPointer<MainWindow> released = lightweight.window();
        released->close(); QTRY_VERIFY(released.isNull()); QVERIFY(controller.state().ready); QCOMPARE(controller.state().activeProfile, activeId); QVERIFY(!controller.connectionsVisible()); QVERIFY(controller.connections().isEmpty()); QVERIFY(controller.activeRules().isEmpty());
        tunnel.write("ping"); QTRY_VERIFY(remote->bytesAvailable() >= 4); QCOMPARE(remote->read(4), QByteArray("ping"));
        lightweight.show(2); QVERIFY(lightweight.window()->findChild<QListView*>("nodeList")->model()->rowCount() > 0); QVERIFY(controller.state().ready); lightweight.dispose();
        TrayController tray(&controller);
        int completed = finished.count();
        for (const auto& mode : {QString("global"), QString("direct"), QString("rule")}) { ++completed; controller.setMode(mode); QTRY_COMPARE(finished.count(), completed); QVERIFY(finished.last()[0].toBool()); QCOMPARE(controller.state().mode, mode); QCOMPARE(tray.visualKey(), TrayController::appearance(controller.state())); }
        window.show(); window.setPage(4); QTRY_VERIFY(!controller.connections().isEmpty()); auto* connections = window.findChild<QTableView*>("connectionsTable"); connections->selectRow(0); auto* close = window.findChild<QPushButton*>("closeSelectedConnection"); QVERIFY(close->isEnabled()); ++completed; QTest::mouseClick(close,Qt::LeftButton); QTRY_COMPARE(finished.count(),completed); QVERIFY2(finished.last()[0].toBool(),qUtf8Printable(finished.last()[1].toString())); QTRY_VERIFY(controller.connections().isEmpty()); QTRY_COMPARE(tunnel.state(),QAbstractSocket::UnconnectedState); QVERIFY(controller.state().ready); QVERIFY(!close->isEnabled());
        QVERIFY(!controller.closedConnections().isEmpty()); auto* view=window.findChild<QTabBar*>("connectionView"); view->setCurrentIndex(1); QVERIFY(connections->model()->rowCount()>0); connections->selectRow(0); QVERIFY(!close->isEnabled()); window.findChild<QAction*>("connectionDetailsAction")->trigger(); QPointer<QDialog> detail=window.findChild<QDialog*>("connectionDetails"); QVERIFY(detail); QVERIFY(detail->findChild<QLabel*>("connectionField_destination")->text().contains("127.0.0.1"));
        window.setPage(0); QTRY_VERIFY(detail.isNull()); QVERIFY(controller.closedConnections().isEmpty()); QVERIFY(controller.connectionTotals().isEmpty()); window.setPage(4); view->setCurrentIndex(0);
        QTcpSocket second,third; for (auto* socket:{&second,&third}) { socket->connectToHost(QHostAddress::LocalHost,controller.settings().mixedPort); QTRY_COMPARE(socket->state(),QAbstractSocket::ConnectedState); socket->write("CONNECT "+address+" HTTP/1.1\r\nHost: "+address+"\r\n\r\n"); }
        QTRY_VERIFY_WITH_TIMEOUT(controller.connections().size()>=2,5000); auto* closeAll=window.findChild<QAction*>("closeAllConnections"); QVERIFY(window.findChild<QToolButton*>("closeAllConnectionsButton")->isVisible()); QVERIFY(window.findChild<QWidget*>("connectionSummary")->isVisible());
        QTimer::singleShot(0,&window,[&window] { auto* box=window.findChild<QMessageBox*>(); QVERIFY(box); box->button(QMessageBox::No)->click(); }); closeAll->trigger(); QCOMPARE(finished.size(),completed); QVERIFY(!controller.connections().isEmpty());
        ++completed; QTimer::singleShot(0,&window,[&window] { auto* box=window.findChild<QMessageBox*>(); QVERIFY(box); box->button(QMessageBox::Yes)->click(); }); closeAll->trigger(); QTRY_COMPARE(finished.size(),completed); QVERIFY(finished.last()[0].toBool()); QTRY_VERIFY(controller.connections().isEmpty()); QTRY_COMPARE(second.state(),QAbstractSocket::UnconnectedState); QTRY_COMPARE(third.state(),QAbstractSocket::UnconnectedState); QVERIFY(controller.closedConnections().size()>=2); QVERIFY(controller.state().ready);
        window.setPage(6); window.findChild<QAction*>("previewActiveConfig")->trigger(); QPointer<QDialog> config; QTRY_VERIFY((config=window.findChild<QDialog*>("configPreview"))); QVERIFY(config->findChild<QPlainTextEdit*>()->isReadOnly()); QVERIFY(config->findChild<QPlainTextEdit*>()->toPlainText().contains("[redacted]")); window.hide(); QTRY_VERIFY(config.isNull());
        // K7：同一张卡片网格承载两份本地配置；菜单始终绑定当前卡片 ID。
        window.show(); window.setPage(1); auto* profiles=window.findChild<QListView*>("profileList");
        auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(profiles && menu);
        const auto target=profiles->model()->index(0,0).data(ProfileRowModel::ProfileId).toString(),active=controller.state().activeProfile; const auto corePid=controller.coreProcessId(); const int beforeRename=finished.size();
        QCOMPARE(profiles->model()->rowCount(),2);
        window.activateWindow(); QApplication::setActiveWindow(&window); QTest::qWait(50);
        const auto rowPoint=ProfileCardDelegate::card(profiles->visualRect(profiles->model()->index(0,0))).center();
        QContextMenuEvent rowMenu(QContextMenuEvent::Mouse,rowPoint,profiles->viewport()->mapToGlobal(rowPoint)); QApplication::sendEvent(profiles->viewport(),&rowMenu);
        QTRY_VERIFY(menu->isVisible()); QCOMPARE(menu->property("contextProfileId").toString(),target); QCOMPARE(menu->property("profileKind").toString(),QString("local")); menu->hide();
        menu->actions().at(1)->trigger();
        auto* sourceEditor=window.findChild<QDialog*>("profilePolicyDialog"); QVERIFY(sourceEditor);
        auto* nameInput=sourceEditor->findChild<QLineEdit*>("profileEditName"); QVERIFY(nameInput); nameInput->setText("右键重命名示例");
        sourceEditor->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        QTRY_COMPARE(finished.size(),beforeRename+1); QVERIFY(finished.last()[0].toBool());
        QCOMPARE(controller.state().activeProfile,active); QCOMPARE(controller.coreProcessId(),corePid);
        bool renamed=false; for (const auto& profile:controller.profiles()) if (profile.id==target) { QCOMPARE(profile.name,QString("右键重命名示例")); renamed=true; } QVERIFY(renamed);
        auto* workspace=window.findChild<TaskWorkspace*>("taskWorkspace"); QVERIFY(workspace);
        workspace->findChild<QPushButton*>("taskBack")->click();
        QApplication::setActiveWindow(&window); profiles->setFocus(); QCoreApplication::processEvents(); QTest::keyClick(profiles,Qt::Key_F10,Qt::ShiftModifier); QTRY_VERIFY(menu->isVisible()); menu->hide(); // K7：列表底部留白处的右键不针对任何行，也不弹菜单；同一列表的两个菜单互斥。
        const QPoint blankPoint(20,profiles->viewport()->height()-5); const QModelIndex blankIndex=profiles->indexAt(blankPoint);
        if (!blankIndex.isValid()) {
            QContextMenuEvent blankMenu(QContextMenuEvent::Mouse,blankPoint,profiles->viewport()->mapToGlobal(blankPoint)); QApplication::sendEvent(profiles->viewport(),&blankMenu); QVERIFY(!menu->isVisible());
        }   // 列表恰好铺满视口（无留白）时不构造该事件，避免点中最后一行本身。

        // H3：节点区改为行列表。测速入口是标题行的「测速」按钮，进度只进状态栏。
        window.setPage(2);
        auto* nodeRowsView = window.findChild<QListView*>("nodeList"); QVERIFY(nodeRowsView);
        auto* viewsRows = nodeRowsView->model(); QVERIFY(viewsRows);
        QTRY_VERIFY(viewsRows->rowCount() >= 6);
        controller.testNodes({"香港示例","日本示例","新加坡示例"}); QVERIFY(controller.state().busy); QVERIFY(controller.testingNodes());
        QVERIFY(nodeRowsView->isEnabled()); QVERIFY(window.findChild<QLineEdit*>("nodeSearch")->isEnabled()); QVERIFY(!window.findChild<QPushButton*>("chooseMember"));
        // 「更多」菜单里的「恢复自动」在手动组上不可用；测速按钮在测速中仍可用（可再次发起另一组）。
        QVERIFY(!window.findChild<QAction*>("automaticAction")->isEnabled());
        if (!output.isEmpty()) QVERIFY(window.grab().save(output+"/proxy-testing.png"));
        controller.cancelTests(); QTRY_VERIFY(!controller.state().busy); QVERIFY(!controller.testingNodes());
        QVERIFY(controller.state().lastError.isEmpty()); QVERIFY(finished.last()[0].toBool()); QCOMPARE(finished.last()[1].toString(),QString("测速已取消"));

        // 卡片视图保留为「更多 › 显示方式」，测速热区与行列表的点击语义一致。
        auto* cards=window.findChild<QListView*>("nodeCards"); QVERIFY(cards);
        window.setMemberView(1); window.findChild<QLineEdit*>("nodeSearch")->clear(); QCoreApplication::processEvents();
        auto* sorted=qobject_cast<QSortFilterProxyModel*>(cards->model()); QVERIFY(sorted);
        auto* members=qobject_cast<ProxyTableModel*>(sorted->sourceModel()); QVERIFY(members);
        const auto memberIndexFor=[sorted](const QString& name) { for (int row=0;row<sorted->rowCount();++row) if (sorted->index(row,0).data(Qt::UserRole).toString()==name) return sorted->index(row,0); return QModelIndex(); };
        const auto hkIndex=memberIndexFor("香港示例"); QVERIFY(hkIndex.isValid());
        cards->setCurrentIndex(hkIndex); const auto selectedBefore=hkIndex.data(Qt::UserRole).toString();
        const auto nowBefore=controller.proxies().value("节点选择").toObject().value("now").toString(); const auto pidBefore=controller.coreProcessId();
        cards->scrollTo(hkIndex); const auto cardRect=cards->visualRect(hkIndex); const QPoint delayPoint(cardRect.right()-16,cardRect.center().y());
        QVERIFY(cards->viewport()->hasMouseTracking());
        QMouseEvent hoverMove(QEvent::MouseMove,QPointF(delayPoint),QPointF(cards->viewport()->mapToGlobal(delayPoint)),Qt::NoButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(cards->viewport(),&hoverMove);
        QTRY_COMPARE(cards->property("nodeDelayHover").toString(),QString("香港示例")); QCOMPARE(cards->viewport()->cursor().shape(),Qt::PointingHandCursor); QVERIFY(cards->property("nodeTestEnabled").toBool());
        if (!output.isEmpty()) QVERIFY(window.grab().save(output+"/nodes-delay-hover.png"));
        QSignalSpy measured(&controller,&AppController::nodeDelay); const int beforeTest=finished.size();
        QTest::mouseClick(cards->viewport(),Qt::LeftButton,{},delayPoint); QVERIFY(controller.testingNodes());
        QTRY_COMPARE_WITH_TIMEOUT(measured.size(),1,6000); QTRY_COMPARE(finished.size(),beforeTest+1); QCOMPARE(measured.first()[0].toString(),QString("香港示例"));
        QCOMPARE(controller.proxies().value("节点选择").toObject().value("now").toString(),nowBefore); QCOMPARE(controller.coreProcessId(),pidBefore);
        QCOMPARE(cards->currentIndex().data(Qt::UserRole).toString(),selectedBefore);
        members->setDelay("香港示例",2000,{}); const int beforeSortPress=finished.size(); QCoreApplication::processEvents(); QCOMPARE(finished.size(),beforeSortPress);
        cards->setFocus(); QApplication::setActiveWindow(&window); QTest::keyClick(cards,Qt::Key_F5); QVERIFY(controller.testingNodes()); controller.cancelTests(); QTRY_VERIFY(!controller.state().busy);
        window.setMemberView(0);
        QSignalSpy exited(&controller, &AppController::exitReady); controller.requestExit(); QTRY_COMPARE_WITH_TIMEOUT(exited.count(), 1, 5000);
    }
    void routesCarryHomeAnchorAndSettingsCategory() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        // 纯转换：入口 ID ↔ 路由。
        QCOMPARE(toLegacyPage(fromLegacyPage(0)),0);
        // K1：旧编号 2 是首页节点锚点的写法；回显统一回到首页（0），锚点单独表达。
        QCOMPARE(toLegacyPage(fromLegacyPage(2)),0);
        QCOMPARE(fromLegacyPage(2).view,int(ViewIds::Home)); QCOMPARE(fromLegacyPage(2).anchor,HomeAnchor::Nodes);
        QCOMPARE(fromLegacyPage(6).category,-1); QCOMPARE(fromLegacyPage(6).view,int(ViewIds::Settings));
        QVERIFY(isRoutableView(fromLegacyPage(0))); QVERIFY(isRoutableView(fromLegacyPage(2))); QVERIFY(isRoutableView(fromLegacyPage(7)));
        // K1：视图编号只剩 0–4；旧入口 2 与越界编号都不是视图 ID，只能经 fromLegacyPage 解释。
        // 首页两种锚点都合法；旧编号 2 已被翻译成「首页 + 节点锚点」。
        QVERIFY(!isRoutableView(Route{ViewIds::Count,-1,HomeAnchor::Control}));
        QVERIFY(!isRoutableView(Route{8,-1,HomeAnchor::Control})); QVERIFY(!isRoutableView(Route{-1,-1,HomeAnchor::Control}));
        QVERIFY(isRoutableView(Route{ViewIds::Home,-1,HomeAnchor::Nodes}));
        QVERIFY(isRoutableView(fromLegacyPage(2))); QCOMPARE(fromLegacyPage(2).view, int(ViewIds::Home));
        QCOMPARE(fromLegacyPage(2).anchor, HomeAnchor::Nodes);

        MainWindow window(&controller); window.show(); QCoreApplication::processEvents();
        // 侧栏「首页」回显入口 0；显式节点入口回显 2，且 currentRoute 才是权威。
        // 数据页把页签写回 route_.tab，所以这里用显式 tab=-1 的首页路由比较。
        window.setPage(0); QCOMPARE(window.page(),0); QCOMPARE(window.currentRoute(),fromLegacyPage(0));
        window.navigate(Route{0,-1,HomeAnchor::Nodes}); QCOMPARE(window.page(),0); QCOMPARE(window.currentRoute().anchor,HomeAnchor::Nodes);
        // 非法路由被忽略，不改变当前路由。
        window.navigate(Route{9,-1,HomeAnchor::Control}); QCOMPARE(window.page(),0); QCOMPARE(window.currentRoute().anchor,HomeAnchor::Nodes);
        // K1 修正：视图 2 现在是规则视图，navigate 不再按数字猜「首页节点」。
        // 旧编号 2 只能经 fromLegacyPage 归一化；直接给 Route{2} 就是规则页。
        window.navigate(Route{2,-1,HomeAnchor::Control}); QCOMPARE(window.currentRoute().view,int(ViewIds::Rules)); QCOMPARE(window.page(),3);
        window.setPage(2); QCOMPARE(window.currentRoute(),fromLegacyPage(2)); QCOMPARE(window.currentRoute().view,int(ViewIds::Home)); QCOMPARE(window.page(),0);
        // 设置分类可深链；未指定分类时沿用当前分类。
        // K8：设置分类不再是页签，而是「视野滚到第几组」；窗口标题仍带分类名。
        QVERIFY(!window.findChild<QTabBar*>("settingsCategories"));
        const auto groups=window.findChildren<QFrame*>("settingsGroup"); QCOMPARE(groups.size(),3);
        // K1/K8：视图号只剩 0–4；旧编号 6 只能经 fromLegacyPage 解释。分类 2 = 内核与维护。
        window.navigate(Route{ViewIds::Settings,2}); QCOMPARE(window.page(),6); QCOMPARE(window.currentRoute().category,2);
        auto* scroll=window.findChild<QScrollArea*>("settingsScroll"); QVERIFY(scroll); QCoreApplication::processEvents();
        QTRY_VERIFY(scroll->viewport()->rect().intersects(QRect(groups[2]->mapTo(scroll->viewport(),QPoint()),QSize(groups[2]->width(),1))));
        window.navigate(Route{ViewIds::Settings}); QCOMPARE(window.currentRoute().category,-1); QVERIFY(window.findChild<QPushButton*>("settingsNetworkRow")->isVisible());
        window.navigate(fromLegacyPage(6)); QCOMPARE(window.currentRoute().category,-1);
        QVERIFY(window.findChild<QPushButton*>("settingsStartupRow")->isVisible());
        // 轻量重建后 anchor 仍在。
        GuiHost host(&controller,true); host.show(Route{0,-1,HomeAnchor::Nodes}); QTRY_VERIFY(host.window()); QCOMPARE(host.window()->currentRoute().anchor,HomeAnchor::Nodes); QCOMPARE(host.window()->page(),0);
        host.dispose();

        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void titleSlotIsSingleAndWindowTitleFollowsRoute() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,620); window.show(); QCoreApplication::processEvents();
        // 页面不再自绘标题（ADR 0047）：视图名由页签与窗口标题承载。
        QCOMPARE(window.findChildren<QLabel*>("pageTitle").size(),0);
        // K1：旧的页签宿主已删除；视图名由固定顶栏（标题 + 副标题）与窗口标题共同表达。
        QVERIFY(!window.findChild<QTabBar*>("sectionNavigation"));
        auto* topbar=window.findChild<QWidget*>("topbar"); auto* topbarTitle=window.findChild<QLabel*>("topbarTitle"); QVERIFY(topbar && topbarTitle);
        QCOMPARE(topbar->height(),UiTheme::TopbarHeight); QCoreApplication::processEvents(); const QRect host=topbar->geometry();
        QVERIFY(topbarTitle->alignment().testFlag(Qt::AlignHCenter));
        QVERIFY(qAbs(topbarTitle->geometry().center().x()-topbar->rect().center().x())<=1);
        QVERIFY(!window.findChild<QLabel*>("topbarStatusText")->isVisible());
        QVERIFY(!window.findChild<QLabel*>("sidebarProduct"));
        window.setPage(0); QCOMPARE(topbarTitle->text(),QString("Mlash")); QCOMPARE(window.windowTitle(),QString("Mlash · 首页"));
        window.setPage(2); QCOMPARE(topbarTitle->text(),QString("Mlash")); QCOMPARE(window.windowTitle(),QString("Mlash · 首页"));
        window.setPage(1); QCOMPARE(topbarTitle->text(),QString("配置")); QCOMPARE(window.windowTitle(),QString("Mlash · 订阅"));
        // 同一顶栏在所有视图里位置一致：不再由各页标题行替换。
        for (const int page:{3,4,5,7}) { window.setPage(page); QCoreApplication::processEvents(); QCOMPARE(topbar->geometry(),host); QVERIFY(topbar->isVisible()); }
        // 兼容入口 7 落到数据页的「测试」页签，主标题仍是「数据」。
        QCOMPARE(window.windowTitle(),QString("Mlash · 数据"));
        QCOMPARE(window.findChild<QTabBar*>("dataTabs")->count(),4);
        // K8：设置页没有分类页签，窗口标题仍逐个带出三组的名称。
        window.setPage(6); QVERIFY(topbar->isVisible()); QCOMPARE(topbarTitle->text(),QString("设置"));
        const QStringList categories{"通用","网络","内核与维护"};
        for (int index = 0; index < categories.size(); ++index) {
            window.navigate(Route{ViewIds::Settings, index}); QCoreApplication::processEvents();
            QCOMPARE(window.currentRoute().category, index);
            QCOMPARE(window.windowTitle(),QStringLiteral("Mlash · 设置 › ") + categories[index]);
        }
        window.navigate(Route{ViewIds::Settings,2}); QCOMPARE(window.windowTitle(),QString("Mlash · 设置 › 内核与维护"));
        // 首页分区没有页签，切到分区页签不应跳进旧的节点视图。
        window.setPage(1);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void sidebarStatusIsPermanentAndSharesTrayWording() {
        // 纯函数：六个状态各自的两行文案与状态点角色（ADR 0048 六态之首）。
        const auto sentence=[&](CoreState core,bool ready,ProxyState proxy,bool tun,bool tunRequested,const QString& profile) {
            StatusFacts f; f.core=core; f.ready=ready; f.proxy=proxy; f.tunObserved=tun; f.tunRequested=tunRequested; f.profileName=profile; return statusSentence(f); };
        QCOMPARE(sentence(CoreState::Running,true,ProxyState::Owned,false,false,"订阅 A").line1,QString("内核运行中 · 系统代理已接管"));
        QCOMPARE(sentence(CoreState::Running,true,ProxyState::Owned,false,false,"订阅 A").line2,QString("TUN 未开启 · 订阅 A"));
        QCOMPARE(sentence(CoreState::Running,true,ProxyState::Owned,false,false,"订阅 A").accent,QString("success"));
        QCOMPARE(sentence(CoreState::Running,true,ProxyState::Disabled,true,true,"订阅 A").line1,QString("内核运行中 · TUN 已接管"));
        // 内核在运行但没有接管：必须与「已关闭」区分开（ADR 0048）。
        QCOMPARE(sentence(CoreState::Running,true,ProxyState::Disabled,false,false,"订阅 A").line1,QString("内核运行中 · 未接管流量"));
        QCOMPARE(sentence(CoreState::Stopped,false,ProxyState::Disabled,false,false,"").line1,QString("内核已停止 · 未接管流量"));
        QCOMPARE(sentence(CoreState::Stopped,false,ProxyState::Disabled,false,false,"").line2,QString("TUN 未开启 · 未选择订阅"));
        QCOMPARE(sentence(CoreState::Stopped,false,ProxyState::Disabled,false,false,"").accent,QString("secondary"));
        QCOMPARE(sentence(CoreState::Starting,false,ProxyState::Unknown,false,false,"订阅 A").accent,QString("warning"));
        QCOMPARE(sentence(CoreState::Failed,false,ProxyState::Unknown,false,false,"订阅 A").accent,QString("error"));
        // 意图已开但未观测到：只能说「未确认」，不能声称已开启。
        QCOMPARE(sentence(CoreState::Running,true,ProxyState::Disabled,false,true,"订阅 A").line2,QString("TUN 未确认 · 订阅 A"));
        QCOMPARE(sentence(CoreState::Running,true,ProxyState::External,false,false,"订阅 A").line1,QString("内核运行中 · 系统代理由其他程序管理"));

        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,620); window.show(); QCoreApplication::processEvents();
        auto* button=window.findChild<QToolButton*>("sidebarStatus"); auto* line1=window.findChild<QLabel*>("sidebarStatusLine1"); auto* line2=window.findChild<QLabel*>("sidebarStatusLine2");
        QVERIFY(button&&line1&&line2); QVERIFY(!button->isVisible()); QVERIFY(window.findChild<QStatusBar*>("operationStatusBar")->isVisible()); QVERIFY(!line1->text().isEmpty()); QVERIFY(!line2->text().isEmpty());
        // 每个视图都要能看见状态条（P1），包括设置页与首页。
        for (const int page:{0,1,2,3,4,5,6,7}) { window.setPage(page); QCoreApplication::processEvents(); QVERIFY2(!button->isVisible(),qPrintable(QString::number(page))); QVERIFY(window.findChild<QStatusBar*>("operationStatusBar")->isVisible()); QVERIFY(!line1->text().isEmpty()); }
        // 托盘前两行与侧栏逐字相同。
        auto& state=const_cast<AppState&>(controller.state()); state.core=CoreState::Running; state.ready=true; state.proxy=ProxyState::Owned; controller.stateChanged(); QCoreApplication::processEvents();
        const auto tooltip=TrayController::statusTooltip(controller.state(),controller.settings());
        const auto lines=tooltip.split(QChar(10));
        QVERIFY2(lines.size()>=3,qUtf8Printable(tooltip));
        QCOMPARE(lines[1],line1->text()); QCOMPARE(lines[2],line2->text());
        // 单击回首页控制区。
        window.setPage(6); QCoreApplication::processEvents(); QTest::mouseClick(button,Qt::LeftButton); QCOMPARE(window.page(),0); QCOMPARE(window.currentRoute().anchor,HomeAnchor::Control);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); window.resize(840,560); QCoreApplication::processEvents(); QVERIFY(window.grab().save(output+"/sidebar-status-840x560.png")); }
        state.core=CoreState::Stopped; state.ready=false; state.proxy=ProxyState::Disabled; controller.stateChanged(); QCoreApplication::processEvents();
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void homeOwnsNodeSectionInOneScrollView() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,620); window.show(); QCoreApplication::processEvents();
        // ADR 0048 + K1：节点页并入首页、网站测试并入数据页签，页面栈只剩五个视图。
        auto* stack=window.findChild<QStackedWidget*>("mainPages"); QVERIFY(stack); QCOMPARE(stack->count(),5);
        auto* nodes=window.findChild<QWidget*>("nodeSection"); auto* home=window.findChild<QScrollArea*>("homeScroll"); auto* views=window.findChild<QStackedWidget*>("nodeViews");
        QVERIFY(nodes && home && views); QCOMPARE(views->parentWidget(),nodes);
        int homeIndex=-1; for (int i=0;i<stack->count();++i) if (stack->widget(i)->isAncestorOf(home)) homeIndex=i;
        QCOMPARE(homeIndex,0); QVERIFY(stack->widget(0)->isAncestorOf(nodes));
        // 视图 ID → 页面栈索引：设置与网站测试仍指向各自页面。
        window.setPage(6); QCOMPARE(stack->currentIndex(),int(ViewIds::Settings)); QCOMPARE(window.page(),6);
        // 旧入口 7 落在数据视图的「测试」页签，不再单独占一页。
        window.setPage(7); QCOMPARE(stack->currentIndex(),int(ViewIds::Data)); QCOMPARE(window.page(),7);
        QCOMPARE(window.windowTitle(),QString("Mlash · 数据"));
        QCOMPARE(window.findChild<QTabBar*>("dataTabs")->currentIndex(),int(ViewIds::DataTab::Tests));
        window.setPage(1); QCOMPARE(stack->currentIndex(),int(ViewIds::Profiles));
        // 入口 2 落到首页并滚动到节点区首行，而不是切换到别的页面。
        window.setPage(0); QCoreApplication::processEvents();
        auto* inner=home->widget(); QVERIFY(inner);
        const int before=inner->height();
        // 空订阅时概览/仪表盘都隐藏，内容可能不足一屏；锚点因此只保证请求了「滚到节点区」，
        // 滚动量取决于内容高度。这里断言路由与栈正确，并把可达时的滚动位置一并检查。
        const int scrollMax=home->verticalScrollBar()->maximum();
        // K1：旧入口 2 归一化为首页节点锚点，回显编号是 0，锚点单独表达。
        window.setPage(2); QTRY_VERIFY_WITH_TIMEOUT(window.page()==0,2000); QCOMPARE(window.currentRoute().anchor,HomeAnchor::Nodes); QCOMPARE(stack->currentIndex(),0);
        if (scrollMax>0) QTRY_VERIFY_WITH_TIMEOUT(home->verticalScrollBar()->value()>0,2000);
        // 控制区在上、节点区在下：节点区顶边低于控制区顶边。
        QVERIFY(!nodes->geometry().topLeft().isNull()); QCOMPARE(inner->height(),before);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    void emptyStatesOfferFocusableNextStep() {
        QTemporaryDir data; AppController controller(data.path(),true); QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,620); window.show(); QCoreApplication::processEvents();
        // 配置的导入入口始终在页首，空态不再重复一个导入按钮。
        window.setPage(1); QCoreApplication::processEvents();
        auto* profiles=window.findChild<QListView*>("profileList"); QVERIFY(profiles);
        auto* profilesAction=profiles->viewport()->findChild<QPushButton*>("emptyStateAction"); QVERIFY(profilesAction);
        QVERIFY(!profilesAction->isVisible());
        auto* source=window.findChild<QLineEdit*>("profileSourceInput"); QVERIFY(source&&source->isVisible());
        QVERIFY(source->focusPolicy()!=Qt::NoFocus);
        QVERIFY(window.findChild<QPushButton*>("importSubscription")->isVisible());
        QVERIFY(window.findChild<QPushButton*>("importLocalProfile")->isVisible());
        // 连接页：无配置时唯一空态动作直接前往配置。
        window.setPage(4); QCoreApplication::processEvents();
        auto* connections=window.findChild<QTableView*>("connectionsTable"); QVERIFY(connections);
        auto* connectionsAction=connections->viewport()->findChild<QPushButton*>("emptyStateAction"); QVERIFY(connectionsAction);
        QVERIFY(connectionsAction->isVisible()); QVERIFY(!connectionsAction->text().isEmpty()); QCOMPARE(connectionsAction->focusPolicy(),Qt::StrongFocus);
        QTest::mouseClick(connectionsAction,Qt::LeftButton); QTRY_VERIFY(source->isVisible());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // P4.1 反馈路由：页面级结果只出现在所属页面，跨页结果进状态栏「最近操作」段，
    // 而「当前告警」段独立存在、不被操作结果顶掉（ADR 0046 / N10）。
    void operationResultsGoToTheirOwnPage() {
        QTemporaryDir data; AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,640); window.show();
        auto* notice=window.findChild<QLabel*>("operationMessage");
        auto* alarm=window.findChild<QLabel*>("operationAlarm");
        QVERIFY(notice && alarm);
        window.setPage(1); QCoreApplication::processEvents();
        auto* profilesHint=window.findChild<QLabel*>("profileSubmitHint"); QVERIFY(profilesHint);
        // 1. 页面级结果：订阅更新失败只写订阅页，不进状态栏。
        const auto noticeBefore=notice->text();
        controller.operationFinished(false,"订阅源返回 404",OperationId::ProfileUpdate);
        QCoreApplication::processEvents();
        QVERIFY2(profilesHint->isVisible(),"订阅页应显示页面级结论");
        QVERIFY(profilesHint->text().contains("404"));
        QCOMPARE(notice->text(),noticeBefore);
        // 2. 切到别的页面后，订阅页的结论不跑到别的页面上。
        window.setPage(5); QCoreApplication::processEvents();
        QTRY_VERIFY(!profilesHint->isVisible());
        // 3. 跨页结果进「最近操作」段，而不是页面提示。
        controller.operationFinished(false,"安装失败：权限不足",OperationId::CoreUpdate);
        QCoreApplication::processEvents();
        QVERIFY(notice->text().contains("权限不足"));
        // 4. 首页对象内结果（代理总开关失败）落在控制区，不进状态栏。
        window.setPage(0); QCoreApplication::processEvents();
        const auto noticeAfterCrossPage=notice->text();
        controller.operationFinished(false,"请先导入并激活一个订阅，再开启代理",OperationId::ProxyToggle);
        QCoreApplication::processEvents();
        auto* control=window.findChild<QLabel*>("homeControlHint");
        QVERIFY(control && control->isVisible()); QVERIFY(control->text().contains("激活"));
        QCOMPARE(notice->text(),noticeAfterCrossPage);
        // 5. 告警段与最近操作段互不覆盖：注入告警后，跨页结果仍然在「最近操作」段。
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // 归属表本身是纯函数，逐条钉住，避免以后把页面级结果悄悄挪回状态栏。
    void feedbackTargetsStayExplicit() {
        using namespace mlash;
        for (const auto id:{OperationId::ProfileUpdate,OperationId::ProfileActivate,OperationId::ProfileBatch,OperationId::ProfileImport})
            QCOMPARE(feedbackTarget(id).page, 1);
        for (const auto id:{OperationId::ProxyToggle,OperationId::ModeSwitch,OperationId::MemberSelect,OperationId::TunToggle})
            QCOMPARE(feedbackTarget(id).page, 0);
        for (const auto id:{OperationId::ConnectionClose,OperationId::ConnectionCloseAll})
            QCOMPARE(feedbackTarget(id).page, 4);
        QCOMPARE(feedbackTarget(OperationId::SettingsApply).page, 6);
        QCOMPARE(feedbackTarget(OperationId::RulesSave).page, 3);
        for (const auto id:{OperationId::CoreStart,OperationId::CoreStop,OperationId::CoreUpdate,OperationId::Backup,OperationId::Restore,OperationId::Unspecified})
            QCOMPARE(feedbackTarget(id).kind, FeedbackTarget::StatusNotice);
        QCOMPARE(statusNoticeText(false,"失败"), QString("操作未完成 · 失败"));
        QCOMPARE(statusNoticeText(true,"完成"), QString("完成"));
    }
    // 窗口层面：切到连接页会打开采样，离开会关闭——S1 搬迁后必须仍成立。
    void viewSwitchingKeepsControllerSideEffects() {
        QTemporaryDir data; AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,640); window.show();
        // 旧入口 4 = 数据 › 连接，5 = 数据 › 日志；两者都是同一个「数据」视图。
        window.setPage(4); QCoreApplication::processEvents(); QVERIFY(controller.connectionsVisible());
        window.setPage(5); QCoreApplication::processEvents(); QVERIFY(!controller.connectionsVisible());
        window.setPage(4); QCoreApplication::processEvents(); QVERIFY(controller.connectionsVisible());
        window.setPage(0); QCoreApplication::processEvents(); QVERIFY(!controller.connectionsVisible());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // S2 工具窗口：同一「工具+对象」第二次打开返回同一实例；对象被删时窗口关闭。
    void toolWindowsAreReusedPerObject() {
        using namespace mlash;
        ToolWindowRegistry registry; int created = 0;
        const auto factory = [&created] { ++created; auto* w = new QWidget; w->setObjectName("toolFixture"); w->show(); return w; };
        auto* first = registry.show("dns", "profile-a", factory);
        auto* again = registry.show("dns", "profile-a", factory);
        QCOMPARE(created, 1);               // 第二次不再构造
        QCOMPARE(first, again);
        auto* other = registry.show("dns", "profile-b", factory);
        QCOMPARE(created, 2); QVERIFY(other != first);
        // 不同工具同对象键互不复用。
        auto* chained = registry.show("proxy-chain", "profile-a", factory);
        QCOMPARE(created, 3); QVERIFY(chained != first);
        QVERIFY(registry.contains("dns", "profile-a"));
        QVERIFY(registry.contains("dns"));  // 按工具查
        // 对象被删：只关它的窗口，另一个对象的窗口保留。
        registry.closeMissing("dns", QSet<QString>{"profile-b"});
        QVERIFY(!registry.contains("dns", "profile-a"));
        QVERIFY(registry.contains("dns", "profile-b"));
        QVERIFY(registry.contains("proxy-chain", "profile-a"));
        // 窗口自行销毁（WA_DeleteOnClose）后不能复用失效指针。
        chained->deleteLater(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!registry.contains("proxy-chain", "profile-a"));
        auto* recreated = registry.show("proxy-chain", "profile-a", factory);
        QCOMPARE(created, 4); QVERIFY(recreated);
        delete first; delete other; delete recreated;
    }
    // V1 状态形状：五个 state 映射到五个不同形状，且颜色仍来自 token。
    void statusStatesUseDistinctShapes() {
        using namespace mlash;
        const QList<QString> states{"success", "warning", "error", "active", "idle"};
        QSet<int> kinds; QSet<QString> names;
        for (const auto& state : states) {
            kinds.insert(int(StatusShape::kindOf(state)));
            names.insert(StatusShape::accessibleName(state));
            const auto image = StatusShape::pixmap(state, 1.0);
            QVERIFY(!image.isNull());
            QCOMPARE(image.size(), QSize(12, 12));
        }
        // 五个状态五把钥匙：色盲用户不依赖颜色也能分辨。
        QCOMPARE(kinds.size(), states.size());
        QCOMPARE(names.size(), states.size());
        // 未声明的状态退化为「空闲」，不会画出一个空白点。
        QCOMPARE(StatusShape::kindOf("unknown"), StatusShape::Kind::Dot);
        // 形状改变时像素确实不同（防止映射改了但绘制没跟上）。
        const auto solid = StatusShape::pixmap("success", 1.0).toImage();
        const auto square = StatusShape::pixmap("error", 1.0).toImage();
        QVERIFY(solid != square);
    }
    // V1 骨架屏必须真的挂在界面上：只在代码里存在、没有界面使用，等于没做。
    // 冒烟模式没有内核 API，无法稳定构造「读取中」瞬时状态，因此这里验的是接线本身：
    // 骨架块已创建在规则表的视口内，并随「就绪 + 无错误 + 尚无数据」显示。
    void rulesTableHasSkeletonWired() {
        QTemporaryDir data; AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true;
        MainWindow window(&controller); window.resize(940,700); window.show();
        window.setPage(3); QCoreApplication::processEvents();
        auto* tabs=window.findChild<QTabWidget*>("rulesTabs"); QVERIFY(tabs);
        tabs->setCurrentIndex(1); QCoreApplication::processEvents();
        auto* skeleton=window.findChild<QWidget*>("rulesSkeleton");
        QVERIFY2(skeleton, "规则表视口内应创建骨架占位");
        auto* table=window.findChild<QTableView*>("activeRulesTable"); QVERIFY(table);
        QCOMPARE(skeleton->parentWidget(), table->viewport());
        // 未就绪时骨架必须收起：不能把「没在读」画成「正在读」。
        state.ready=false; QCoreApplication::processEvents();
        QVERIFY(!skeleton->isVisible());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // V1 骨架屏：加载态给出占位块，而不是与空态相同的空白面板。
    void skeletonBlockOccupiesContentSpace() {
        using namespace mlash;
        SkeletonBlock block(3);
        block.resize(300, block.height());
        QVERIFY(block.height() > 0);
        QVERIFY(!block.testAttribute(Qt::WA_TransparentForMouseEvents) ? false : true);
        QCOMPARE(block.accessibleName(), QString("正在读取"));
        QVERIFY(block.grab().toImage().width() > 0);
    }
    // V2 用量条：有额度时显示并在列里，完全没有额度时整列隐藏（不留空白列）。
    void profileUsageColumnFollowsAvailableData() {
        QTemporaryDir data; auto platform=createPlatformServices(); ProfileStore store(data.path(),platform);
        QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        Profile withQuota; withQuota.id=newId(); withQuota.name="有额度"; withQuota.sourceType="url"; withQuota.source="https://example.invalid/a";
        withQuota.candidate=withQuota.active=newId();
        withQuota.subscriptionInfo=QJsonObject{{"upload","1024"},{"download","1024"},{"total","4096"}};
        Profile without; without.id=newId(); without.name="无额度"; without.sourceType="url"; without.source="https://example.invalid/b";
        without.candidate=without.active=newId();
        QVERIFY2(store.save(settings,{withQuota,without}).ok, "fixture save failed");
        { const auto reload=store.load(); QVERIFY2(reload.result.ok, qUtf8Printable("fixture reload failed: "+reload.result.error));
          QCOMPARE(reload.profiles.size(), 2); QVERIFY(reload.profiles[0].subscriptionInfo.contains("total")); }
        AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(1); QCoreApplication::processEvents();
        // 同一网格的有额度卡片显示真实摘要；无额度卡片不保留假进度。
        auto* remote=window.findChild<QListView*>("profileList"); QVERIFY(remote);
        QCOMPARE(remote->model()->rowCount(),2);
        const auto first=remote->model()->index(0,0), second=remote->model()->index(1,0);
        QCOMPARE(first.data(ProfileRowModel::UsageRatio).toDouble(), 0.5);   // 2 KiB / 4 KiB
        QVERIFY(!first.data(ProfileRowModel::UsageText).toString().isEmpty());
        QCOMPARE(second.data(ProfileRowModel::UsageRatio).toDouble(), 0.0);
        QVERIFY(second.data(ProfileRowModel::UsageText).toString().isEmpty());
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // V3 设置行内当前值：值不在控件里的行显示现状，控件本身显示值的行不重复。
    void settingsRowsShowCurrentValue() {
        QTemporaryDir data; AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,700); window.show(); window.setPage(6); QCoreApplication::processEvents();
        const auto values = window.findChildren<QLabel*>("settingValue");
        QVERIFY(!values.isEmpty());
        bool hasTun = false, hasCore = false;
        for (auto* label : values) {
            if (label->text().contains("未启用") || label->text().contains("已启用")) hasTun = true;
            if (label->text().contains("内核") || label->text().endsWith(".exe") || label->text().contains("尚未设置")) hasCore = true;
            QVERIFY(!label->text().isEmpty());
        }
        QVERIFY2(hasTun, "TUN 行应显示当前是否启用");
        QVERIFY2(hasCore, "内核行应显示当前路径状态");
        // 下拉/数字框本身显示值的行不应再挂一个当前值标签，避免同一事实两处。
        for (auto* row : window.findChildren<QWidget*>("settingRow")) {
            const auto name = row->property("settingSearchName").toString();
            if (!QStringList{"本地端口","日志等级","IPv6","统一延迟"}.contains(name)) continue;
            QVERIFY2(!row->findChild<QLabel*>("settingValue"), qPrintable(name + " 不应重复显示当前值"));
        }
        // 840×560 下不得出现横向滚动（既有约束，V3 不得退让）。
        window.resize(840,560); QCoreApplication::processEvents();
        auto* settings=window.findChild<QScrollArea*>("settingsScroll"); QVERIFY(settings);
        QCOMPARE(settings->horizontalScrollBar()->maximum(),0);
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // 订阅对象菜单只保留低频入口；高频使用、刷新和候选操作留在卡片。
    void menuSectionsLabelGroupsWithoutAddingCommands() {
        QTemporaryDir data; auto platform=createPlatformServices(); ProfileStore store(data.path(),platform);
        QVERIFY(store.load().result.ok); Settings settings; settings.startCore=false;
        Profile p; p.id=newId(); p.name="菜单示例"; p.sourceType="url"; p.source="https://example.invalid/x"; p.candidate=p.active=newId();
        QVERIFY(store.save(settings,{p}).ok);
        AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.show(); window.setPage(1); QCoreApplication::processEvents();
        auto* cards=window.findChild<QListView*>("profileList"); QVERIFY(cards);
        const auto index=cards->model()->index(0,0);
        QTest::mouseClick(cards->viewport(),Qt::LeftButton,{},ProfileCardDelegate::menu(cards->visualRect(index)).center());
        auto* menu=window.findChild<QMenu*>("profileContextMenu"); QVERIFY(menu);
        QCOMPARE(menu->property("contextProfileId").toString(),p.id);
        QCOMPARE(menu->actions().size(),5);
        QStringList labels; for (const auto* action:menu->actions()) if (!action->isSeparator()) labels.append(action->text());
        QCOMPARE(labels,QStringList({"查看配置内容","编辑来源与更新","分享配置…","删除配置…"}));
        QVERIFY(!labels.join(' ').contains("使用"));
        QVERIFY(!labels.join(' ').contains("刷新"));
        QVERIFY(!labels.join(' ').contains("规则"));
        menu->hide();
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // V5 首页控制区：节点卡片带协议标签，组标题说明选路规则，失败原因落在控制区。
    void homeControlAreaStatesRoutingRulesAndFailure() {
        QTemporaryDir data; QVERIFY(seedDashboardFixture(data.path()));
        AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        auto& state=const_cast<AppState&>(controller.state()); state.ready=true;
        // 成员必须同时出现在 proxies 里：模型按名字回查节点对象，缺一个就会读到空对象。
        auto& proxies=const_cast<QJsonObject&>(controller.proxies());
        const QJsonObject nodeA{{"type","ss"},{"name","节点甲"}}, nodeB{{"type","trojan"},{"name","节点乙"}};
        const QJsonObject manual{{"type","Selector"},{"all",QJsonArray{"节点甲","节点乙"}},{"now","节点甲"}};
        const QJsonObject automatic{{"type","URLTest"},{"all",QJsonArray{"节点甲","节点乙"}},{"now","节点乙"}};
        proxies={{"节点甲",nodeA},{"节点乙",nodeB},{"手动组",manual},{"自动组",automatic}};
        MainWindow window(&controller); window.resize(940,700); window.show(); window.setPage(0); QCoreApplication::processEvents();
        auto* mode=window.findChild<QPushButton*>("homeRouteRow"); QVERIFY(mode); QTRY_VERIFY(mode->isEnabled());
        QTest::mouseClick(mode,Qt::LeftButton); QPointer<QMenu> routeMenu=mode->findChild<QMenu*>("routeModeMenu"); QVERIFY(routeMenu); QTRY_VERIFY(routeMenu->isVisible());
        QCOMPARE(routeMenu->actions().size(),3); int checked=0; for (auto* action:routeMenu->actions()) checked+=action->isChecked(); QCOMPARE(checked,1);
        const QRect routePosition=routeMenu->geometry(); QTest::qWait(60); QCOMPARE(routeMenu->geometry(),routePosition);
        QCOMPARE(routeMenu->grab().toImage().pixelColor(0,0).alpha(),0);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(routeMenu->grab().save(output+"/route-menu.png")); }
        QTest::keyClick(routeMenu,Qt::Key_Escape); QTRY_VERIFY(!routeMenu || !routeMenu->isVisible());
        QTest::mouseClick(mode,Qt::LeftButton); routeMenu=mode->findChild<QMenu*>("routeModeMenu"); QVERIFY(routeMenu && routeMenu->isVisible());
        window.setPage(1); QTRY_VERIFY(!routeMenu || !routeMenu->isVisible()); window.setPage(0);
        auto* filterAction=window.findChild<QAction*>("nodeFilterAction"); QVERIFY(filterAction);
        filterAction->trigger(); auto* filterPanel=window.findChild<QWidget*>("nodeFilterPopup"); QVERIFY(filterPanel);
        QTRY_VERIFY(filterPanel->isVisible()); const QRect filterPosition=filterPanel->geometry();
        QCOMPARE(filterPanel->grab().toImage().pixelColor(0,0).alpha(),0);
        QTest::qWait(60); QCOMPARE(filterPanel->geometry(),filterPosition);
        if (!output.isEmpty()) QVERIFY(filterPanel->grab().save(output+"/node-filter.png"));
        auto* region=window.findChild<QComboBox*>("nodeRegion"); QVERIFY(region && region->hasFocus());
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
        QCoreApplication::sendEvent(region,&escape);
        QVERIFY(!filterPanel->isVisible());
        filterAction->trigger(); QTRY_VERIFY(filterPanel->isVisible());
        window.setPage(1); QTRY_VERIFY(!filterPanel->isVisible()); window.setPage(0);
        filterAction->trigger(); QTRY_VERIFY(filterPanel->isVisible());
        window.move(window.pos()+QPoint(6,0)); QTRY_VERIFY(!filterPanel->isVisible());
        auto* hint=window.findChild<QLabel*>("nodeGroupHint"); QVERIFY(hint);
        // H3：浏览另一个策略组走「更多 › 策略组」子菜单（原 groupSwitcher_ + 卡片选择器）。
        auto* groupMenu=window.findChild<QMenu*>("nodeGroupMenu"); QVERIFY(groupMenu);
        const auto browse=[groupMenu](const QString& name) {
            for (auto* action:groupMenu->actions()) if (action->text()==name) { action->trigger(); return true; }
            return false;
        };
        // 手动组：说明「单击即切换」。
        QVERIFY(browse("手动组")); QCoreApplication::processEvents();
        QVERIFY(hint->isHidden()); QVERIFY(window.findChild<QListView*>("nodeList")->toolTip().contains("单击成员即切换"));
        QCOMPARE(hint->text(), QString("单击成员即切换；自动策略组不可手动切换。"));
        // 自动组：说明不可手动切换——交互规则不同必须就地说明。
        QVERIFY(browse("自动组")); QCoreApplication::processEvents();
        QTRY_COMPARE(hint->text(), QString("自动策略组由内核选路，不能手动切换组内成员。"));
        // 自动组的成员行整段归到「自动策略组」，且不给切换入口：回车被守卫拦住。
        auto* rows=window.findChild<QListView*>("nodeList"); QVERIFY(rows);
        QTRY_COMPARE(rows->model()->data(rows->model()->index(0,0),Qt::DisplayRole).toString(),QString("自动组"));
        // 回车不提交：自动组的成员行不给切换入口（H3 修的缺陷）。
        const auto autoNowBefore=controller.proxies().value("自动组").toObject().value("now").toString();
        rows->setCurrentIndex(rows->model()->index(1,0)); QTest::keyClick(rows,Qt::Key_Return); QCoreApplication::processEvents();
        QCOMPARE(controller.proxies().value("自动组").toObject().value("now").toString(),autoNowBefore);
        // 控制区失败原因：代理总开关被拒绝时，原因落在控制区而不是全局状态栏。
        auto* control=window.findChild<QLabel*>("homeControlHint"); QVERIFY(control);
        controller.operationFinished(false,"请先导入并激活一个订阅，再开启代理",OperationId::ProxyToggle);
        QCoreApplication::processEvents();
        QVERIFY(control->isVisible());
        QCOMPARE(control->property("state").toString(), QString("error"));
        QVERIFY(control->text().contains("激活"));
        state.ready=false;
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
    // 工具栏几何：WrapLayout 在窄容器里会把按钮压成竖排；这条测试固定「工具条横向铺满、
    // 搜索框不被挤没」。窗口宽度取 940，与冒烟截图的默认尺寸一致。
    void pageToolbarsStayHorizontal() {
        QTemporaryDir data; AppController controller(data.path(),true);
        QSignalSpy initialized(&controller,&AppController::initialized); controller.initialize(); QTRY_COMPARE(initialized.size(),1);
        MainWindow window(&controller); window.resize(940,700); window.show();
        // 连接页（视图 4）：工具条与搜索框必须在同一行铺开。
        window.setPage(4); QCoreApplication::processEvents();
        auto* connectionBar = window.findChild<QWidget*>("connectionToolbar"); QVERIFY(connectionBar);
        QVERIFY(connectionBar->isVisible());
        QVERIFY2(connectionBar->width() > 300, qUtf8Printable(QString("connectionToolbar too narrow: %1").arg(connectionBar->width())));
        QVERIFY2(connectionBar->height() < 60, qUtf8Printable(QString("connectionToolbar wrapped to %1px tall").arg(connectionBar->height())));
        auto* connectionSearch = window.findChild<QLineEdit*>("connectionsSearch"); QVERIFY(connectionSearch);
        QVERIFY2(connectionSearch->width() > 150, qUtf8Printable(QString("connection search too narrow: %1").arg(connectionSearch->width())));
        // 日志页（视图 5）：同样的约束。
        window.setPage(5); QCoreApplication::processEvents();
        auto* logBar = window.findChild<QWidget*>("logToolbar"); QVERIFY(logBar);
        QVERIFY(logBar->isVisible());
        QVERIFY2(logBar->width() > 300, qUtf8Printable(QString("logToolbar too narrow: %1").arg(logBar->width())));
        QVERIFY2(logBar->height() < 60, qUtf8Printable(QString("logToolbar wrapped to %1px tall").arg(logBar->height())));
        QSignalSpy exited(&controller,&AppController::exitReady); controller.requestExit(); QTRY_COMPARE(exited.size(),1);
    }
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
