#pragma once
#include <QtWidgets>

namespace mlash::WindowParts {
class DashboardCardButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
    QSize sizeHint() const override { auto size=QPushButton::sizeHint(); if (layout()) size.setHeight(qMax(size.height(),layout()->minimumSize().height())); return size; }
    QSize minimumSizeHint() const override { auto size=QPushButton::minimumSizeHint(); if (layout()) size.setHeight(qMax(size.height(),layout()->minimumSize().height())); return size; }
protected:
    bool event(QEvent* event) override {
        const auto handled=QPushButton::event(event);
        if ((event->type()==QEvent::LayoutRequest||event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange)&&layout()) { const int height=layout()->minimumSize().height(); if (height>0&&minimumHeight()!=height) setMinimumHeight(height); }
        return handled;
    }
};
}
