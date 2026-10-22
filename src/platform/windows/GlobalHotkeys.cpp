#include "platform/GlobalHotkeys.h"
#include "HotkeyNative.h"
#include <windows.h>
#include <QThread>
#include <QPointer>
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QMap>
#include <atomic>
namespace mlash {
namespace {
class NativeState final : public QAbstractNativeEventFilter {
public:
    QMap<int,HotkeyBinding> registered;
    std::atomic<quint64> generation{0};
    std::function<void(QString)> action;
    bool release() { generation.fetch_add(1,std::memory_order_relaxed); bool ok=true; for (const auto id:registered.keys()) { if (UnregisterHotKey(nullptr,id)) registered.remove(id); else ok=false; } return ok; }
    QJsonObject bindings() const { QJsonObject result; for (const auto& binding:registered) result[binding.action]=binding.sequence; return result; }
    bool add(const QList<HotkeyBinding>& values) {
        for (const auto& binding:values) {
            const auto index=hotkeyActionIndex(binding.action); if (index<0) return false; const int id=10100+index; const UINT key=hotkeyVirtualKey(binding); const UINT modifiers=MOD_NOREPEAT|hotkeyModifiers(binding);
            if (!RegisterHotKey(nullptr,id,modifiers,key)) return false;
            registered[id]=binding;
        }
        return true;
    }
    bool nativeEventFilter(const QByteArray& type,void* message,qintptr* result) override {
        // Thread messages use generic_MSG in Qt 6.10.2's worker dispatcher.
        if (type!="windows_dispatcher_MSG"&&type!="windows_generic_MSG") return false;
        const auto* msg=static_cast<MSG*>(message); if (msg->message!=WM_HOTKEY||!registered.contains(int(msg->wParam))) return false;
        const auto binding=registered.value(int(msg->wParam)); if (LOWORD(msg->lParam)!=hotkeyModifiers(binding)||HIWORD(msg->lParam)!=hotkeyVirtualKey(binding)) return false;
        if (action) action(registered.value(int(msg->wParam)).action); if (result) *result=0; return true;
    }
};
class WindowsHotkeys final : public GlobalHotkeys {
    QThread thread_;
    QObject* worker_=nullptr;
    std::shared_ptr<NativeState> native_=std::make_shared<NativeState>();
    bool pending_=false;
public:
    ~WindowsHotkeys() override {
        if (thread_.isRunning()) { const auto native=native_; QMetaObject::invokeMethod(worker_,[native] { native->release(); QAbstractEventDispatcher::instance()->removeNativeEventFilter(native.get()); },Qt::BlockingQueuedConnection); thread_.quit(); thread_.wait(); }
    }
    void configure(const QJsonObject& bindings,std::function<void(Result)> done) override {
        QList<HotkeyBinding> values; const auto parsed=parseHotkeys(bindings,values); if (!parsed.ok) { done(parsed); return; }
        if (pending_) { done(Result::fail("快捷键注册尚未结束")); return; }
        if (!worker_&&values.isEmpty()) { done({true,{},{{"bindings",QJsonObject{}}}}); return; }
        if (!worker_) {
            worker_=new QObject; worker_->moveToThread(&thread_); connect(&thread_,&QThread::finished,worker_,&QObject::deleteLater); thread_.setObjectName("MlashHotkeys"); thread_.start();
            const QPointer<WindowsHotkeys> guard(this); const auto native=native_;
            QMetaObject::invokeMethod(worker_,[native,guard] { QAbstractEventDispatcher::instance()->installNativeEventFilter(native.get()); const std::weak_ptr<NativeState> weak=native; native->action=[guard,weak](QString action) { const auto source=weak.lock(); if (!source||!guard) return; const auto generation=source->generation.load(std::memory_order_relaxed); QMetaObject::invokeMethod(guard,[guard,weak,action,generation] { const auto source=weak.lock(); if (guard&&source&&!guard->pending_&&source->generation.load(std::memory_order_relaxed)==generation) emit guard->activated(action); },Qt::QueuedConnection); }; },Qt::QueuedConnection);
        }
        pending_=true; const QPointer<WindowsHotkeys> guard(this); const auto native=native_;
        QMetaObject::invokeMethod(worker_,[native,values,guard,done=std::move(done)]() mutable {
            const auto previous=native->registered.values(); Result result;
            if (!native->release()) result=Result::fail("部分原快捷键未能注销，未注册新配置");
            else if (!native->add(values)) { const bool released=native->release(); const bool restored=released&&native->add(previous); result=Result::fail(restored?"快捷键与系统或其他程序冲突；原快捷键已恢复":"新快捷键注册失败，原快捷键也未全部恢复，请重新设置"); }
            result.data={{"bindings",native->bindings()},{"thread_id",double(GetCurrentThreadId())}};
            if (guard) QMetaObject::invokeMethod(guard,[guard,result,done=std::move(done)]() mutable { if (guard) { guard->pending_=false; done(result); } },Qt::QueuedConnection);
        },Qt::QueuedConnection);
    }
};
}
std::unique_ptr<GlobalHotkeys> createGlobalHotkeys() { return std::make_unique<WindowsHotkeys>(); }
}
