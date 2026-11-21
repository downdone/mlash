#pragma once
#include "UiTheme.h"
#include "UiIcons.h"
#include <QtWidgets>

namespace mlash::WindowParts {
// K3：右侧详情抽屉（原型 .drawer）。它是主窗口上的定位覆盖层，不是独立弹窗：
// 贴右缘、随窗口缩放、Esc 关闭。节点详情、配置详情共用同一外壳，内容由调用方填。
class DetailsDrawer final : public QFrame {
    Q_OBJECT
public:
    explicit DetailsDrawer(QWidget* host) : QFrame(host), host_(host) {
        setObjectName("nodeDetailsDrawer"); setFrameShape(QFrame::NoFrame); pageHost_=host->findChild<QStackedWidget*>("mainPages"); setFocusPolicy(Qt::StrongFocus);
        auto* outer = new QVBoxLayout(this); outer->setContentsMargins(14,12,14,12); outer->setSpacing(0);
        auto* header = new QHBoxLayout; header->setSpacing(10); auto* titles = new QVBoxLayout; titles->setSpacing(3);
        title_ = new QLabel; title_->setObjectName("drawerTitle"); title_->setWordWrap(true);
        subtitle_ = new QLabel; subtitle_->setObjectName("drawerSub"); subtitle_->setWordWrap(true); subtitle_->hide();
        titles->addWidget(title_); titles->addWidget(subtitle_); header->addLayout(titles, 1);
        auto* close = new QToolButton; close->setObjectName("drawerClose"); close->setIcon(UiIcons::icon(UiIcons::Glyph::Close)); close->setToolTip(QStringLiteral("关闭")); close->setAutoRaise(true);
        connect(close, &QToolButton::clicked, this, &DetailsDrawer::closeDrawer);
        header->addWidget(close, 0, Qt::AlignTop);
        outer->addLayout(header); outer->addSpacing(10);
        auto* scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* body=new QWidget; body->setMaximumWidth(540); body_=new QVBoxLayout(body); body_->setContentsMargins(0,0,0,0); body_->setSpacing(0); body_->addStretch(1); scroll->setWidget(body); outer->addWidget(scroll,1);
        actions_ = new QHBoxLayout; actions_->setSpacing(8); actions_->addStretch(1); outer->addSpacing(22); outer->addLayout(actions_);
        hide(); host_->installEventFilter(this);
    }
    void setHeader(const QString& title, const QString& subtitle = QString()) { title_->setText(title); subtitle_->setText(subtitle); subtitle_->setVisible(!subtitle.isEmpty()); }
    QLabel* addKeyValue(const QString& key, const QString& value) {
        auto* row = new QWidget; row->setObjectName("drawerKeyValue");
        auto* layout = new QHBoxLayout(row); layout->setContentsMargins(0, 6, 0, 6); layout->setSpacing(10);
        auto* keyLabel = new QLabel(key); keyLabel->setObjectName("drawerKey"); keyLabel->setMinimumWidth(110); layout->addWidget(keyLabel);
        auto* valueLabel = new QLabel(value.isEmpty() ? QStringLiteral("—") : value); valueLabel->setObjectName("drawerValue");
        valueLabel->setTextFormat(Qt::PlainText); valueLabel->setWordWrap(true); valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); layout->addWidget(valueLabel, 1);
        body_->insertWidget(qMax(0,body_->count()-1),row); return valueLabel;
    }
    void addHint(const QString& text) { auto* hint = new QLabel(text); hint->setObjectName("drawerHint"); hint->setProperty("secondary", true); hint->setWordWrap(true); hint->setTextFormat(Qt::PlainText); body_->insertWidget(qMax(0,body_->count()-1),hint); }
    QHBoxLayout* actions() const { return actions_; }
    void clearActions() { while (actions_->count() > 1) { auto* item = actions_->takeAt(0); if (auto* widget = item->widget()) widget->deleteLater(); delete item; } }
    void clearContent() { clearActions(); while (auto* item = body_->takeAt(0)) { if (auto* widget = item->widget()) widget->deleteLater(); delete item; } body_->addStretch(1); }
    void addWidget(QWidget* widget) { body_->insertWidget(qMax(0,body_->count()-1),widget); }
    void open() { previousFocus_=QApplication::focusWidget(); syncGeometry(); show(); raise(); setFocus(); }
    void closeDrawer() { hide(); restorePage(); if (previousFocus_) previousFocus_->setFocus(Qt::OtherFocusReason); }
    void toggle() { isVisible() ? closeDrawer() : open(); }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == host_ && isVisible() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) syncGeometry();
        return QFrame::eventFilter(watched, event);
    }
    void keyPressEvent(QKeyEvent* event) override { if (event->key() == Qt::Key_Escape) { closeDrawer(); return; } QFrame::keyPressEvent(event); }
private:
    void restorePage() { if (pageHost_) { pageHost_->setContentsMargins(UiTheme::ContentInset,UiTheme::ContentTop,UiTheme::ContentInset,UiTheme::ContentTop); pageHost_->show(); } }
    void hideEvent(QHideEvent* event) override { restorePage(); QFrame::hideEvent(event); }
    void syncGeometry() {
        const int top=UiTheme::TopbarHeight+UiTheme::ContentInset;
        const bool wide=host_->width()-2*UiTheme::ContentInset>=860;
        const int width=wide?UiTheme::DrawerWidth:host_->width()-2*UiTheme::ContentInset;
        setProperty("inspectorMode",wide?"side":"page");
        if (pageHost_) { pageHost_->setContentsMargins(UiTheme::ContentInset,UiTheme::ContentTop,wide?UiTheme::DrawerWidth+2*UiTheme::ContentInset:UiTheme::ContentInset,UiTheme::ContentTop); pageHost_->setVisible(wide); }
        setGeometry(host_->width()-UiTheme::ContentInset-width,top,width,qMax(1,host_->height()-top-UiTheme::ContentInset-30));
    }
    QWidget* host_;
    QPointer<QStackedWidget> pageHost_;
    QPointer<QWidget> previousFocus_;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QVBoxLayout* body_ = nullptr;
    QHBoxLayout* actions_ = nullptr;
};
}
