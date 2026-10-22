#include "NodeLinksEditor.h"
#include "ElidedLabel.h"
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace mlash {
NodeLinksEditor::NodeLinksEditor(AppController* controller,QWidget* parent,const QStringList& reserved):QWidget(parent),controller_(controller),reserved_(reserved) {
    setObjectName("nodeLinksEditor"); auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(6);
    auto* note=new QLabel("每行一个节点链接，最多64个；检查仅在本机解析，不测试网络。预览后再确认。",this); note->setWordWrap(true); note->setProperty("secondary",true); layout->addWidget(note);
    input_=new QPlainTextEdit; input_->setObjectName("nodeLinksInput"); input_->setAccessibleName("节点分享链接"); input_->setPlaceholderText("ss://…\nvmess://…\nvless://…\ntrojan://…\nhysteria2://…"); input_->setMinimumHeight(70); input_->setMaximumHeight(100); layout->addWidget(input_,1);
    auto* row=new QHBoxLayout; check_=new QPushButton("检查链接"); check_->setObjectName("checkNodeLinks"); check_->setAutoDefault(false); check_->setEnabled(false); row->addWidget(check_); hint_=new ElidedLabel("尚未检查；名称冲突会自动加后缀，不覆盖原节点。"); hint_->setObjectName("nodeLinksHint"); hint_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); row->addWidget(hint_,1); layout->addLayout(row);
    preview_=new QTableWidget(0,3); preview_->setObjectName("nodeLinksPreview"); preview_->setHorizontalHeaderLabels({"名称","类型","服务器 / 端口"}); preview_->setEditTriggers(QAbstractItemView::NoEditTriggers); preview_->setSelectionBehavior(QAbstractItemView::SelectRows); preview_->verticalHeader()->hide(); preview_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch); preview_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch); preview_->setColumnWidth(1,80); preview_->setMinimumHeight(70); preview_->setMaximumHeight(110); layout->addWidget(preview_,1);
    connect(input_,&QPlainTextEdit::textChanged,this,[this] { ++revision_; result_={}; hint_->setAccessibleDescription({}); preview_->setRowCount(0); const bool fits=text().toUtf8().size()<=64*1024; check_->setEnabled(!checking_&&!text().trimmed().isEmpty()&&fits); hint_->setText(!fits?"链接总长度最多64KiB，请减少内容":checking_?"内容已修改；本次检查结束后请重新检查":"内容已修改，请先检查链接"); emit changed(); });
    connect(check_,&QPushButton::clicked,this,&NodeLinksEditor::check);
}
QString NodeLinksEditor::text() const { return input_->toPlainText(); }
void NodeLinksEditor::setText(const QString& text) { input_->setPlainText(text); }
void NodeLinksEditor::check() {
    if (checking_||!check_->isEnabled()||!controller_) return; checking_=true; result_={}; check_->setEnabled(false); hint_->setAccessibleDescription({}); hint_->setText("正在检查…"); const auto revision=revision_; emit changed();
    const QPointer<NodeLinksEditor> guard(this); controller_->previewNodeLinks(text(),reserved_,[guard,revision](Result result) {
        if (!guard) return; guard->checking_=false; guard->check_->setEnabled(!guard->text().trimmed().isEmpty()&&guard->text().toUtf8().size()<=64*1024);
        if (revision!=guard->revision_) { guard->hint_->setText("内容已修改，请重新检查"); emit guard->changed(); return; }
        if (!result.ok) { guard->hint_->setText(result.error); guard->hint_->setAccessibleDescription(result.error); emit guard->changed(); return; }
        guard->result_=result.data; const auto items=result.data.value("items").toArray(); guard->preview_->setRowCount(items.size());
        for (int row=0;row<items.size();++row) { const auto item=items[row].toObject(); const auto host=item.value("server").toString(); const QStringList values{item.value("name").toString(),item.value("type").toString(),(host.contains(':')?"["+host+"]":host)+":"+QString::number(item.value("port").toInt())}; for (int column=0;column<3;++column) { auto* cell=new QTableWidgetItem(values[column]); cell->setToolTip(values[column]); guard->preview_->setItem(row,column,cell); } }
        guard->hint_->setText(QString("%1 个节点已检查；尚未保存或应用").arg(items.size())); emit guard->changed();
    });
}
}
