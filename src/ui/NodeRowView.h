#pragma once
#include "UiTheme.h"
#include "NodeRowDelegate.h"

namespace mlash {
// Delay is an independent hit target: consume press/release before QListView selection.
class NodeRowView final : public QListView {
public:
    explicit NodeRowView(QWidget* parent=nullptr):QListView(parent) { setMouseTracking(true); }
    std::function<void(const QModelIndex&)> delayClicked,infoClicked;
    void setTestEnabled(bool enabled) { setProperty("nodeTestEnabled",enabled); viewport()->update(); }
protected:
    void paintEvent(QPaintEvent* event) override {
        QListView::paintEvent(event);
        const QRectF bounds(0,0,viewport()->width(),viewport()->height());
        QPainterPath outside; outside.addRect(bounds);
        QPainterPath inside; inside.addRoundedRect(bounds,8,8);
        QPainter painter(viewport()); painter.setRenderHint(QPainter::Antialiasing);
        painter.fillPath(outside.subtracted(inside),QColor(UiTheme::Window));
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        const auto index=delayAt(event->position().toPoint());
        setProperty("nodeDelayHover",index.isValid()?index.data().toString():QString());
        viewport()->setCursor((infoAt(event->position().toPoint()).isValid()||(index.isValid()&&property("nodeTestEnabled").toBool()))?Qt::PointingHandCursor:Qt::ArrowCursor);
        viewport()->update(); QListView::mouseMoveEvent(event);
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button()==Qt::LeftButton&&infoAt(event->position().toPoint()).isValid()) { pressedInfo_=infoAt(event->position().toPoint()); event->accept(); return; }
        if (event->button()==Qt::LeftButton && delayAt(event->position().toPoint()).isValid()) { pressed_=delayAt(event->position().toPoint()); event->accept(); return; }
        pressed_=QPersistentModelIndex{}; QListView::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button()==Qt::LeftButton&&(pressedInfo_.isValid()||infoAt(event->position().toPoint()).isValid())) {
            const auto pressed=pressedInfo_; pressedInfo_=QPersistentModelIndex{};
            if (pressed.isValid()&&pressed==infoAt(event->position().toPoint())&&infoClicked) infoClicked(pressed);
            event->accept(); return;
        }
        if (event->button()==Qt::LeftButton && (pressed_.isValid()||delayAt(event->position().toPoint()).isValid())) {
            const auto pressed=pressed_; pressed_=QPersistentModelIndex{};
            if (pressed.isValid()&&pressed==delayAt(event->position().toPoint())&&property("nodeTestEnabled").toBool()&&delayClicked) delayClicked(pressed);
            event->accept(); return;
        }
        QListView::mouseReleaseEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override { if (delayAt(event->position().toPoint()).isValid()) { event->accept(); return; } QListView::mouseDoubleClickEvent(event); }
    bool viewportEvent(QEvent* event) override {
        if (event->type()==QEvent::Leave) { setProperty("nodeDelayHover",QString()); viewport()->unsetCursor(); viewport()->update(); }
        if (event->type()==QEvent::ToolTip) {
            auto* help=static_cast<QHelpEvent*>(event);
            if (delayAt(help->pos()).isValid()) { QToolTip::showText(help->globalPos(),property("nodeTestEnabled").toBool()?QString("重新测试此节点延迟，不切换节点"):QString("当前无法测速，请等待内核就绪或任务结束"),this); return true; }
        }
        return QListView::viewportEvent(event);
    }
private:
    QPersistentModelIndex pressed_,pressedInfo_;
    QModelIndex infoAt(const QPoint& point) const { const auto index=indexAt(point); return index.isValid()&&NodeRowDelegate::infoRect(visualRect(index)).contains(point)?index:QModelIndex{}; }
    QModelIndex delayAt(const QPoint& point) const {
        const auto index=indexAt(point);
        return index.isValid()&&index.data(NodeRowModel::RowKind).toInt()!=0&&NodeRowDelegate::delayRect(visualRect(index)).contains(point)?index:QModelIndex{};
    }
};

}
