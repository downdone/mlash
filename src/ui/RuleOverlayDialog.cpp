#include "DialogDraftGuard.h"
#include "RuleOverlayDialog.h"
#include "RuleCellDelegate.h"
#include "UiTheme.h"
#include "ProviderDefinitionDialog.h"
#include "profile/ProviderEdits.h"
#include <algorithm>
#include <QUrl>
#include <QColor>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace mlash {
RuleOverlayDialog::RuleOverlayDialog(AppController* controller, const QString& profileId, QWidget* parent,const QJsonObject& draft) : QDialog(parent), controller_(controller), id_(profileId) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("ruleOverlayDialog"); setWindowTitle("规则列表与来源"); resize(720,440);
    Profile profile; for (const auto& item : controller_->profiles()) if (item.id == id_) profile = item; savedDisabled_ = profile.disabledRules;
    savedPrepend_=profile.customRules; savedAppend_=profile.appendRules; expected_=RuleDraft::snapshot(profile); setWindowTitle("规则列表与来源 · "+profile.name);
    if (draft.contains("expected")) expected_=draft.value("expected").toObject();
    providerEdits_=savedProviderEdits_=profile.ruleProviderEdits;
    auto initial=profile.customRules; if (draft.value("prepend").isArray()&&draft.value("prepend").toArray().size()<=256) { initial.clear(); for (const auto& rule:draft.value("prepend").toArray()) initial.append(rule.toString()); }
    auto* layout = new QVBoxLayout(this); auto* order = new QLabel("匹配顺序：前置 → 未屏蔽的订阅原规则 → 追加。原规则中的 MATCH 会先于追加规则命中。"); order->setWordWrap(true); layout->addWidget(order);
    if (draft.value("contextNote").isString()) { auto* origin=new QLabel(draft.value("contextNote").toString().left(512)); origin->setObjectName("ruleDraftOrigin"); origin->setTextFormat(Qt::PlainText); origin->setWordWrap(true); layout->addWidget(origin); }
    tabs_ = new QTabWidget; tabs_->setObjectName("ruleOverlayTabs"); prepend_ = editableTable("prependRulesTable",initial); source_ = new QTableWidget(0,3); source_->setObjectName("sourceRulesTable"); source_->setHorizontalHeaderLabels({"保留","来源规则","状态"}); source_->verticalHeader()->hide(); source_->setEditTriggers(QAbstractItemView::NoEditTriggers); source_->setColumnWidth(0,50); source_->setColumnWidth(2,95); source_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch); append_ = editableTable("appendRulesTable",profile.appendRules);
    tabs_->addTab(prepend_,"前置"); tabs_->addTab(source_,"订阅原规则"); tabs_->addTab(append_,"追加"); layout->addWidget(tabs_,1);
    providerDefinitions_=new QTableWidget(0,4); providerDefinitions_->setObjectName("ruleProviderDefinitions"); providerDefinitions_->setHorizontalHeaderLabels({"规则集名称","声明","行为 / 格式","已显示引用"}); providerDefinitions_->setEditTriggers(QAbstractItemView::NoEditTriggers); providerDefinitions_->setSelectionBehavior(QAbstractItemView::SelectRows); providerDefinitions_->setSelectionMode(QAbstractItemView::SingleSelection); providerDefinitions_->verticalHeader()->hide(); providerDefinitions_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch); providerDefinitions_->setColumnWidth(1,105); providerDefinitions_->setColumnWidth(2,145); providerDefinitions_->setColumnWidth(3,90); providerDefinitions_->setShowGrid(false); tabs_->addTab(providerDefinitions_,"规则集声明");
    auto* providerRow=new QHBoxLayout; auto* addProvider=new QPushButton("添加远程规则集…"); addProvider->setObjectName("addRuleProvider"); auto* editProvider=new QPushButton("编辑声明…"); editProvider->setObjectName("editRuleProvider"); auto* removeProvider=new QPushButton("移除声明"); removeProvider->setObjectName("removeRuleProvider"); auto* resetProvider=new QPushButton("恢复订阅声明"); resetProvider->setObjectName("resetRuleProvider"); providerActions_={addProvider,editProvider,removeProvider,resetProvider}; for (auto* control:providerActions_) { providerRow->addWidget(control); control->hide(); } providerRow->addStretch(); layout->addLayout(providerRow);
    connect(providerDefinitions_,&QTableWidget::itemSelectionChanged,this,[this] { updateSave(); });
    connect(addProvider,&QPushButton::clicked,this,[this] { editProviderDefinition(true); }); connect(editProvider,&QPushButton::clicked,this,[this] { editProviderDefinition(false); });
    connect(removeProvider,&QPushButton::clicked,this,[this] { const auto* item=providerDefinitions_->item(providerDefinitions_->currentRow(),0); if (!item) return; const auto name=item->data(Qt::UserRole).toString(); if (sourceProviderDefinitions_.contains(name)) providerEdits_[name]=QJsonValue(QJsonValue::Null); else providerEdits_.remove(name); operationError_.clear(); rebuildProviderDefinitions(); updateSave(); });
    connect(resetProvider,&QPushButton::clicked,this,[this] { const auto* item=providerDefinitions_->item(providerDefinitions_->currentRow(),0); if (!item) return; providerEdits_.remove(item->data(Qt::UserRole).toString()); operationError_.clear(); rebuildProviderDefinitions(); updateSave(); });
    auto* row = new QHBoxLayout; auto* add = new QPushButton("添加"); add->setObjectName("addOverlayRule"); row->addWidget(add); auto* remove = new QPushButton("删除"); row->addWidget(remove); auto* up = new QPushButton("上移"); row->addWidget(up); auto* down = new QPushButton("下移"); row->addWidget(down); row->addStretch(); layout->addLayout(row);
    hint_ = new QLabel("正在读取规则来源…"); hint_->setObjectName("ruleOverlayHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); layout->addWidget(hint_);
    auto* actions = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); save_ = actions->button(QDialogButtonBox::Save); save_->setText("保存规则"); save_->setObjectName("saveRuleOverlay"); save_->setEnabled(false); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions);
    mutations_ = {prepend_,source_,append_,add,remove,up,down,save_};
    mutations_.append(providerDefinitions_); mutations_.append(providerActions_);
    for (auto* control:mutations_) control->setEnabled(false);
    const auto selected = [this] { return tabs_->currentIndex() == 0 ? prepend_ : tabs_->currentIndex() == 2 ? append_ : nullptr; };
    connect(add,&QPushButton::clicked,this,[this,selected] { auto* table = selected(); if (!table || table->rowCount()+((table==prepend_)?append_:prepend_)->rowCount()>=256) return; appendRule(table,"DOMAIN-SUFFIX,example.com,DIRECT"); table->setCurrentCell(table->rowCount()-1,1); });
    connect(remove,&QPushButton::clicked,this,[selected] { if (auto* table=selected(); table && table->currentRow()>=0) table->removeRow(table->currentRow()); }); connect(up,&QPushButton::clicked,this,[this,selected] { if (auto* table=selected()) moveRow(table,-1); }); connect(down,&QPushButton::clicked,this,[this,selected] { if (auto* table=selected()) moveRow(table,1); });
    connect(tabs_,&QTabWidget::currentChanged,this,[this,add,remove,up,down](int index) { for (auto* control : {add,remove,up,down}) { control->setVisible(index==0||index==2); control->setEnabled(loaded_&&!saving_); } for (auto* control:providerActions_) control->setVisible(index==3); if (index==3) rebuildProviderDefinitions(); updateSave(); });
    connect(source_,&QTableWidget::itemChanged,this,[this](QTableWidgetItem* item) { if (item->column()!=0) return; operationError_.clear(); const QSignalBlocker block(source_); const auto rule=source_->item(item->row(),1)->text(); for (int row=0;row<source_->rowCount();++row) if (source_->item(row,1)->text()==rule) source_->item(row,0)->setCheckState(item->checkState()); updateSave(); });
    for (auto* table : {prepend_,append_}) { connect(table,&QTableWidget::itemChanged,this,[this] { operationError_.clear(); updateSave(); }); connect(table->model(),&QAbstractItemModel::rowsInserted,this,[this] { operationError_.clear(); updateSave(); }); connect(table->model(),&QAbstractItemModel::rowsRemoved,this,[this] { operationError_.clear(); updateSave(); }); }
    connect(controller_,&AppController::stateChanged,this,[this] { updateSave(); });
    connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(actions,&QDialogButtonBox::accepted,this,[this] { if (!loaded_ || saving_) return; QStringList disabled; for (int row=0;row<source_->rowCount();++row) if (source_->item(row,0)->checkState()==Qt::Unchecked) disabled.append(source_->item(row,1)->text()); disabled.removeDuplicates(); saving_ = true; for (auto* control : mutations_) control->setEnabled(false); controller_->updateRuleDefinitions(id_,rules(prepend_),rules(append_),disabled,providerEdits_,expected_); });
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (!saving_) return; saving_ = false; if (ok) accept(); else { for (auto* control : mutations_) control->setEnabled(true); operationError_=message; updateSave(); } });
    const QPointer<RuleOverlayDialog> guard(this);
    controller_->inspectRuleSource(id_,[guard](Result result) {
        if (!guard) return; if (!result.ok) { guard->hint_->setText(result.error); return; }
        const QSignalBlocker block(guard->source_); QSet<QString> present;
        const auto add = [&](const QString& rule, bool stale) { const int row = guard->source_->rowCount(); guard->source_->insertRow(row); auto* keep=new QTableWidgetItem; keep->setFlags(Qt::ItemIsEnabled | (rule.size()<=1024?Qt::ItemIsUserCheckable:Qt::NoItemFlags)); keep->setCheckState(guard->savedDisabled_.contains(rule)?Qt::Unchecked:Qt::Checked); guard->source_->setItem(row,0,keep); guard->source_->setItem(row,1,new QTableWidgetItem(rule)); guard->source_->setItem(row,2,new QTableWidgetItem(stale?"不在当前来源":rule.size()>1024?"超长只读":"订阅")); };
        for (const auto& value : result.data.value("rules").toArray()) { const auto rule=value.toString(); present.insert(rule); add(rule,false); }
        for (const auto& rule : guard->savedDisabled_) if (!present.contains(rule)) add(rule,true);
        QStringList names; for (const auto& value : result.data.value("rule_providers").toArray()) names.append(value.toString());
        guard->sourceProviderDefinitions_=result.data.value("source_provider_definitions").toObject();
        guard->sourceRuleCount_=result.data.value("total").toInt();
        guard->providers_=names; for (const auto& value:result.data.value("targets").toArray()) guard->targets_.append(value.toString());
        guard->loaded_=true; for (auto* control:guard->mutations_) control->setEnabled(true); guard->rebuildProviderDefinitions(); guard->updateSave();
    });
    DialogDraftGuard::protect(this,[this] { if (!loaded_) return false; QSet<QString> disabled,original(savedDisabled_.begin(),savedDisabled_.end()); for (int row=0;row<source_->rowCount();++row) if (source_->item(row,0)->checkState()==Qt::Unchecked) disabled.insert(source_->item(row,1)->text()); return rules(prepend_)!=savedPrepend_ || rules(append_)!=savedAppend_ || disabled!=original || providerEdits_!=savedProviderEdits_; },[this] { return saving_; });
}
void RuleOverlayDialog::rebuildProviderDefinitions() {
    auto effective=sourceProviderDefinitions_; for (auto it=providerEdits_.begin();it!=providerEdits_.end();++it) { if (it.value().isNull()) effective.remove(it.key()); else effective[it.key()]=it.value(); } providers_=effective.keys();
    sourceHint_=QString("前置和追加合计最多256条，屏蔽最多256条。来源共%1条%2；可引用规则集：%3").arg(sourceRuleCount_).arg(sourceRuleCount_>5000?"（仅显示前5000）":"").arg(providers_.isEmpty()?QString("无"):providers_.join(", ").left(300)); hint_->setToolTip(providers_.join('\n'));
    for (auto* table:{prepend_,append_}) for (int column=0;column<4;++column) { auto* previous=table->itemDelegateForColumn(column); table->setItemDelegateForColumn(column,new RuleCellDelegate(column,targets_,providers_,table)); if (previous&&previous->parent()==table&&previous!=table->itemDelegate()) previous->deleteLater(); }
    const auto* selected=providerDefinitions_->item(providerDefinitions_->currentRow(),0); const auto selectedName=selected?selected->data(Qt::UserRole).toString():QString(); QSet<QString> all; for (auto it=sourceProviderDefinitions_.begin();it!=sourceProviderDefinitions_.end();++it) all.insert(it.key()); for (auto it=providerEdits_.begin();it!=providerEdits_.end();++it) all.insert(it.key()); auto names=all.values(); std::sort(names.begin(),names.end());
    QHash<QString,int> uses; const auto count=[&uses](const QString& text) { const auto parts=text.split(','); if (parts.value(0)=="RULE-SET") ++uses[parts.value(1)]; }; for (const auto& text:rules(prepend_)+rules(append_)) count(text); for (int row=0;row<source_->rowCount();++row) if (source_->item(row,0)->checkState()==Qt::Checked) count(source_->item(row,1)->text());
    const QSignalBlocker block(providerDefinitions_); providerDefinitions_->setRowCount(names.size()); for (int row=0;row<names.size();++row) { const auto name=names[row]; const bool local=providerEdits_.contains(name),removed=local&&providerEdits_.value(name).isNull(); const auto definition=local&&!removed?providerEdits_.value(name).toObject():sourceProviderDefinitions_.value(name).toObject(); const QStringList cells{name,removed?QString("已移除"):local?sourceProviderDefinitions_.contains(name)?QString("本地覆盖"):QString("本地新增"):QString("沿用订阅"),definition.value("behavior").toString()+" / "+definition.value("format").toString("yaml"),QString::number(uses.value(name))}; for (int column=0;column<4;++column) { auto* item=new QTableWidgetItem(cells[column]); item->setData(Qt::UserRole,name); item->setToolTip(column==3?QString("只统计已显示的普通RULE-SET；完整及复杂引用由内核校验"):QString()); providerDefinitions_->setItem(row,column,item); } if (name==selectedName) providerDefinitions_->setCurrentCell(row,0); } if (providerDefinitions_->currentRow()<0&&providerDefinitions_->rowCount()) providerDefinitions_->setCurrentCell(0,0);
}
void RuleOverlayDialog::editProviderDefinition(bool adding) {
    if (!loaded_||saving_||controller_->state().busy) return; QString name; QJsonObject initial;
    if (!adding) { const auto* item=providerDefinitions_->item(providerDefinitions_->currentRow(),0); if (!item) return; name=item->data(Qt::UserRole).toString(); initial=providerEdits_.value(name).isObject()?providerEdits_.value(name).toObject():sourceProviderDefinitions_.value(name).toObject(); }
    QSet<QString> occupied; for (auto it=sourceProviderDefinitions_.begin();it!=sourceProviderDefinitions_.end();++it) occupied.insert(it.key()); for (auto it=providerEdits_.begin();it!=providerEdits_.end();++it) occupied.insert(it.key());
    auto* dialog=new ProviderDefinitionDialog(name,initial,occupied,targets_,prepend_->rowCount()+append_->rowCount()<256,this); connect(dialog,&QDialog::accepted,this,[this,dialog] { auto next=providerEdits_; next[dialog->providerName()]=dialog->definition(); const auto valid=validateRuleProviderEdits(next); if (!valid.ok) { operationError_=valid.error; updateSave(); return; } providerEdits_=next; operationError_.clear(); if (dialog->addReference()) appendRule(prepend_,"RULE-SET,"+dialog->providerName()+','+dialog->target()); rebuildProviderDefinitions(); updateSave(); }); dialog->open();
}
QTableWidget* RuleOverlayDialog::editableTable(const QString& name, const QStringList& definitions) {
    auto* table=new QTableWidget(0,4); table->setObjectName(name); table->setHorizontalHeaderLabels({"类型","匹配内容","目标策略","选项"}); table->verticalHeader()->hide(); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection); table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch); table->setColumnWidth(0,145); table->setColumnWidth(2,120); table->setColumnWidth(3,95); for (const auto& rule : definitions) appendRule(table,rule); return table;
}
void RuleOverlayDialog::appendRule(QTableWidget* table, const QString& definition) {
    const auto cells=RuleDraft::fields(definition); const int row=table->rowCount(); table->insertRow(row); for (int column=0;column<4;++column) table->setItem(row,column,new QTableWidgetItem(cells[column]));
}
QStringList RuleOverlayDialog::rules(QTableWidget* table) const {
    QStringList result; for (int row=0;row<table->rowCount();++row) { QStringList cells; for (int column=0;column<4;++column) cells.append(table->item(row,column)?table->item(row,column)->text().trimmed():QString()); result.append(RuleDraft::compose(cells)); } return result;
}
void RuleOverlayDialog::moveRow(QTableWidget* table, int delta) {
    const int source=table->currentRow(), target=source+delta; if (source<0 || target<0 || target>=table->rowCount() || saving_) return; for (int column=0;column<4;++column) { auto* first=table->takeItem(source,column); auto* second=table->takeItem(target,column); table->setItem(source,column,second); table->setItem(target,column,first); } table->setCurrentCell(target,0); updateSave();
}
void RuleOverlayDialog::updateSave() {
    if (!loaded_ || saving_ || controller_->state().busy || controller_->state().readOnly || controller_->state().exiting) { save_->setEnabled(false); return; }
    if (prepend_->rowCount()+append_->rowCount()>256) { hint_->setText("前置和追加规则合计最多256条"); save_->setEnabled(false); return; }
    for (int i=0;i<providerActions_.size();++i) providerActions_[i]->setEnabled(i==0||providerDefinitions_->currentRow()>=0);
    const QSet<QString> targets(targets_.begin(),targets_.end()),providers(providers_.begin(),providers_.end());
    Result first; for (auto* table:{prepend_,append_}) { const auto checked=RuleDraft::check(rules(table),targets,providers,true,table==prepend_?"custom_rules":"append_rules"); const QSignalBlocker block(table); for (int row=0;row<table->rowCount();++row) for (int column=0;column<4;++column) { auto* cell=table->item(row,column); if (!cell) continue; const bool error=!checked.ok&&row==checked.data.value("row").toInt()-1&&column==checked.data.value("column").toInt(); QColor background(UiTheme::Error); background.setAlpha(error?25:0); cell->setBackground(error?QBrush(background):QBrush()); cell->setToolTip(error?checked.error:QString()); } if (!checked.ok&&first.ok) first=checked; }
    if (!first.ok) { hint_->setText(first.error); save_->setEnabled(false); return; }
    hint_->setText(operationError_.isEmpty()?sourceHint_:operationError_);
    QSet<QString> disabled, original(savedDisabled_.begin(),savedDisabled_.end()); for (int row=0;row<source_->rowCount();++row) if (source_->item(row,0) && source_->item(row,1) && source_->item(row,0)->checkState()==Qt::Unchecked) disabled.insert(source_->item(row,1)->text());
    save_->setEnabled(rules(prepend_)!=savedPrepend_ || rules(append_)!=savedAppend_ || disabled!=original || providerEdits_!=savedProviderEdits_);
}
}
