#include "ProfileChangesDialog.h"
#include "DialogDraftGuard.h"
#include "UiTheme.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
namespace mlash {
ProfileChangesDialog::ProfileChangesDialog(AppController* controller,const QString& id,QWidget* parent):QDialog(parent),controller_(controller),id_(id) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("profileChangesDialog"); setWindowTitle("候选版本变化"); resize(720,510); auto* layout=new QVBoxLayout(this);
    hint_=new QLabel("正在比较已使用与候选快照…"); hint_->setObjectName("versionChangesHint"); hint_->setWordWrap(true); hint_->setTextFormat(Qt::PlainText); layout->addWidget(hint_);
    table_=new QTableWidget(0,6); table_->setObjectName("versionChangesTable"); table_->setHorizontalHeaderLabels({"类别","已使用","候选","新增","移除","修改"}); table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection); table_->verticalHeader()->hide(); table_->setShowGrid(false); table_->verticalHeader()->setDefaultSectionSize(UiTheme::TableRow); table_->setMinimumHeight(UiTheme::TableRow*6+8); table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); layout->addWidget(table_,1);
    details_=new QPlainTextEdit; details_->setObjectName("versionChangesDetails"); details_->setReadOnly(true); layout->addWidget(details_,1); auto* note=new QLabel("应用后将切换到此候选版本。"); note->setWordWrap(true); note->setObjectName("secondaryText"); layout->addWidget(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close); buttons->button(QDialogButtonBox::Close)->setText("关闭"); auto* refresh=buttons->addButton("重新比较",QDialogButtonBox::ActionRole); apply_=buttons->addButton("使用候选版本",QDialogButtonBox::AcceptRole); apply_->setObjectName("applyComparedVersion"); apply_->setProperty("primary",true); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(refresh,&QPushButton::clicked,this,&ProfileChangesDialog::reload); connect(table_,&QTableWidget::currentCellChanged,this,[this](int row) { showSection(row); });
    connect(apply_,&QPushButton::clicked,this,[this] { if (!apply_->isEnabled()) return; applying_=true; updateActions(); controller_->activateProfile(id_); }); connect(controller_,&AppController::stateChanged,this,&ProfileChangesDialog::updateActions); connect(controller_,&AppController::profilesChanged,this,&ProfileChangesDialog::updateActions);
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& error) { if (!applying_) return; applying_=false; if (ok) accept(); else { hint_->setText("应用失败，原运行版本保持或已恢复 · "+error); updateActions(); } });
    DialogDraftGuard::protect(this,[] { return false; },[this] { return applying_; }); reload();
}
void ProfileChangesDialog::reload() {
    if (loading_||applying_) return; loading_=true; report_={}; candidate_.clear(); updateActions(); const QPointer<ProfileChangesDialog> guard(this);
    controller_->inspectProfileChanges(id_,[guard](Result result) { if (!guard) return; guard->loading_=false; if (!result.ok) { guard->hint_->setText(result.error); guard->table_->setRowCount(0); guard->details_->clear(); guard->updateActions(); return; } guard->report_=result.data; guard->candidate_=result.data.value("candidate_version").toString(); const auto sections=result.data.value("sections").toArray(); guard->table_->setRowCount(sections.size()); for (int i=0;i<sections.size();++i) { const auto item=sections[i].toObject(); const QStringList cells{item.value("label").toString(),QString::number(item.value("before").toInt()),QString::number(item.value("after").toInt()),QString::number(item.value("added_count").toInt()),QString::number(item.value("removed_count").toInt()),QString::number(item.value("changed_count").toInt())}; for (int col=0;col<cells.size();++col) guard->table_->setItem(i,col,new QTableWidgetItem(cells[col])); } guard->hint_->setText((result.data.value("first_use").toBool()?QString("首次使用，尚无已使用版本"):QString("已使用快照：")+result.data.value("active_time").toString())+"\n候选快照："+result.data.value("candidate_time").toString()+""); guard->table_->setCurrentCell(0,0); guard->showSection(0); guard->updateActions(); });
}
void ProfileChangesDialog::updateActions() { bool fresh=false; for (const auto& profile:controller_->profiles()) if (profile.id==id_) fresh=!candidate_.isEmpty()&&profile.candidate==candidate_&&profile.candidate!=profile.active; const auto& state=controller_->state(); apply_->setEnabled(fresh&&!loading_&&!applying_&&!state.busy&&!state.exiting&&!state.readOnly); apply_->setToolTip(fresh?"确认使用当前比较的候选版本":"候选已变化或已经使用，请重新比较"); }
void ProfileChangesDialog::showSection(int row) {
    const auto sections=report_.value("sections").toArray(); if (row<0||row>=sections.size()) return; const auto section=sections[row].toObject(); QStringList lines; for (const auto& key:QStringList{"added","removed","changed"}) { const auto entries=section.value(key).toArray(); if (entries.isEmpty()) continue; lines.append(key=="added"?"新增：":key=="removed"?"移除：":"修改："); for (const auto& name:entries) lines.append("  "+name.toString()); } if (section.value("order_changed").toBool()) lines.append("规则内容或匹配顺序发生变化。"); if (section.value("truncated").toBool()) lines.append("各类最多展示100个名称；总数见表格。");
    const auto lost=report_.value("lost_selections").toArray(); if (!lost.isEmpty()) { lines.append("\n以下已保存选择在候选中不再可用："); for (const auto& item:lost) lines.append("  "+item.toString()); lines.append("使用新版本时按内核默认选择恢复。"); } const auto unknown=report_.value("unverified_selections").toArray(); if (!unknown.isEmpty()) lines.append("部分选择属于动态Provider，激活后才核对，不预判失效。"); details_->setPlainText(lines.isEmpty()?"此类别没有名称变化。":lines.join('\n'));
}
}
