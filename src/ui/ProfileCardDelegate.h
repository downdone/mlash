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
    static QRect review(const QRect& row) { const auto r=card(row); return {r.right()-165,r.bottom()-35,64,30}; }
    static QRect apply(const QRect& row) { const auto r=card(row); return {r.right()-95,r.bottom()-35,84,30}; }
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
        painter->setPen(QPen(QColor(UiTheme::Border),1));
        painter->setBrush(QColor(UiTheme::Surface));
        painter->drawRoundedRect(r,9,9);
        if (current) {
            painter->setPen(QPen(QColor(UiTheme::Brand),3,Qt::SolidLine,Qt::RoundCap));
            painter->drawLine(r.left()+3,r.top()+15,r.left()+3,r.bottom()-15);
        }
        if (option.state.testFlag(QStyle::State_MouseOver)) {
            auto hover=QColor(UiTheme::Hover); hover.setAlpha(75);
            painter->setPen(Qt::NoPen); painter->setBrush(hover); painter->drawRoundedRect(r.adjusted(1,1,-1,-1),9,9);
        }
        const QRect emblem(r.left()+18,r.top()+17,31,31);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(UiTheme::Selection));
        painter->drawEllipse(emblem);
        const auto kind=index.data(ProfileRowModel::Kind).toString();
        UiIcons::icon(kind==QStringLiteral("remote")?UiIcons::Glyph::Globe:UiIcons::Glyph::Folder,
                      UiTheme::Brand).paint(painter,emblem.adjusted(7,7,-7,-7));
        const int left=r.left()+62, right=r.right()-100;
        auto title=option.font; title.setWeight(QFont::DemiBold); title.setPointSizeF(title.pointSizeF()+1.5);
        painter->setFont(title); painter->setPen(QColor(UiTheme::Ink));
        const auto name=index.data(Qt::DisplayRole).toString();
        painter->drawText(QRect(left,r.top()+16,qMax(1,right-left),25),Qt::AlignVCenter,
                          QFontMetrics(title).elidedText(name,Qt::ElideRight,qMax(1,right-left)));
        auto small=UiTheme::rowCaptionFont(option.font); painter->setFont(small); painter->setPen(QColor(UiTheme::Secondary));
        const auto source=index.data(ProfileRowModel::SourceLabel).toString();
        painter->drawText(QRect(left,r.top()+44,qMax(1,r.right()-left-130),20),Qt::AlignVCenter,
                          QFontMetrics(small).elidedText(source,Qt::ElideRight,qMax(1,r.right()-left-122)));
        const auto updated=index.data(ProfileRowModel::UpdatedLabel).toString();
        painter->drawText(QRect(r.right()-126,r.top()+44,112,20),Qt::AlignRight|Qt::AlignVCenter,updated);
        const auto usage=index.data(ProfileRowModel::UsageText).toString();
        const auto error=index.data(ProfileRowModel::UpdateError).toString();
        painter->setPen(QColor(!error.isEmpty()?UiTheme::Error:current?UiTheme::Brand:UiTheme::Secondary));
        const auto footer=!error.isEmpty()?QStringLiteral("更新失败"):
            current?QStringLiteral("当前使用"):QStringLiteral("未使用");
        painter->setPen(QColor(UiTheme::Separator)); painter->drawLine(r.left()+17,r.bottom()-35,r.right()-17,r.bottom()-35);
        if (!candidate && !footer.isEmpty()) { painter->setPen(QColor(!error.isEmpty()?UiTheme::Error:current?UiTheme::Brand:UiTheme::Secondary)); painter->drawText(QRect(left,r.bottom()-30,qMax(1,r.width()-170),23),Qt::AlignVCenter,
                                                QFontMetrics(small).elidedText(footer,Qt::ElideRight,qMax(1,r.width()-170)));
        }
        if (!candidate && !usage.isEmpty()) { painter->setPen(QColor(UiTheme::Secondary));
            painter->drawText(QRect(r.right()-170,r.bottom()-30,156,23),Qt::AlignRight|Qt::AlignVCenter,
                              QFontMetrics(small).elidedText(usage,Qt::ElideLeft,156)); }
        painter->setPen(QColor(UiTheme::Brand));
        if (index.data(ProfileRowModel::CanRefresh).toBool())
            UiIcons::icon(UiIcons::Glyph::Refresh,UiTheme::Brand).paint(painter,refresh(option.rect).adjusted(5,5,-5,-5));
        UiIcons::icon(UiIcons::Glyph::More,UiTheme::Secondary).paint(painter,menu(option.rect).adjusted(5,5,-5,-5));
        if (candidate) {
            painter->setPen(QColor(UiTheme::Secondary));
            painter->drawText(QRect(left,r.bottom()-30,qMax(1,r.width()-209),23),Qt::AlignVCenter,
                              QStringLiteral("待应用"));
            painter->setPen(QColor(UiTheme::Brand));
            painter->drawText(review(option.rect),Qt::AlignCenter,QStringLiteral("查看"));
            painter->drawText(apply(option.rect),Qt::AlignCenter,QStringLiteral("应用更新"));
        }
        if (!current && option.state.testFlag(QStyle::State_HasFocus) && option.widget && option.widget->property("keyboardFocus").toBool()) {
            auto focus=QColor(UiTheme::Brand); focus.setAlpha(130);
            painter->setPen(QPen(focus,1)); painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(r.adjusted(2,2,-2,-2),8,8);
        }
        painter->restore();
    }
};
}
