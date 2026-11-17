#pragma once

#include <QColor>
#include <QPainter>
#include <QRect>
#include <QtMath>

namespace mlash::UiHairline {

// A one-device-pixel separator. Logical 1px QSS borders become 1.5 physical
// pixels at 150% scaling, which makes adjacent rows alternate in darkness.
inline void bottom(QPainter* painter, const QRect& row, int leftInset, const QColor& color) {
    const qreal dpr = painter->device()->devicePixelRatioF();
    const qreal left = qRound((row.left() + leftInset) * dpr) / dpr;
    const qreal right = qRound((row.right() + 1) * dpr) / dpr;
    const qreal edge = qRound((row.bottom() + 1) * dpr) / dpr;
    if (right > left) painter->fillRect(QRectF(left, edge - 1.0 / dpr, right - left, 1.0 / dpr), color);
}

} // namespace mlash::UiHairline
