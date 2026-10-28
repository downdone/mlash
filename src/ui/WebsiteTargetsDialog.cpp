#include "DialogDraftGuard.h"
#include "WebsiteTargetsDialog.h"
#include "storage/Storage.h"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace mlash {
WebsiteTargetsDialog::WebsiteTargetsDialog(AppController* controller, QWidget* parent) : QDialog(parent), controller_(controller) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("websiteTargetsDialog"); setWindowTitle("测速目标"); resize(600,410);
    auto* layout = new QVBoxLayout(this); auto* note = new QLabel("填写 HTTPS 测试地址。"); note->setWordWrap(true); layout->addWidget(note);
    table_ = new QTableWidget(0,2); table_->setObjectName("websiteTargetsTable"); table_->setHorizontalHeaderLabels({"名称","HTTPS 目标地址"}); table_->verticalHeader()->hide(); table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection); table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch); table_->setColumnWidth(0,155); layout->addWidget(table_,1);
    auto* tools = new QHBoxLayout; auto* add = new QPushButton("添加"); add->setObjectName("addWebsiteTarget"); tools->addWidget(add); auto* remove = new QPushButton("删除"); remove->setObjectName("removeWebsiteTarget"); tools->addWidget(remove); auto* up = new QPushButton("上移"); tools->addWidget(up); auto* down = new QPushButton("下移"); tools->addWidget(down); tools->addStretch(); auto* defaults = new QPushButton("恢复默认"); defaults->setObjectName("defaultWebsiteTargets"); tools->addWidget(defaults); layout->addLayout(tools);
    hint_ = new QLabel; hint_->setObjectName("websiteTargetsHint"); hint_->setWordWrap(true); hint_->setTextFormat(Qt::PlainText); layout->addWidget(hint_);
    auto* actions = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); actions->button(QDialogButtonBox::Save)->setText("保存"); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions);
    mutations_ = {table_,add,remove,up,down,defaults,actions->button(QDialogButtonBox::Save)};
    load(controller_->websiteTests()->targets());
    const auto initial=definitions(); DialogDraftGuard::protect(this,[this,initial] { return definitions()!=initial; },[this] { return saving_; });
    connect(add,&QPushButton::clicked,this,[this] { if (table_->rowCount()>=24) { hint_->setText("最多 24 个目标"); return; } const int row = table_->rowCount(); table_->insertRow(row); auto* name = new QTableWidgetItem("新目标"); name->setData(Qt::UserRole,"custom-"+newId()); table_->setItem(row,0,name); table_->setItem(row,1,new QTableWidgetItem("https://example.com/")); table_->setCurrentCell(row,0); table_->editItem(name); });
    connect(remove,&QPushButton::clicked,this,[this] { if (table_->currentRow()>=0) table_->removeRow(table_->currentRow()); });
    connect(up,&QPushButton::clicked,this,[this] { moveRow(-1); }); connect(down,&QPushButton::clicked,this,[this] { moveRow(1); }); connect(defaults,&QPushButton::clicked,this,[this] { load(WebsiteTests::defaults()); });
    connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(actions,&QDialogButtonBox::accepted,this,[this] {
        if (saving_) return;
        auto targets = definitions(); if (targets.isEmpty()) { hint_->setText("至少保留一个目标，或恢复默认列表"); return; }
        const auto valid = WebsiteTests::validateTargets(targets); if (!valid.ok) { hint_->setText(valid.error); return; }
        QJsonArray defaults; for (const auto& item : WebsiteTests::defaults()) defaults.append(item.toJson()); if (targets == defaults) targets = {};
        saving_ = true; for (auto* control : mutations_) control->setEnabled(false); controller_->updateWebsiteTargets(targets);
    });
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (!saving_) return; saving_ = false; if (ok) accept(); else { for (auto* control : mutations_) control->setEnabled(true); hint_->setText(message); } });
}
void WebsiteTargetsDialog::load(const QList<WebsiteTarget>& targets) {
    table_->setRowCount(0);
    for (const auto& item : targets) { const int row = table_->rowCount(); table_->insertRow(row); auto* name = new QTableWidgetItem(item.name); name->setData(Qt::UserRole,item.id); name->setData(Qt::UserRole+1,item.mark); table_->setItem(row,0,name); table_->setItem(row,1,new QTableWidgetItem(item.url)); }
}
QJsonArray WebsiteTargetsDialog::definitions() const {
    QJsonArray targets; for (int row = 0; row < table_->rowCount(); ++row) { const auto* name = table_->item(row,0); const auto* url = table_->item(row,1); if (!name || !url) return {}; targets.append(WebsiteTarget{name->data(Qt::UserRole).toString(),name->text().trimmed(),url->text().trimmed(),name->data(Qt::UserRole+1).toString()}.toJson()); } return targets;
}
void WebsiteTargetsDialog::moveRow(int delta) {
    const int source = table_->currentRow(), target = source + delta; if (source<0 || target<0 || target>=table_->rowCount() || saving_) return;
    for (int column=0;column<2;++column) { auto* first = table_->takeItem(source,column); auto* second = table_->takeItem(target,column); table_->setItem(source,column,second); table_->setItem(target,column,first); } table_->setCurrentCell(target,0);
}
}
