#pragma once
#include "app/Types.h"
#include <QObject>
#include <QList>
#include <functional>
#include <memory>
namespace mlash {
struct HotkeyAction { QString id,title; bool network=false; };
const QList<HotkeyAction>& hotkeyActions();
int hotkeyActionIndex(const QString& action);
struct HotkeyBinding { QString action,sequence,key; bool control=false,alt=false,shift=false; };
Result parseHotkeys(const QJsonObject& bindings,QList<HotkeyBinding>& parsed);
class GlobalHotkeys : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void configure(const QJsonObject& bindings,std::function<void(Result)> done)=0;
signals:
    void activated(const QString& action);
};
std::unique_ptr<GlobalHotkeys> createGlobalHotkeys();
}
