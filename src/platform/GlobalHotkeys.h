#pragma once
#include "domain/HotkeyBindings.h"
#include <QObject>
#include <QList>
#include <functional>
#include <memory>
namespace mlash {
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
