#pragma once
#include <QIcon>
#include <QString>

namespace mlash::UiIcons {
enum class Glyph { Play, Clear, Clock, Search, Refresh, Plus, Folder, Settings, Copy, Close, Save, Globe, Info, Home, Chart, List,
    // 原型图标表（GPT 原型 ICON 词典）里首页与标题行用到的几个：
    Route, Rocket, Gauge, Locate, Layers, Sliders, Check, ChevronRight, ChevronDown, Moon, More, Import, Sidebar, Link };
QIcon icon(Glyph glyph,const QString& color={});
QIcon service(const QString& id);
}
