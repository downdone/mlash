#include "platform/ProtocolTestSupport.h"
#include "ProtocolRegistry.h"
#include <QUuid>
#include <windows.h>
namespace mlash {
Result testIsolatedProtocolRegistry() {
    const auto root="Software\\MlashProtocolTest-"+QUuid::createUuid().toString(QUuid::WithoutBraces),path=root+"\\mlash"; const auto wide=[](const QString& text) { return reinterpret_cast<const wchar_t*>(text.utf16()); }; struct Cleanup { QString path; ~Cleanup() { RegDeleteTreeW(HKEY_CURRENT_USER,reinterpret_cast<const wchar_t*>(path.utf16())); } } cleanup{root};
    const auto exe=QString("C:/Mlash Test/Mlash.exe"); auto r=readProtocolRegistry(path,exe); if (!r.ok||r.data.value("enabled").toBool()||r.data.value("exists").toBool()) return Result::fail("isolated initial read"); r=writeProtocolRegistry(path,exe,true); if (!r.ok||!r.data.value("enabled").toBool()) return Result::fail("isolated register: "+r.error);
    if (!readProtocolRegistry(path,"C:/Other/Mlash.exe").data.value("conflict").toBool()||writeProtocolRegistry(path,"C:/Other/Mlash.exe",true).ok||writeProtocolRegistry(path,"C:/Other/Mlash.exe",false).ok) return Result::fail("foreign path ownership");
    HKEY key=nullptr; if (RegOpenKeyExW(HKEY_CURRENT_USER,wide(path),0,KEY_WRITE,&key)!=ERROR_SUCCESS) return Result::fail("isolated open"); const wchar_t value[]=L"foreign"; RegSetValueExW(key,L"foreign",0,REG_SZ,reinterpret_cast<const BYTE*>(value),sizeof(value)); RegCloseKey(key); if (!readProtocolRegistry(path,exe).data.value("conflict").toBool()||writeProtocolRegistry(path,exe,false).ok) return Result::fail("modified tree retained");
    RegOpenKeyExW(HKEY_CURRENT_USER,wide(path),0,KEY_WRITE,&key); RegDeleteValueW(key,L"foreign"); RegCloseKey(key); r=writeProtocolRegistry(path,exe,false); if (!r.ok||r.data.value("exists").toBool()) return Result::fail("isolated unregister: "+r.error); if (!writeProtocolRegistry(path,exe,false).ok||writeProtocolRegistry(path,"C:/bad\"/Mlash.exe",true).ok) return Result::fail("idempotence and command quoting"); return {};
}
}
