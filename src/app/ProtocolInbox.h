#pragma once
#include "ProtocolLink.h"
#include <QLocalServer>
#include <functional>
namespace mlash {
class ProtocolInbox final:public QObject {
    Q_OBJECT
public:
    explicit ProtocolInbox(QObject* parent=nullptr);
    Result listen(const QString& dataRoot);
    static void forward(QObject* owner,const QString& dataRoot,const QString& link,std::function<void(Result)> done);
signals:
    void received(const QString& link);
private:
    static QString name(const QString& root);
    QLocalServer server_;
    int clients_=0;
};
}
