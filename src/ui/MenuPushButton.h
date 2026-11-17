#pragma once
#include "UiTheme.h"
#include <QtWidgets>

namespace mlash::WindowParts {
class MenuPushButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
    QSize sizeHint() const override { auto size=QPushButton::sizeHint(); size.setWidth(qMax(size.width(),fontMetrics().horizontalAdvance(text())+38)); return size; }
protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QStyleOptionButton option; initStyleOption(&option); option.text.clear(); QPainter background(this); style()->drawControl(QStyle::CE_PushButton,&option,&background,this); background.end();
        if (!menu()) return;
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(isEnabled()?QColor(UiTheme::Secondary):QColor(UiTheme::Muted),1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const qreal x=width()-12,y=height()/2.0; p.drawPolyline(QPolygonF{{x-3,y-1},{x,y+2},{x+3,y-1}});
        p.setFont(font()); p.setPen(QColor(isEnabled()?UiTheme::Ink:UiTheme::Muted)); const auto textRect=rect().adjusted(10,0,-28,0); p.drawText(textRect,Qt::AlignCenter,fontMetrics().elidedText(text(),Qt::ElideRight,textRect.width()));
    }
};
}
