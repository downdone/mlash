#pragma once
#include <QtGlobal>

namespace mlash::WindowsWindowChrome {
// The only Win32-facing part of the optional frameless window.
bool resizeHitTest(void* message,qintptr* result);
}
