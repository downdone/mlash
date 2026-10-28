#include "DialogDraftGuard.h"
#include "ProxyBypassDialog.h"
#include "ContextNote.h"
#include "storage/Storage.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
namespace mlash {
ProxyBypassDialog::ProxyBypassDialog(AppController* controller,QWidget* parent):QDialog(parent),controller_(controller),saved_(controller->settings().proxyBypass) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("proxyBypassDialog"); setWindowTitle("系统代理排除列表"); resize(490,370); auto* layout=new QVBoxLayout(this); layout->addWidget(contextNote("每行一个主机、IP 或通配模式。","每行一个主机、IP或通配模式，也可用分号分隔。<local> 排除不含点的本地主机名。这里只影响Windows系统代理；应用自行设置代理或TUN不受此列表控制。")); editor_=new QPlainTextEdit(saved_.split(';').join('\n')); editor_->setObjectName("proxyBypassEditor"); layout->addWidget(editor_,1); defaults_=new QPushButton("恢复默认列表"); defaults_->setObjectName("defaultProxyBypass"); layout->addWidget(defaults_,0,Qt::AlignLeft); hint_=new QLabel("系统代理已开启时立即生效，否则下次开启时生效。"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); hint_->setObjectName("proxyBypassHint"); layout->addWidget(hint_); auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); save_=buttons->button(QDialogButtonBox::Save); save_->setObjectName("saveProxyBypass"); save_->setText("保存排除列表"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(buttons,&QDialogButtonBox::accepted,this,[this] { saving_=true; updateActions(); controller_->updateProxyBypass(editor_->toPlainText(),saved_); }); connect(defaults_,&QPushButton::clicked,this,[this] { editor_->setPlainText(Settings{}.proxyBypass.split(';').join('\n')); }); connect(editor_,&QPlainTextEdit::textChanged,this,[this] { updateActions(); }); connect(controller,&AppController::stateChanged,this,[this] { updateActions(); }); connect(controller,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (!saving_) return; saving_=false; if (ok) accept(); else { hint_->setText(message); updateActions(); } }); updateActions();
    DialogDraftGuard::protect(this,[this] { const auto value=normalizeProxyBypass(editor_->toPlainText()); return !value.ok || value.data.value("bypass").toString()!=saved_; },[this] { return saving_; });
}
void ProxyBypassDialog::updateActions() { const auto checked=normalizeProxyBypass(editor_->toPlainText()); const bool available=!saving_&&!controller_->state().busy&&!controller_->state().readOnly&&!controller_->state().exiting; save_->setEnabled(available&&(!checked.ok||checked.data.value("bypass").toString()!=saved_)); editor_->setEnabled(available); defaults_->setEnabled(available); }
}
