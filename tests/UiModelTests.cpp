#include "ui/ProxyTableModel.h"
#include "ui/UiHairline.h"
#include <QJsonArray>
#include <QImage>
#include <QSortFilterProxyModel>
#include <QtTest>

using namespace mlash;

class UiModelTests final : public QObject {
    Q_OBJECT
private slots:
    void hairlineStaysOnePhysicalPixelAtFractionalDpi() {
        QImage image(90,60,QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(1.5);
        image.fill(Qt::white);
        {
            QPainter painter(&image);
            UiHairline::bottom(&painter,QRect(0,0,60,20),0,Qt::black);
        }
        QCOMPARE(image.pixelColor(20,28),QColor(Qt::white));
        QCOMPARE(image.pixelColor(20,29),QColor(Qt::black));
        QCOMPARE(image.pixelColor(20,30),QColor(Qt::white));
    }
    void largeNodeModelSorting() {
        QJsonObject proxies; QJsonArray members;
        for (int i=0;i<500;++i) {
            const auto name=QString("香港 %1").arg(i,3,10,QChar('0'));
            members.append(name); proxies[name]=QJsonObject{{"type","Shadowsocks"}};
        }
        proxies["Group"]=QJsonObject{{"all",members},{"now","香港 250"},{"type","Selector"}};
        ProxyTableModel model; model.setGroup(proxies,"Group");
        QCOMPARE(model.rowCount(),500);
        QVERIFY(model.data(model.index(250,0)).toString().startsWith("●"));
        QCOMPARE(model.data(model.index(42,2)).toString(),"—");
        model.setDelay("香港 042",5,{}); model.setDelay("香港 043",-1,"timeout");
        QSortFilterProxyModel filter; filter.setSourceModel(&model); filter.setSortRole(Qt::UserRole); filter.sort(2);
        QCOMPARE(model.nameAt(filter.mapToSource(filter.index(0,0)).row()),"香港 042");
        QCOMPARE(model.data(model.index(43,2)).toString(),"失败");
        filter.setFilterKeyColumn(0); filter.setFilterFixedString("香港 04"); QCOMPARE(filter.rowCount(),10);
        const QPersistentModelIndex selected(model.index(42,0)); model.setGroup(proxies,"Group");
        QVERIFY(selected.isValid()); QCOMPARE(selected.row(),42);
        QCOMPARE(model.columnCount(),4);
        QCOMPARE(model.data(model.index(42,3)).toString(),QString("已测"));
        QCOMPARE(model.data(model.index(43,3)).toString(),QString("失败"));
        QVERIFY(model.headerData(0,Qt::Horizontal,Qt::TextAlignmentRole).toInt() & Qt::AlignLeft);
        proxies["Child"]=QJsonObject{{"all",QJsonArray{"香港 042"}},{"now","香港 042"},{"type","Selector"}};
        proxies["Parent"]=QJsonObject{{"all",QJsonArray{"Child","香港 042"}},{"now","Child"},{"type","Selector"}};
        model.setGroup(proxies,"Parent");
        QCOMPARE(model.data(model.index(0,1)).toString(),QString("策略组 ›"));
        QVERIFY(model.data(model.index(0,3)).toString().contains("子策略组"));
    }
};

QTEST_GUILESS_MAIN(UiModelTests)
#include "UiModelTests.moc"
