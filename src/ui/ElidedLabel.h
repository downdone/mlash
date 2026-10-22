#pragma once
#include <QLabel>
#include <QPainter>
#include <QHelpEvent>
#include <QToolTip>

namespace mlash {
class ElidedLabel final : public QLabel {
public:
    explicit ElidedLabel(QWidget* parent=nullptr):QLabel(parent) { setTextFormat(Qt::PlainText); }
    explicit ElidedLabel(const QString& text,QWidget* parent=nullptr):QLabel(text,parent) { setTextFormat(Qt::PlainText); }
    QSize sizeHint() const override { auto size=QLabel::sizeHint(); const auto m=contentsMargins(); size.setHeight(fontMetrics().height()+m.top()+m.bottom()); return size; }
    QSize minimumSizeHint() const override { return QSize(0,sizeHint().height()); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this); painter.setPen(palette().color(QPalette::WindowText)); painter.setFont(font());
        painter.drawText(contentsRect(),Qt::AlignLeft|Qt::AlignVCenter|Qt::TextSingleLine,fontMetrics().elidedText(QString(text()).replace('\n',' ').replace('\r',' '),Qt::ElideRight,contentsRect().width()));
    }
    bool event(QEvent* event) override {
        if (event->type()==QEvent::ToolTip) {
            auto full=text(); const auto hint=toolTip();
            if (!hint.isEmpty()&&hint!=full) {
                if (hint.contains(full)) full=hint;
                else if (!full.contains(hint)) full+="\n"+hint;
            }
            QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(),"<qt>"+full.toHtmlEscaped().replace('\n',"<br>")+"</qt>",this); return true;
        }
        return QLabel::event(event);
    }
};
}
