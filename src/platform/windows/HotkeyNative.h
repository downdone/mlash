#pragma once
#include "platform/GlobalHotkeys.h"
#include <windows.h>
namespace mlash {
inline UINT hotkeyVirtualKey(const HotkeyBinding& binding) { return binding.key.startsWith('F')&&binding.key.size()>1?UINT(VK_F1+binding.key.mid(1).toInt()-1):UINT(binding.key[0].unicode()); }
inline UINT hotkeyModifiers(const HotkeyBinding& binding) { return (binding.control?MOD_CONTROL:0)|(binding.alt?MOD_ALT:0)|(binding.shift?MOD_SHIFT:0); }
}
