#include "UiIcons.h"
#include "UiTheme.h"
#include <QPainter>
#include <QPainterPath>

static void initializeServices() { Q_INIT_RESOURCE(services); }
namespace mlash::UiIcons {
QIcon service(const QString& id) {
    static const bool initialized=[] { initializeServices(); return true; }(); Q_UNUSED(initialized);
    const QIcon result(":/services/"+id+".png"); return result.isNull() ? icon(Glyph::Globe) : result;
}
QIcon icon(Glyph glyph,const QString& color) {
    QIcon result;
    for (const auto mode:{QIcon::Normal,QIcon::Disabled}) {
        QPixmap image(48,48); image.setDevicePixelRatio(2); image.fill(Qt::transparent);
        QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(QColor(mode==QIcon::Disabled?UiTheme::Disabled:color.isEmpty()?QString(UiTheme::Secondary):color),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPainterPath path;
        switch(glyph) {
        case Glyph::Info: p.drawEllipse(QRectF(3,3,18,18)); p.drawPoint(12,7); p.drawLine(12,11,12,17); break;
        case Glyph::Globe: p.drawEllipse(QRectF(3,3,18,18)); p.drawEllipse(QRectF(8,3,8,18)); p.drawLine(3,12,21,12); break;
        case Glyph::Play: path.moveTo(8,4); path.lineTo(20,12); path.lineTo(8,20); path.closeSubpath(); p.setBrush(p.pen().color()); break;
        case Glyph::Clear: p.drawRoundedRect(QRectF(6,7,12,14),2,2); p.drawLine(4,6,20,6); path.moveTo(9,5); path.lineTo(9,3); path.lineTo(15,3); path.lineTo(15,5); p.drawLine(10,10,10,17); p.drawLine(14,10,14,17); break;
        case Glyph::Clock: p.drawEllipse(QRectF(3,3,18,18)); path.moveTo(12,6); path.lineTo(12,12); path.lineTo(16,14); break;
        case Glyph::Search: p.drawEllipse(QRectF(3,3,13,13)); p.drawLine(15,15,21,21); break;
        case Glyph::Refresh: p.drawArc(QRectF(4,4,16,16),35*16,290*16); path.moveTo(19,3); path.lineTo(20,8); path.lineTo(15,8); break;
        case Glyph::Plus: p.drawLine(12,5,12,19); p.drawLine(5,12,19,12); break;
        case Glyph::Folder: path.moveTo(3,6); path.lineTo(9,6); path.lineTo(12,9); path.lineTo(21,9); path.lineTo(21,20); path.lineTo(3,20); path.closeSubpath(); break;
        case Glyph::Settings: p.drawEllipse(QRectF(5,5,14,14)); p.drawEllipse(QRectF(10,10,4,4)); for (int angle=0;angle<360;angle+=45) { p.save(); p.translate(12,12); p.rotate(angle); p.drawLine(0,7,0,10); p.restore(); } break;
        case Glyph::Copy: p.drawRoundedRect(QRectF(8,7,12,14),2,2); path.moveTo(5,17); path.lineTo(3,17); path.lineTo(3,3); path.lineTo(15,3); break;
        case Glyph::Close: p.drawLine(6,6,18,18); p.drawLine(6,18,18,6); break;
        case Glyph::Save: path.moveTo(4,3); path.lineTo(17,3); path.lineTo(21,7); path.lineTo(21,21); path.lineTo(4,21); path.closeSubpath(); p.drawRect(QRectF(8,3,8,6)); p.drawRect(QRectF(8,14,9,7)); break;
        }
        p.drawPath(path); result.addPixmap(image,mode);
    }
    return result;
}
}
