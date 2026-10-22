#include "NodePresentation.h"
#include <QPainter>
#include <QRegularExpression>
#include <QImage>
#include <QCache>
#include <QHash>

static void initializeFlags() { Q_INIT_RESOURCE(flags); }

namespace mlash {
namespace {
struct Region { const char* code; const char* label; const char* aliases; };
const Region regions[] = {
    {"HK","香港","香港|Hong[ -]?Kong|HKG"}, {"TW","台湾","台湾|臺灣|Taiwan|Taipei"},
    {"JP","日本","日本|东京|東京|大阪|Japan|Tokyo|Osaka"}, {"SG","新加坡","新加坡|狮城|Singapore"},
    {"US","美国","美国|美國|United States|USA|Los Angeles|San Jose"}, {"GB","英国","英国|英國|United Kingdom|UK|London"},
    {"KR","韩国","韩国|韓國|首尔|首爾|South Korea|Seoul"}, {"DE","德国","德国|德國|Germany|Frankfurt"},
    {"FR","法国","法国|法國|France|Paris"}, {"CA","加拿大","加拿大|Canada"},
    {"AU","澳大利亚","澳大利亚|澳洲|Australia|Sydney"}, {"NZ","新西兰","新西兰|New Zealand"},
    {"MY","马来西亚","马来西亚|Malaysia"}, {"TH","泰国","泰国|Thailand"},
    {"VN","越南","越南|Vietnam"}, {"ID","印度尼西亚","印度尼西亚|印尼|Indonesia"},
    {"IN","印度","印度|India"}, {"PH","菲律宾","菲律宾|Philippines"},
    {"PK","巴基斯坦","巴基斯坦|Pakistan"}, {"IL","以色列","以色列|Israel"},
    {"AE","阿联酋","阿联酋|阿聯酋|UAE|Dubai"}, {"EG","埃及","埃及|Egypt"},
    {"NG","尼日利亚","尼日利亚|Nigeria"}, {"TR","土耳其","土耳其|Turkey|Türkiye"},
    {"NL","荷兰","荷兰|荷蘭|Netherlands|Amsterdam"}, {"CH","瑞士","瑞士|Switzerland"},
    {"SE","瑞典","瑞典|Sweden"}, {"RU","俄罗斯","俄罗斯|俄羅斯|Russia"},
    {"BR","巴西","巴西|Brazil"}, {"AR","阿根廷","阿根廷|Argentina"},
    {"ZA","南非","南非|South Africa"}, {"MO","澳门","澳门|澳門|Macau|Macao"},
    {"CN","中国","中国|中國|大陆|大陸|China"},
    {"ES","西班牙","西班牙|Spain|Madrid"}, {"IT","意大利","意大利|義大利|Italy|Milan"},
    {"HU","匈牙利","匈牙利|Hungary"}, {"CL","智利","智利|Chile"},
    {"NO","挪威","挪威|Norway"}, {"AT","奥地利","奥地利|奧地利|Austria"},
    {"PL","波兰","波兰|波蘭|Poland"}, {"BE","比利时","比利时|比利時|Belgium"},
    {"FI","芬兰","芬兰|芬蘭|Finland"}, {"DK","丹麦","丹麦|丹麥|Denmark"},
    {"IE","爱尔兰","爱尔兰|愛爾蘭|Ireland"}, {"PT","葡萄牙","葡萄牙|Portugal"},
    {"UA","乌克兰","乌克兰|烏克蘭|Ukraine"}, {"MA","摩洛哥","摩洛哥|Morocco"},
    {"RO","罗马尼亚","罗马尼亚|羅馬尼亞|Romania"}, {"CZ","捷克","捷克|Czechia|Czech Republic"},
    {"MX","墨西哥","墨西哥|Mexico"}, {"IS","冰岛","冰岛|冰島|Iceland"}
};
}
namespace {
// Names are transient subscription data; assets have a fixed, finite key space.
thread_local QCache<QString, NodeLocation> locations(1024);
NodeLocation identifyLocation(const QString& name) {
    const auto chars=name.toUcs4();
    for (qsizetype i=0;i+1<chars.size();++i) {
        if (chars[i]<0x1f1e6 || chars[i]>0x1f1ff || chars[i+1]<0x1f1e6 || chars[i+1]>0x1f1ff) continue;
        const QString code=QString(QChar(ushort('A'+chars[i]-0x1f1e6)))+QChar(ushort('A'+chars[i+1]-0x1f1e6));
        for (const auto& r:regions) if (code==QLatin1String(r.code)) return {code,QString::fromUtf8(r.label)};
    }
    struct Prepared { NodeLocation region; QRegularExpression hint; };
    static const QList<Prepared> prepared=[] {
        QList<Prepared> result;
        for (const auto& r:regions) {
            QStringList patterns;
            for (const auto& alias:QString::fromUtf8(r.aliases).split('|')) {
                const bool latin=!alias.isEmpty() && alias.front().unicode()<128;
                patterns.append(latin?QString("(?<![\\p{L}\\p{N}])(?:%1)(?![\\p{L}\\p{N}])").arg(alias):alias);
            }
            patterns.append(QString("(?-i:(?<![\\p{L}\\p{N}])%1(?![\\p{L}\\p{N}]))").arg(QString::fromLatin1(r.code)));
            result.append({{QString::fromLatin1(r.code),QString::fromUtf8(r.label)},QRegularExpression(patterns.join('|'),QRegularExpression::CaseInsensitiveOption)});
        }
        return result;
    }();
    for (const auto& r:prepared) if (r.hint.match(name).hasMatch()) return r.region;
    return {};
}
}
NodeLocation NodePresentation::location(const QString& name) {
    if (const auto* cached = locations.object(name)) return *cached;
    const auto result = identifyLocation(name);
    if (name.size() <= 512) locations.insert(name, new NodeLocation(result));
    return result;
}
void NodePresentation::clearTransientCache() { locations.clear(); }
QString NodePresentation::iconKey(const QString& name, bool group) {
    const auto region=location(name);
    return !region.code.isEmpty()?region.code:group?QString("group"):name=="DIRECT"?QString("direct"):name=="REJECT" || name=="REJECT-DROP"?QString("reject"):QString("unknown");
}
QIcon NodePresentation::icon(const QString& name, bool group) {
    const auto region=location(name);
    const auto key=!region.code.isEmpty()?region.code:group?QString("group"):name=="DIRECT"?QString("direct"):name=="REJECT"||name=="REJECT-DROP"?QString("reject"):QString("unknown");
    static QHash<QString,QIcon> assets;
    if (const auto cached=assets.constFind(key); cached!=assets.cend()) return cached.value();
    if (!region.code.isEmpty()) {
        static const bool initialized=[] { initializeFlags(); return true; }(); Q_UNUSED(initialized);
        QImage flag(":/flags/"+region.code+".png");
        if (!flag.isNull()) {
            QRect bounds;
            for (int y=0;y<flag.height();++y) for (int x=0;x<flag.width();++x) if (qAlpha(flag.pixel(x,y))>0) bounds=bounds.united(QRect(x,y,1,1));
            const QIcon icon(QPixmap::fromImage(flag.copy(bounds)));
            assets.insert(key,icon);
            return icon;
        }
    }
    QPixmap pixmap(96,72); pixmap.fill(Qt::transparent); QPainter p(&pixmap); p.scale(4,4); p.setRenderHint(QPainter::Antialiasing);
    const QColor ink(group?"#7863ab":key=="reject"?"#bb5151":key=="direct"?"#348263":"#4e789e");
    p.setPen(Qt::NoPen); p.setBrush(group?QColor("#eee9f7"):QColor("#e9f1f8")); p.drawRoundedRect(QRectF(0,0,24,18),3,3); p.setPen(QPen(ink,1.2));
    if (!region.code.isEmpty()) { QFont font("Segoe UI"); font.setPixelSize(9); font.setBold(true); p.setFont(font); p.drawText(QRectF(0,0,24,18),Qt::AlignCenter,region.code); }
    else if (group) { p.drawLine(5,5,12,2); p.drawLine(12,2,19,5); p.drawLine(19,5,12,8); p.drawLine(12,8,5,5); p.drawPolyline(QPolygonF({{5,9},{12,12},{19,9}})); p.drawPolyline(QPolygonF({{5,13},{12,16},{19,13}})); }
    else if (key=="direct") { p.drawLine(5,9,19,9); p.drawLine(14,4,19,9); p.drawLine(14,14,19,9); }
    else if (key=="reject") { p.drawLine(8,5,16,13); p.drawLine(16,5,8,13); }
    else { p.drawEllipse(QRectF(6,3,12,12)); p.drawEllipse(QRectF(9,3,6,12)); p.drawLine(6,9,18,9); }
    p.end(); const QIcon icon(pixmap); assets.insert(key,icon); return icon;
}
}
