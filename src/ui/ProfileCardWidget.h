#pragma once
#include "SummaryCardStyle.h"
#include <QFrame>
#include <QModelIndex>

class QAbstractButton;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;

namespace mlash {
class ProfileCardText;
class WrapLayout;

class ProfileCardWidget final : public QFrame {
    Q_OBJECT
public:
    enum class Action { Refresh, Menu, ReviewUpdate };
    explicit ProfileCardWidget(QWidget* parent = nullptr);
    void setProfile(const QModelIndex& index);
    void setSelected(bool selected);
    void setWritable(bool writable);
    QAbstractButton* button(Action action) const;
    static constexpr int CellHeight = 124;
    static int cellHeightForWidth(int width);
    int minimumWidthForMetadata() const;
    int extraTextHeight(int width) const;
    int fitText(int width);
    static constexpr int CellInset = 8;
    static constexpr int HorizontalInset = 20;
    static constexpr int VerticalInset = 12;
    static QRect surfaceRect(const QRect& cell) { return cell.adjusted(CellInset,CellInset,-CellInset,-CellInset); }
signals:
    void actionRequested(Action action);
protected:
    void resizeEvent(QResizeEvent* event) override;
private:
    void arrangeFooter();
    void updateAppearance();
    struct TextHeights { int title,metadata; };
    int identityWidth(int width) const;
    TextHeights textHeights(int width) const;
    QLabel *mark_, *status_, *statusMark_, *updateStatus_, *updateMark_, *source_, *time_;
    ProfileCardText* title_;
    QAbstractButton *refresh_, *menu_, *reviewUpdate_;
    QWidget* metadata_;
    WrapLayout* tags_;
    QWidget* statusLine_;
    QVBoxLayout* cardLayout_;
    QHBoxLayout* footer_;
    bool stackedFooter_ = false;
    bool canRefresh_ = false;
    bool writable_ = true;
};
}
