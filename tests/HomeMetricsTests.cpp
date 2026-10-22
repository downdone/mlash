#include "app/HomeMetrics.h"
#include "app/AppController.h"
#include "ui/HomeMetricsView.h"
#include "ui/MainWindow.h"
#include "ui/UiTheme.h"
#include "diagnostics/ProcessMetrics.h"
#include "MockHttp.h"
#include "storage/Storage.h"
#include <QtTest>
#include <QJsonDocument>
#include <QLabel>
#include <QTemporaryDir>
#include <QScrollArea>
using namespace mlash;
class HomeMetricsTests final : public QObject {
    Q_OBJECT
private slots:
    void visibleSamplingRatesAndBounds() {
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); double upload=100,download=200; server.handler=[&](const QByteArray&) { return HttpResponse{200,QJsonDocument(QJsonObject{{"connections",QJsonArray{QJsonObject{}}},{"uploadTotal",upload},{"downloadTotal",download}}).toJson()}; }; MihomoClient api; api.configure(server.serverPort(),"test"); int memoryCalls=0; HomeMetrics metrics(&api,nullptr,[&](qint64 pid) { ++memoryCalls; return pid==11?Result{true,{},{{"working_set",1024*1024}}}:Result::fail("missing"); }); metrics.setContext(true,11,1); QTest::qWait(40); QCOMPARE(server.requests.size(),0); metrics.setVisible(true); QTRY_VERIFY(metrics.snapshot().contains("active_count")); QCOMPARE(server.requests.size(),1); QCOMPARE(metrics.snapshot()["active_count"].toInt(),1); QVERIFY(metrics.snapshot()["uploadTotal_rate"].isNull()); QCOMPARE(metrics.snapshot()["core_working_set"].toInt(),1024*1024);
        upload=200; download=400; metrics.refresh(); QTRY_COMPARE(metrics.snapshot()["uploadTotal"].toDouble(),upload); QVERIFY(metrics.snapshot()["uploadTotal_rate"].toDouble()>0); QCOMPARE(metrics.history().size(),2);
        for (int i=0;i<31;++i) { upload+=100; metrics.refresh(); QTRY_COMPARE(metrics.snapshot()["uploadTotal"].toDouble(),upload); } QCOMPARE(metrics.history().size(),30);
        upload=1; metrics.refresh(); QTRY_COMPARE(metrics.snapshot()["uploadTotal"].toDouble(),upload); QVERIFY(metrics.snapshot()["uploadTotal_rate"].isNull()); metrics.setVisible(false); QVERIFY(metrics.snapshot().isEmpty()); QVERIFY(metrics.history().isEmpty()); const auto requests=server.requests.size(); const auto calls=memoryCalls; QTest::qWait(2100); QCOMPARE(server.requests.size(),requests); QCOMPARE(memoryCalls,calls); metrics.setVisible(true); QTRY_VERIFY(!metrics.snapshot().isEmpty()); QVERIFY(metrics.snapshot()["downloadTotal_rate"].isNull()); metrics.setVisible(false);
    }
    void errorsGenerationsAndUnknownDisplay() {
        MockHttp server; QVERIFY(server.listen(QHostAddress::LocalHost)); bool bad=false; int delay=0; server.handler=[&](const QByteArray&) { return HttpResponse{200,bad?QByteArray("{\"connections\":[],\"uploadTotal\":-1,\"downloadTotal\":2}"):QByteArray("{\"connections\":null,\"uploadTotal\":100,\"downloadTotal\":200}"),delay}; }; MihomoClient api; api.configure(server.serverPort(),"test"); HomeMetrics metrics(&api,nullptr,[](qint64) { return Result::fail("unavailable"); }); metrics.setContext(true,1,1); HomeMetricsView view(&metrics); view.show(); metrics.setVisible(true); QTRY_VERIFY(metrics.snapshot().contains("active_count")); QCOMPARE(view.findChild<QLabel*>("homeMetric3")->text(),QString("—")); QVERIFY(!metrics.error().isEmpty()); bad=true; metrics.refresh(); QTRY_VERIFY(metrics.snapshot().isEmpty()); QVERIFY(!metrics.error().isEmpty()); QCOMPARE(view.findChild<QLabel*>("homeMetric2")->text(),QString("—"));
        bad=false; delay=200; metrics.refresh(); QTRY_VERIFY(server.requests.size()>=3); metrics.setContext(false,0,2); QTest::qWait(300); QVERIFY(metrics.snapshot().isEmpty()); QVERIFY(metrics.history().isEmpty()); metrics.setContext(true,2,3); QTRY_VERIFY_WITH_TIMEOUT(metrics.snapshot().contains("active_count"),3000); QCOMPARE(metrics.snapshot()["active_count"].toInt(),0); metrics.setVisible(false);
    }
    void nativeMemoryAndPageVisibility() {
#ifdef Q_OS_WIN
        const auto memory=processMemory(QCoreApplication::applicationPid()); QVERIFY(memory.ok); QVERIFY(memory.data.value("working_set").toDouble()>0); QVERIFY(!processMemory(0).ok);
#endif
        QTemporaryDir root; AppController controller(root.path(),true); MainWindow window(&controller); window.resize(840,560); window.show(); QTest::qWait(50); auto* metrics=window.findChild<HomeMetricsView*>(); QVERIFY(metrics); QVERIFY(!metrics->isVisible()); QVERIFY(window.findChild<QWidget*>("homeOnboarding")->isVisible()); auto& presentation=const_cast<AppState&>(controller.state()); presentation.activeProfile=newId(); presentation.ready=true; emit controller.stateChanged(); QVERIFY(metrics->isVisible()); QVERIFY(!controller.homeMetrics()->sampling()); window.setPage(4); QVERIFY(!metrics->isVisible()); window.hide(); QVERIFY(controller.homeMetrics()->snapshot().isEmpty()); window.show(); window.setPage(0); QVERIFY(metrics->isVisible()); auto* scroll=window.findChild<QScrollArea*>("homeScroll"); QVERIFY(scroll); scroll->ensureWidgetVisible(metrics,0,0); QCoreApplication::processEvents(); QVERIFY(scroll->viewport()->rect().contains(metrics->mapTo(scroll->viewport(),metrics->rect().bottomRight())));
    }
};
QTEST_MAIN(HomeMetricsTests)
#include "HomeMetricsTests.moc"
