#pragma once
#include "app/Types.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSet>
#include <QUrlQuery>
#include <functional>

namespace mlash {
using Completion = std::function<void(Result)>;
class MihomoClient : public QObject {
    Q_OBJECT
public:
    explicit MihomoClient(QObject* parent = nullptr);
    void configure(int port, const QString& secret);
    QNetworkReply* request(const QByteArray& method, const QByteArray& path, const QJsonObject& body, Completion done,
        int timeoutMs = 4000, const QUrlQuery& query = {});
    void version(Completion done);
    void config(Completion done);
    void proxies(Completion done);
    void select(const QString& group, const QString& member, Completion done);
    QNetworkReply* delay(const QString& name, Completion done, int timeoutMs = 5000, const QString& url = "https://www.gstatic.com/generate_204");
    void setLogsVisible(bool visible, const QString& level = "info");
    void cancelAll();
    static QByteArray segment(const QString& name);
signals:
    void logReceived(const QString& line);
private:
    QNetworkAccessManager network_;
    QUrl base_;
    QString secret_;
    QSet<QNetworkReply*> pending_;
    QPointer<QNetworkReply> logReply_;
    QByteArray logBuffer_;
    quint64 generation_ = 0;
};
}
