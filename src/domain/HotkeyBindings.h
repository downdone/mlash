#pragma once
#include "domain/Types.h"
#include <QList>

namespace mlash {
struct HotkeyAction { QString id,title; bool network=false; };
const QList<HotkeyAction>& hotkeyActions();
int hotkeyActionIndex(const QString& action);
struct HotkeyBinding { QString action,sequence,key; bool control=false,alt=false,shift=false; };
Result parseHotkeys(const QJsonObject& bindings,QList<HotkeyBinding>& parsed);
}
