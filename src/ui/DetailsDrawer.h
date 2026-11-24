#pragma once
#include <QtWidgets>

namespace mlash::WindowParts {

// K3：右侧详情抽屉（原型 .drawer）。它是主窗口上的定位覆盖层，不是独立弹窗：
// 贴右缘、随窗口缩放、Esc 关闭。节点详情、配置详情共用同一外壳，内容由调用方填。
class DetailsDrawer final : public QFrame {
    Q_OBJECT
public:
    explicit DetailsDrawer(QWidget* host);

    void setHeader(const QString& title, const QString& subtitle = QString());
    QLabel* addKeyValue(const QString& key, const QString& value);
    void addHint(const QString& text);
    QHBoxLayout* actions() const { return actions_; }
    void clearActions();
    void clearContent();
    void addWidget(QWidget* widget);
    void open();
    void closeDrawer();
    void toggle();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void restorePage();
    void syncGeometry();

    QWidget* host_;
    QPointer<QStackedWidget> pageHost_;
    QPointer<QWidget> previousFocus_;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QVBoxLayout* body_ = nullptr;
    QHBoxLayout* actions_ = nullptr;
};

} // namespace mlash::WindowParts