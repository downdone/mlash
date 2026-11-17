#pragma once
#include "UiTheme.h"
#include <QtWidgets>

namespace mlash::WindowParts {
class ChevronToolButton final : public QToolButton {
public:
    using QToolButton::QToolButton;
protected:
    void paintEvent(QPaintEvent* event) override {
        QToolButton::paintEvent(event);
        if ((objectName()!="testGroupButton" || !menu()) && objectName()!="browseGroup") return;
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(isEnabled()?QColor(UiTheme::Secondary):QColor(UiTheme::Disabled),1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const qreal x=width()-11,y=height()/2.0; p.drawLine(QPointF(x-3,y-1),QPointF(x,y+2)); p.drawLine(QPointF(x,y+2),QPointF(x+3,y-1));
    }
};
}
