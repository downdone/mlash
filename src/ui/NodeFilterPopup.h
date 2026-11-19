#pragma once

#include "UiTheme.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidget>

namespace mlash {

// Persistent controls in a transient surface: hiding the popup never resets filters.
class NodeFilterPopup final : public QWidget {
public:
    explicit NodeFilterPopup(QWidget* anchor)
        : QWidget(anchor, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint), anchor_(anchor) {
        setObjectName("nodeFilterPopup");
        setAttribute(Qt::WA_TranslucentBackground);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(6);
        auto* heading = new QLabel("筛选节点", this);
        heading->setProperty("secondary", true);
        layout->addWidget(heading);
    }

    void addField(QWidget* field) { layout()->addWidget(field); field->installEventFilter(this); }

    void openBelow(QWidget* anchor) {
        if (!anchor || !anchor->isVisible()) return;
        auto* host=anchor->window();
        if (host_ != host) {
            if (host_) host_->removeEventFilter(this);
            host_=host;
            if (host_) host_->installEventFilter(this);
        }
        const QPoint point=anchor->mapToGlobal(QPoint(0,anchor->height()));
        auto* screen=QGuiApplication::screenAt(point);
        if (!screen) screen=anchor->screen();
        if (!screen) screen=QGuiApplication::primaryScreen();
        if (!screen) return;
        const QRect available=screen->availableGeometry().adjusted(6,6,-6,-6);
        layout()->activate();
        const QSize wanted(qMin(240,available.width()),qMin(sizeHint().height(),available.height()));
        setFixedSize(wanted);
        const int x=qBound(available.left(),point.x(),qMax(available.left(),available.right()-wanted.width()+1));
        const int above=anchor->mapToGlobal(QPoint(0,0)).y()-wanted.height()-4;
        const int below=point.y()+4;
        const int y=below+wanted.height()<=available.bottom()+1?below:qMax(available.top(),above);
        move(x,y);
        show();
        if (auto* field=findChild<QWidget*>("nodeRegion")) {
            field->setFocus(Qt::PopupFocusReason);
        }
    }

protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if (watched==host_ && isVisible()
            && (event->type()==QEvent::Move || event->type()==QEvent::Resize || event->type()==QEvent::Hide)) {
            close();
        }
        if (isVisible() && event->type()==QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape) {
            close();
            return true;
        }
        return QWidget::eventFilter(watched,event);
    }
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key()==Qt::Key_Escape) { close(); event->accept(); return; }
        QWidget::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(UiTheme::Border),0));
        painter.setBrush(QColor(UiTheme::Surface));
        const qreal inset=0.5/devicePixelRatioF();
        painter.drawRoundedRect(QRectF(rect()).adjusted(inset,inset,-inset,-inset),8,8);
    }
    void hideEvent(QHideEvent* event) override {
        QWidget::hideEvent(event);
        if (anchor_ && anchor_->isVisible()) anchor_->setFocus(Qt::PopupFocusReason);
    }
private:
    QPointer<QWidget> anchor_;
    QPointer<QWidget> host_;
};

} // namespace mlash
