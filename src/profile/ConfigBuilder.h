#pragma once
#include "app/Types.h"
#include <QByteArray>
#include <QStringList>

namespace mlash {
struct BuiltConfig { Result result; QByteArray yaml; QStringList secrets, remoteUrls; };
class ConfigBuilder {
public:
    static BuiltConfig build(const QByteArray& source, const Settings& settings, int apiPort, const QString& secret, const QStringList& customRules = {}, const QStringList& appendRules = {}, const QStringList& disabledRules = {});
    static BuiltConfig buildProfile(const QByteArray& source, const Settings& settings, int apiPort, const QString& secret, const Profile& profile);
    static BuiltConfig checkProvider(const QByteArray& payload, const QString& kind, const QString& format, const QString& behavior = {});
};
}
