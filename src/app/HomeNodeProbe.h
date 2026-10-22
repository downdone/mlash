#pragma once
#include "core/MihomoClient.h"
#include <QTimer>
namespace mlash {
class HomeNodeProbe final:public QObject {
    Q_OBJECT
public:
    HomeNodeProbe(MihomoClient* api,QObject* parent=nullptr,int millisecondsPerSecond=1000);
    ~HomeNodeProbe() override;
    void setVisible(bool visible);
    void setContext(bool ready,const QString& node,quint64 generation,const QString& url,int intervalSeconds);
    void refresh();
    const QJsonObject& result() const { return result_; }
signals:
    void changed();
    void measured(const QString& node,int delay,const QString& error);
private:
    void reset(); void schedule();
    MihomoClient* api_; QTimer timer_; QPointer<QNetworkReply> request_;
    bool visible_=false,ready_=false; int interval_=0,unit_=1000;
    QString node_,url_; quint64 generation_=0,token_=0; QJsonObject result_;
};
}
