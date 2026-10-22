#pragma once
#include <QIcon>
#include <QString>

namespace mlash {
struct NodeLocation { QString code; QString label; };
// Name hints only; never an IP-geolocation result.
class NodePresentation {
public:
    static NodeLocation location(const QString& name);
    static QString iconKey(const QString& name, bool group = false);
    static QIcon icon(const QString& name, bool group = false);
    static void clearTransientCache();
};
}
