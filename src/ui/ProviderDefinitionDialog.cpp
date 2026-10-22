#include "ProviderDefinitionDialog.h"
#include "CompactComboBox.h"
#include "DialogDraftGuard.h"
#include "profile/ProviderEdits.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace mlash {
ProviderDefinitionDialog::ProviderDefinitionDialog(const QString& name,const QJsonObject& initial,const QSet<QString>& occupied,const QStringList& targets,bool canAddRule,QWidget* parent):QDialog(parent),occupied_(occupied),adding_(name.isEmpty()) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("providerDefinitionDialog"); setWindowTitle(adding_?"添加远程规则集":"编辑规则集声明"); resize(530,340); auto* box=new QVBoxLayout(this); auto* form=new QFormLayout;
    name_=new QLineEdit(name); name_->setObjectName("ruleProviderName"); name_->setMaxLength(qMax(128,name.size())); name_->setReadOnly(!adding_); name_->setPlaceholderText("例如 CustomSites"); form->addRow("名称",name_);
    url_=new QLineEdit(initial.value("url").toString()); url_->setObjectName("ruleProviderUrl"); url_->setMaxLength(qMax(2048,initial.value("url").toString().size())); url_->setEchoMode(QLineEdit::Password); url_->setPlaceholderText("https://example.com/rules.yaml"); form->addRow("HTTPS地址",url_);
    behavior_=new CompactComboBox; behavior_->setObjectName("ruleProviderBehavior"); behavior_->addItem("域名 · domain","domain"); behavior_->addItem("IP网段 · ipcidr","ipcidr"); behavior_->addItem("完整规则 · classical","classical"); behavior_->setCurrentIndex(qMax(0,behavior_->findData(initial.value("behavior").toString("domain")))); form->addRow("行为",behavior_);
    format_=new CompactComboBox; format_->setObjectName("ruleProviderFormat"); for (const auto& item:QStringList{"yaml","text","mrs"}) format_->addItem(item.toUpper(),item); format_->setCurrentIndex(qMax(0,format_->findData(initial.value("format").toString("yaml")))); form->addRow("格式",format_); box->addLayout(form);
    auto* show=new QCheckBox("显示私人地址"); box->addWidget(show); connect(show,&QCheckBox::toggled,this,[this](bool visible) { url_->setEchoMode(visible?QLineEdit::Normal:QLineEdit::Password); });
    reference_=new QCheckBox("同时添加一条前置引用"); reference_->setObjectName("addRuleProviderReference"); reference_->setEnabled(canAddRule); box->addWidget(reference_); target_=new CompactComboBox; target_->setObjectName("ruleProviderTarget"); for (const auto& item:targets) target_->addItem(item=="DIRECT"?QString("直连 · DIRECT"):item=="REJECT"?QString("阻断 · REJECT"):item,item); target_->setEnabled(false); box->addWidget(target_); connect(reference_,&QCheckBox::toggled,target_,&QWidget::setEnabled);
    auto* note=new QLabel("加入草稿不会联网；保存主窗口规则时才下载并校验，失败保留原版本。不改写订阅原文。"); note->setWordWrap(true); note->setObjectName("secondaryText"); box->addWidget(note); hint_=new QLabel; hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); box->addWidget(hint_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); save_=buttons->button(QDialogButtonBox::Save); save_->setText("加入草稿"); save_->setObjectName("stageRuleProvider"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); box->addWidget(buttons); connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept); connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    const auto baseline=definition(); const auto baselineName=providerName(); DialogDraftGuard::protect(this,[this,baseline,baselineName] { return providerName()!=baselineName||definition()!=baseline||reference_->isChecked(); });
    for (auto* field:{name_,url_}) connect(field,&QLineEdit::textChanged,this,[this] { validate(); }); for (auto* field:{behavior_,format_,target_}) connect(field,&QComboBox::currentIndexChanged,this,[this] { validate(); }); connect(reference_,&QCheckBox::toggled,this,[this] { validate(); }); validate();
}
QString ProviderDefinitionDialog::providerName() const { return name_->text().trimmed(); }
QJsonObject ProviderDefinitionDialog::definition() const { return {{"url",url_->text().trimmed()},{"behavior",behavior_->currentData().toString()},{"format",format_->currentData().toString()}}; }
bool ProviderDefinitionDialog::addReference() const { return reference_->isChecked(); }
QString ProviderDefinitionDialog::target() const { return target_->currentData().toString(); }
void ProviderDefinitionDialog::validate() { auto result=validateRuleProviderEdits({{providerName(),definition()}}); if (adding_&&occupied_.contains(providerName())) result=Result::fail("同名规则集已存在，请编辑或恢复现有声明"); if (reference_->isChecked()&&target().isEmpty()) result=Result::fail("请选择引用的目标策略"); hint_->setText(result.ok?QString():result.error); save_->setEnabled(result.ok); }
}
