#pragma once
#include <QIcon>
#include <QString>

namespace mlash::UiIcons {
enum class Glyph { Play, Clear, Clock, Search, Refresh, Plus, Folder, Settings, Copy, Close, Save, Globe, Info };
QIcon icon(Glyph glyph,const QString& color={});
QIcon service(const QString& id);
}
