#pragma once
#include "UiTheme.h"
#include "NodeNameDelegate.h"

namespace mlash::WindowParts {
class NodeCardDelegate final : public NodeNameDelegate {
public:
    using NodeNameDelegate::NodeNameDelegate;
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override { const auto* view=qobject_cast<QListView*>(parent()); return view?view->gridSize():QSize(300,UiTheme::NodeCardHeight); }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing); const auto rect=option.rect.adjusted(3,3,-5,-5); const bool selected=option.state.testFlag(QStyle::State_Selected); const bool current=index.data(Qt::FontRole).value<QFont>().bold();
        const bool hover=option.state.testFlag(QStyle::State_MouseOver);
        p->setPen(Qt::NoPen); p->setBrush(QColor(selected?UiTheme::Selection:hover?UiTheme::Hover:UiTheme::Control)); p->drawRoundedRect(rect,6,6);
        if (current) { p->setPen(Qt::NoPen); p->setBrush(QColor(UiTheme::Brand)); p->drawEllipse(QPointF(rect.left()+6,rect.center().y()),2,2); }
        nodeIcon(index).paint(p,QRect(rect.left()+14,rect.center().y()-9,24,18)); auto font=option.font; font.setBold(false); const auto delay=index.siblingAtColumn(2); const auto value=delay.data().toString(); const auto hit=nodeDelayRect(rect,font,value);
        const auto name=index.data(Qt::UserRole).toString(); const auto* view=qobject_cast<QListView*>(parent());
        if (view && view->property("nodeTestEnabled").toBool() && view->property("nodeDelayHover").toString()==name) { p->setPen(Qt::NoPen); p->setBrush(QColor(UiTheme::Selection)); p->drawRoundedRect(hit,4,4); }
        const bool group=index.siblingAtColumn(1).data().toString()=="策略组 ›";
        const auto protocol=group?QStringLiteral("策略组 ›"):index.siblingAtColumn(1).data().toString();
        const auto caption=UiTheme::rowCaptionFont(option.font);
        const int titleHeight=QFontMetrics(font).height(),captionHeight=QFontMetrics(caption).height();
        const int textTop=rect.center().y()-(titleHeight+captionHeight+2)/2;
        const int textWidth=qMax(0,hit.left()-rect.left()-57);
        const QRect titleRect(rect.left()+48,textTop,textWidth,titleHeight);
        const QRect captionRect(rect.left()+48,textTop+titleHeight+2,textWidth,captionHeight);
        font.setBold(current); p->setFont(font); p->setPen(QColor(UiTheme::Ink));
        p->drawText(titleRect,Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(font).elidedText((index.data(Qt::UserRole+8).toBool()?QString("★ "):QString())+name,Qt::ElideRight,textWidth));
        p->setFont(caption); p->setPen(QColor(UiTheme::Secondary));
        p->drawText(captionRect,Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(caption).elidedText(protocol,Qt::ElideRight,textWidth));
        font.setBold(false); p->setFont(font); const auto brush=delay.data(Qt::ForegroundRole).value<QBrush>(); p->setPen(brush.style()==Qt::NoBrush?QColor(UiTheme::Secondary):brush.color()); p->drawText(hit.adjusted(4,0,-5,0),Qt::AlignRight | Qt::AlignVCenter,value);
        if (option.state.testFlag(QStyle::State_HasFocus) && view && view->property("keyboardFocus").toBool()) { p->setPen(QPen(QColor(UiTheme::Focus),1.5)); p->setBrush(Qt::NoBrush); p->drawRoundedRect(rect.adjusted(2,2,-2,-2),4,4); } p->restore();
    }
};
}
