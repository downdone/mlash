#include "GlobalHotkeys.h"
namespace mlash {
class UnsupportedHotkeys final : public GlobalHotkeys {
    void configure(const QJsonObject& bindings,std::function<void(Result)> done) override { done(bindings.isEmpty()?Result{true,{},{{"bindings",QJsonObject{}}}}:Result::fail("Unsupported: Windows全局快捷键")); }
};
std::unique_ptr<GlobalHotkeys> createGlobalHotkeys() { return std::make_unique<UnsupportedHotkeys>(); }
}
