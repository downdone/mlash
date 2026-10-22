#include "platform/HotkeyTestSupport.h"
#include "HotkeyNative.h"
#include <windows.h>
namespace mlash {
Result postRegisteredHotkeyForTest(quint32 threadId,int id,const QString& sequence) { QList<HotkeyBinding> parsed; if (!threadId||id<10100||id>=10100+hotkeyActions().size()||!parseHotkeys({{"show-window",sequence}},parsed).ok||parsed.size()!=1) return Result::fail("test message target invalid"); return PostThreadMessageW(threadId,WM_HOTKEY,WPARAM(id),MAKELPARAM(hotkeyModifiers(parsed[0]),hotkeyVirtualKey(parsed[0])))?Result{}:Result::fail("test message failed"); }
}
