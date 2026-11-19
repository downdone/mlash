#pragma once
#include "ProfileRowModel.h"
#include "UiTheme.h"
#include "UiIcons.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>

namespace mlash {
// One card has one primary action (use) and independent secondary hit areas.
// The view consumes secondary mouse events before its clicked signal fires.
class ProfileCardDelegate final : public QStyledItemDelegate {
public:
    enum class Hit { Use, Refresh, Menu, Review, Apply };
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {300, 144}; }
    static QRect card(const QRect& row) { return row.adjusted(4, 4, -4, -4); }
    static QRect refresh(const QRect& row) { const auto r=card(row); return {r.right()-77,r.top()+12,30,30}; }
    static QRect menu(const QRect& row) { const auto r=card(row); return {r.right()-41,r.top()+12,30,30}; }
    static QRect review(const QRect& row) { const auto r=card(row); return {r.right()-161,r.bottom()-37,64,30}; }
    static QRect apply(const QRect& row) { const auto r=card(row); return {r.right()-91,r.bottom()-37,80,30}; }
    static Hit hit(const QRect& row, const QPoint& point, const QModelIndex& index) {
        if (index.data(ProfileRowModel::HasCandidate).toBool()) {
            if (review(row).contains(point)) return Hit::Review;
            if (apply(row).contains(point)) return Hit::Apply;
        }
        if (index.data(ProfileRowModel::CanRefresh).toBool() && refresh(row).contains(point)) return Hit::Refresh;
        return menu(row).contains(point) ? Hit::Menu : Hit::Use;
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save(); painter->setRenderHint(QPainter::Antialiasing);
        const auto r=card(option.rect); const bool current=index.data(ProfileRowModel::IsCurrent).toBool();
        const bool candidate=index.data(ProfileRowModel::HasCandidate).toBool();
        const QColor edge(current ? UiTheme::Brand : UiTheme::Separator);
        painter->setPen(QPen(edge,1)); painter->setBrush(QColor(UiTheme::Surface)); painter->drawRoundedRect(r,10,10);
        if (option.state.testFlag(QStyle::State_MouseOver)) {
            auto hover=QColor(UiTheme::Hover); hover.setAlpha(75);
            painter->setPen(Qt::NoPen); painter->setBrush(hover); painter->drawRoundedRect(r.adjusted(1,1,-1,-1),9,9);
        }
        const int left=r.left()+18, right=r.right()-100;
        auto title=option.font; title.setBold(true); title.setPointSizeF(title.pointSizeF()+1);
        painter->setFont(title); painter->setPen(QColor(UiTheme::Ink));
        const auto name=index.data(Qt::DisplayRole).toString();
        painter->drawText(QRect(left,r.top()+11,qMax(1,right-left),22),Qt::AlignVCenter,
                          QFontMetrics(title).elidedText(name,Qt::ElideRight,qMax(1,right-left)));
        auto small=UiTheme::rowCaptionFont(option.font); painter->setFont(small); painter->setPen(QColor(UiTheme::Secondary));
        const auto source=index.data(ProfileRowModel::SourceLabel).toString();
        painter->drawText(QRect(left,r.top()+39,qMax(1,r.right()-left-122),19),Qt::AlignVCenter,
                          QFontMetrics(small).elidedText(source,Qt::ElideRight,qMax(1,r.right()-left-122)));
        const auto updated=index.data(ProfileRowModel::UpdatedLabel).toString();
        painter->drawText(QRect(r.right()-118,r.top()+39,104,19),Qt::AlignRight|Qt::AlignVCenter,updated);
        const auto usage=index.data(ProfileRowModel::UsageText).toString();
        if (!usage.isEmpty()) painter->drawText(QRect(left,r.top()+59,r.width()-36,18),Qt::AlignVCenter,
                                  QFontMetrics(small).elidedText(usage,Qt::ElideRight,r.width()-36));
        const auto error=index.data(ProfileRowModel::UpdateError).toString();
        if (!error.isEmpty()) {
            painter->setPen(QColor(UiTheme::Error));
            painter->drawText(QRect(left,r.top()+(usage.isEmpty()?59:79),r.width()-36,18),Qt::AlignVCenter,
                              QFontMetrics(small).elidedText(QStringLiteral("更新失败 · ")+error,Qt::ElideRight,r.width()-36));
        }
        if (current) { painter->setPen(QColor(UiTheme::Brand));
            painter->drawText(QRect(left,r.bottom()-(candidate?63:28),r.width()-36,19),Qt::AlignVCenter,QStringLiteral("当前使用")); }
        painter->setPen(QColor(UiTheme::Brand));
        if (index.data(ProfileRowModel::CanRefresh).toBool())
            UiIcons::icon(UiIcons::Glyph::Refresh,UiTheme::Brand).paint(painter,refresh(option.rect).adjusted(5,5,-5,-5));
        UiIcons::icon(UiIcons::Glyph::More,UiTheme::Secondary).paint(painter,menu(option.rect).adjusted(5,5,-5,-5));
        if (candidate) {
            const QRect strip(r.left()+1,r.bottom()-41,r.width()-2,40);
            QPainterPath clip; clip.addRoundedRect(r.adjusted(1,1,-1,-1),9,9);
            painter->setClipPath(clip);
            auto pendingFill=QColor(UiTheme::Warning); pendingFill.setAlpha(21);
            painter->fillRect(strip,pendingFill); painter->setClipping(false);
            painter->setPen(QColor(UiTheme::Warning));
            painter->drawText(QRect(left,strip.top()+7,qMax(1,r.width()-209),28),Qt::AlignVCenter,
                              QStringLiteral("待应用"));
            painter->setPen(QColor(UiTheme::Brand));
            painter->drawText(review(option.rect),Qt::AlignCenter,QStringLiteral("查看"));
            painter->drawText(apply(option.rect),Qt::AlignCenter,QStringLiteral("应用更新"));
        }
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            painter->setPen(QPen(QColor(UiTheme::Brand),1)); painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(r.adjusted(2,2,-2,-2),8,8);
        }
        painter->restore();
    }
};
}
