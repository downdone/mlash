#pragma once
#include "app/Types.h"
#include <QSet>

namespace mlash {
// Shared CSV vocabulary and draft checks; the core remains authoritative for payload semantics.
class RuleDraft {
public:
    static QStringList types();
    static QString label(const QString& type);
    static QStringList fields(const QString& rule);
    static QString compose(const QStringList& fields);
    static Result check(const QStringList& rules,const QSet<QString>& targets={},const QSet<QString>& providers={},bool checkReferences=true,const QString& location="custom_rules");
    static QJsonObject snapshot(const Profile& profile);
};
}
