#pragma once
#include "ProfileRowModel.h"
#include "UiTheme.h"
#include "UiIcons.h"
#include <QPainter>
#include <QPainterPath>
#include <QListView>
#include <QStyledItemDelegate>

namespace mlash {
// One card has one primary action (use) and independent secondary hit areas.
// The view consumes secondary mouse events before its clicked signal fires.
class ProfileCardDelegate final : public QStyledItemDelegate {
public:
    enum class Hit { Use, Refresh, Menu, Review, Apply };
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        const auto* view=qobject_cast<const QListView*>(option.widget);
        return {view && view->gridSize().width()>0 ? view->gridSize().width() : 300, 112};
    }
    static QRect card(const QRect& row) { return row.adjusted(4, 4, -4, -4); }
    static QRect refresh(const QRect& row) { const auto r=card(row); return {r.right()-77,r.top()+9,30,30}; }
    static QRect menu(const QRect& row) { const auto r=card(row); return {r.right()-41,r.top()+9,30,30}; }
    static QRect review(const QRect& row) { const auto r=card(row); return {r.right()-161,r.bottom()-32,64,28}; }
    static QRect apply(const QRect& row) { const auto r=card(row); return {r.right()-91,r.bottom()-32,80,28}; }
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
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(current?UiTheme::Selection:UiTheme::Surface));
        painter->drawRoundedRect(r,10,10);
        if (current) {
            painter->setPen(QPen(QColor(UiTheme::Brand),3,Qt::SolidLine,Qt::RoundCap));
            painter->drawLine(r.left()+2,r.top()+13,r.left()+2,r.bottom()-13);
        }
        if (option.state.testFlag(QStyle::State_MouseOver)) {
            auto hover=QColor(UiTheme::Hover); hover.setAlpha(75);
            painter->setPen(Qt::NoPen); painter->setBrush(hover); painter->drawRoundedRect(r.adjusted(1,1,-1,-1),9,9);
        }
        // Daymark tiles use a small emblem and compact adjacent metadata.
        const QRect emblem(r.left()+16,r.top()+13,26,26);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(UiTheme::Primary));
        painter->drawEllipse(emblem);
        const auto kind=index.data(ProfileRowModel::Kind).toString();
        UiIcons::icon(kind==QStringLiteral("remote")?UiIcons::Glyph::Globe:UiIcons::Glyph::Folder,
                      UiTheme::OnPrimary).paint(painter,emblem.adjusted(5,5,-5,-5));
        const int left=r.left()+53, right=r.right()-100;
        auto title=option.font; title.setWeight(QFont::DemiBold); title.setPointSizeF(title.pointSizeF()+0.5);
        painter->setFont(title); painter->setPen(QColor(UiTheme::Ink));
        const auto name=index.data(Qt::DisplayRole).toString();
        painter->drawText(QRect(left,r.top()+11,qMax(1,right-left),22),Qt::AlignVCenter,
                          QFontMetrics(title).elidedText(name,Qt::ElideRight,qMax(1,right-left)));
        auto small=UiTheme::rowCaptionFont(option.font); painter->setFont(small); painter->setPen(QColor(UiTheme::Secondary));
        const auto source=index.data(ProfileRowModel::SourceLabel).toString();
        painter->drawText(QRect(left,r.top()+37,qMax(1,r.right()-left-122),19),Qt::AlignVCenter,
                          QFontMetrics(small).elidedText(source,Qt::ElideRight,qMax(1,r.right()-left-122)));
        const auto updated=index.data(ProfileRowModel::UpdatedLabel).toString();
        painter->drawText(QRect(r.right()-118,r.top()+37,104,19),Qt::AlignRight|Qt::AlignVCenter,updated);
        const auto usage=index.data(ProfileRowModel::UsageText).toString();
        const auto error=index.data(ProfileRowModel::UpdateError).toString();
        painter->setPen(QColor(!error.isEmpty()?UiTheme::Error:current?UiTheme::Brand:UiTheme::Secondary));
        const auto footer=!error.isEmpty()?QStringLiteral("更新失败 · 点击刷新重试"):
            current?QStringLiteral("当前使用"):QStringLiteral("点击卡片使用");
        if (!candidate && !footer.isEmpty()) painter->drawText(QRect(left,r.top()+64,qMax(1,r.width()-170),18),Qt::AlignVCenter,
                                                QFontMetrics(small).elidedText(footer,Qt::ElideRight,qMax(1,r.width()-170)));
        if (!candidate && !usage.isEmpty()) { painter->setPen(QColor(UiTheme::Secondary));
            painter->drawText(QRect(r.right()-156,r.top()+64,142,18),Qt::AlignRight|Qt::AlignVCenter,
                              QFontMetrics(small).elidedText(usage,Qt::ElideLeft,142)); }
        painter->setPen(QColor(UiTheme::Brand));
        if (index.data(ProfileRowModel::CanRefresh).toBool())
            UiIcons::icon(UiIcons::Glyph::Refresh,UiTheme::Brand).paint(painter,refresh(option.rect).adjusted(5,5,-5,-5));
        UiIcons::icon(UiIcons::Glyph::More,UiTheme::Secondary).paint(painter,menu(option.rect).adjusted(5,5,-5,-5));
        if (candidate) {
            const QRect strip(r.left()+1,r.bottom()-31,r.width()-2,30);
            QPainterPath clip; clip.addRoundedRect(r.adjusted(1,1,-1,-1),9,9);
            painter->setClipPath(clip);
            painter->fillRect(strip,QColor(UiTheme::Hover)); painter->setClipping(false);
            painter->setPen(QColor(UiTheme::Secondary));
            painter->drawText(QRect(left,strip.top()+2,qMax(1,r.width()-209),25),Qt::AlignVCenter,
                              QStringLiteral("待应用"));
            painter->setPen(QColor(UiTheme::Brand));
            painter->drawText(review(option.rect),Qt::AlignCenter,QStringLiteral("查看"));
            painter->drawText(apply(option.rect),Qt::AlignCenter,QStringLiteral("应用更新"));
        }
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            auto focus=QColor(UiTheme::Brand); focus.setAlpha(130);
            painter->setPen(QPen(focus,1)); painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(r.adjusted(2,2,-2,-2),8,8);
        }
        painter->restore();
    }
};
}
