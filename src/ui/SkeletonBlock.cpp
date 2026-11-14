#include "SkeletonBlock.h"
#include "UiTheme.h"
#include <QPainter>

namespace mlash {

SkeletonBlock::SkeletonBlock(int lines, QWidget* parent) : QWidget(parent), lines_(qMax(1, lines)) {
    setObjectName("skeletonBlock");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(lines_ * (UiTheme::TableRow / 2) + (lines_ - 1) * 6);
    setAccessibleName("正在读取");
}

void SkeletonBlock::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(UiTheme::Track));
    const int rowHeight = qMax(6, UiTheme::TableRow / 2 - 6);
    const int gap = 6;
    // 末行短一些：提示「这里有内容，但还没到」，而不是假装已经填满。
    const int widths[] = {90, 76, 52};
    for (int line = 0; line < lines_; ++line) {
        const int percent = widths[qMin(line, 2)];
        const int blockWidth = qMax(rowHeight, width() * percent / 100);
        p.drawRoundedRect(QRect(0, line * (rowHeight + gap), blockWidth, rowHeight), 3, 3);
    }
}

} // namespace mlash
