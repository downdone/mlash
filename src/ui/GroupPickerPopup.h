#pragma once

#include "UiIcons.h"
#include "UiTheme.h"
#include <QApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>
#include <functional>

namespace mlash {

// Searchable choices are a popup panel, not a command menu with an embedded form.
// Size and anchor are final before show(), so the first painted frame is rounded.
class GroupPickerPopup final : public QWidget {
public:
    explicit GroupPickerPopup(QWidget* anchor)
        : QWidget(anchor, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) {
        setObjectName("groupPickerPopup");
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);
        query_ = new QLineEdit(this);
        query_->setObjectName("groupSearch");
        query_->setPlaceholderText("搜索策略组");
        layout->addWidget(query_);
        choices_ = new QListWidget(this);
        choices_->setObjectName("groupChoices");
        choices_->setFrameShape(QFrame::NoFrame);
        choices_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        choices_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        choices_->setIconSize(QSize(16, 16));
        layout->addWidget(choices_);
        connect(query_, &QLineEdit::textChanged, choices_, [this](const QString& text) {
            int visible = 0;
            for (int row = 0; row < choices_->count(); ++row) {
                auto* item = choices_->item(row);
                const bool matches = item->text().contains(text, Qt::CaseInsensitive);
                item->setHidden(!matches);
                visible += matches;
            }
            if (visible) for (int row = 0; row < choices_->count(); ++row)
                if (!choices_->item(row)->isHidden()) { choices_->setCurrentRow(row); break; }
        });
        const auto choose = [this](QListWidgetItem* item) {
            if (!item) return;
            if (chosen) chosen(item->data(Qt::UserRole).toString());
            close();
        };
        connect(choices_, &QListWidget::itemClicked, this, choose);
        connect(choices_, &QListWidget::itemActivated, this, choose);
        connect(query_, &QLineEdit::returnPressed, this, [this, choose] {
            for (int row = 0; row < choices_->count(); ++row)
                if (!choices_->item(row)->isHidden()) { choose(choices_->item(row)); break; }
        });
        auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        connect(escape, &QShortcut::activated, this, &QWidget::close);
    }

    std::function<void(const QString&)> chosen;

    void addChoice(const QString& name, bool current) {
        auto* item = new QListWidgetItem(name, choices_);
        item->setData(Qt::UserRole, name);
        item->setSizeHint(QSize(0, qMax(30, fontMetrics().height() + 10)));
        if (current) {
            item->setIcon(UiIcons::icon(UiIcons::Glyph::Check, UiTheme::Brand));
            choices_->setCurrentItem(item);
        }
    }

    void openBelow(QWidget* anchor) {
        if (!anchor) return;
        const QPoint global = anchor->mapToGlobal(QPoint(0, anchor->height()));
        auto* screen = QGuiApplication::screenAt(global);
        if (!screen) screen = anchor->screen();
        const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
        const int rows = qMin(8, qMax(1, choices_->count()));
        const int listHeight = rows * qMax(30, fontMetrics().height() + 10) + 4;
        choices_->setFixedHeight(qMin(listHeight, qMax(80, available.height() - 80)));
        const int width = qMin(300, available.width());
        setFixedSize(width, choices_->height() + query_->sizeHint().height() + 22);
        const int x = qBound(available.left(), global.x(), qMax(available.left(), available.right() - width + 1));
        const int below = global.y() + 4;
        const int above = anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 4;
        const int y = below + height() <= available.bottom() ? below : qMax(available.top(), above);
        move(x, y);
        show();
        query_->setFocus(Qt::PopupFocusReason);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(UiTheme::Border), 1));
        painter.setBrush(QColor(UiTheme::Surface));
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 8, 8);
    }

private:
    QLineEdit* query_ = nullptr;
    QListWidget* choices_ = nullptr;
};

} // namespace mlash
