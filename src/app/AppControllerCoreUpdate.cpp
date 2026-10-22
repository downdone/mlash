#include "AppController.h"
#include "core/TunPolicy.h"
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
void AppController::checkOfficialCore(Completion done) {
    if (state_.busy||state_.exiting||state_.readOnly) { done(Result::fail("当前不能检查内核版本")); return; }
    officialCore_.check(std::move(done));
}
void AppController::downloadOfficialCore(Completion done) {
    if (officialCore_.release().isEmpty()||officialCore_.busy()) { done(Result::fail("请先检查官方稳定版本")); return; }
    if (!begin("正在下载并校验官方内核")) { done(Result::fail("当前操作尚未结束")); return; }
    const auto token=++corePreviewGeneration_; const auto store=store_;
    officialCore_.download([this,token,store,done=std::move(done)](Result result,QByteArray bytes) mutable {
        if (!result.ok) { finish(result); done(result); return; }
        runTask(this,[store,release=result.data,bytes=std::move(bytes)] {
            QByteArray executable; const auto checked=OfficialCoreSource::unpack(bytes,release,executable);
            return checked.ok?CoreUpdate::stageBytes(*store,executable):checked;
        },[this,token,expected=result.data.value("version").toString(),done=std::move(done)](Result staged) mutable {
            if (token!=corePreviewGeneration_||exitPending_) staged=Result::fail("官方内核预检已取消；当前内核保持");
            if (!staged.ok) { finish(staged); done(staged); return; }
            finish({true,{},{{"notice","官方下载已校验，正在预检独立副本"}}});
            previewCoreUpdate(staged.data.value("path").toString(),std::move(done),expected);
        });
    });
}
void AppController::previewCoreUpdate(const QString& file,Completion done,const QString& expectedVersion) {
    if (state_.busy||state_.exiting||state_.readOnly) { done(Result::fail("当前不能预检内核")); return; } if (!begin("正在预检本地内核副本")) { done(Result::fail("当前操作尚未结束")); return; }
    corePreview_={}; const auto token=++corePreviewGeneration_; const auto store=store_;
    auto complete=[this,done=std::move(done)](Result result) mutable { finish(result); done(result); };
    runTask(this,[store,file] { return CoreUpdate::stage(*store,file); },[this,token,store,complete,expectedVersion](Result staged) mutable {
        if (!staged.ok||token!=corePreviewGeneration_||exitPending_) { complete(staged.ok?Result::fail("内核预检已取消"):staged); return; }
        coreVersionProbe_.inspect(staged.data.value("path").toString(),[this,token,store,staged,complete,expectedVersion](Result version) mutable { if (!version.ok||token!=corePreviewGeneration_||exitPending_) { complete(version.ok?Result::fail("内核预检已取消"):version); return; }
            if (!expectedVersion.isEmpty()&&version.data.value("version").toString()!=expectedVersion) { complete(Result::fail("程序版本与官方资产声明不符，未允许应用")); return; }
            runTask(this,[store,staged] { return CoreUpdate::verify(store->root(),staged.data); },[this,token,staged,version,complete](Result checked) mutable { if (!checked.ok||token!=corePreviewGeneration_||exitPending_) { complete(checked.ok?Result::fail("内核预检已取消"):checked); return; } corePreview_=staged.data; corePreview_["version"]=version.data.value("version"); corePreview_["token"]=newId(); complete({true,{},corePreview_}); });
        });
    });
}

void AppController::cancelCoreUpdatePreview() { ++corePreviewGeneration_; corePreview_={}; coreVersionProbe_.cancel(); officialCore_.cancel(); }

void AppController::cleanCoreCopies() { if (!begin("正在清理未使用内核副本")) return; const auto store=store_; const QStringList keep{settings_.corePath,settings_.previousCorePath,corePreview_.value("path").toString()}; runTask(this,[store,keep] { return CoreUpdate::clean(*store,keep); },[this](Result result) { finish(result); }); }

void AppController::applyCoreUpdate(const QString& token) {
    if (token.isEmpty()||corePreview_.value("token").toString()!=token) { emit operationFinished(false,"请先重新预检内核，确认版本与哈希"); return; } if (settings_.tunEnabled&&corePreview_.value("sha256").toString()!=QString::fromLatin1(TunCoreSha256)) { emit operationFinished(false,"TUN仅支持随包固定内核；请先关闭TUN，再应用其他内核。服务固定哈希不会自动改变。"); return; } if (!begin("正在校验并应用内核副本")) return; const auto record=corePreview_; corePreview_={}; const auto store=store_; const auto previous=settings_; const bool running=core_.state()==CoreState::Running;
    protectMajorChange([this,store,record,previous,running] {
    runTask(this,[store,record,previous,running] { auto result=CoreUpdate::verify(store->root(),record); if (!result.ok) return result; const auto old=CoreUpdate::stage(*store,previous.corePath); if (!old.ok&&running) return Result::fail("无法保留当前内核副本："+old.error); result.data=record; result.data["previous_path"]=old.ok&&old.data.value("path")!=record.value("path")?old.data.value("path"):QJsonValue(previous.previousCorePath); return result; },[this,store,running,previous](Result staged) {
        if (!staged.ok||exitPending_) { finish(staged.ok?Result::fail("内核更新已取消"):staged); return; } auto next=previous; next.corePath=staged.data.value("path").toString(); next.previousCorePath=staged.data.value("previous_path").toString(); const auto record=staged.data; const int index=profileIndex(settings_.activeProfile); const auto secret=createSecret(); const int port=chooseApiPort();
        const auto validate=[this,store,next,record,running](PreparedProfile prepared) {
            if (!prepared.result.ok) { finish(prepared.result); return; }
            diagnostics_.setSecrets(prepared.secrets);
            checkPrepared(prepared,[this,store,next,record,running](PreparedProfile checked) mutable {
                const auto work=checked.workPath; const bool temporary=checked.profile.id.isEmpty();
                runTask(this,[store,record,work,temporary] { const auto verified=CoreUpdate::verify(store->root(),record); if (temporary) { const auto info=QFileInfo(work); const auto base=QFileInfo(store->root()).canonicalFilePath(); const auto canonical=info.canonicalFilePath(); if (!base.isEmpty()&&!info.isSymLink()&&info.fileName().startsWith("core-check-")&&QFileInfo(canonical).absolutePath().compare(base,Qt::CaseInsensitive)==0) QDir(canonical).removeRecursively(); } return verified; },[this,next,record,running,checked](Result stable) mutable {
                    if (!checked.result.ok||!stable.ok||exitPending_) { finish(!checked.result.ok?checked.result:!stable.ok?stable:Result::fail("内核更新已取消")); return; }
                    if (!running) { saveSnapshot(next,profiles_,[this](Result saved) { finish(saved); }); return; }
                    const int index=profileIndex(checked.profile.id); if (index<0) { finish(Result::fail("运行订阅已变化")); return; } if (profiles_[index].candidate!=profiles_[index].active) checked.profile.candidate=profiles_[index].candidate; checked.expectedCoreVersion=record.value("version").toString(); transact(checked,next);
                });
            },next.corePath);
        };
        if (index>=0) { const auto profile=profiles_[index]; const auto version=profile.active.isEmpty()?profile.candidate:profile.active; runTask(this,[store,profile,version,next,port,secret] { return store->prepareExisting(profile,version,next,port,secret); },validate); }
        else runTask(this,[store,secret,port] { PreparedProfile prepared; prepared.workPath=store->root()+"/core-check-"+newId(); prepared.configPath=prepared.workPath+"/config.yaml"; prepared.secret=secret; prepared.apiPort=port; prepared.result=store->createPrivateDirectory(prepared.workPath); if (prepared.result.ok) prepared.result=writeAtomic(prepared.configPath,"mixed-port: 0\nallow-lan: false\nproxies: []\nrules: ['MATCH,DIRECT']\n"); return prepared; },validate);
    });
    });
}

}
