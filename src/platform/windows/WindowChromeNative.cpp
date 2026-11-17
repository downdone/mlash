#include "WindowChromeNative.h"
#include <windows.h>
#include <windowsx.h>

namespace mlash::WindowsWindowChrome {
bool resizeHitTest(void* message,qintptr* result) {
    auto* native=static_cast<MSG*>(message);
    if (native->message!=WM_NCHITTEST || IsZoomed(native->hwnd)) return false;
    RECT bounds{}; if (!GetWindowRect(native->hwnd,&bounds)) return false;
    const int border=MulDiv(7,int(GetDpiForWindow(native->hwnd)),96);
    const int x=GET_X_LPARAM(native->lParam),y=GET_Y_LPARAM(native->lParam);
    const bool left=x>=bounds.left && x<bounds.left+border;
    const bool right=x<=bounds.right && x>bounds.right-border;
    const bool top=y>=bounds.top && y<bounds.top+border;
    const bool bottom=y<=bounds.bottom && y>bounds.bottom-border;
    if (top&&left) *result=HTTOPLEFT;
    else if (top&&right) *result=HTTOPRIGHT;
    else if (bottom&&left) *result=HTBOTTOMLEFT;
    else if (bottom&&right) *result=HTBOTTOMRIGHT;
    else if (left) *result=HTLEFT;
    else if (right) *result=HTRIGHT;
    else if (top) *result=HTTOP;
    else if (bottom) *result=HTBOTTOM;
    else return false;
    return true;
}
}
