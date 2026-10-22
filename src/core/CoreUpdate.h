#pragma once
#include "profile/ProfileStore.h"
#include "MihomoClient.h"
#include <QProcess>
namespace mlash {
class CoreUpdate {
public:
    static Result stage(ProfileStore& store,const QString& source);
    static Result stageBytes(ProfileStore& store,const QByteArray& bytes);
    static Result verify(const QString& root,const QJsonObject& record);
    static Result clean(ProfileStore& store,const QStringList& keep);
};
class CoreVersionProbe final:public QObject {
    Q_OBJECT
public:
    explicit CoreVersionProbe(QObject* parent=nullptr):QObject(parent) {}
    void inspect(const QString& path,Completion done);
    void cancel();
private:
    QPointer<QProcess> process_;
};
}
