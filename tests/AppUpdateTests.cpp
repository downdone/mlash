#include "app/AppUpdateSource.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
using namespace mlash;

namespace {
QJsonObject release(const QByteArray& archive,const QString& version="1.20.0") {
    const auto tag="v"+version,asset="Mlash-"+version+"-windows-x64-portable.zip";
    const auto digest="sha256:"+QString::fromLatin1(QCryptographicHash::hash(archive,QCryptographicHash::Sha256).toHex());
    return {{"tag_name",tag},{"draft",false},{"prerelease",false},{"html_url","https://github.com/downdone/mlash/releases/tag/"+tag},
            {"assets",QJsonArray{QJsonObject{{"name",asset},{"browser_download_url","https://github.com/downdone/mlash/releases/download/"+tag+'/'+asset},
                                             {"digest",digest},{"size",archive.size()}}}}};
}
class FakeDownloader final:public SubscriptionDownloader {
public:
    QByteArray bytes; int waitMs=0; bool cancelled=false;
    void download(const QUrl&,std::function<void(Result,QByteArray)> done,const DownloadPolicy&,int,qint64,bool) override {
        QTimer::singleShot(waitMs,this,[this,done=std::move(done)] { if (!cancelled) done({},bytes); });
    }
    void cancel() override { cancelled=true; }
};
}
class AppUpdateTests final:public QObject {
    Q_OBJECT
private slots:
    void releaseAndCandidateValidation() {
        const QByteArray archive("known-test-package");
        const auto metadata=QJsonDocument(QJsonArray{release(archive)}).toJson();
        const auto newer=AppUpdateSource::parseReleases(metadata,"1.19.0");
        QVERIFY(newer.ok); QCOMPARE(newer.data.value("version").toString(),QString("1.20.0"));
        QVERIFY(!newer.data.value("up_to_date").toBool());
        QCOMPARE(AppUpdateSource::parseReleases(metadata,"1.20.0").data.value("up_to_date").toBool(),true);
        auto wrong=release(archive); auto assets=wrong.value("assets").toArray(); auto asset=assets[0].toObject();
        asset["digest"]="sha256:"+QString(64,'0'); assets[0]=asset; wrong["assets"]=assets;
        const auto invalid=AppUpdateSource::parseReleases(QJsonDocument(QJsonArray{wrong}).toJson(),"1.19.0");
        QVERIFY(invalid.ok); QVERIFY(!AppUpdateSource::verifyArchive(archive,invalid.data).ok);
        auto misplaced=release(archive); assets=misplaced.value("assets").toArray(); asset=assets[0].toObject();
        asset["browser_download_url"]="https://example.org/package.zip"; assets[0]=asset; misplaced["assets"]=assets;
        QVERIFY(!AppUpdateSource::parseReleases(QJsonDocument(QJsonArray{misplaced}).toJson(),"1.19.0").ok);
        QTemporaryDir root; QVERIFY(root.isValid());
        const auto staged=AppUpdateSource::stageArchive(archive,newer.data,root.path()); QVERIFY(staged.ok);
        QFile file(staged.data.value("path").toString()); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),archive); file.close();
        QVERIFY(!AppUpdateSource::stageArchive("corrupt",newer.data,root.path()).ok);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),archive);
    }
    void cancellationDiscardsLateMetadata() {
        auto downloader=std::make_unique<FakeDownloader>();
        downloader->bytes=QJsonDocument(QJsonArray{release("payload")}).toJson(); downloader->waitMs=30;
        AppUpdateSource source(nullptr,std::move(downloader));
        int callbacks=0; Result answer;
        source.check("1.19.0",[&](Result result) { ++callbacks; answer=result; });
        source.cancel();
        QTest::qWait(70);
        QCOMPARE(callbacks,1); QVERIFY(!answer.ok); QVERIFY(source.release().isEmpty());
    }
};
QTEST_GUILESS_MAIN(AppUpdateTests)
#include "AppUpdateTests.moc"
