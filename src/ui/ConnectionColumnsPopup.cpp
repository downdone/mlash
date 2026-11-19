#include "ConnectionColumnsPopup.h"
#include "UiTheme.h"
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace mlash {

ConnectionColumnsPopup::ConnectionColumnsPopup(QWidget* anchor)
    : QWidget(anchor, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint), anchor_(anchor) {
    setObjectName("connectionColumnsPopup");
    setAttribute(Qt::WA_TranslucentBackground);
    auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(10,10,10,10);
    layout->setSpacing(6);
    heading_=new QLabel("显示列",this);
    heading_->setProperty("secondary",true);
    layout->addWidget(heading_);
    scroll_=new QScrollArea(this);
    scroll_->setObjectName("connectionColumnScroll");
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rows_=new QWidget;
    rows_->setObjectName("connectionColumnRows");
    rowsLayout_=new QVBoxLayout(rows_);
    rowsLayout_->setContentsMargins(0,0,0,0);
    rowsLayout_->setSpacing(2);
    scroll_->setWidget(rows_);
    layout->addWidget(scroll_);
    auto* separator=new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    layout->addWidget(separator);
    footerHost_=new QWidget(this);
    footer_=new QVBoxLayout(footerHost_);
    footer_->setContentsMargins(0,0,0,0);
    footer_->setSpacing(2);
    layout->addWidget(footerHost_);
}

QCheckBox* ConnectionColumnsPopup::addColumn(int id,const QString& title,bool visible,bool required) {
    auto* option=new QCheckBox(title,rows_);
    option->setObjectName("connectionColumn"+QString::number(id));
    option->setChecked(visible);
    option->setEnabled(!required);
    option->setMinimumHeight(qMax(28,fontMetrics().height()+8));
    option->setAccessibleName(title);
    option->installEventFilter(this);
    rowsLayout_->addWidget(option);
    return option;
}

QPushButton* ConnectionColumnsPopup::addFooterAction(const QString& title,const QString& name) {
    auto* action=new QPushButton(title,this);
    action->setObjectName(name);
    action->setProperty("quiet",true);
    action->installEventFilter(this);
    footer_->addWidget(action);
    return action;
}

void ConnectionColumnsPopup::openBelow(QWidget* anchor) {
    if (!anchor || !anchor->isVisible()) return;
    anchor_=anchor;
    auto* host=anchor->window();
    if (host_!=host) {
        if (host_) host_->removeEventFilter(this);
        host_=host;
        if (host_) host_->installEventFilter(this);
    }
    const QPoint bottom=anchor->mapToGlobal(QPoint(anchor->width(),anchor->height()));
    auto* screen=QGuiApplication::screenAt(bottom);
    if (!screen) screen=anchor->screen();
    if (!screen) screen=QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect available=screen->availableGeometry().adjusted(6,6,-6,-6);
    rowsLayout_->activate();
    const int rowsHeight=rows_->sizeHint().height();
    const int reserved=20+heading_->sizeHint().height()+footerHost_->sizeHint().height()+24;
    scroll_->setFixedHeight(qMin(rowsHeight,qMin(360,qMax(0,available.height()-reserved))));
    layout()->activate();
    const int width=qMin(280,available.width());
    const int height=qMin(480,qMin(sizeHint().height(),available.height()));
    setFixedSize(width,height);
    const int x=qBound(available.left(),bottom.x()-width,qMax(available.left(),available.right()-width+1));
    const int below=bottom.y()+4;
    const int above=anchor->mapToGlobal(QPoint(0,0)).y()-height-4;
    const int y=below+height<=available.bottom()+1?below:qMax(available.top(),above);
    move(x,y);
    show();
    if (auto* first=findChild<QCheckBox*>("connectionColumn1")) first->setFocus(Qt::PopupFocusReason);
}

bool ConnectionColumnsPopup::eventFilter(QObject* watched,QEvent* event) {
    if (watched==host_ && isVisible()
        && (event->type()==QEvent::Move || event->type()==QEvent::Resize || event->type()==QEvent::Hide)) close();
    if (isVisible() && event->type()==QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape) {
        close(); return true;
    }
    return QWidget::eventFilter(watched,event);
}

void ConnectionColumnsPopup::keyPressEvent(QKeyEvent* event) {
    if (event->key()==Qt::Key_Escape) { close(); event->accept(); return; }
    QWidget::keyPressEvent(event);
}

void ConnectionColumnsPopup::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(UiTheme::Border),0));
    painter.setBrush(QColor(UiTheme::Surface));
    const qreal inset=0.5/devicePixelRatioF();
    painter.drawRoundedRect(QRectF(rect()).adjusted(inset,inset,-inset,-inset),8,8);
}

void ConnectionColumnsPopup::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (anchor_ && anchor_->isVisible()) anchor_->setFocus(Qt::PopupFocusReason);
}

} // namespace mlash
