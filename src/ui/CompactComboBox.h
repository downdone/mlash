#pragma once
#include "UiTheme.h"
#include <QComboBox>
#include <QEvent>
#include <QAbstractItemView>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionComboBox>

namespace mlash {
// Shared rule: a compact selector must not change value from a wheel gesture that was
// only passing over it while scrolling the surrounding list. Focus or an open popup
// keeps the usual behaviour.
class CompactComboBox final : public QComboBox {
public:
    explicit CompactComboBox(QWidget* parent=nullptr):QComboBox(parent) { setProperty("customChevron",true); }
    QSize sizeHint() const override { return QComboBox::sizeHint()+QSize(8,0); }
    bool event(QEvent* event) override {
        if (event->type()==QEvent::Wheel && !hasFocus() && !view()->isVisible()) { event->ignore(); return false; }
        return QComboBox::event(event);
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        if (!isEditable()) {
            QStylePainter label(this); QStyleOptionComboBox option; initStyleOption(&option);
            if (property("shortLabel").toBool()) option.currentText=option.currentText.section(" · ",0,0);
            const auto field=style()->subControlRect(QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxEditField,this);
            const int iconSpace=option.currentIcon.isNull()?0:option.iconSize.width()+4;
            option.currentText=fontMetrics().elidedText(option.currentText,Qt::ElideRight,qMax(0,field.width()-iconSpace-4));
            label.drawComplexControl(QStyle::CC_ComboBox,option); label.drawControl(QStyle::CE_ComboBoxLabel,option);
        } else QComboBox::paintEvent(event);
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(isEnabled()?QColor(UiTheme::Secondary):QColor(UiTheme::Disabled),1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const qreal x=width()-12,y=height()/2.0;
        painter.drawLine(QPointF(x-3,y-1),QPointF(x,y+2)); painter.drawLine(QPointF(x,y+2),QPointF(x+3,y-1));
    }
};
}
