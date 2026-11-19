#pragma once

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QKeyEvent;
class QLabel;
class QPaintEvent;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace mlash {

// A persistent, bounded chooser. Checking several columns does not close it.
class ConnectionColumnsPopup final : public QWidget {
public:
    explicit ConnectionColumnsPopup(QWidget* anchor);
    QCheckBox* addColumn(int id, const QString& title, bool visible, bool required);
    QPushButton* addFooterAction(const QString& title, const QString& name);
    void openBelow(QWidget* anchor);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QPointer<QWidget> anchor_;
    QPointer<QWidget> host_;
    QScrollArea* scroll_ = nullptr;
    QLabel* heading_ = nullptr;
    QWidget* footerHost_ = nullptr;
    QWidget* rows_ = nullptr;
    QVBoxLayout* rowsLayout_ = nullptr;
    QVBoxLayout* footer_ = nullptr;
};

} // namespace mlash
