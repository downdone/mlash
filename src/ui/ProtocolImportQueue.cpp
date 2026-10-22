#include "ProtocolImportQueue.h"
#include "app/ProtocolLink.h"
#include <QTimer>
namespace mlash {
ProtocolImportQueue::ProtocolImportQueue(AppController* controller,GuiHost* gui,QObject* parent):QObject(parent),controller_(controller),gui_(gui),initialized_(!controller->state().busy) { connect(controller,&AppController::initialized,this,[this] { initialized_=true; process(); }); connect(controller,&AppController::stateChanged,this,[this] { QTimer::singleShot(0,this,[this] { process(); }); }); }
Result ProtocolImportQueue::enqueue(const QString& link) { const auto parsed=parseProtocolLink(link); if (!parsed.ok) return parsed; if (pending_.size()>=4) return Result::fail("待确认导入链接最多四个，请先处理现有确认框"); pending_.enqueue(parsed.data); process(); return {}; }
void ProtocolImportQueue::process() {
    if (!initialized_||dialog_||pending_.isEmpty()||controller_->state().busy||controller_->state().exiting||controller_->state().readOnly) return;
    gui_->show(1); if (!gui_->window()) return; const auto next=pending_.dequeue(); dialog_=new ImportDialog(gui_->window()); auto* dialog=dialog_.data(); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setProtocolSource(next.value("url").toString(),next.value("name").toString());
    dialog->bindController(controller_);
    connect(dialog,&QObject::destroyed,this,[this] { dialog_=nullptr; QTimer::singleShot(0,this,[this] { process(); }); }); dialog->open();
}
}
