#include "AppController.h"
#include "Async.h"

namespace mlash {
void AppController::checkAppUpdate(Completion done) {
    if (state_.exiting || state_.busy || state_.readOnly) { done(Result::fail("当前不能检查应用更新")); return; }
    appUpdate_.check(QStringLiteral(MLASH_VERSION),std::move(done));
}

void AppController::downloadAppUpdate(Completion done) {
    if (state_.exiting || state_.readOnly || appUpdate_.release().isEmpty()) { done(Result::fail("请先检查应用版本")); return; }
    if (!begin("正在下载应用更新")) { done(Result::fail("当前操作尚未结束")); return; }
    const auto token=++appUpdateGeneration_;
    const auto root=root_;
    appUpdate_.download([this,token,root,done=std::move(done)](Result result,QByteArray bytes) mutable {
        if (token!=appUpdateGeneration_ || state_.exiting) { finish(Result::fail("应用更新已取消")); done(Result::fail("应用更新已取消")); return; }
        if (!result.ok) { finish(result); done(result); return; }
        runTask(this,[root,release=result.data,bytes=std::move(bytes)]() mutable {
            return AppUpdateSource::stageArchive(bytes,release,root);
        },[this,token,done=std::move(done)](Result staged) mutable {
            if (token!=appUpdateGeneration_ || state_.exiting) staged=Result::fail("应用更新已取消；当前应用保持不变");
            if (staged.ok) appUpdateCandidate_=staged.data;
            finish(staged); done(staged);
        });
    });
}

void AppController::cancelAppUpdate() {
    ++appUpdateGeneration_;
    appUpdate_.cancel();
}
}
