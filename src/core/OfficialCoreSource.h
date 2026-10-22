#pragma once
#include "profile/SubscriptionDownloader.h"
#include <memory>

namespace mlash {
class OfficialCoreSource final : public QObject {
    Q_OBJECT
public:
    using DownloadDone=std::function<void(Result,QByteArray)>;
    explicit OfficialCoreSource(QObject* parent=nullptr,std::unique_ptr<SubscriptionDownloader> downloader={});
    ~OfficialCoreSource() override { ++generation_; done_={}; downloader_->cancel(); }
    void check(Completion done,const DownloadPolicy& policy={},int port=0);
    void download(DownloadDone done,const DownloadPolicy& policy={},int port=0);
    void cancel();
    bool busy() const { return busy_; }
    const QJsonObject& release() const { return release_; }
    static Result parseRelease(const QByteArray& bytes);
    static Result unpack(const QByteArray& archive,const QJsonObject& release,QByteArray& executable);
private:
    std::unique_ptr<SubscriptionDownloader> downloader_;
    QJsonObject release_;
    DownloadDone done_;
    bool busy_=false;
    quint64 generation_=0;
};
}
