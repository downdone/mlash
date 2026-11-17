#pragma once
#include "profile/SubscriptionDownloader.h"
#include <QJsonObject>
#include <memory>

namespace mlash {
// Fetches only immutable Mlash releases. The controller owns staging and never
// replaces the executable while it is running.
class AppUpdateSource final : public QObject {
    Q_OBJECT
public:
    using DownloadDone=std::function<void(Result,QByteArray)>;
    explicit AppUpdateSource(QObject* parent=nullptr,std::unique_ptr<SubscriptionDownloader> downloader={});
    static Result parseReleases(const QByteArray& bytes,const QString& currentVersion);
    static Result verifyArchive(const QByteArray& bytes,const QJsonObject& release);
    static Result stageArchive(const QByteArray& bytes,const QJsonObject& release,const QString& dataRoot);
    void check(const QString& currentVersion,Completion done);
    void download(DownloadDone done);
    void cancel();
    const QJsonObject& release() const { return release_; }
    bool busy() const { return busy_; }
private:
    std::unique_ptr<SubscriptionDownloader> downloader_;
    QJsonObject release_;
    bool busy_=false;
    quint64 generation_=0;
    DownloadDone done_;
};
}
