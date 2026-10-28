#include "ChainPreview.h"
#include "DialogDraftGuard.h"
#include "ProxyChainDialog.h"
#include "ContextNote.h"
#include "CompactComboBox.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
namespace mlash {
ProxyChainDialog::ProxyChainDialog(AppController* controller,const QString& id,QWidget* parent):QDialog(parent),controller_(controller),id_(id) {
    QString name; for (const auto& p:controller->profiles()) if (p.id==id) { name=p.name; saved_=draft_=p.proxyEdits.value("dialers").toObject(); }
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("proxyChainDialog"); setWindowTitle("上游代理链 · "+name); resize(600,430); auto* layout=new QVBoxLayout(this);
    layout->addWidget(contextNote("为节点选择连接上游。","为静态节点指定连接其服务器时使用的上游节点或策略组。流量先经过上游，再连接所选节点；保存前检查间接循环。动态Provider节点沿用来源配置。"));
    nodes_=new QListWidget; nodes_->setObjectName("chainNodes"); layout->addWidget(nodes_,1); auto* row=new QHBoxLayout; row->addWidget(new QLabel("连接上游")); target_=new CompactComboBox; target_->setObjectName("chainTarget"); target_->addItem("沿用订阅或节点定义",QVariant()); target_->addItem("无上游（直接拨号）",QString()); row->addWidget(target_,1); layout->addLayout(row);
    preview_=new QLabel("选择节点以预览草稿路径"); preview_->setObjectName("chainFlowPreview"); preview_->setTextFormat(Qt::PlainText); preview_->setWordWrap(true); preview_->setAccessibleName("草稿中的代理链方向"); layout->addWidget(preview_);
    reset_=new QPushButton("全部沿用来源"); reset_->setObjectName("resetProxyDialers"); layout->addWidget(reset_,0,Qt::AlignLeft); hint_=new QLabel("正在读取节点…"); hint_->setObjectName("chainHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); layout->addWidget(hint_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); save_=buttons->button(QDialogButtonBox::Save); save_->setText("保存并校验"); save_->setObjectName("saveProxyDialers"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(buttons,&QDialogButtonBox::accepted,this,[this] { saving_=true; updateActions(); controller_->updateProxyDialers(id_,draft_,saved_); });
    connect(controller,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (!saving_) return; saving_=false; if (ok) accept(); else { hint_->setText(message); updateActions(); } }); connect(controller,&AppController::stateChanged,this,[this] { updateActions(); });
    connect(nodes_,&QListWidget::currentRowChanged,this,[this] { updateSelection(); }); connect(target_,&QComboBox::currentIndexChanged,this,[this](int index) { if (changing_||!nodes_->currentItem()) return; const auto name=nodes_->currentItem()->data(Qt::UserRole).toString(); if (index==0) draft_.remove(name); else draft_[name]=target_->currentData().toString(); updateActions(); });
    connect(reset_,&QPushButton::clicked,this,[this] { draft_={}; updateSelection(); });
    const QPointer<ProxyChainDialog> guard(this); controller->inspectProxyEntries(id,[guard](Result result) { if (!guard) return; guard->loaded_=result.ok; if (!result.ok) { guard->hint_->setText(result.error); guard->updateActions(); return; } guard->observed_=result.data.value("dialers").toObject(); guard->sourceDialers_=result.data.value("source_dialers").toObject(); guard->groupMembers_=result.data.value("group_members").toObject(); for (const auto& target:result.data.value("targets").toArray()) guard->target_->addItem(target.toString(),target.toString()); for (const auto& node:result.data.value("node_names").toArray()) { auto* item=new QListWidgetItem(node.toString(),guard->nodes_); item->setData(Qt::UserRole,node.toString()); } if (guard->nodes_->count()) guard->nodes_->setCurrentRow(0); else guard->hint_->setText("此订阅没有可编辑的静态节点"); guard->updateActions(); }); updateActions();
    DialogDraftGuard::protect(this,[this] { return draft_!=saved_; },[this] { return saving_; });
}
void ProxyChainDialog::updateSelection() { changing_=true; if (nodes_->currentItem()) { const auto name=nodes_->currentItem()->data(Qt::UserRole).toString(); const auto value=draft_.value(name).toString(); target_->setCurrentIndex(!draft_.contains(name)?0:value.isEmpty()?1:qMax(0,target_->findData(value))); const auto observed=observed_.value(name).toString(); hint_->setText("已保存上游："+(observed.isEmpty()?QString("无（直接拨号）"):observed)+""); } changing_=false; updateActions(); }
void ProxyChainDialog::updatePreview() { QSet<QString> nodes,targets; for (int i=0;i<nodes_->count();++i) nodes.insert(nodes_->item(i)->data(Qt::UserRole).toString()); for (int i=2;i<target_->count();++i) targets.insert(target_->itemData(i).toString()); const auto selected=nodes_->currentItem()?nodes_->currentItem()->data(Qt::UserRole).toString():QString(); const auto result=chainPreview(selected,sourceDialers_,draft_,nodes,targets,groupMembers_); preview_->setText(result.ok?result.data.value("preview").toString():result.error); preview_->setToolTip(result.ok?result.data.value("full").toString():result.error); }
void ProxyChainDialog::updateActions() { updatePreview(); const bool available=loaded_&&!saving_&&!controller_->state().busy&&!controller_->state().readOnly&&!controller_->state().exiting; target_->setEnabled(available&&nodes_->currentItem()); save_->setEnabled(available&&draft_!=saved_); reset_->setEnabled(available&&!draft_.isEmpty()); }
}
