#include "DetailsDrawer.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace mlash::WindowParts {

DetailsDrawer::DetailsDrawer(QWidget* host) : QFrame(host), host_(host) {
    setObjectName("nodeDetailsDrawer");
    setFrameShape(QFrame::NoFrame);
    pageHost_ = host->findChild<QStackedWidget*>("mainPages");
    setFocusPolicy(Qt::StrongFocus);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(0);
    auto* header = new QHBoxLayout;
    header->setSpacing(10);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(3);
    title_ = new QLabel;
    title_->setObjectName("drawerTitle");
    title_->setWordWrap(true);
    subtitle_ = new QLabel;
    subtitle_->setObjectName("drawerSub");
    subtitle_->setWordWrap(true);
    subtitle_->hide();
    titles->addWidget(title_);
    titles->addWidget(subtitle_);
    header->addLayout(titles, 1);
    auto* close = new QToolButton;
    close->setObjectName("drawerClose");
    close->setIcon(UiIcons::icon(UiIcons::Glyph::Close));
    close->setToolTip(QStringLiteral("关闭"));
    close->setAutoRaise(true);
    connect(close, &QToolButton::clicked, this, &DetailsDrawer::closeDrawer);
    header->addWidget(close, 0, Qt::AlignTop);
    outer->addLayout(header);
    outer->addSpacing(10);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget;
    body->setMaximumWidth(540);
    body_ = new QVBoxLayout(body);
    body_->setContentsMargins(0, 0, 0, 0);
    body_->setSpacing(0);
    body_->addStretch(1);
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
    actions_ = new QHBoxLayout;
    actions_->setSpacing(8);
    actions_->addStretch(1);
    outer->addSpacing(22);
    outer->addLayout(actions_);
    hide();
    host_->installEventFilter(this);
}

void DetailsDrawer::setHeader(const QString& title, const QString& subtitle) {
    title_->setText(title);
    subtitle_->setText(subtitle);
    subtitle_->setVisible(!subtitle.isEmpty());
}

QLabel* DetailsDrawer::addKeyValue(const QString& key, const QString& value) {
    auto* row = new QWidget;
    row->setObjectName("drawerKeyValue");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(10);
    auto* keyLabel = new QLabel(key);
    keyLabel->setObjectName("drawerKey");
    keyLabel->setMinimumWidth(110);
    layout->addWidget(keyLabel);
    auto* valueLabel = new QLabel(value.isEmpty() ? QStringLiteral("—") : value);
    valueLabel->setObjectName("drawerValue");
    valueLabel->setTextFormat(Qt::PlainText);
    valueLabel->setWordWrap(true);
    valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(valueLabel, 1);
    body_->insertWidget(qMax(0, body_->count() - 1), row);
    return valueLabel;
}

void DetailsDrawer::addHint(const QString& text) {
    auto* hint = new QLabel(text);
    hint->setObjectName("drawerHint");
    hint->setProperty("secondary", true);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    body_->insertWidget(qMax(0, body_->count() - 1), hint);
}

void DetailsDrawer::clearActions() {
    while (actions_->count() > 1) {
        auto* item = actions_->takeAt(0);
        if (auto* widget = item->widget()) widget->deleteLater();
        delete item;
    }
}

void DetailsDrawer::clearContent() {
    clearActions();
    while (auto* item = body_->takeAt(0)) {
        if (auto* widget = item->widget()) widget->deleteLater();
        delete item;
    }
    body_->addStretch(1);
}

void DetailsDrawer::addWidget(QWidget* widget) {
    body_->insertWidget(qMax(0, body_->count() - 1), widget);
}

void DetailsDrawer::open() {
    previousFocus_ = QApplication::focusWidget();
    syncGeometry();
    show();
    raise();
    setFocus();
}

void DetailsDrawer::closeDrawer() {
    hide();
    restorePage();
    if (previousFocus_) previousFocus_->setFocus(Qt::OtherFocusReason);
}

void DetailsDrawer::toggle() {
    isVisible() ? closeDrawer() : open();
}

bool DetailsDrawer::eventFilter(QObject* watched, QEvent* event) {
    if (watched == host_ && isVisible() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        syncGeometry();
    return QFrame::eventFilter(watched, event);
}

void DetailsDrawer::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        closeDrawer();
        return;
    }
    QFrame::keyPressEvent(event);
}

void DetailsDrawer::hideEvent(QHideEvent* event) {
    restorePage();
    QFrame::hideEvent(event);
}

void DetailsDrawer::restorePage() {
    if (pageHost_) {
        pageHost_->setContentsMargins(UiTheme::ContentInset, UiTheme::ContentTop,
                                      UiTheme::ContentInset, UiTheme::ContentTop);
        pageHost_->show();
    }
}

void DetailsDrawer::syncGeometry() {
    const int top = UiTheme::TopbarHeight + UiTheme::ContentInset;
    const bool wide = host_->width() - 2 * UiTheme::ContentInset >= 860;
    const int width = wide ? UiTheme::DrawerWidth : host_->width() - 2 * UiTheme::ContentInset;
    setProperty("inspectorMode", wide ? "side" : "page");
    if (pageHost_) {
        pageHost_->setContentsMargins(UiTheme::ContentInset, UiTheme::ContentTop,
                                      wide ? UiTheme::DrawerWidth + 2 * UiTheme::ContentInset : UiTheme::ContentInset,
                                      UiTheme::ContentTop);
        pageHost_->setVisible(wide);
    }
    setGeometry(host_->width() - UiTheme::ContentInset - width, top, width,
                qMax(1, host_->height() - top - UiTheme::ContentInset - 30));
}

} // namespace mlash::WindowParts