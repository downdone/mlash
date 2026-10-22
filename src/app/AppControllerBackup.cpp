#include "AppController.h"
#include "Async.h"
#include "storage/Storage.h"
#include "profile/BackupArchive.h"
#include "profile/RemoteBackup.h"
#include "profile/LocalBackup.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QTcpServer>

namespace mlash {
void AppController::updateLocalBackupConfig(const QJsonObject& config,const QJsonObject& expected) {
    if (localBackupUserFields(settings_.localBackup)!=localBackupUserFields(expected)) {
        emit operationFinished(false,"本地备份设置已在其他窗口修改，请重新打开；草稿保留"); return;
    }
    const auto checked=validateLocalBackup(config);
    if (!checked.ok) { emit operationFinished(false,checked.error); return; }
    if (!beginPreferences("正在保存本地备份计划")) return;
    auto next=settings_; next.localBackup=localBackupUserFields(config);
    if (!next.localBackup.isEmpty()) {
        next.localBackup["next_due_utc"]=QDateTime::currentDateTimeUtc().addSecs(next.localBackup.value("interval_hours").toInt()*3600).toString(Qt::ISODate);
        if (settings_.localBackup.contains("last_success_utc")) next.localBackup["last_success_utc"]=settings_.localBackup.value("last_success_utc");
    }
    saveSnapshot(next,profiles_,[this](Result result) {
        if (result.ok) {
            diagnostics_.setSecrets({settings_.localBackup.value("password").toString()}); localBackupRetryAfter_={};
            if (!smoke_&&!startupSuppressed_&&!settings_.localBackup.isEmpty()) localBackupTimer_.start(); else localBackupTimer_.stop();
        }
        finish(result);
    });
}
void AppController::runLocalMaintenance() {
    const auto now=QDateTime::currentDateTimeUtc();
    if (smoke_||startupSuppressed_||readOnly_||state_.busy||state_.exiting||exitPending_ ||
        (localBackupRetryAfter_.isValid()&&now<localBackupRetryAfter_) || !LocalBackup::due(settings_.localBackup,now)) return;
    startLocalBackup(true);
}
void AppController::startLocalBackup(bool scheduled) {
    if (settings_.localBackup.isEmpty()) { if (!scheduled) emit operationFinished(false,"请先保存本地备份设置"); return; }
    if (!beginPreferences("正在创建本地加密备份")) return;
    backgroundOperation_=scheduled;
    const auto store=store_; const auto settings=settings_; const auto profiles=profiles_;
    runTask(this,[store,settings,profiles,scheduled] { return LocalBackup::create(*store,settings,profiles,scheduled?"scheduled":"manual"); },[this](Result result) {
        const auto now=QDateTime::currentDateTimeUtc(); localBackupRetryAfter_=now.addSecs(result.ok?60:900);
        if (!result.ok) { finish(result); return; }
        auto next=settings_;
        next.localBackup["last_success_utc"]=now.toString(Qt::ISODate);
        next.localBackup["next_due_utc"]=now.addSecs(next.localBackup.value("interval_hours").toInt()*3600).toString(Qt::ISODate);
        saveSnapshot(next,profiles_,[this](Result saved) {
            if (!saved.ok) localBackupRetryAfter_=QDateTime::currentDateTimeUtc().addSecs(900);
            finish(saved.ok?Result{true,{},{{"notice","本地备份已创建并校验"}}}:Result::fail("本地备份已保留，但计划状态保存失败；稍后重试"));
        });
    });
}
void AppController::inspectLocalBackups(Completion done) {
    if (state_.busy||state_.exiting||localBackupInspectPending_) { done(Result::fail("当前操作尚未结束，请稍后刷新")); return; }
    localBackupInspectPending_=true; const auto store=store_;
    runTask(this,[store] { return LocalBackup::list(*store); },[this,done=std::move(done)](Result result) mutable {
        localBackupInspectPending_=false; done(result);
    });
}
void AppController::protectMajorChange(std::function<void()> action) {
    if (changeBackupDone_ || !settings_.localBackup.value("before_changes").toBool()) { action(); return; }
    const auto store=store_; const auto settings=settings_; const auto profiles=profiles_;
    runTask(this,[store,settings,profiles] { return LocalBackup::create(*store,settings,profiles,"before-change"); },
        [this,action=std::move(action)](Result result) mutable {
            if (!result.ok) { finish(Result::fail("变更前备份失败，未执行变更："+result.error)); return; }
            if (exitPending_||state_.exiting) { finish(Result::fail("备份已保留；退出前未继续变更")); return; }
            changeBackupDone_=true; action();
        });
}
void AppController::exportBackup(const QString& path, const QString& password, bool includeProfiles) {
    if (!begin("正在加密备份订阅与偏好")) return;
    const auto store = store_; const auto settings = settings_; const auto profiles = profiles_;
    runTask(this, [store, settings, profiles, path, password, includeProfiles] { return BackupArchive::create(*store, settings, profiles, path, password, includeProfiles); }, [this](Result result) { finish(result); });
}

void AppController::previewBackup(const QString& path, const QString& password, Completion done) {
    if (backupPreviewPending_ || state_.busy || state_.exiting) { done(Result::fail("当前操作尚未结束")); return; }
    backupPreviewPending_ = true; const auto generation = state_.generation;
    runTask(this, [path, password] { return BackupArchive::inspect(path, password); }, [this, generation, done = std::move(done)](Result result) mutable { backupPreviewPending_ = false; done(generation == state_.generation && !state_.exiting ? result : Result::fail("状态已变化，请重新预览备份")); });
}

void AppController::restoreBackup(const QString& path, const QString& password, const QString& expectedHash, bool restoreSettings, const QJsonValue& selectedProfiles) {
    if (state_.core != CoreState::Stopped || state_.proxy == ProxyState::Owned) { emit operationFinished(false, "请先停止内核并释放 Mlash 系统代理，再恢复备份"); return; }
    if (!begin("正在校验并恢复备份；现有订阅保持")) return;
    protectMajorChange([this,path,password,expectedHash,restoreSettings,selectedProfiles] {
    const auto store = store_; const auto platform = platform_; const auto settings = settings_; const auto profiles = profiles_;
    runTask(this, [store, platform, settings, profiles, path, password, expectedHash, restoreSettings, selectedProfiles] { return BackupArchive::prepareRestore(*store, platform, settings, profiles, path, password, expectedHash, restoreSettings, selectedProfiles); }, [this, store](BackupRestore restored) {
        if (!restored.snapshot.result.ok) { finish(restored.snapshot.result); return; }
        saveSnapshot(restored.snapshot.settings, restored.snapshot.profiles, [this, store, ids = restored.createdProfiles](Result result) {
            if (result.ok) { finish(result); return; }
            runTask(this, [store, ids, result] { for (const auto& id : ids) store->removeProfile(id); return result; }, [this](Result failed) { finish(failed); });
        });
    });
    });
}

void AppController::rememberRemoteSecrets() { const auto config=settings_.remoteBackup; diagnostics_.setSecrets({config.value("password").toString(),config.value("backup_password").toString(),config.value("username").toString(),config.value("url").toString()}); }

void AppController::updateRemoteBackupConfig(const QJsonObject& config,const QJsonObject& expected) {
    if (remoteBackupUserFields(settings_.remoteBackup)!=remoteBackupUserFields(expected)) { emit operationFinished(false,"远端备份设置已在其他窗口修改，请重新打开；草稿保留"); return; } auto nextConfig=remoteBackupUserFields(config); const auto checked=validateRemoteBackup(nextConfig); if (!checked.ok) { emit operationFinished(false,checked.error); return; } if (!beginPreferences("正在加密保存远端备份设置")) { emit operationFinished(false,"当前不能保存远端备份设置"); return; }
    if (!nextConfig.isEmpty()) { nextConfig["next_due_utc"]=QDateTime::currentDateTimeUtc().addSecs(nextConfig.value("interval_hours").toInt(24)*3600).toString(Qt::ISODate); if (nextConfig.value("url")==settings_.remoteBackup.value("url")) nextConfig["last_success_utc"]=settings_.remoteBackup.value("last_success_utc").toString(); }
    auto next=settings_; next.remoteBackup=nextConfig; saveSnapshot(next,profiles_,[this](Result result) { if (result.ok) { rememberRemoteSecrets(); remoteRetryAfter_={}; if (!smoke_&&!startupSuppressed_&&settings_.remoteBackup.value("enabled").toBool()) remoteTimer_.start(); else remoteTimer_.stop(); remoteBackupStatus_={}; emit remoteBackupChanged(); } finish(result); });
}

void AppController::runRemoteMaintenance() { if (!remoteInitialized_||smoke_||startupSuppressed_||readOnly_||state_.busy||state_.exiting||exitPending_||!settings_.remoteBackup.value("enabled").toBool()) return; const auto now=QDateTime::currentDateTimeUtc(),due=QDateTime::fromString(settings_.remoteBackup.value("next_due_utc").toString(),Qt::ISODate); if (due.isValid()&&now>=due&&(!remoteRetryAfter_.isValid()||now>=remoteRetryAfter_)) startRemoteBackup(true); }

void AppController::remoteRoute(const QJsonObject& config,std::function<void(Result,int)> done) {
    if (config.value("route").toString("direct")=="direct") { done({},0); return; } if (!state_.ready||core_.processId()<=0) { done(Result::fail("备份代理路径的内核未就绪，未回退直连"),0); return; } const auto platform=platform_; const auto pid=core_.processId(); const auto port=settings_.mixedPort; const auto generation=state_.generation;
    runTask(this,[platform,pid,port] { return platform->ownsTcpPort(pid,port); },[this,generation,port,done=std::move(done)](Result result) mutable { if (!result.ok||!result.data.value("owned").toBool()||generation!=state_.generation||!state_.ready||remoteCancelled_||exitPending_) done(Result::fail("备份代理端口归属或当前任务已失效，未发送请求"),0); else done({},port); });
}

void AppController::startRemoteBackup(bool scheduled) {
    const auto config=settings_.remoteBackup; const auto checked=validateRemoteBackup(config); if (!checked.ok||config.isEmpty()) { if (!scheduled) emit operationFinished(false,checked.ok?"请先配置WebDAV连接和备份密码":checked.error); return; } if (!beginPreferences("正在创建并上传加密备份")) { if (!scheduled) emit operationFinished(false,"当前操作尚未结束"); return; } backgroundOperation_=scheduled; remoteActive_=true; remoteCancelled_=remoteMetadata_=false; remoteBackupStatus_={{"busy",true},{"cancellable",true},{"message","正在创建本地加密备份…"}}; emit remoteBackupChanged(); const auto store=store_; const auto settings=settings_; const auto profiles=profiles_;
    runTask(this,[store,settings,profiles] { return createRemoteBackup(*store,settings,profiles); },[this,config](RemoteBackupPayload payload) { if (!payload.result.ok||remoteCancelled_||exitPending_) { completeRemoteUpload(payload.result.ok?Result{false,"远端备份已取消",{{"cancelled",true}}}:payload.result,payload.path); return; } remoteBackupStatus_["message"]="正在上传并核对远端版本…"; emit remoteBackupChanged(); remoteRoute(config,[this,config,payload](Result route,int port) { if (!route.ok||remoteCancelled_||exitPending_) { completeRemoteUpload(route.ok?Result{false,"远端备份已取消",{{"cancelled",true}}}:route,payload.path); return; } remoteClient_->upload(config,port,payload.name,payload.bytes,[this,path=payload.path](Result result,QByteArray) { completeRemoteUpload(result,path); }); }); });
}

void AppController::completeRemoteUpload(Result result,const QString& localPath) {
    remoteMetadata_=true; remoteBackupStatus_["cancellable"]=false; remoteBackupStatus_["local_path"]=localPath; emit remoteBackupChanged(); const auto now=QDateTime::currentDateTimeUtc(); auto next=settings_; if (!next.remoteBackup.isEmpty()) { next.remoteBackup["next_due_utc"]=now.addSecs(result.ok?next.remoteBackup.value("interval_hours").toInt(24)*3600:900).toString(Qt::ISODate); if (result.ok) next.remoteBackup["last_success_utc"]=now.toString(Qt::ISODate); } remoteRetryAfter_=now.addSecs(result.ok?60:900); saveSnapshot(next,profiles_,[this,result](Result saved) { if (!saved.ok) remoteRetryAfter_=QDateTime::currentDateTimeUtc().addSecs(900); finishRemoteOperation(saved.ok?result:Result::fail(result.ok?"远端备份已提交，但本机计划保存失败；本地备份保留":result.error+"；本机重试计划保存失败")); });
}

void AppController::finishRemoteOperation(Result result) { remoteActive_=remoteMetadata_=false; remoteBackupStatus_["busy"]=false; remoteBackupStatus_["cancellable"]=false; remoteBackupStatus_["message"]=result.ok?QString("远端操作完成"):result.error; remoteBackupStatus_["cancelled"]=result.data.value("cancelled"); if (result.ok) for (auto item=result.data.begin();item!=result.data.end();++item) remoteBackupStatus_[item.key()]=item.value(); emit remoteBackupChanged(); if (result.data.value("cancelled").toBool()) finish({true,{},{{"notice","远端操作已取消"}}}); else finish(result); }

void AppController::cancelRemoteBackup() { if (!remoteActive_||remoteMetadata_) return; remoteCancelled_=true; remoteBackupStatus_["message"]="正在取消远端操作…"; emit remoteBackupChanged(); remoteClient_->cancel(); }

void AppController::inspectRemoteBackups(Completion done) {
    if (settings_.remoteBackup.isEmpty()||state_.busy||state_.exiting||state_.readOnly) { done(Result::fail("请先保存连接设置，等待当前操作结束")); return; } if (!beginPreferences("正在读取WebDAV备份目录")) { done(Result::fail("当前不能读取远端目录")); return; } remoteActive_=true; remoteCancelled_=remoteMetadata_=false; remoteBackupStatus_={{"busy",true},{"cancellable",true},{"message","正在处理远端请求…"}}; emit remoteBackupChanged(); const auto config=settings_.remoteBackup;
    remoteRoute(config,[this,config,done=std::move(done)](Result route,int port) mutable { if (!route.ok) { finishRemoteOperation(route); done(route); return; } remoteClient_->list(config,port,[this,done=std::move(done)](Result result,QByteArray) mutable { finishRemoteOperation(result); done(result); }); });
}

void AppController::downloadRemoteBackup(const QString& file,const QString& destination) {
    if (settings_.remoteBackup.isEmpty()||!remoteBackupFileName(file)) { emit operationFinished(false,"请选择有效远端备份"); return; } if (!beginPreferences("正在下载并校验加密备份")) { emit operationFinished(false,"当前操作尚未结束"); return; } remoteActive_=true; remoteCancelled_=remoteMetadata_=false; remoteBackupStatus_={{"busy",true},{"cancellable",true},{"message","正在处理远端请求…"}}; emit remoteBackupChanged(); const auto config=settings_.remoteBackup; const auto store=store_;
    remoteRoute(config,[this,config,store,file,destination](Result route,int port) { if (!route.ok) { finishRemoteOperation(route); return; } remoteClient_->download(config,port,file,[this,store,config,destination](Result result,QByteArray bytes) { if (!result.ok||remoteCancelled_||exitPending_) { finishRemoteOperation(result.ok?Result{false,"下载已取消",{{"cancelled",true}}}:result); return; } remoteMetadata_=true; remoteBackupStatus_["cancellable"]=false; emit remoteBackupChanged(); runTask(this,[store,config,destination,bytes=std::move(bytes)] { return saveRemoteBackupDownload(*store,bytes,config.value("backup_password").toString(),destination); },[this,destination](Result saved) { finishRemoteOperation(saved); if (saved.ok) emit remoteBackupDownloaded(destination); }); }); });
}

}
