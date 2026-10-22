#pragma once
#include "core/MihomoClient.h"
#include <QTimer>
#include <QElapsedTimer>
namespace mlash {
class HomeMetrics final : public QObject {
    Q_OBJECT
public:
    explicit HomeMetrics(MihomoClient* api,QObject* parent=nullptr,std::function<Result(qint64)> memoryReader={});
    void setVisible(bool visible);
    void setContext(bool ready,qint64 pid,quint64 generation);
    void refresh();
    const QJsonObject& snapshot() const { return snapshot_; }
    const QJsonArray& history() const { return history_; }
    QString error() const { return error_; }
    bool sampling() const { return visible_&&ready_; }
signals:
    void changed();
private:
    void reset();
    MihomoClient* api_;
    std::function<Result(qint64)> memoryReader_;
    QTimer timer_;
    QElapsedTimer clock_;
    QPointer<QNetworkReply> request_;
    bool visible_=false,ready_=false,pending_=false;
    qint64 pid_=0,previousMs_=-1;
    quint64 coreGeneration_=0,token_=0;
    QJsonObject snapshot_;
    QJsonArray history_;
    QString error_;
};
}
