#include "CoreUpdateDialog.h"
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
namespace mlash {
CoreUpdateDialog::CoreUpdateDialog(AppController* controller,QWidget* parent):QDialog(parent),controller_(controller) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("coreUpdateDialog"); setWindowTitle("内核管理"); resize(610,400); auto* layout=new QVBoxLayout(this); auto* note=new QLabel("选择本地Windows x64 Mihomo文件；先建立独立副本并运行版本预检，确认后再校验配置/应用。当前内核文件保留。官方版本需显式检查、下载和应用，不保证包含随包补丁；TUN仅支持随包固定内核。"); note->setWordWrap(true); layout->addWidget(note);
    auto* official=new QHBoxLayout; checkOfficial_=new QPushButton("检查官方稳定版"); checkOfficial_->setObjectName("checkOfficialCore"); downloadOfficial_=new QPushButton("下载并预检"); downloadOfficial_->setObjectName("downloadOfficialCore"); official->addWidget(checkOfficial_); official->addWidget(downloadOfficial_); official->addStretch(); layout->addLayout(official);
    connect(checkOfficial_,&QPushButton::clicked,this,[this] { previewing_=true; officialChecked_=false; token_.clear(); hint_->setText("正在检查MetaCubeX/mihomo官方稳定版本…"); updateActions(); const QPointer<CoreUpdateDialog> guard(this); controller_->checkOfficialCore([guard](Result result) { if (!guard) return; guard->previewing_=false; guard->officialChecked_=result.ok; if (result.ok) { guard->details_->setPlainText("官方稳定版："+result.data.value("version").toString()+"\n资产："+result.data.value("asset").toString()+"\nZIP SHA-256："+result.data.value("sha256").toString()+"\n当前运行内核："+guard->controller_->state().coreVersion); guard->hint_->setText("下载并预检后仍需明确应用。官方下载不替换随包本地修复内核或TUN服务。"); } else guard->hint_->setText(result.error); guard->updateActions(); }); });
    connect(downloadOfficial_,&QPushButton::clicked,this,[this] { previewing_=true; token_.clear(); hint_->setText("正在下载、核验SHA-256并预检独立副本…"); updateActions(); const QPointer<CoreUpdateDialog> guard(this); controller_->downloadOfficialCore([guard](Result result) { if (guard) guard->presentPreview(result); }); });
    auto* row=new QHBoxLayout; path_=new QLineEdit; path_->setObjectName("coreUpdatePath"); path_->setMaxLength(4096); path_->setPlaceholderText("选择本地 mihomo.exe"); row->addWidget(path_,1); browse_=new QPushButton("选择…"); row->addWidget(browse_); preview_=new QPushButton("预检版本"); preview_->setObjectName("previewCoreUpdate"); row->addWidget(preview_); layout->addLayout(row); details_=new QPlainTextEdit; details_->setObjectName("coreUpdateDetails"); details_->setReadOnly(true); details_->setPlainText("当前配置："+controller->settings().corePath+"\n运行版本："+(controller->state().coreVersion.isEmpty()?QString("未运行"):controller->state().coreVersion)); layout->addWidget(details_,1);
    auto* tools=new QHBoxLayout; rollback_=new QPushButton("预检上次内核"); rollback_->setObjectName("rollbackCoreUpdate"); clean_=new QPushButton("清理未使用副本"); clean_->setObjectName("cleanCoreCopies"); cancel_=new QPushButton("取消预检"); cancel_->setObjectName("cancelCorePreview"); tools->addWidget(rollback_); tools->addWidget(clean_); tools->addStretch(); tools->addWidget(cancel_); layout->addLayout(tools); hint_=new QLabel("应用前显示版本、大小与SHA-256；运行中的订阅更新保持独立。"); hint_->setObjectName("coreUpdateHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); layout->addWidget(hint_); auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close); buttons->button(QDialogButtonBox::Close)->setText("关闭"); apply_=buttons->addButton("应用此版本",QDialogButtonBox::AcceptRole); apply_->setObjectName("applyCoreUpdate"); apply_->setProperty("primary",true); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(apply_,&QPushButton::clicked,this,[this] { const auto token=token_; token_.clear(); saving_=true; updateActions(); controller_->applyCoreUpdate(token); }); connect(preview_,&QPushButton::clicked,this,&CoreUpdateDialog::inspect); connect(browse_,&QPushButton::clicked,this,[this] { const auto file=QFileDialog::getOpenFileName(this,"选择本地Mihomo",{},"Windows程序 (*.exe)"); if (!file.isEmpty()) path_->setText(file); }); connect(path_,&QLineEdit::textChanged,this,[this] { token_.clear(); updateActions(); });
    connect(rollback_,&QPushButton::clicked,this,[this] { path_->setText(controller_->settings().previousCorePath); inspect(); }); connect(cancel_,&QPushButton::clicked,controller,&AppController::cancelCoreUpdatePreview); connect(clean_,&QPushButton::clicked,this,[this] { cleaning_=true; updateActions(); controller_->cleanCoreCopies(); });
    connect(controller,&AppController::stateChanged,this,[this] { updateActions(); }); connect(controller,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (cleaning_) { cleaning_=false; hint_->setText(ok?"已清理未使用且清单完整的内核副本；当前、上次和已预检副本保留":message); updateActions(); } else if (saving_) { saving_=false; if (ok) accept(); else { hint_->setText(message+"；重新预检后可再试"); updateActions(); } } }); updateActions();
}
CoreUpdateDialog::~CoreUpdateDialog() { if (previewing_) controller_->cancelCoreUpdatePreview(); }
void CoreUpdateDialog::inspect() {
    token_.clear(); previewing_=true; hint_->setText("正在建立副本并预检版本…"); updateActions(); const QPointer<CoreUpdateDialog> guard(this);
    controller_->previewCoreUpdate(path_->text().trimmed(),[guard](Result result) { if (guard) guard->presentPreview(result); });
}
void CoreUpdateDialog::presentPreview(Result result) {
    previewing_=false;
    if (!result.ok) { hint_->setText(result.error); updateActions(); return; }
    path_->setText(result.data.value("path").toString()); token_=result.data.value("token").toString();
    details_->setPlainText("预检版本："+result.data.value("version").toString()+"\n文件大小："+QString::number(result.data.value("bytes").toDouble(),'f',0)+" B\nSHA-256："+result.data.value("sha256").toString()+"\n独立副本："+result.data.value("path").toString());
    hint_->setText("预检完成；应用时还会校验配置与运行版本。失败恢复原内核，未使用订阅更新保持不变。"); updateActions();
}
void CoreUpdateDialog::updateActions() { const bool available=!controller_->state().busy&&!controller_->state().readOnly&&!controller_->state().exiting&&!previewing_&&!saving_&&!cleaning_; checkOfficial_->setEnabled(available); downloadOfficial_->setEnabled(available&&officialChecked_); preview_->setEnabled(available&&!path_->text().trimmed().isEmpty()); browse_->setEnabled(available); path_->setEnabled(available); apply_->setEnabled(available&&!token_.isEmpty()); rollback_->setEnabled(available&&!controller_->settings().previousCorePath.isEmpty()&&controller_->settings().previousCorePath!=controller_->settings().corePath); clean_->setEnabled(available); cancel_->setEnabled(previewing_); }
}
