#pragma once
#include "NodePresentation.h"
#include "UiTheme.h"
#include <QtWidgets>

namespace mlash::WindowParts {
inline QRect nodeDelayRect(const QRect& card,const QFont& font,const QString& value) {
    const int width=qMax(48,QFontMetrics(font).horizontalAdvance(value)+14);
    return {card.right()-width-5,card.center().y()-13,width,26};
}
class NodeNameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool helpEvent(QHelpEvent* event,QAbstractItemView* view,const QStyleOptionViewItem& option,const QModelIndex& index) override {
        if (!index.isValid()) return false;
        const auto card=option.rect.adjusted(3,3,-5,-5);
        if (view->objectName()=="nodeCards" && QRect(card.right()-30,card.center().y()-13,26,26).contains(event->pos())) {
            QToolTip::showText(event->globalPos(),QStringLiteral("查看节点详情；不会切换节点"),view); return true;
        }
        if (view->objectName()=="nodeCards" && nodeDelayRect(card.adjusted(0,0,-32,0),view->font(),index.siblingAtColumn(2).data().toString()).contains(event->pos())) {
            const auto action=view->property("nodeTestEnabled").toBool()?QString("点击重新测试此节点延迟；不会使用或切换节点"):QString("当前无法测速；等待任务结束或内核就绪"); QToolTip::showText(event->globalPos(),action+"<br>"+index.data(Qt::ToolTipRole).toString().toHtmlEscaped().replace('\n',"<br>"),view); return true;
        }
        const auto name=index.siblingAtColumn(0).data(Qt::UserRole).toString(); const bool group=index.siblingAtColumn(1).data().toString()=="策略组 ›";
        const auto region=NodePresentation::location(name);
        const auto hint=(group?QString("策略组 · "):QString())+(!region.code.isEmpty()?region.label+"（按名称识别）":QString("位置未识别"));
        QToolTip::showText(event->globalPos(),index.data(Qt::ToolTipRole).toString().toHtmlEscaped().replace('\n',"<br>")+"<br>"+hint,view); return true;
    }
protected:
    QIcon nodeIcon(const QModelIndex& index) const {
        const auto name=index.siblingAtColumn(0).data(Qt::UserRole).toString(); const bool group=index.siblingAtColumn(1).data().toString()=="策略组 ›";
        const auto key=NodePresentation::iconKey(name,group)+QChar::Null+QString(UiTheme::Secondary);
        if (!icons_.contains(key)) icons_.insert(key,NodePresentation::icon(name,group)); return icons_.value(key);
    }
    void initStyleOption(QStyleOptionViewItem* option,const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option,index); option->icon=nodeIcon(index); option->decorationSize={24,18}; option->features|=QStyleOptionViewItem::HasDecoration;
    }
private:
    mutable QHash<QString,QIcon> icons_;
};
}
