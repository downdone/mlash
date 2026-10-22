#pragma once
#include "app/Types.h"
#include <QStringList>
namespace mlash {
struct YamlField { QString key,label,kind; QStringList choices; };
QVector<YamlField> yamlFields(const QString& kind);
Result inspectYamlFields(const QString& text,const QString& kind);
Result patchYamlFields(const QString& text,const QString& kind,const QJsonObject& changes);
}
