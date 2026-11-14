#pragma once
#include <QPixmap>
#include <QString>

namespace mlash {

// 状态形状编码（界面分层设计 P7「尊重平台与可访问性」/ 原型 07）。
//
// 状态此前只靠颜色表达（一个换颜色的「●」），色盲用户拿不到第二通道。
// 这里把 state 映射成形状：成功=实心圆、警告=三角、失败=方、进行中=空心环、空闲=小点。
// 颜色仍来自 UiTheme token，形状与颜色是两个独立通道。
class StatusShape {
public:
    enum class Kind { SolidCircle, Triangle, Square, Ring, Dot };
    static Kind kindOf(const QString& state);
    static QPixmap pixmap(const QString& state, qreal devicePixelRatio = 2.0);
    // 读屏用：形状说出来的名字，与视觉通道一致。
    static QString accessibleName(const QString& state);
};

} // namespace mlash
