#pragma once
#include "ProfileRowModel.h"
#include "UiTheme.h"
#include "UiIcons.h"
#include "UiHairline.h"
#include <QtWidgets>

namespace mlash::WindowParts {
class ProfileRowDelegate final : public QStyledItemDelegate {
public:
    enum Role { IsCurrent = ProfileRowModel::IsCurrent, Kind = ProfileRowModel::Kind,
        Detail = ProfileRowModel::Detail, UsageRatio = ProfileRowModel::UsageRatio, UsageText = ProfileRowModel::UsageText };
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override { return {320,UiTheme::twoLineRowHeight(option.font,UiTheme::ProfileRowHeight)}; }
    static QRect chevronRect(const QRect& row) { return {row.right()-38,row.center().y()-14,28,28}; }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing); const auto rect=option.rect;
        if (option.state.testFlag(QStyle::State_MouseOver)) p->fillRect(rect,QColor(UiTheme::Hover));
        UiHairline::bottom(p,rect,46,QColor(UiTheme::Separator));
        const bool current=index.data(IsCurrent).toBool();
        UiIcons::icon(index.data(Kind).toString()=="remote"?UiIcons::Glyph::Globe:UiIcons::Glyph::Folder,UiTheme::Brand).paint(p,QRect(rect.left()+12,rect.center().y()-10,20,20));
        const auto caption=UiTheme::rowCaptionFont(option.font); const int left=rect.left()+46,width=qMax(0,rect.width()-(current?188:100)),line=QFontMetrics(option.font).height(),top=rect.center().y()-(line+QFontMetrics(caption).height()+1)/2;
        auto title=option.font; title.setBold(current); p->setFont(title); p->setPen(QColor(UiTheme::Ink));
        p->drawText(QRect(left,top,width,line),Qt::AlignVCenter,QFontMetrics(title).elidedText(index.data().toString(),Qt::ElideRight,width));
        auto small=UiTheme::rowCaptionFont(option.font); p->setFont(small); p->setPen(QColor(UiTheme::Secondary));
        const auto detail=index.data(Detail).toString();
        p->drawText(QRect(left,top+line+1,width,QFontMetrics(small).height()),Qt::AlignVCenter,QFontMetrics(small).elidedText(detail,Qt::ElideRight,width));
        if (current) UiIcons::icon(UiIcons::Glyph::Check,UiTheme::Brand).paint(p,QRect(rect.right()-133,rect.center().y()-8,16,16));
        UiIcons::icon(UiIcons::Glyph::ChevronRight,UiTheme::Secondary).paint(p,chevronRect(rect).adjusted(4,4,-4,-4));
        p->restore();
    }
};
}
