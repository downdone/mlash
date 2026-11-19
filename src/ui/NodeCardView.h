#pragma once
#include "UiTheme.h"
#include "NodeCardDelegate.h"
#include <QTimer>

namespace mlash::WindowParts {
class NodeCardView final : public QListView {
public:
    explicit NodeCardView(QWidget* parent=nullptr):QListView(parent) { setMouseTracking(true); }
    std::function<void(const QModelIndex&)> delayClicked;
    std::function<void()> gridChanged;
    void setTestEnabled(bool enabled) { if (property("nodeTestEnabled").toBool()!=enabled) { setProperty("nodeTestEnabled",enabled); viewport()->setCursor(enabled && !property("nodeDelayHover").toString().isEmpty()?Qt::PointingHandCursor:Qt::ArrowCursor); viewport()->update(); } }
protected:
    void mouseMoveEvent(QMouseEvent* event) override { const auto index=delayAt(event->position().toPoint()); const auto name=index.isValid()?index.data(Qt::UserRole).toString():QString(); if (property("nodeDelayHover").toString()!=name) { setProperty("nodeDelayHover",name); viewport()->update(); } viewport()->setCursor(index.isValid() && property("nodeTestEnabled").toBool()?Qt::PointingHandCursor:Qt::ArrowCursor); QListView::mouseMoveEvent(event); }
    void mousePressEvent(QMouseEvent* event) override { delayPress_=event->button()==Qt::LeftButton && delayAt(event->position().toPoint()).isValid(); if (delayPress_) { pressedDelay_=delayAt(event->position().toPoint()); event->accept(); return; } pressedDelay_=QPersistentModelIndex{}; QListView::mousePressEvent(event); }
    void mouseReleaseEvent(QMouseEvent* event) override { if (event->button()==Qt::LeftButton && delayPress_) { delayPress_=false; const auto pressed=pressedDelay_; pressedDelay_=QPersistentModelIndex{}; if (pressed.isValid() && pressed==delayAt(event->position().toPoint()) && property("nodeTestEnabled").toBool() && delayClicked) delayClicked(pressed); event->accept(); return; } if (event->button()==Qt::LeftButton && delayAt(event->position().toPoint()).isValid()) { event->accept(); return; } QListView::mouseReleaseEvent(event); }
    void mouseDoubleClickEvent(QMouseEvent* event) override { if (delayAt(event->position().toPoint()).isValid()) { pressedDelay_=QPersistentModelIndex{}; event->accept(); return; } QListView::mouseDoubleClickEvent(event); }
    void resizeEvent(QResizeEvent* event) override { QListView::resizeEvent(event); adjustGrid(); }
    bool viewportEvent(QEvent* event) override { const bool result=QListView::viewportEvent(event); if (event->type()==QEvent::Resize) adjustGrid(); if (event->type()==QEvent::Leave) { setProperty("nodeDelayHover",QString()); viewport()->unsetCursor(); viewport()->update(); } return result; }
private:
    QPersistentModelIndex pressedDelay_;
    bool delayPress_=false;
    QModelIndex delayAt(const QPoint& point) const { const auto index=indexAt(point); if (!index.isValid()) return {}; return nodeDelayRect(visualRect(index).adjusted(3,3,-5,-5),font(),index.siblingAtColumn(2).data().toString()).contains(point)?index:QModelIndex{}; }
    void adjustGrid() {
        // Use the laid-out viewport: maximumViewportSize() can be wider than the
        // actual viewport after the sidebar or a scrollbar changes its width.
        const int width=qMax(1,viewport()->width()-3),columns=qMax(1,width/300);
        const QSize size(qMax(1,width/columns-1),UiTheme::NodeCardHeight);
        if (gridSize()!=size) {
            setGridSize(size);
            if (model() && model()->rowCount()>0) itemDelegate()->sizeHintChanged(model()->index(0,0));
            // Recompute the list's height cap after QListView has reflowed the grid.
            QTimer::singleShot(0,this,[this] { if (gridChanged) gridChanged(); });
        }
    }
};
}
