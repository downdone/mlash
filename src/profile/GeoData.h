#pragma once
#include "ProfileStore.h"
#include <QUrl>
namespace mlash {
class GeoData {
public:
    static QUrl source(const QString& name);
    static Result inspect(const QString& root,const QString& activeWork = {});
    static Result stage(ProfileStore& store,const QString& name,const QByteArray& bytes);
    static Result commit(ProfileStore& store,const Result& staged);
    static void discard(const QString& root,const QString& stage);
};
}
