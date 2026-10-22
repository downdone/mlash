#pragma once
#include "app/AppController.h"
#include "GuiHost.h"
#include "ImportDialog.h"
#include <QQueue>
namespace mlash {
class ProtocolImportQueue final:public QObject {
    Q_OBJECT
public:
    ProtocolImportQueue(AppController* controller,GuiHost* gui,QObject* parent=nullptr);
    Result enqueue(const QString& link);
private:
    void process();
    AppController* controller_; GuiHost* gui_; bool initialized_=false;
    QQueue<QJsonObject> pending_; QPointer<ImportDialog> dialog_;
};
}
