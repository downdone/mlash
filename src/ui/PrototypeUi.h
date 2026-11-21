#pragma once
#include "UiTheme.h"
#include "UiIcons.h"
#include "ElidedLabel.h"
#include "UiHairline.h"
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace mlash::PrototypeUi {
// Shared directory row: the complete row is one accessible action.
class ActionRow final : public QPushButton {
public:
    ActionRow(const QString& title, UiIcons::Glyph glyph, const QString& value={}, const QString& subtitle={}, QWidget* parent=nullptr)
        : QPushButton(parent) {
        setProperty("prototypeRow",true); setCursor(Qt::PointingHandCursor); setAccessibleName(title); setAutoDefault(false);
        auto* layout=new QHBoxLayout(this); layout->setContentsMargins(12,3,10,3); layout->setSpacing(9);
        icon_=new QLabel; icon_->setFixedSize(22,22); icon_->setPixmap(UiIcons::icon(glyph,UiTheme::Secondary).pixmap(22,22));
        icon_->setAttribute(Qt::WA_TransparentForMouseEvents); layout->addWidget(icon_);
        auto* text=new QVBoxLayout; text->setSpacing(1); title_=new ElidedLabel(title); title_->setObjectName("directoryTitle");
        title_->setAttribute(Qt::WA_TransparentForMouseEvents); text->addWidget(title_);
        subtitle_=new ElidedLabel(subtitle); subtitle_->setProperty("secondary",true); subtitle_->setProperty("caption",true);
        subtitle_->setAttribute(Qt::WA_TransparentForMouseEvents); subtitle_->setVisible(!subtitle.isEmpty()); text->addWidget(subtitle_); layout->addLayout(text,1);
        value_=new ElidedLabel(value); value_->setProperty("secondary",true); value_->setMaximumWidth(240); value_->setAttribute(Qt::WA_TransparentForMouseEvents); layout->addWidget(value_);
        chevron_=new QLabel; chevron_->setFixedSize(13,18); chevron_->setPixmap(UiIcons::icon(UiIcons::Glyph::ChevronRight,UiTheme::Secondary).pixmap(13,18));
        chevron_->setAttribute(Qt::WA_TransparentForMouseEvents); layout->addWidget(chevron_);
        connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,[this,glyph] {
            icon_->setPixmap(UiIcons::icon(glyph,UiTheme::Secondary).pixmap(22,22));
            chevron_->setPixmap(UiIcons::icon(UiIcons::Glyph::ChevronRight,UiTheme::Secondary).pixmap(13,18));
        });
    }
    QPoint trailingMenuAnchor() const { return mapToGlobal(QPoint(chevron_->geometry().right()+1,height())); }
    void setTitle(const QString& title) { title_->setText(title); setAccessibleName(title); }
    void setValue(const QString& value) { value_->setText(value); setAccessibleDescription(value); }
    void setSubtitle(const QString& text) { subtitle_->setText(text); subtitle_->setVisible(!text.isEmpty()); updateGeometry(); }
    void setTrailingControl(QWidget* control) { value_->hide(); chevron_->hide(); static_cast<QHBoxLayout*>(layout())->addWidget(control,0,Qt::AlignVCenter); }
    QSize sizeHint() const override { return {260,qMax(UiTheme::RowHeight,layout()->minimumSize().height())}; }
    QSize minimumSizeHint() const override { return {100,sizeHint().height()}; }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        if (isDown()||underMouse()) {
            QPainterPath shape; shape.setFillRule(Qt::WindingFill); shape.addRoundedRect(QRectF(rect()),8,8);
            if (!property("firstRow").toBool()) shape.addRect(QRectF(0,0,width(),8));
            if (!property("lastRow").toBool()) shape.addRect(QRectF(0,height()-8,width(),8));
            painter.fillPath(shape,QColor(isDown()?UiTheme::Pressed:UiTheme::Hover));
        }
        if (!property("lastRow").toBool()) UiHairline::bottom(&painter,rect(),46,QColor(UiTheme::Separator));
        if (hasFocus()&&property("keyboardFocus").toBool()) { painter.setPen(QPen(QColor(UiTheme::Focus),1)); painter.setBrush(Qt::NoBrush); painter.drawRoundedRect(rect().adjusted(1,1,-2,-2),5,5); }
    }
    bool event(QEvent* event) override {
        const auto result=QPushButton::event(event);
        if ((event->type()==QEvent::LayoutRequest||event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange)&&layout()) setMinimumHeight(qMax(UiTheme::RowHeight,layout()->minimumSize().height()));
        return result;
    }
private:
    QLabel *icon_,*chevron_;
    ElidedLabel *title_,*subtitle_,*value_;
};

inline QVBoxLayout* group(QVBoxLayout* page,const QString& title={}) {
    if (!title.isEmpty()) { page->addSpacing(2); auto* label=new QLabel(title); label->setObjectName("prototypeSection"); label->setProperty("secondary",true); page->addWidget(label); }
    auto* frame=new QFrame; frame->setObjectName("prototypeGroup"); frame->setProperty("settingSection",title); auto* rows=new QVBoxLayout(frame);
    rows->setContentsMargins(0,0,0,0); rows->setSpacing(0); page->addWidget(frame); return rows;
}
inline ActionRow* row(QVBoxLayout* rows,const QString& name,const QString& title,UiIcons::Glyph glyph,const QString& value={},const QString& subtitle={}) {
    if (rows->count()) if (auto* previous=rows->itemAt(rows->count()-1)->widget()) previous->setProperty("lastRow",false);
    auto* result=new ActionRow(title,glyph,value,subtitle); result->setObjectName(name); result->setProperty("firstRow",rows->count()==0); result->setProperty("lastRow",true); rows->addWidget(result); return result;
}
inline QPushButton* back(QVBoxLayout* page,const QString& name,const QString& text) {
    auto* button=new QPushButton(QStringLiteral("‹ ")+text); button->setObjectName(name); button->setProperty("backLink",true);
    page->addWidget(button,0,Qt::AlignLeft); return button;
}
}
