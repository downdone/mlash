#include "AppUpdateDialog.h"
#include "app/AppController.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace mlash {
AppUpdateDialog::AppUpdateDialog(AppController* controller,QWidget* parent)
    :TaskDialog(parent),controller_(controller) {
    setObjectName("appUpdateDialog"); setWindowTitle("Mlash 更新"); resize(470,230);
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(18,16,18,16); layout->setSpacing(10);
    auto* title=new QLabel("应用更新",this); title->setObjectName("dialogTitle"); layout->addWidget(title);
    version_=new QLabel(QStringLiteral("当前版本：v" MLASH_VERSION),this); layout->addWidget(version_);
    status_=new QLabel("从 Mlash 官方发布检查新版本。",this); status_->setWordWrap(true); layout->addWidget(status_);
    detail_=new QLabel("更新包会先验证大小和 SHA-256，保存为独立候选；当前程序继续运行。",this);
    detail_->setProperty("secondary",true); detail_->setWordWrap(true); layout->addWidget(detail_); layout->addStretch();
    auto* actions=new QHBoxLayout; actions->setSpacing(8); layout->addLayout(actions);
    check_=new QPushButton("检查更新",this); actions->addWidget(check_);
    download_=new QPushButton("下载并验证",this); download_->setEnabled(false); actions->addWidget(download_);
    open_=new QPushButton("打开候选包",this); open_->setEnabled(false); actions->addWidget(open_);
    release_=new QPushButton("发行说明",this); release_->setEnabled(false); actions->addWidget(release_);
    cancel_=new QPushButton("取消",this); cancel_->setEnabled(false); actions->addWidget(cancel_);
    connect(check_,&QPushButton::clicked,this,&AppUpdateDialog::check);
    connect(download_,&QPushButton::clicked,this,&AppUpdateDialog::download);
    connect(open_,&QPushButton::clicked,this,[this] { const auto path=controller_->appUpdateCandidate().value("path").toString(); if (QFileInfo::exists(path)) QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
    connect(release_,&QPushButton::clicked,this,[this] { const auto url=available_.value("release_url").toString(); if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url)); });
    connect(cancel_,&QPushButton::clicked,this,[this] { if (working_) controller_->cancelAppUpdate(); });
    if (!controller_->appUpdateCandidate().isEmpty()) showCandidate(controller_->appUpdateCandidate());
}
AppUpdateDialog::~AppUpdateDialog() { if (working_) controller_->cancelAppUpdate(); }

void AppUpdateDialog::check() {
    working_=true; available_={}; check_->setEnabled(false); download_->setEnabled(false); open_->setEnabled(false); release_->setEnabled(false); cancel_->setEnabled(true);
    status_->setText("正在检查发布版本…");
    QPointer<AppUpdateDialog> self(this);
    controller_->checkAppUpdate([self](Result result) {
        if (!self) return;
        self->working_=false; self->check_->setEnabled(true); self->cancel_->setEnabled(false);
        if (!result.ok) { self->status_->setText(result.error); return; }
        if (result.data.value("up_to_date").toBool()) { self->status_->setText("当前已是最新可验证版本。"); return; }
        self->available_=result.data;
        self->status_->setText(QString("发现 v%1%2").arg(result.data.value("version").toString(),
            result.data.value("prerelease").toBool()?"（预发布）":""));
        self->download_->setEnabled(true); self->release_->setEnabled(true);
    });
}
void AppUpdateDialog::download() {
    working_=true; check_->setEnabled(false); download_->setEnabled(false); cancel_->setEnabled(true);
    status_->setText("正在下载并验证更新包…");
    QPointer<AppUpdateDialog> self(this);
    controller_->downloadAppUpdate([self](Result result) {
        if (!self) return;
        self->working_=false; self->check_->setEnabled(true); self->cancel_->setEnabled(false);
        if (!result.ok) { self->status_->setText(result.error); self->download_->setEnabled(!self->available_.isEmpty()); return; }
        self->showCandidate(result.data);
    });
}
void AppUpdateDialog::showCandidate(const QJsonObject& candidate) {
    const auto path=candidate.value("path").toString();
    open_->setEnabled(!path.isEmpty() && QFileInfo::exists(path));
    status_->setText(QString("v%1 已下载并校验。打开候选包后退出 Mlash，按便携包说明更新。").arg(candidate.value("version").toString()));
    detail_->setText("候选包位于独立更新目录；当前运行版本和内核未被替换。");
}
}
