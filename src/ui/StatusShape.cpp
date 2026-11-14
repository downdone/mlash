#include "StatusShape.h"
#include "UiTheme.h"
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

namespace mlash {

namespace {
constexpr qreal kSize = 12.0;
constexpr qreal kStroke = 1.6;
constexpr qreal kGap = 1.6;   // 形状与描边之间留的空隙，让两个通道都看得见
}

QPixmap StatusShape::pixmap(const QString& state, qreal devicePixelRatio) {
    // 颜色来自 token，形状来自 state（P7 的可访问性要求）：色盲用户不依赖颜色也能分辨。
    const QColor color = state == "success" ? QColor(UiTheme::Success)
        : state == "warning" ? QColor(UiTheme::Warning)
        : state == "error" ? QColor(UiTheme::Error)
        : state == "active" ? QColor(UiTheme::Brand)
        : QColor(UiTheme::Secondary);
    QPixmap image(int(kSize * devicePixelRatio), int(kSize * devicePixelRatio));
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, kStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const qreal c = kSize / 2.0, r = c - kStroke;
    // 形状编码：成功=实心圆，警告=三角，失败=方，进行中=空心环，空闲=小圆点。
    // 三角与方都是空心+描边，圆/环靠填充区分——形状本身已经不同，不依赖颜色。
    switch (kindOf(state)) {
    case Kind::SolidCircle:
        p.setBrush(color); p.drawEllipse(QPointF(c, c), r - kGap, r - kGap); break;
    case Kind::Triangle: {
        QPolygonF triangle; triangle << QPointF(c, c - r) << QPointF(c + r, c + r * 0.75) << QPointF(c - r, c + r * 0.75);
        p.drawPolygon(triangle); break;
    }
    case Kind::Square:
        p.drawRect(QRectF(kGap, kGap, kSize - 2 * kGap, kSize - 2 * kGap)); break;
    case Kind::Ring:
        p.drawEllipse(QPointF(c, c), r - kGap, r - kGap); break;
    case Kind::Dot:
        p.setBrush(color); p.drawEllipse(QPointF(c, c), r * 0.45, r * 0.45); break;
    }
    return image;
}

StatusShape::Kind StatusShape::kindOf(const QString& state) {
    if (state == "success") return Kind::SolidCircle;
    if (state == "warning") return Kind::Triangle;
    if (state == "error") return Kind::Square;
    if (state == "active") return Kind::Ring;
    return Kind::Dot;
}

QString StatusShape::accessibleName(const QString& state) {
    switch (kindOf(state)) {
    case Kind::SolidCircle: return QStringLiteral("成功");
    case Kind::Triangle: return QStringLiteral("警告");
    case Kind::Square: return QStringLiteral("失败");
    case Kind::Ring: return QStringLiteral("进行中");
    case Kind::Dot: return QStringLiteral("空闲");
    }
    return {};
}

} // namespace mlash
