#include "ProtocolRegistry.h"
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QUuid>
#include <windows.h>
namespace mlash {
namespace {
const wchar_t* wide(const QString& text) { return reinterpret_cast<const wchar_t*>(text.utf16()); }
using Tree=QMap<QString,QString>;
Tree expected(const QString& executable) { return {{"|","URL:Mlash Subscription"},{"|URL Protocol",""},{"shell\\open\\command|",QString("\"%1\" --import-url \"%2\"").arg(QDir::toNativeSeparators(executable),"%1")}}; }
bool validExe(const QString& exe) { return QFileInfo(exe).isAbsolute()&&!exe.contains('"')&&!exe.contains('\r')&&!exe.contains('\n')&&exe.size()<4096; }
Result collect(HKEY key,const QString& prefix,Tree& values,int depth) {
    if (depth>3) return Result::fail("协议键包含额外层级");
    for (DWORD i=0;;++i) { wchar_t name[128]{}; DWORD chars=128,type=0,size=32768; QByteArray data(32768,0); const auto status=RegEnumValueW(key,i,name,&chars,nullptr,&type,reinterpret_cast<BYTE*>(data.data()),&size); if (status==ERROR_NO_MORE_ITEMS) break; if (status!=ERROR_SUCCESS||type!=REG_SZ||size%sizeof(wchar_t)||size<sizeof(wchar_t)||reinterpret_cast<const wchar_t*>(data.constData())[size/sizeof(wchar_t)-1]!=0||values.size()>=8) return Result::fail("协议键值不可读或包含额外内容"); values[prefix+'|'+QString::fromWCharArray(name,int(chars))]=QString::fromWCharArray(reinterpret_cast<const wchar_t*>(data.constData()),int(size/sizeof(wchar_t)-1)); }
    for (DWORD i=0;;++i) { wchar_t name[128]{}; DWORD chars=128; const auto status=RegEnumKeyExW(key,i,name,&chars,nullptr,nullptr,nullptr,nullptr); if (status==ERROR_NO_MORE_ITEMS) break; if (status!=ERROR_SUCCESS||i>3) return Result::fail("协议键包含未知子项"); const auto sub=QString::fromWCharArray(name,int(chars)); if ((depth==0&&sub!="shell")||(depth==1&&sub!="open")||(depth==2&&sub!="command")||depth==3) return Result::fail("协议键包含其他应用内容"); HKEY child=nullptr; if (RegOpenKeyExW(key,name,0,KEY_READ,&child)!=ERROR_SUCCESS) return Result::fail("无法读取协议子项"); const auto result=collect(child,prefix.isEmpty()?sub:prefix+'\\'+sub,values,depth+1); RegCloseKey(child); if (!result.ok) return result; }
    return {};
}
}
Result readProtocolRegistry(const QString& path,const QString& executable) {
    if (!validExe(executable)) return Result::fail("协议程序路径无效"); HKEY key=nullptr; const auto status=RegOpenKeyExW(HKEY_CURRENT_USER,wide(path),0,KEY_READ,&key); if (status==ERROR_FILE_NOT_FOUND) return {true,{},{{"enabled",false},{"exists",false},{"conflict",false}}}; if (status!=ERROR_SUCCESS) return Result::fail("无法读取当前账户协议关联"); Tree values; const auto read=collect(key,{},values,0); RegCloseKey(key); const bool owned=read.ok&&values==expected(executable); return {true,{},{{"enabled",owned},{"exists",true},{"conflict",!owned}}};
}
Result writeProtocolRegistry(const QString& path,const QString& executable,bool enabled) {
    const auto before=readProtocolRegistry(path,executable); if (!before.ok) return before; if (before.data.value("conflict").toBool()) return Result::fail("mlash协议关联属于其他路径或已被修改，未覆盖或移除"); if (before.data.value("enabled").toBool()==enabled) return before;
    if (enabled) {
        const auto split=path.lastIndexOf('\\'); if (split<1) return Result::fail("协议关联位置无效"); const auto parentPath=path.left(split),base=path.mid(split+1),stage=base+".stage-"+QUuid::createUuid().toString(QUuid::WithoutBraces),stagePath=parentPath+'\\'+stage; HKEY parent=nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER,wide(parentPath),0,nullptr,0,KEY_WRITE|KEY_READ,nullptr,&parent,nullptr)!=ERROR_SUCCESS) return Result::fail("无法打开协议关联目录");
        LONG status=ERROR_SUCCESS; const auto values=expected(executable);
        for (auto item=values.begin();item!=values.end();++item) { const auto parts=item.key().split('|'); const auto full=stagePath+(parts[0].isEmpty()?QString():"\\"+parts[0]); HKEY key=nullptr; status=RegCreateKeyExW(HKEY_CURRENT_USER,wide(full),0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr); if (status!=ERROR_SUCCESS) break; const auto text=item.value(); status=RegSetValueExW(key,wide(parts[1]),0,REG_SZ,reinterpret_cast<const BYTE*>(text.utf16()),DWORD((text.size()+1)*sizeof(wchar_t))); RegCloseKey(key); if (status!=ERROR_SUCCESS) break; }
        if (status==ERROR_SUCCESS) status=RegRenameKey(parent,wide(stage),wide(base));
        if (status!=ERROR_SUCCESS) RegDeleteTreeW(parent,wide(stage)); RegCloseKey(parent); if (status!=ERROR_SUCCESS) return Result::fail("协议关联提交失败，原关联保持；临时键已清理");
    }
    else if (RegDeleteTreeW(HKEY_CURRENT_USER,wide(path))!=ERROR_SUCCESS) return Result::fail("移除协议关联失败");
    const auto observed=readProtocolRegistry(path,executable); if (!observed.ok||observed.data.value("enabled").toBool()!=enabled||observed.data.value("conflict").toBool()) return Result::fail("协议关联回读不一致"); return observed;
}
}
