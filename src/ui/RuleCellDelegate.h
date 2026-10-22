#pragma once
#include "CompactComboBox.h"
#include "profile/RuleDraft.h"
#include <QStyledItemDelegate>
#include <QLineEdit>

namespace mlash {
class RuleCellDelegate final : public QStyledItemDelegate {
public:
    RuleCellDelegate(int column,QStringList targets,QStringList providers,QObject* parent):QStyledItemDelegate(parent),column_(column),targets_(std::move(targets)),providers_(std::move(providers)) {}
    QWidget* createEditor(QWidget* parent,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        QStringList choices; bool editable=false;
        if (column_==0) choices=RuleDraft::types();
        else if (column_==2) { choices=targets_; editable=true; }
        else if (column_==3) choices={"","no-resolve"};
        else if (index.siblingAtColumn(0).data(Qt::EditRole).toString()=="MATCH") return nullptr;
        else if (index.siblingAtColumn(0).data(Qt::EditRole).toString()=="RULE-SET") choices=providers_;
        else return QStyledItemDelegate::createEditor(parent,option,index);
        auto* box=new CompactComboBox(parent); box->setObjectName("ruleCellEditor"); box->setEditable(editable);
        for (const auto& choice:choices) box->addItem(column_==0?RuleDraft::label(choice):choice=="DIRECT"?QString("直连 · DIRECT"):choice=="REJECT"?QString("阻断 · REJECT"):choice,choice);
        const auto current=index.data(Qt::EditRole).toString(); if (box->findData(current)<0) box->addItem(current,current); return box;
    }
    void setEditorData(QWidget* editor,const QModelIndex& index) const override { if (auto* box=qobject_cast<QComboBox*>(editor)) box->setCurrentIndex(box->findData(index.data(Qt::EditRole))); else QStyledItemDelegate::setEditorData(editor,index); }
    void setModelData(QWidget* editor,QAbstractItemModel* model,const QModelIndex& index) const override { if (auto* box=qobject_cast<QComboBox*>(editor)) { const auto value=box->currentIndex()>=0&&box->currentText()==box->itemText(box->currentIndex())?box->currentData().toString():box->currentText().trimmed(); model->setData(index,value,Qt::EditRole); } else QStyledItemDelegate::setModelData(editor,model,index); }
    QString displayText(const QVariant& value,const QLocale& locale) const override { if (column_==0) return RuleDraft::label(value.toString()); if (column_==2&&value.toString()=="DIRECT") return "直连 · DIRECT"; if (column_==2&&value.toString()=="REJECT") return "阻断 · REJECT"; return QStyledItemDelegate::displayText(value,locale); }
private:
    int column_; QStringList targets_,providers_;
};
}
