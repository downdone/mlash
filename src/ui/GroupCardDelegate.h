#pragma once
#include "UiTheme.h"
#include <QtWidgets>

namespace mlash::WindowParts {
class GroupCardDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override { return {154,54}; }
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        painter->save(); painter->setRenderHint(QPainter::Antialiasing); const auto rect=option.rect.adjusted(2,2,-4,-4); const bool selected=option.state.testFlag(QStyle::State_Selected);
        painter->setPen(QColor(selected?UiTheme::Brand:UiTheme::Border)); painter->setBrush(QColor(selected?UiTheme::Selection:UiTheme::Surface)); painter->drawRoundedRect(rect,6,6);
        auto font=option.font; font.setBold(selected); painter->setFont(font); painter->setPen(QColor(UiTheme::Ink)); const QFontMetrics metrics(font);
        painter->drawText(rect.adjusted(10,5,-8,-23),Qt::AlignLeft | Qt::AlignVCenter,metrics.elidedText(index.data().toString(),Qt::ElideRight,rect.width()-18));
        font.setBold(false); painter->setFont(font); painter->setPen(QColor(UiTheme::Secondary)); painter->drawText(rect.adjusted(10,25,-8,-3),Qt::AlignLeft | Qt::AlignVCenter,QString("%1 个成员").arg(index.data(Qt::UserRole+1).toInt())); painter->restore();
    }
};
}
