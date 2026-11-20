#pragma once
#include <QFrame>
#include <QModelIndex>

class QAbstractButton;
class QLabel;

namespace mlash {
class ElidedLabel;

class ProfileCardWidget final : public QFrame {
    Q_OBJECT
public:
    enum class Action { Refresh, Menu, Review, Apply, Use };
    explicit ProfileCardWidget(QWidget* parent = nullptr);
    void setProfile(const QModelIndex& index);
    void setSelected(bool selected);
    void setWritable(bool writable);
    QAbstractButton* button(Action action) const;
    static constexpr int CellHeight = 152;
    static constexpr int CellInset = 4;
    static constexpr int HorizontalInset = 16;
    static constexpr int VerticalInset = 14;
    static QRect surfaceRect(const QRect& cell) { return cell.adjusted(CellInset,CellInset,-CellInset,-CellInset); }
signals:
    void actionRequested(Action action);
private:
    void updateIcons();
    QLabel *mark_, *time_, *status_;
    ElidedLabel *title_, *source_, *usage_;
    QAbstractButton *refresh_, *menu_, *review_, *apply_, *use_;
    bool remote_ = false;
};
}
