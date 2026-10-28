#include "YamlFormEditor.h"
#include "DialogDraftGuard.h"
#include "DnsDialog.h"
#include "ContextNote.h"
#include "CompactComboBox.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace mlash {
namespace {
class DnsTabs final : public QTabWidget {
public:
    // Every page scrolls. QTabWidget's default heightForWidth uses the largest
    // preferred page height; Windows treats that as a mandatory client height.
    int heightForWidth(int) const override { return minimumSizeHint().height(); }
};
}
DnsDialog::DnsDialog(AppController* controller, const QString& profileId, QWidget* parent) : QDialog(parent), controller_(controller), id_(profileId) {
    Profile profile; for (const auto& item : controller_->profiles()) if (item.id==id_) profile=item; saved_=profile.dnsOverride; savedEnabled_=profile.dnsOverrideEnabled; expected_={{"yaml",profile.dnsOverride},{"enabled",profile.dnsOverrideEnabled},{"candidate",profile.candidate},{"active",profile.active}};
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("dnsDialog"); setWindowTitle("DNS · "+profile.name); resize(640,460);
    auto* layout=new QVBoxLayout(this); setMinimumSize(560,400); layout->addWidget(contextNote("未填写的字段沿用订阅。","未填写字段沿用订阅；策略映射按键合并。仅配置内核 DNS，不修改系统 DNS 或新增监听。"));
    auto* tabs=new DnsTabs; tabs->setObjectName("dnsTabs"); auto* edit=new QWidget; auto* editLayout=new QVBoxLayout(edit); enabled_=new QCheckBox("使用此订阅的 DNS 覆盖"); enabled_->setObjectName("dnsOverrideEnabled"); enabled_->setChecked(savedEnabled_); editLayout->addWidget(enabled_);
    editor_=new QPlainTextEdit; editor_->setObjectName("dnsOverrideEditor"); editor_->setPlaceholderText("例如：\nenable: true\nnameserver:\n  - https://dns.google/dns-query\n\n直接填写字段，不再包裹 dns 层"); editor_->setPlainText(saved_); fields_=new YamlFormEditor("dns",editor_,this); editLayout->addWidget(fields_,1); inherit_=new QPushButton("沿用订阅"); inherit_->setObjectName("resetDnsOverride"); editLayout->addWidget(inherit_,0,Qt::AlignLeft); tabs->addTab(edit,"覆盖字段");
    source_=new QPlainTextEdit; source_->setObjectName("sourceDnsPreview"); source_->setReadOnly(true); tabs->addTab(source_,"来源 DNS"); effective_=new QPlainTextEdit; effective_->setObjectName("effectiveDnsPreview"); effective_->setReadOnly(true); tabs->addTab(effective_,"已保存配置预览");
    auto* diagnostic=new QWidget; auto* diagnosticLayout=new QVBoxLayout(diagnostic); auto* row=new QHBoxLayout; name_=new QLineEdit("example.com"); name_->setObjectName("dnsQueryName"); name_->setMaxLength(253); row->addWidget(name_,1); type_=new CompactComboBox; type_->addItems({"A","AAAA"}); row->addWidget(type_); query_=new QPushButton("解析"); query_->setObjectName("queryDns"); row->addWidget(query_); diagnosticLayout->addLayout(row); auto* queryNote=new QLabel("使用当前内核的 DNS 配置解析。"); queryNote->setWordWrap(true); diagnosticLayout->addWidget(queryNote); answer_=new QPlainTextEdit; answer_->setObjectName("dnsQueryResult"); answer_->setReadOnly(true); diagnosticLayout->addWidget(answer_,1); tabs->addTab(diagnostic,"解析测试"); layout->addWidget(tabs,1);
    hint_=new QLabel("正在读取已保存 DNS 配置…"); hint_->setObjectName("dnsHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); layout->addWidget(hint_); auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); save_=actions->button(QDialogButtonBox::Save); save_->setObjectName("saveDnsOverride"); save_->setText("保存 DNS"); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions);
    connect(fields_,&YamlFormEditor::activityChanged,this,[this] { updateActions(); });
    connect(enabled_,&QCheckBox::toggled,this,[this] { updateActions(); }); connect(editor_,&QPlainTextEdit::textChanged,this,[this] { updateActions(); }); connect(inherit_,&QPushButton::clicked,this,[this] { enabled_->setChecked(false); editor_->clear(); });
    connect(controller_,&AppController::stateChanged,this,[this] { updateActions(); }); connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(actions,&QDialogButtonBox::accepted,this,[this] { if (saving_) return; saving_=true; updateActions(); controller_->updateDnsOverride(id_,editor_->toPlainText(),enabled_->isChecked(),expected_); });
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (!saving_) return; saving_=false; if (ok) accept(); else { hint_->setText(message); updateActions(); } });
    connect(query_,&QPushButton::clicked,this,[this] {
        const QPointer<DnsDialog> guard(this); answer_->setPlainText("查询中…");
        queryToken_=controller_->queryDns(name_->text(),type_->currentText(),[guard](Result result) {
            if (!guard) return; guard->queryToken_=0;
            if (!result.ok) guard->answer_->setPlainText(result.error);
            else { QStringList lines{"DNS 状态码："+QString::number(result.data.value("Status").toInt(-1))}; const auto answers=result.data.value("Answer").toArray(); if (answers.isEmpty()) lines.append("没有返回 Answer 记录"); for (int i=0;i<qMin(128,int(answers.size()));++i) { const auto row=answers[i].toObject(); lines.append(row.value("name").toString().left(253)+"  "+row.value("data").toString().left(512)+"  TTL "+QString::number(row.value("TTL").toInt())); } guard->answer_->setPlainText(lines.join('\n')); }
            guard->updateActions();
        }); updateActions();
    });
    const QPointer<DnsDialog> guard(this); controller_->inspectDns(id_,[guard](Result result) { if (!guard) return; if (!result.ok) guard->hint_->setText(result.error); else { QSet<QString> keys; for (const auto& value:result.data.value("source_fields").toArray()) keys.insert(value.toString()); guard->fields_->setSourceFields(keys); guard->source_->setPlainText(result.data.value("source_yaml").toString()); guard->effective_->setPlainText(result.data.value("effective_yaml").toString()); guard->hint_->setText(result.data.value("overridden").toBool()?"已启用 DNS 覆盖":"沿用订阅 DNS"); } }); updateActions();
    DialogDraftGuard::protect(this,[this] { return enabled_->isChecked()!=savedEnabled_ || editor_->toPlainText()!=saved_ || fields_->unapplied(); },[this] { return saving_; }); ensurePolished(); layout->activate(); resize(640,500);
}
DnsDialog::~DnsDialog() { if (queryToken_) controller_->cancelDnsQuery(queryToken_); }
void DnsDialog::updateActions() {
    const bool available=!controller_->state().busy&&!controller_->state().exiting&&!saving_;
    const bool dirty=enabled_->isChecked()!=savedEnabled_ || editor_->toPlainText()!=saved_;
    inherit_->setEnabled(available); save_->setEnabled(available&&!controller_->state().readOnly&&dirty&&!fields_->unapplied()&&(!enabled_->isChecked()||fields_->readyToStage())); enabled_->setEnabled(available); fields_->setOverrideEnabled(enabled_->isChecked()); fields_->setEnabled(available&&enabled_->isChecked()); query_->setEnabled(available&&controller_->state().ready&&controller_->state().activeProfile==id_&&!queryToken_); name_->setEnabled(!queryToken_); type_->setEnabled(!queryToken_);
}
}
