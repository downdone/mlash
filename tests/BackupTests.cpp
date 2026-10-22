#include "app/AppController.h"
#include "profile/BackupArchive.h"
#include "platform/PortableCrypto.h"
#include "storage/Storage.h"
#include "ui/BackupDialog.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFileInfo>
#include <QListWidget>
#include <QCheckBox>
#include "ui/WidgetStyle.h"
#include "ui/UiTheme.h"
#include "ui/LocalBackupDialog.h"
#include "profile/LocalBackup.h"
#include <QSpinBox>
#include <QTableWidget>

using namespace mlash;
class BackupPlatform final : public PlatformServices {
public:
    int writes = 0, seals = 0, failSealAt = -1;
    Result readProxy() override { return {true, {}, {{"flags", 1}, {"server", ""}, {"bypass", ""}, {"pac", ""}}}; }
    Result writeProxy(const QJsonObject&) override { ++writes; return Result::fail("test must not write proxy"); }
    QJsonObject proxyTarget(int) const override { return {}; }
    bool proxyEnabled(const QJsonObject&) const override { return false; }
    Result autoStart(bool, const QString&) override { ++writes; return Result::fail("test must not write Run"); }
    Result readAutoStart(const QString&) override { return {true, {}, {{"enabled",false}}}; }
    Result seal(const QByteArray& plain, QByteArray& encrypted) override { if (++seals == failSealAt) return Result::fail("injected state commit failure"); encrypted = "sealed:" + plain.toBase64(); return {}; }
    Result open(const QByteArray& encrypted, QByteArray& plain) override { if (!encrypted.startsWith("sealed:")) return Result::fail("invalid index"); plain = QByteArray::fromBase64(encrypted.mid(7)); return {}; }
    Result secureDirectory(const QString& path) override { return QDir().mkpath(path) ? Result{} : Result::fail("mkdir"); }
    Result attachChild(qint64) override { return {}; }
    Result ownsTcpPort(qint64, int) override { return {true, {}, {{"owned",true}}}; }
};
class BackupTests : public QObject {
    Q_OBJECT
    const QString password = "controlled-backup-password";
    const QByteArray yaml = "proxies: [{name: Sample, type: ss, server: example.test, port: 443, cipher: aes-128-gcm, password: node-private-secret}]\nproxy-groups: [{name: Test, type: select, proxies: [Sample]}]\nrules: [\"MATCH,Test\"]\n";
    Profile seed(ProfileStore& store) {
        Profile profile; profile.id = newId(); profile.name = "Backup sample"; profile.sourceType = "url"; profile.source = "https://example.invalid/sub?token=subscription-private-token"; profile.customRules = {"DOMAIN-SUFFIX,example.test,DIRECT"}; profile.autoUpdate = true;
        profile.proxyEdits={{"nodes",QJsonObject{{"Extra","name: Extra\ntype: http\nserver: example.test\nport: 443\npassword: edited-private-secret"}}}};
        auto prepared = store.prepare(profile, yaml, Settings{}, 19090, QString(64,'a')); if (!prepared.result.ok) return {};
        profile = prepared.profile; profile.active = profile.candidate;
        auto next = store.prepare(profile, yaml + "# candidate\n", Settings{}, 19091, QString(64,'b')); return next.profile;
    }
private slots:
    void automaticPlanRunsWithoutGuiAndBacksOffOnFailure() {
        QTemporaryDir data; auto platform=std::make_shared<BackupPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore=false;
        settings.localBackup={{"enabled",true},{"before_changes",false},{"interval_hours",1},{"retain",2},{"password",password},{"next_due_utc",QDateTime::currentDateTimeUtc().addSecs(-60).toString(Qt::ISODate)}};
        QVERIFY(store.save(settings,{}).ok);
        AppController c(data.path(),false,nullptr,platform); QSignalSpy init(&c,&AppController::initialized); c.initialize(); QTRY_COMPARE(init.size(),1);
        auto* timer=c.findChild<QTimer*>("localBackupTimer"); QVERIFY(timer); QVERIFY(timer->isActive());
        QVERIFY(writeAtomic(data.path()+"/backups",QByteArray("block-directory")).ok);
        QVERIFY(QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection)); QTRY_VERIFY(!c.state().busy);
        QVERIFY(!c.state().lastError.isEmpty()); const auto count=c.diagnostics()->lines().size(); QVERIFY(count>0);
        QVERIFY(QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection)); QTest::qWait(60); QCOMPARE(c.diagnostics()->lines().size(),count);
        QVERIFY(QFile::remove(data.path()+"/backups")); c.startLocalBackup(); QTRY_VERIFY_WITH_TIMEOUT(!c.state().busy,10000);
        QVERIFY(c.settings().localBackup.contains("last_success_utc")); QVERIFY(!LocalBackup::due(c.settings().localBackup,QDateTime::currentDateTimeUtc()));
        QCOMPARE(LocalBackup::list(store).data.value("rows").toArray().size(),1);
        // A successful archive with a failed index commit must keep the archive and back off too.
        platform->failSealAt=platform->seals+1; c.startLocalBackup(); QTRY_VERIFY_WITH_TIMEOUT(!c.state().busy,10000);
        QVERIFY(c.state().lastError.contains("计划状态保存失败")); QCOMPARE(LocalBackup::list(store).data.value("rows").toArray().size(),2);
        auto& dueSettings=const_cast<Settings&>(c.settings()); dueSettings.localBackup["next_due_utc"]=QDateTime::currentDateTimeUtc().addSecs(-60).toString(Qt::ISODate);
        const auto failedSeals=platform->seals; QVERIFY(QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection)); QTest::qWait(60); QCOMPARE(platform->seals,failedSeals);
        platform->failSealAt=-1; c.startLocalBackup(); QTRY_VERIFY_WITH_TIMEOUT(!c.state().busy,10000); QVERIFY(c.state().lastError.isEmpty());
        QCOMPARE(LocalBackup::list(store).data.value("rows").toArray().size(),2); QCOMPARE(platform->writes,0); QVERIFY(c.findChildren<QWidget*>().isEmpty());
    }
    void localPlanValidationAndOwnedRetention() {
        QTemporaryDir data; auto platform=std::make_shared<BackupPlatform>(); ProfileStore store(data.path(),platform);
        QVERIFY(store.load().result.ok); const auto profile=seed(store);
        Settings settings; settings.localBackup={{"enabled",true},{"before_changes",true},{"interval_hours",24},{"retain",2},{"password",password}};
        QVERIFY(validateLocalBackup(settings.localBackup).ok);
        const auto now=QDateTime::currentDateTimeUtc(); QVERIFY(LocalBackup::due(settings.localBackup,now));
        auto future=settings.localBackup; future["next_due_utc"]=now.addSecs(100).toString(Qt::ISODate); QVERIFY(!LocalBackup::due(future,now));
        auto invalid=settings.localBackup; invalid["retain"]=1.5; QVERIFY(!validateLocalBackup(invalid).ok);
        invalid=settings.localBackup; invalid["interval_hours"]="24"; QVERIFY(!validateLocalBackup(invalid).ok);
        invalid=settings.localBackup; invalid["enabled"]=1; QVERIFY(!validateLocalBackup(invalid).ok);
        invalid=settings.localBackup; invalid["password"]="short"; QVERIFY(!validateLocalBackup(invalid).ok);
        QStringList created; for (int i=0;i<3;++i) { const auto result=LocalBackup::create(store,settings,{profile},"manual"); QVERIFY(result.ok); created.append(result.data.value("path").toString()); QTest::qWait(2); }
        auto listed=LocalBackup::list(store); QVERIFY(listed.ok); QCOMPARE(listed.data.value("rows").toArray().size(),2); QCOMPARE(listed.data.value("rows").toArray().first().toObject().value("path").toString(),created.last()); QVERIFY(!QFileInfo::exists(created.first()));
        const auto corrupt=listed.data.value("rows").toArray().first().toObject().value("path").toString();
        QVERIFY(writeAtomic(corrupt,"damaged").ok);
        const auto foreign=data.path()+"/backups/local/unrelated.mlashbackup"; QVERIFY(writeAtomic(foreign,"keep-foreign").ok);
        settings.localBackup["retain"]=1;
        const auto fresh=LocalBackup::create(store,settings,{profile},"before-change"); QVERIFY(fresh.ok);
        QVERIFY(QFileInfo::exists(corrupt)); QByteArray foreignBytes; QVERIFY(readFile(foreign,foreignBytes).ok); QCOMPARE(foreignBytes,QByteArray("keep-foreign"));
        listed=LocalBackup::list(store); int valid=0,broken=0;
        for (const auto& item:listed.data.value("rows").toArray()) item.toObject().value("valid").toBool()?++valid:++broken;
        QCOMPARE(valid,1); QCOMPARE(broken,1);
        QByteArray encrypted,plain; QVERIFY(readFile(fresh.data.value("path").toString(),encrypted).ok); QVERIFY(openPortable(encrypted,password,plain).ok);
        QVERIFY(QJsonDocument::fromJson(plain).object().value("settings").toObject().value("local_backup").toObject().isEmpty());
        QVERIFY(QFile::remove(store.versionRoot(profile.id,profile.candidate)+"/source.yaml"));
        QVERIFY(!LocalBackup::create(store,settings,{profile},"manual").ok);
        QCOMPARE(LocalBackup::list(store).data.value("rows").toArray().size(),2); QCOMPARE(platform->writes,0);
    }
    void changeProtectionFailureStopsDeleteAndRestoreKeepsPlan() {
        QTemporaryDir data; auto platform=std::make_shared<BackupPlatform>(); ProfileStore store(data.path(),platform);
        QVERIFY(store.load().result.ok); const auto profile=seed(store);
        Settings settings; settings.startCore=false; settings.localBackup={{"enabled",true},{"before_changes",true},{"interval_hours",24},{"retain",7},{"password",password}};
        QVERIFY(store.save(settings,{profile}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished);
        c.initialize(); QTRY_COMPARE(init.size(),1);
        QByteArray before,source; QVERIFY(readFile(data.path()+"/state.dat",before).ok);
        const auto sourcePath=store.versionRoot(profile.id,profile.candidate)+"/source.yaml"; QVERIFY(readFile(sourcePath,source).ok); QVERIFY(QFile::remove(sourcePath));
        c.deleteProfile(profile.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(!finished.last()[0].toBool()); QCOMPARE(c.profiles().size(),1);
        QByteArray after; QVERIFY(readFile(data.path()+"/state.dat",after).ok); QCOMPARE(after,before);
        QVERIFY(writeAtomic(sourcePath,source).ok); c.deleteProfile(profile.id); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000); QVERIFY(finished.last()[0].toBool()); QVERIFY(c.profiles().isEmpty());
        const auto row=LocalBackup::list(store).data.value("rows").toArray().first().toObject();
        const auto preview=BackupArchive::inspect(row.value("path").toString(),password); QVERIFY(preview.ok);
        const auto restored=BackupArchive::prepareRestore(store,platform,c.settings(),{},row.value("path").toString(),password,preview.data.value("sha256").toString(),true);
        QVERIFY(restored.snapshot.result.ok); QCOMPARE(restored.snapshot.settings.localBackup,c.settings().localBackup); QCOMPARE(platform->writes,0);
    }
    void localBackupEditorFailureRetryAndManualRun() {
        QTemporaryDir data; auto platform=std::make_shared<BackupPlatform>(); ProfileStore store(data.path(),platform); QVERIFY(store.load().result.ok);
        Settings settings; settings.startCore=false; QVERIFY(store.save(settings,{}).ok);
        AppController c(data.path(),true,nullptr,platform); QSignalSpy init(&c,&AppController::initialized),finished(&c,&AppController::operationFinished); c.initialize(); QTRY_COMPARE(init.size(),1);
        QPointer<LocalBackupDialog> dialog=new LocalBackupDialog(&c); dialog->show();
        auto* save=dialog->findChild<QPushButton*>("saveLocalBackupSettings"); auto* edit=dialog->findChild<QLineEdit*>("localBackupPassword");
        QCOMPARE(edit->echoMode(),QLineEdit::Password); edit->setText(password); dialog->findChild<QLineEdit*>("localBackupConfirm")->setText(password);
        QTRY_VERIFY(save->isEnabled()); platform->failSealAt=platform->seals+1; save->click(); QTRY_COMPARE(finished.size(),1); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog); QCOMPARE(edit->text(),password); QVERIFY(c.settings().localBackup.isEmpty());
        platform->failSealAt=-1; save->click(); QTRY_COMPARE(finished.size(),2); QVERIFY(finished.last()[0].toBool()); QVERIFY(!c.settings().localBackup.isEmpty());
        auto* run=dialog->findChild<QPushButton*>("runLocalBackup"); QTRY_VERIFY(run->isEnabled()); run->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,15000); QVERIFY(finished.last()[0].toBool());
        QTRY_COMPARE(dialog->findChild<QTableWidget*>("localBackupFiles")->rowCount(),1);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(dialog->grab().save(output+"/local-backups.png")); }
        QVERIFY(!c.diagnostics()->lines().join('\n').contains(password)); dialog->close(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(platform->writes,0);
    }
    void initTestCase() { WidgetStyle::install(); UiTheme::install(); }
    void selectedRestoreNamesValidationAndDialog() {
        QTemporaryDir data,target; auto platform=std::make_shared<BackupPlatform>(); ProfileStore source(data.path(),platform),destination(target.path(),platform); QVERIFY(source.load().result.ok); QVERIFY(destination.load().result.ok); const auto first=seed(source),second=seed(source); auto existing=seed(destination); existing.name="BACKUP SAMPLE"; Settings settings; settings.startCore=false; QVERIFY(destination.save(settings,{existing}).ok); const auto path=data.path()+"/selected.mlashbackup"; QVERIFY(BackupArchive::create(source,settings,{first,second},path,password).ok); const auto preview=BackupArchive::inspect(path,password); QVERIFY(preview.ok); const auto hash=preview.data.value("sha256").toString(); QByteArray before; QVERIFY(readFile(target.path()+"/state.dat",before).ok);
        for (const auto& invalid:QList<QJsonValue>{QJsonValue(),true,QJsonArray{0,0},QJsonArray{-1},QJsonArray{2},QJsonArray{0.5},QJsonArray{"0"},QJsonArray{}}) { const auto result=BackupArchive::prepareRestore(destination,platform,settings,{existing},path,password,hash,false,invalid); QVERIFY(!result.snapshot.result.ok); QVERIFY(result.createdProfiles.isEmpty()); QCOMPARE(QDir(target.path()+"/profiles").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size(),1); } QByteArray after; QVERIFY(readFile(target.path()+"/state.dat",after).ok); QCOMPARE(after,before);
        const auto preferences=BackupArchive::prepareRestore(destination,platform,settings,{existing},path,password,hash,true,QJsonArray{}); QVERIFY(preferences.snapshot.result.ok); QCOMPARE(preferences.snapshot.profiles.size(),1); QVERIFY(preferences.createdProfiles.isEmpty());
        const auto names=restoreProfileNames({first.name,second.name},{existing}); QCOMPARE(names,QStringList({"Backup sample（恢复 2）","Backup sample（恢复 3）"})); const auto longNames=restoreProfileNames({QString(120,'x'),QString(120,'x')},{}); QCOMPARE(longNames[1].size(),120); QVERIFY(longNames[0]!=longNames[1]);
        AppController controller(target.path(),true,nullptr,platform); QSignalSpy initialized(&controller,&AppController::initialized),finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1); QPointer<BackupDialog> dialog=new BackupDialog(&controller,true); dialog->resize(560,500); dialog->show(); dialog->findChild<QLineEdit*>("backupPath")->setText(path); dialog->findChild<QLineEdit*>("backupPassword")->setText(password); dialog->findChild<QPushButton*>("previewBackup")->click(); auto* submit=dialog->findChild<QPushButton*>("submitBackup"); QTRY_VERIFY_WITH_TIMEOUT(submit->isEnabled(),15000); auto* list=dialog->findChild<QListWidget*>("backupSelection"); QCOMPARE(list->count(),2); QVERIFY(list->item(0)->text().contains(names[0])); QVERIFY(list->item(1)->text().contains(names[1])); dialog->findChild<QCheckBox*>("backupRestoreSettings")->setChecked(false); list->item(0)->setCheckState(Qt::Unchecked); list->item(1)->setCheckState(Qt::Unchecked); QVERIFY(!submit->isEnabled()); list->item(1)->setCheckState(Qt::Checked); QVERIFY(submit->isEnabled()); QVERIFY(list->item(1)->text().contains(names[0]));
        // Presentation reacts to observed preconditions; stopping never submits a restore.
        auto& state=const_cast<AppState&>(controller.state()); state.proxy=ProxyState::Owned;
        emit controller.stateChanged(); QVERIFY(!submit->isEnabled());
        auto* stop=dialog->findChild<QPushButton*>("backupStopCore"); QVERIFY(stop->isVisible()); QVERIFY(stop->isEnabled());
        const auto blockedOutput=qEnvironmentVariable("MLASH_UI_SCREENSHOTS");
        if (!blockedOutput.isEmpty()) { QVERIFY(QDir().mkpath(blockedOutput)); QVERIFY(dialog->grab().save(blockedOutput+"/restore-stop-required.png")); }

        stop->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000); QVERIFY(finished.last()[0].toBool());
        QVERIFY(dialog); QCOMPARE(controller.profiles().size(),1); QCOMPARE(list->item(1)->checkState(),Qt::Checked);
        QTRY_VERIFY(submit->isEnabled()); QVERIFY(stop->isHidden()); finished.clear();
        // Explicit stop is its own committed operation; compare restore failure to this checkpoint.
        QVERIFY(readFile(target.path()+"/state.dat",before).ok);
        const auto output=qEnvironmentVariable("MLASH_UI_SCREENSHOTS"); if (!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(dialog->grab().save(output+"/selected-restore.png")); }
        platform->failSealAt=platform->seals+2; submit->click(); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(!finished.last()[0].toBool()); QVERIFY(dialog); QCOMPARE(list->item(1)->checkState(),Qt::Checked); QCOMPARE(controller.profiles().size(),1); QCOMPARE(QDir(target.path()+"/profiles").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size(),1); QVERIFY(readFile(target.path()+"/state.dat",after).ok); QCOMPARE(after,before); platform->failSealAt=-1; submit->click(); QTRY_VERIFY_WITH_TIMEOUT(dialog.isNull(),15000); QCOMPARE(controller.profiles().size(),2); QCOMPARE(controller.profiles()[0].id,existing.id); QCOMPARE(controller.profiles()[1].name,names[0]); QVERIFY(controller.profiles()[1].id!=first.id&&controller.profiles()[1].id!=second.id); QCOMPARE(QDir(target.path()+"/profiles").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size(),2); QCOMPARE(platform->writes,0);
    }
    void authenticatedPortableEncryption() {
#ifndef Q_OS_WIN
        QSKIP("Windows CNG implementation only");
#else
        QByteArray first, second, plain("old"), encrypted;
        QVERIFY(sealPortable("private payload",password,first).ok); QVERIFY(sealPortable("private payload",password,second).ok); QVERIFY(first != second); QVERIFY(!first.contains("private payload"));
        QVERIFY(openPortable(first,password,plain).ok); QCOMPARE(plain,QByteArray("private payload"));
        QVERIFY(!openPortable(first,"incorrect password",plain).ok); QVERIFY(plain.isEmpty());
        auto damaged = first; damaged[damaged.size()-1] = damaged.back() ^ 1; QVERIFY(!openPortable(damaged,password,plain).ok); QVERIFY(plain.isEmpty());
        damaged = first; damaged[24] = damaged[24] ^ 1; QVERIFY(!openPortable(damaged,password,plain).ok);
        QVERIFY(!sealPortable("payload","short",encrypted).ok); QVERIFY(encrypted.isEmpty()); QVERIFY(!openPortable("bad",password,plain).ok);
#endif
    }
    void roundTripAndNoSystemIntent() {
        QTemporaryDir data, target; auto platform = std::make_shared<BackupPlatform>(); ProfileStore source(data.path(),platform), destination(target.path(),platform); QVERIFY(source.load().result.ok); QVERIFY(destination.load().result.ok);
        const auto profile = seed(source); QVERIFY(isId(profile.id)); Settings original; original.homeSummaryMask=5; original.mixedPort = 19001; original.desiredProxy = true; original.autoStart = true; QVERIFY(source.save(original,{profile}).ok);
        const auto archive = data.path()+"/snapshot.mlashbackup"; auto result = BackupArchive::create(source,original,{profile},archive,password); QVERIFY2(result.ok,qUtf8Printable(result.error)); QByteArray bytes; QVERIFY(readFile(archive,bytes).ok); QVERIFY(!bytes.contains("node-private-secret")); QVERIFY(!bytes.contains("subscription-private-token"));
        const auto preview = BackupArchive::inspect(archive,password); QVERIFY(preview.ok); QCOMPARE(preview.data.value("profiles").toArray().size(),1); QVERIFY(!QJsonDocument(preview.data).toJson().contains("private-token"));
        Settings current; current.autoStart = false; current.corePath = QString(MLASH_SOURCE_DIR)+"/cores/mihomo.exe"; current.hotkeys={{"show-proxy","Ctrl+Alt+P"}}; current.previousCorePath="local-machine-previous.exe";
        auto restored = BackupArchive::prepareRestore(destination,platform,current,{},archive,password,preview.data.value("sha256").toString(),true); QVERIFY2(restored.snapshot.result.ok,qUtf8Printable(restored.snapshot.result.error)); QCOMPARE(restored.snapshot.profiles.size(),1); const auto copy = restored.snapshot.profiles[0];
        QVERIFY(copy.id != profile.id); QVERIFY(copy.candidate != profile.candidate); QVERIFY(copy.active != profile.active); QCOMPARE(copy.source,profile.source); QCOMPARE(copy.customRules,profile.customRules); QCOMPARE(copy.proxyEdits,profile.proxyEdits); QVERIFY(!copy.autoUpdate);
        QCOMPARE(restored.snapshot.settings.homeSummaryMask,5); QCOMPARE(restored.snapshot.settings.mixedPort,19001); QVERIFY(!restored.snapshot.settings.startCore); QVERIFY(!restored.snapshot.settings.desiredProxy); QVERIFY(!restored.snapshot.settings.restoreProxy); QVERIFY(!restored.snapshot.settings.autoStart); QVERIFY(restored.snapshot.settings.activeProfile.isEmpty()); QCOMPARE(restored.snapshot.settings.corePath,current.corePath);
        QCOMPARE(restored.snapshot.settings.hotkeys,current.hotkeys);
        QCOMPARE(restored.snapshot.settings.previousCorePath,current.previousCorePath);
        QVERIFY(destination.save(restored.snapshot.settings,restored.snapshot.profiles).ok); QVERIFY(destination.load().result.ok);
        QByteArray candidate, active; QVERIFY(readFile(destination.versionRoot(copy.id,copy.candidate)+"/source.yaml",candidate).ok); QVERIFY(readFile(destination.versionRoot(copy.id,copy.active)+"/source.yaml",active).ok); QCOMPARE(candidate,yaml+"# candidate\n"); QCOMPARE(active,yaml);
        const auto prepared = destination.prepareExisting(copy,copy.candidate,restored.snapshot.settings,19090,QString(64,'c')); QVERIFY(prepared.result.ok);
        MihomoClient api; CoreManager core(&api,platform); bool checked = false; core.validate(current.corePath,prepared.configPath,prepared.workPath,[&](Result validation) { QVERIFY2(validation.ok,qUtf8Printable(validation.error)); checked = true; }); QTRY_VERIFY_WITH_TIMEOUT(checked,10000); QCOMPARE(platform->writes,0);
    }
    void providerAndLocalFileMigration() {
        QTemporaryDir data, target; auto platform = std::make_shared<BackupPlatform>(); ProfileStore source(data.path(),platform), destination(target.path(),platform); QVERIFY(source.load().result.ok); QVERIFY(destination.load().result.ok);
        Profile profile; profile.id = newId(); profile.name = "Local"; profile.sourceType = "file"; profile.source = "C:/unavailable-on-other-account/config.yaml";
        const QByteArray config = "proxy-providers: {nodes: {type: http, url: 'https://example.invalid/nodes'}}\nproxy-groups: [{name: Test, type: select, use: [nodes]}]\nrules: [\"MATCH,Test\"]\n";
        const QByteArray payload = "proxies: [{name: Sample, type: http, server: example.test, port: 443}]\n";
        auto prepared = source.prepare(profile,config,Settings{},19090,QString(64,'a')); QVERIFY(prepared.result.ok); const auto provider = prepared.result.data.value("providers").toArray()[0].toObject(); QVERIFY(writeAtomic(prepared.workPath+'/'+provider.value("path").toString(),payload).ok); QVERIFY(source.recordProviderSnapshot(prepared.workPath,provider,payload).ok);
        const auto before = source.inspectProviders(prepared.profile,prepared.version,Settings{}); QVERIFY(before.ok);
        const auto path = data.path()+"/local.mlashbackup"; QVERIFY(BackupArchive::create(source,Settings{},{prepared.profile},path,password).ok); auto preview = BackupArchive::inspect(path,password); QVERIFY(preview.ok);
        auto restored = BackupArchive::prepareRestore(destination,platform,Settings{},{},path,password,preview.data.value("sha256").toString(),false); QVERIFY2(restored.snapshot.result.ok,qUtf8Printable(restored.snapshot.result.error)); const auto copy = restored.snapshot.profiles[0]; QByteArray bytes; QVERIFY(readFile(copy.source,bytes).ok); QCOMPARE(bytes,config); QVERIFY(copy.source.startsWith(target.path()));
        const auto after = destination.inspectProviders(copy,copy.candidate,Settings{}); QVERIFY(after.ok); QCOMPARE(after.data.value("providers").toArray()[0].toObject().value("updated_utc"),before.data.value("providers").toArray()[0].toObject().value("updated_utc")); QVERIFY(readFile(destination.versionRoot(copy.id,copy.candidate)+"/work/"+provider.value("path").toString(),bytes).ok); QCOMPARE(bytes,payload);
        QVERIFY(writeAtomic(prepared.workPath+'/'+provider.value("path").toString(),payload+"# changed\n").ok); QVERIFY(!BackupArchive::create(source,Settings{},{prepared.profile},data.path()+"/damaged.mlashbackup",password).ok); QVERIFY(!QFileInfo::exists(data.path()+"/damaged.mlashbackup"));
    }
    void invalidArchiveNeverChangesIndex() {
        QTemporaryDir data, target; auto platform = std::make_shared<BackupPlatform>(); ProfileStore source(data.path(),platform), destination(target.path(),platform); QVERIFY(source.load().result.ok); QVERIFY(destination.load().result.ok); QVERIFY(destination.save(Settings{},{}).ok);
        const auto profile = seed(source); const auto path = data.path()+"/snapshot.mlashbackup"; QVERIFY(BackupArchive::create(source,Settings{},{profile},path,password).ok); const auto preview = BackupArchive::inspect(path,password); QVERIFY(preview.ok); QByteArray before; QVERIFY(readFile(target.path()+"/state.dat",before).ok);
        auto restored = BackupArchive::prepareRestore(destination,platform,Settings{},{},path,"incorrect password",preview.data.value("sha256").toString(),true); QVERIFY(!restored.snapshot.result.ok);
        restored = BackupArchive::prepareRestore(destination,platform,Settings{},{},path,password,"changed",true); QVERIFY(!restored.snapshot.result.ok);
        QByteArray encrypted, plain; QVERIFY(readFile(path,encrypted).ok); QVERIFY(openPortable(encrypted,password,plain).ok); auto archive = QJsonDocument::fromJson(plain).object(); auto bundles = archive.value("profiles").toArray(); auto bundle = bundles[0].toObject(); auto versions = bundle.value("versions").toArray(); auto version = versions[0].toObject(); version["providers"] = QJsonObject{{"../../outside",QString::fromLatin1(QByteArray("hidden").toBase64())}}; versions[0] = version; bundle["versions"] = versions; bundles[0] = bundle; archive["profiles"] = bundles;
        QVERIFY(sealPortable(QJsonDocument(archive).toJson(),password,encrypted).ok); QVERIFY(writeAtomic(path,encrypted).ok); const auto badPreview = BackupArchive::inspect(path,password); QVERIFY(badPreview.ok);
        restored = BackupArchive::prepareRestore(destination,platform,Settings{},{},path,password,badPreview.data.value("sha256").toString(),true); QVERIFY(!restored.snapshot.result.ok); QVERIFY(restored.createdProfiles.isEmpty()); QByteArray after; QVERIFY(readFile(target.path()+"/state.dat",after).ok); QCOMPARE(after,before); QVERIFY(QDir(target.path()+"/profiles").entryList(QDir::Dirs|QDir::NoDotAndDotDot).isEmpty()); QVERIFY(!QFileInfo::exists(target.path()+"/outside")); QCOMPARE(platform->writes,0);
    }
    void controllerCommitFailureAndDialogPreview() {
        QTemporaryDir data, target; auto platform = std::make_shared<BackupPlatform>(); ProfileStore source(data.path(),platform), destination(target.path(),platform); QVERIFY(source.load().result.ok); QVERIFY(destination.load().result.ok); QVERIFY(destination.save(Settings{},{}).ok); const auto profile = seed(source); const auto path = data.path()+"/snapshot.mlashbackup"; QVERIFY(BackupArchive::create(source,Settings{},{profile},path,password).ok); const auto preview = BackupArchive::inspect(path,password); QVERIFY(preview.ok);
        AppController controller(target.path(),true,nullptr,platform); QSignalSpy initialized(&controller,&AppController::initialized), finished(&controller,&AppController::operationFinished); controller.initialize(); QTRY_COMPARE(initialized.size(),1); QByteArray before; QVERIFY(readFile(target.path()+"/state.dat",before).ok);
        platform->failSealAt = platform->seals+2; controller.restoreBackup(path,password,preview.data.value("sha256").toString(),true); QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000); QVERIFY(!finished[0][0].toBool()); QVERIFY(controller.profiles().isEmpty()); QByteArray after; QVERIFY(readFile(target.path()+"/state.dat",after).ok); QCOMPARE(after,before); QVERIFY(QDir(target.path()+"/profiles").entryList(QDir::Dirs|QDir::NoDotAndDotDot).isEmpty());
        platform->failSealAt = -1; QPointer<BackupDialog> dialog = new BackupDialog(&controller,true); dialog->show(); dialog->findChild<QLineEdit*>("backupPath")->setText(path); dialog->findChild<QLineEdit*>("backupPassword")->setText(password); QVERIFY(!dialog->findChild<QPushButton*>("submitBackup")->isEnabled()); dialog->findChild<QPushButton*>("previewBackup")->click(); QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QPushButton*>("submitBackup")->isEnabled(),15000); QVERIFY(dialog->findChild<QListWidget*>("backupSelection")->item(0)->text().contains(profile.name)); dialog->findChild<QLineEdit*>("backupPassword")->setText("changed password"); QVERIFY(!dialog->findChild<QPushButton*>("submitBackup")->isEnabled());
        dialog->findChild<QLineEdit*>("backupPassword")->setText(password); dialog->findChild<QPushButton*>("previewBackup")->click(); QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QPushButton*>("submitBackup")->isEnabled(),15000); dialog->findChild<QPushButton*>("submitBackup")->click(); QTRY_VERIFY_WITH_TIMEOUT(dialog.isNull(),15000); QCOMPARE(controller.profiles().size(),1); QVERIFY(!controller.state().ready); QVERIFY(!controller.settings().desiredProxy); QCOMPARE(platform->writes,0);
    }
};
QTEST_MAIN(BackupTests)
#include "BackupTests.moc"
