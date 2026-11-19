#pragma once
#include "UiTheme.h"
#include "UiIcons.h"
#include "NodePresentation.h"
#include "NodeRowModel.h"
#include "UiHairline.h"

namespace mlash {
// Compact two-line rows expand from actual title/caption metrics (ADR 0053).
class NodeRowDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        return {10, UiTheme::twoLineRowHeight(option.font)};
    }
    static QRect infoRect(const QRect& row) { return {row.right()-38,row.center().y()-14,28,28}; }
    static QRect delayRect(const QRect& row) { return {row.right()-108,row.top()+3,62,row.height()-6}; }
    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const QRect rect=option.rect;
        if (index.data(NodeRowModel::RowKind).toInt()==0) { paintHeader(p,option,index); p->restore(); return; }
        const bool current=index.data(NodeRowModel::IsCurrent).toBool();
        const bool hovered=option.state.testFlag(QStyle::State_MouseOver);
        const bool highlighted=option.state.testFlag(QStyle::State_Selected);
        if (hovered || highlighted) p->fillRect(rect,QColor(highlighted?UiTheme::Selection:UiTheme::Hover));
        UiHairline::bottom(p,rect,46,QColor(UiTheme::Separator));
        const QRect badge(rect.left()+12,rect.center().y()-11,22,22);
        const auto name=index.data(Qt::DisplayRole).toString();
        NodePresentation::icon(name,index.data(NodeRowModel::Subtitle).toString().contains("策略组")).paint(p,badge);
        auto small=UiTheme::rowCaptionFont(option.font);
        const auto latency=index.data(NodeRowModel::LatencyText).toString();
        auto color=QColor(UiTheme::Secondary);
        bool numeric=false; const int ms=latency.section(' ',0,0).toInt(&numeric);
        if (numeric) color=QColor(ms<=300?UiTheme::Success:UiTheme::Warning);
        if (latency.contains("失败")||latency.contains("超时")) color=QColor(UiTheme::Error);
        if (const auto* view=qobject_cast<const QListView*>(option.widget); view&&view->property("nodeTestEnabled").toBool()&&view->property("nodeDelayHover").toString()==name) {
            p->setPen(Qt::NoPen); p->setBrush(QColor(UiTheme::Control)); p->drawRoundedRect(delayRect(rect),5,5);
        }
        p->setFont(small); p->setPen(color);
        p->drawText(delayRect(rect).adjusted(3,0,-3,0),Qt::AlignRight|Qt::AlignVCenter,latency.isEmpty()?QStringLiteral("—"):latency);
        if (current) UiIcons::icon(UiIcons::Glyph::Check,UiTheme::Brand).paint(p,QRect(rect.right()-133,rect.center().y()-8,16,16));
        // The independent details action must be discoverable without hover.
        UiIcons::icon(UiIcons::Glyph::Info,UiTheme::Secondary).paint(p,infoRect(rect).adjusted(4,4,-4,-4));
        const int left=rect.left()+46, width=qMax(0,delayRect(rect).left()-left-(current?30:8));
        auto title=option.font; title.setBold(current); p->setFont(title); p->setPen(QColor(UiTheme::Ink));
        const int line=QFontMetrics(title).height(),caption=QFontMetrics(small).height(); const int top=rect.center().y()-(line+caption+1)/2;
        p->drawText(QRect(left,top,width,line),Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(title).elidedText(name,Qt::ElideRight,width));
        p->setFont(small); p->setPen(QColor(UiTheme::Secondary));
        const auto sub=index.data(NodeRowModel::IsAutoGroup).toBool()?QString("自动选路 · 不可手动切换"):index.data(NodeRowModel::Subtitle).toString();
        p->drawText(QRect(left,top+line+1,width,QFontMetrics(small).height()),Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(small).elidedText(sub,Qt::ElideRight,width));
        if (option.state.testFlag(QStyle::State_HasFocus)&&option.widget&&option.widget->property("keyboardFocus").toBool()) { p->setPen(QPen(QColor(UiTheme::Focus),1)); p->setBrush(Qt::NoBrush); p->drawRoundedRect(rect.adjusted(1,1,-2,-2),5,5); }
        p->restore();
    }
private:
    static void paintHeader(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) {
        const auto rect=option.rect; p->fillRect(rect,QColor(UiTheme::Surface)); UiHairline::bottom(p,rect,46,QColor(UiTheme::Separator));
        UiIcons::icon(index.data(NodeRowModel::GroupCollapsed).toBool()?UiIcons::Glyph::ChevronRight:UiIcons::Glyph::ChevronDown,UiTheme::Brand).paint(p,QRect(rect.left()+12,rect.center().y()-10,20,20));
        auto title=option.font; title.setBold(true); auto small=UiTheme::rowCaptionFont(option.font); const int line=QFontMetrics(title).height(),left=rect.left()+46,width=qMax(0,rect.width()-175),top=rect.center().y()-(line+QFontMetrics(small).height()+1)/2;
        p->setFont(title); p->setPen(QColor(UiTheme::Ink)); p->drawText(QRect(left,top,width,line),Qt::AlignVCenter,QFontMetrics(title).elidedText(index.data(NodeRowModel::SourceName).toString().isEmpty()?index.data().toString():index.data(NodeRowModel::SourceName).toString(),Qt::ElideRight,width));
        p->setFont(small); p->setPen(QColor(UiTheme::Secondary));
        p->drawText(QRect(left,top+line+1,width,QFontMetrics(small).height()),Qt::AlignVCenter,QFontMetrics(small).elidedText(index.data(NodeRowModel::SourceStamp).toString().isEmpty()?index.data().toString():index.data(NodeRowModel::SourceStamp).toString(),Qt::ElideRight,width));
        p->drawText(rect.adjusted(0,0,-46,0),Qt::AlignRight|Qt::AlignVCenter,index.data(NodeRowModel::GroupNote).toString());
        UiIcons::icon(UiIcons::Glyph::Settings,UiTheme::Brand).paint(p,infoRect(rect).adjusted(4,4,-4,-4));
    }
};
}
