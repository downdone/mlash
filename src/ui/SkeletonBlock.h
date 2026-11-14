#pragma once
#include <QWidget>

class QTimer;

namespace mlash {

// 骨架屏（原型 07「加载中」）：用三条灰块占位，代替一句「正在读取…」。
//
// 为什么不是纯文字：加载态与空态此前长得一样——都是一块空白面板加一句说明，
// 用户分不清「还没读」和「读到了但没有」。占位块给出「内容即将出现在这里」的
// 形状信息，且不引入动画（关闭动画时仍是稳定可读的静态占位）。
class SkeletonBlock final : public QWidget {
    Q_OBJECT
public:
    explicit SkeletonBlock(int lines = 3, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    int lines_;
};

} // namespace mlash
