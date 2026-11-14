#include "UsageDelegate.h"
#include "UiTheme.h"
#include <QApplication>
#include <QPainter>

namespace mlash {

UsageDelegate::UsageDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize UsageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), UiTheme::TableRow));
    return size;
}

void UsageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const auto ratio = index.data(Qt::UserRole).toDouble();
    const auto text = index.data(Qt::UserRole + 1).toString();
    if (text.isEmpty()) {
        // 没有额度信息：不画任何东西，列本身在调用方隐藏。
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    painter->save();
    if (option.state & QStyle::State_Selected) painter->fillRect(option.rect, option.palette.highlight());
    const auto rect = option.rect.adjusted(4, 0, -8, 0);
    const int textHeight = painter->fontMetrics().height();
    const int barHeight = 4;
    const int centerY = rect.center().y();
    const QRect textRect(rect.left(), centerY - textHeight / 2 - barHeight, rect.width(), textHeight);
    painter->setPen(QColor(option.state & QStyle::State_Selected ? option.palette.highlightedText().color().name() : QString(UiTheme::Secondary)));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    const QRect track(rect.left(), centerY + barHeight / 2, rect.width(), barHeight);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(UiTheme::Track));
    painter->drawRoundedRect(track, barHeight / 2.0, barHeight / 2.0);
    // 比例钳到 [0,1]：超额订阅画出满条而不是溢出，避免把「超了」误读成「更多」。
    const qreal clamped = qBound(0.0, ratio, 1.0);
    if (clamped > 0) {
        painter->setBrush(QColor(clamped >= 1.0 ? UiTheme::Error : UiTheme::Brand));
        painter->drawRoundedRect(QRectF(track.left(), track.top(), track.width() * clamped, track.height()), barHeight / 2.0, barHeight / 2.0);
    }
    painter->restore();
}

} // namespace mlash
