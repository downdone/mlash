#pragma once
#include "UiTheme.h"
#include <QtWidgets>

namespace mlash::WindowParts {
class ProxySwitch final : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
    // K 版（U08 §7.3）：轨道 46×26、旋钮 20px，与原型 .switch 一致。
    static constexpr int TrackWidth = 46, TrackHeight = 26, Knob = 20, Inset = 3;
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text()) + 62, qMax(28, TrackHeight + 2)}; }
protected:
    void focusInEvent(QFocusEvent* event) override { keyboardFocus_=event->reason()==Qt::TabFocusReason || event->reason()==Qt::BacktabFocusReason || event->reason()==Qt::ShortcutFocusReason; QCheckBox::focusInEvent(event); update(); }
    void focusOutEvent(QFocusEvent* event) override { keyboardFocus_=false; QCheckBox::focusOutEvent(event); update(); }
    void mousePressEvent(QMouseEvent* event) override { keyboardFocus_=false; QCheckBox::mousePressEvent(event); update(); }
    bool hitButton(const QPoint& point) const override { return rect().contains(point); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(isEnabled() ? QColor(UiTheme::Ink) : QColor(UiTheme::Muted)); p.drawText(QRect(0, 0, width() - 56, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        const QRectF track(width() - TrackWidth, (height() - TrackHeight) / 2.0, TrackWidth, TrackHeight);
        p.setPen(Qt::NoPen); p.setBrush(isChecked() ? QColor(isEnabled() ? UiTheme::Primary : UiTheme::Disabled) : QColor(UiTheme::Track)); p.drawRoundedRect(track, TrackHeight / 2.0, TrackHeight / 2.0);
        p.setBrush(QColor(UiTheme::OnPrimary)); p.drawEllipse(QPointF(track.left() + (isChecked() ? TrackWidth - Inset - Knob / 2.0 : Inset + Knob / 2.0), track.center().y()), Knob / 2.0, Knob / 2.0);
        if (hasFocus() && (keyboardFocus_ || property("keyboardFocus").toBool())) { p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(UiTheme::Focus), 1)); p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4); }
    }
private:
    bool keyboardFocus_=false;
};
}
