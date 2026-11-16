#include "UiIcons.h"
#include "UiTheme.h"
#include <QPainter>
#include <QPainterPath>
#include <QHash>

static void initializeServices() { Q_INIT_RESOURCE(services); }
namespace mlash::UiIcons {
QIcon service(const QString& id) {
    static const bool initialized=[] { initializeServices(); return true; }(); Q_UNUSED(initialized);
    const QIcon result(":/services/"+id+".png"); return result.isNull() ? icon(Glyph::Globe) : result;
}
QIcon icon(Glyph glyph,const QString& color) {
    // Delegates request these icons while scrolling; keep each theme variant
    // once instead of repainting six raster sizes per visible row.
    static QHash<QString,QIcon> cache;
    const QString key=QString::number(int(glyph))+QChar::Null+(color.isEmpty()?QString(UiTheme::Secondary):color)
        +QChar::Null+QString(UiTheme::Disabled)+QChar::Null+QString(UiTheme::Surface);
    if (const auto it=cache.constFind(key); it!=cache.cend()) return *it;
    QIcon result;
    for (const auto mode:{QIcon::Normal,QIcon::Disabled}) for (const qreal dpr:{1.0,2.0,3.0}) {
        // Keep strokes in logical 24-DIP coordinates and rasterize for the
        // actual screen scale instead of scaling a single bitmap everywhere.
        const int side=qRound(24*dpr);
        QPixmap image(side,side); image.setDevicePixelRatio(dpr); image.fill(Qt::transparent);
        QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(QColor(mode==QIcon::Disabled?UiTheme::Disabled:color.isEmpty()?QString(UiTheme::Secondary):color),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPainterPath path;
        switch(glyph) {
        case Glyph::Info: p.drawEllipse(QRectF(3,3,18,18)); p.drawPoint(12,7); p.drawLine(12,11,12,17); break;
        case Glyph::Globe: p.drawEllipse(QRectF(3,3,18,18)); p.drawEllipse(QRectF(8,3,8,18)); p.drawLine(3,12,21,12); break;
        case Glyph::Play: path.moveTo(8,4); path.lineTo(20,12); path.lineTo(8,20); path.closeSubpath(); p.setBrush(p.pen().color()); break;
        case Glyph::Clear: p.drawRoundedRect(QRectF(6,7,12,14),2,2); p.drawLine(4,6,20,6); path.moveTo(9,5); path.lineTo(9,3); path.lineTo(15,3); path.lineTo(15,5); p.drawLine(10,10,10,17); p.drawLine(14,10,14,17); break;
        case Glyph::Clock: p.drawEllipse(QRectF(3,3,18,18)); path.moveTo(12,6); path.lineTo(12,12); path.lineTo(16,14); break;
        case Glyph::Locate: p.drawEllipse(QRectF(6,6,12,12)); p.drawEllipse(QRectF(10,10,4,4)); p.drawLine(12,2,12,6); p.drawLine(12,18,12,22); p.drawLine(2,12,6,12); p.drawLine(18,12,22,12); break;
        case Glyph::Search: p.drawEllipse(QRectF(3,3,13,13)); p.drawLine(15,15,21,21); break;
        case Glyph::Refresh: p.drawArc(QRectF(4,4,16,16),35*16,290*16); path.moveTo(19,3); path.lineTo(20,8); path.lineTo(15,8); break;
        case Glyph::Plus: p.drawLine(12,5,12,19); p.drawLine(5,12,19,12); break;
        case Glyph::Folder: path.moveTo(3,6); path.lineTo(9,6); path.lineTo(12,9); path.lineTo(21,9); path.lineTo(21,20); path.lineTo(3,20); path.closeSubpath(); break;
        case Glyph::Settings: p.drawEllipse(QRectF(5,5,14,14)); p.drawEllipse(QRectF(10,10,4,4)); for (int angle=0;angle<360;angle+=45) { p.save(); p.translate(12,12); p.rotate(angle); p.drawLine(0,7,0,10); p.restore(); } break;
        case Glyph::Copy: p.drawRoundedRect(QRectF(8,7,12,14),2,2); path.moveTo(5,17); path.lineTo(3,17); path.lineTo(3,3); path.lineTo(15,3); break;
        case Glyph::Close: p.drawLine(6,6,18,18); p.drawLine(6,18,18,6); break;
        case Glyph::Save: path.moveTo(4,3); path.lineTo(17,3); path.lineTo(21,7); path.lineTo(21,21); path.lineTo(4,21); path.closeSubpath(); p.drawRect(QRectF(8,3,8,6)); p.drawRect(QRectF(8,14,9,7)); break;
        // 分区图标（6.2c 合并后导航与页面共用同一绘制器）。
        case Glyph::Home: path.moveTo(3,11); path.lineTo(12,3); path.lineTo(21,11); path.moveTo(6,9); path.lineTo(6,21); path.lineTo(10,21); path.lineTo(10,15); path.lineTo(14,15); path.lineTo(14,21); path.lineTo(18,21); path.lineTo(18,9); break;
        case Glyph::Chart: path.moveTo(2,12); path.lineTo(6,12); path.lineTo(9,3); path.lineTo(13,21); path.lineTo(16,9); path.lineTo(18,12); path.lineTo(22,12); break;
        case Glyph::Import: p.drawLine(12,3,12,15); path.moveTo(7,10); path.lineTo(12,15); path.lineTo(17,10); path.moveTo(4,16); path.lineTo(4,21); path.lineTo(20,21); path.lineTo(20,16); break;
        case Glyph::List: p.drawLine(4,6,20,6); p.drawLine(4,12,20,12); p.drawLine(4,18,14,18); break;
        // K 版：原型 ICON 词典里首页与标题行用到的形状（坐标直接取自原型的 24×24 视框）。
        case Glyph::Route: p.drawEllipse(QRectF(4,16,4,4)); p.drawEllipse(QRectF(16,4,4,4)); path.moveTo(6,16); path.lineTo(6,11); path.arcTo(QRectF(6,6,10,10),180,-90); path.lineTo(16,6); path.moveTo(11,15); path.lineTo(15,19); path.lineTo(19,15); break;
        case Glyph::Rocket: path.moveTo(4,16); path.cubicTo(4,10,10,4,20,3); path.cubicTo(20,13,13,19,7,19); p.drawPath(path); path=QPainterPath(); path.moveTo(4,16); path.lineTo(2,22); path.lineTo(8,20); p.drawPath(path); path=QPainterPath(); path.moveTo(10,8); path.lineTo(16,14); p.drawPath(path); p.drawEllipse(QPointF(15,9),1.6,1.6); break;
        case Glyph::Gauge: { QPainterPath arc; arc.moveTo(4.2,18); arc.arcTo(QRectF(2,1.74,20,20),218.8,-257.6); p.drawPath(arc); path.moveTo(12,14); path.lineTo(17,9); p.drawEllipse(QPointF(12,14),1.2,1.2); break; }
        case Glyph::Layers: path.moveTo(12,2); path.lineTo(21,7); path.lineTo(12,12); path.lineTo(3,7); path.closeSubpath(); path.moveTo(3,12); path.lineTo(12,17); path.lineTo(21,12); path.moveTo(3,17); path.lineTo(12,22); path.lineTo(21,17); break;
        case Glyph::Sliders: p.drawLine(4,7,20,7); p.drawLine(4,17,20,17); p.save(); p.setBrush(QColor(UiTheme::Surface)); p.drawEllipse(QPointF(8,7),3,3); p.drawEllipse(QPointF(16,17),3,3); p.restore(); break;
        case Glyph::Check: path.moveTo(5,12); path.lineTo(10,17); path.lineTo(20,7); break;
        case Glyph::ChevronRight: path.moveTo(9,18); path.lineTo(15,12); path.lineTo(9,6); break;
        case Glyph::ChevronDown: path.moveTo(6,9); path.lineTo(12,15); path.lineTo(18,9); break;
        case Glyph::Moon: path.moveTo(20,15.3); path.arcTo(QRectF(3.5,3.5,17,17),-20,-230); path.lineTo(20,15.3); break;
        case Glyph::More: p.save(); p.setPen(Qt::NoPen); p.setBrush(QColor(color.isEmpty()?QString(UiTheme::Secondary):color)); p.drawEllipse(QPointF(5,12),1.5,1.5); p.drawEllipse(QPointF(12,12),1.5,1.5); p.drawEllipse(QPointF(19,12),1.5,1.5); p.restore(); break;
        }
        p.drawPath(path); result.addPixmap(image,mode);
    }
    cache.insert(key,result);
    return result;
}
}
