#include "app/AppController.h"
#include "YamlFormEditor.h"
#include "DialogDraftGuard.h"
#include "DnsDialog.h"
#include "CompactComboBox.h"
#include "WidgetStyle.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMenu>
#include <QTabBar>
#include <QTabWidget>
#include <QHBoxLayout>
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
DnsDialog::DnsDialog(AppController* controller, const QString& profileId, QWidget* parent) : TaskDialog(parent), controller_(controller), id_(profileId) {
    Profile profile; for (const auto& item : controller_->profiles()) if (item.id==id_) profile=item; saved_=profile.dnsOverride; savedEnabled_=profile.dnsOverrideEnabled; expected_={{"yaml",profile.dnsOverride},{"enabled",profile.dnsOverrideEnabled},{"candidate",profile.candidate},{"active",profile.active}};
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("dnsDialog"); setWindowTitle("DNS · "+profile.name); resize(640,460);
    setProperty("editorOwnsScrolling",true);
    auto* layout=new QVBoxLayout(this); setMinimumSize(560,400);
    auto* viewRow=new QHBoxLayout; auto* viewTitle=new QLabel("当前 DNS"); viewTitle->setObjectName("dnsViewTitle"); viewRow->addWidget(viewTitle); viewRow->addStretch(); auto* views=new QPushButton("查看…"); views->setObjectName("dnsViews"); views->setProperty("quiet",true); viewRow->addWidget(views); layout->addLayout(viewRow);
    tabs_=new DnsTabs; auto* tabs=tabs_; tabs->setObjectName("dnsTabs"); auto* edit=new QWidget; auto* editLayout=new QVBoxLayout(edit); enabled_=new QCheckBox("自定义 DNS"); enabled_->setObjectName("dnsOverrideEnabled"); enabled_->setChecked(savedEnabled_); editLayout->addWidget(enabled_);
    hint_=new QLabel("正在读取当前 DNS…"); hint_->setObjectName("dnsHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); editLayout->addWidget(hint_);
    retry_=new QPushButton("重新读取 DNS"); retry_->setObjectName("retryDnsSource"); retry_->hide();
    editLayout->addWidget(retry_,0,Qt::AlignLeft);
    connect(retry_,&QPushButton::clicked,this,&DnsDialog::inspectSource);
    editor_=new QPlainTextEdit; editor_->setObjectName("dnsOverrideEditor"); editor_->setPlaceholderText("例如：\nenable: true\nnameserver:\n  - https://dns.google/dns-query\n\n直接填写字段，不再包裹 dns 层"); editor_->setPlainText(saved_); fields_=new YamlFormEditor("dns",editor_,this); editLayout->addWidget(fields_,1); inherit_=new QPushButton("撤销自定义"); inherit_->setObjectName("resetDnsOverride"); editLayout->addWidget(inherit_,0,Qt::AlignLeft); editLayout->addStretch(1); tabs->addTab(edit,"当前 DNS");
    auto* editYaml=new QPushButton; editYaml->setObjectName("dnsEditMode"); editYaml->setProperty("quiet",true);
    viewRow->insertWidget(2,editYaml);
    fields_->useModeButton(editYaml);
    const auto showMode=[tabs,this,editYaml] { editYaml->setVisible(tabs->currentIndex()==0 && enabled_->isChecked()); };
    connect(tabs,&QTabWidget::currentChanged,this,showMode);
    connect(enabled_,&QCheckBox::toggled,this,showMode);
    showMode();
    source_=new QPlainTextEdit; source_->setObjectName("sourceDnsPreview"); source_->setReadOnly(true); tabs->addTab(source_,"来源 DNS"); effective_=new QPlainTextEdit; effective_->setObjectName("effectiveDnsPreview"); effective_->setReadOnly(true); tabs->addTab(effective_,"已保存配置预览");
    auto* diagnostic=new QWidget; auto* diagnosticLayout=new QVBoxLayout(diagnostic); auto* row=new QHBoxLayout; name_=new QLineEdit("example.com"); name_->setObjectName("dnsQueryName"); name_->setMaxLength(253); row->addWidget(name_,1); type_=new CompactComboBox; type_->addItems({"A","AAAA"}); row->addWidget(type_); query_=new QPushButton("解析"); query_->setObjectName("queryDns"); row->addWidget(query_); diagnosticLayout->addLayout(row); auto* queryNote=new QLabel("使用当前配置进行解析。"); queryNote->setWordWrap(true); diagnosticLayout->addWidget(queryNote); answer_=new QPlainTextEdit; answer_->setObjectName("dnsQueryResult"); answer_->setReadOnly(true); diagnosticLayout->addWidget(answer_,1); tabs->addTab(diagnostic,"解析测试"); tabs->tabBar()->hide(); layout->addWidget(tabs,1);
    auto* viewMenu=new QMenu(views); viewMenu->setObjectName("dnsViewsMenu"); for (int i=0;i<tabs->count();++i) { auto* action=viewMenu->addAction(tabs->tabText(i)); action->setCheckable(true); action->setChecked(i==0); action->setData(i); connect(action,&QAction::triggered,tabs,[tabs,i] { tabs->setCurrentIndex(i); }); }
    connect(tabs,&QTabWidget::currentChanged,this,[tabs,viewMenu,viewTitle](int index) { viewTitle->setText(tabs->tabText(index)); for (auto* action:viewMenu->actions()) action->setChecked(action->data().toInt()==index); });
    connect(tabs,&QTabWidget::currentChanged,this,[this] { updateActions(); });
    connect(views,&QPushButton::clicked,this,[views,viewMenu] { WidgetStyle::popupMenu(viewMenu,views->mapToGlobal(QPoint(views->width()-viewMenu->sizeHint().width(),views->height()))); });
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); save_=actions->button(QDialogButtonBox::Save); save_->setObjectName("saveDnsOverride"); save_->setText("保存 DNS"); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions);
    connect(fields_,&YamlFormEditor::activityChanged,this,[this] { updateActions(); });
    connect(enabled_,&QCheckBox::toggled,this,[this] { updateSourceHint(); updateActions(); }); connect(editor_,&QPlainTextEdit::textChanged,this,[this] { updateActions(); }); connect(inherit_,&QPushButton::clicked,this,[this] { enabled_->setChecked(false); editor_->clear(); });
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
    inspectSource(); updateActions();
    DialogDraftGuard::protect(this,[this] { return enabled_->isChecked()!=savedEnabled_ || editor_->toPlainText()!=saved_ || fields_->unapplied(); },[this] { return saving_; }); ensurePolished(); layout->activate(); resize(640,500);
}
DnsDialog::~DnsDialog() { if (queryToken_) controller_->cancelDnsQuery(queryToken_); }
void DnsDialog::inspectSource() {
    sourceError_.clear();
    retry_->hide();
    updateSourceHint();
    const QPointer<DnsDialog> guard(this);
    controller_->inspectDns(id_,[guard](Result result) {
        if (!guard) return;
        if (!result.ok) {
            guard->sourceError_=result.error;
            guard->retry_->show();
        } else {
            QSet<QString> keys;
            for (const auto& value:result.data.value("source_fields").toArray()) keys.insert(value.toString());
            guard->fields_->setSourceFields(keys);
            guard->source_->setPlainText(result.data.value("source_yaml").toString());
            guard->effective_->setPlainText(result.data.value("effective_yaml").toString());
            guard->inspected_=true;
            guard->observedOverridden_=result.data.value("overridden").toBool();
        }
        guard->updateSourceHint();
    });
}
void DnsDialog::updateSourceHint() {
    if (!sourceError_.isEmpty()) hint_->setText("读取当前 DNS 失败："+sourceError_);
    else if (enabled_->isChecked()!=savedEnabled_) hint_->setText(enabled_->isChecked()?"保存后使用自定义 DNS":"保存后沿用来源 DNS");
    else if (inspected_) hint_->setText(observedOverridden_?"当前使用自定义 DNS":"当前沿用来源 DNS");
    else hint_->setText("正在读取当前 DNS…");
}
void DnsDialog::updateActions() {
    const bool available=!controller_->state().busy&&!controller_->state().exiting&&!saving_;
    const bool dirty=enabled_->isChecked()!=savedEnabled_ || editor_->toPlainText()!=saved_;
    const bool compact=tabs_->currentIndex()==0&&!enabled_->isChecked();
    if (property("compactProfileTask").toBool()!=compact) { setProperty("compactProfileTask",compact); emit compactModeChanged(); }
    inherit_->setVisible(enabled_->isChecked()); inherit_->setEnabled(available); save_->setVisible(tabs_->currentIndex()==0); save_->setEnabled(available&&!controller_->state().readOnly&&dirty&&!fields_->unapplied()&&(!enabled_->isChecked()||fields_->readyToStage())); enabled_->setEnabled(available); fields_->setOverrideEnabled(enabled_->isChecked()); fields_->setVisible(enabled_->isChecked()); fields_->setEnabled(available&&enabled_->isChecked()); query_->setEnabled(available&&controller_->state().ready&&controller_->state().activeProfile==id_&&!queryToken_); name_->setEnabled(!queryToken_); type_->setEnabled(!queryToken_);
}
}
