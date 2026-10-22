#pragma once
#include "app/Types.h"
namespace mlash {
// Linked only into the Windows test executable; never injects keyboard input.
Result postRegisteredHotkeyForTest(quint32 threadId,int id,const QString& sequence="Ctrl+Alt+Shift+F24");
}
