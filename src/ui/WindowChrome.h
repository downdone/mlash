#pragma once
#include "ViewIds.h"
#include "UiTheme.h"
#include "UiIcons.h"
#include <QtWidgets>

namespace mlash::WindowParts {
enum class NavKind { Home, Config, Data, Settings };
inline constexpr int ActiveNavigationRole = Qt::UserRole + 1;
// 分区归属按视图常量判断（ViewIds.h）。数据页的四个页签共用同一个视图，因此都归「数据」。
inline NavKind navKindOfView(int view) {
    switch (view) {
    case ViewIds::Home: return NavKind::Home;
    case ViewIds::Profiles: case ViewIds::Rules: return NavKind::Config;
    case ViewIds::Data: return NavKind::Data;
    case ViewIds::Settings: return NavKind::Settings;
    default: return NavKind::Home;
    }
}
inline UiIcons::Glyph navigationGlyph(NavKind kind) {
    return kind == NavKind::Home ? UiIcons::Glyph::Home
        : kind == NavKind::Config ? UiIcons::Glyph::Folder
        : kind == NavKind::Data ? UiIcons::Glyph::Chart : UiIcons::Glyph::Settings;
}
// K1：顶栏文案的旁表。MainWindow.h 不能反向包含本文件（本文件包含它），
// 因此顶栏控件登记在 WindowParts 自己的表里，updateTopbar() 统一写。
// 用 QPointer：MainWindow 析构时标签先被删除，表项自动置空，不会留下悬空指针。
struct TopbarInfo { QPointer<QLabel> title, subtitle; QPointer<QWidget> status; QPointer<QLabel> dot, statusText; };
inline QHash<QWidget*, TopbarInfo>& topbarRegistry() { static QHash<QWidget*, TopbarInfo> registry; return registry; }
inline void registerTopbar(QWidget* window, const TopbarInfo& info) { topbarRegistry().insert(window, info); }
inline void updateTopbar(QWidget* window, const QString& title, const QString& subtitle, const QString& status, const QString& dotState) {
    const auto it = topbarRegistry().find(window); if (it == topbarRegistry().end()) return;
    const auto& info = *it;
    if (info.title) info.title->setText(title);
    if (info.subtitle) { info.subtitle->setText(subtitle); info.subtitle->setVisible(!subtitle.isEmpty()); }
    if (info.statusText) info.statusText->setText(status);
    if (info.dot) { info.dot->setProperty("state", dotState); info.dot->style()->unpolish(info.dot); info.dot->style()->polish(info.dot); }
}
}
