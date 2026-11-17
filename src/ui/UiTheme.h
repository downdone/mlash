#pragma once
#include <QPalette>
#include <QString>
#include <QObject>
#include <QFont>
#include <QFontMetrics>
#include "design/ShadowrocketStyle.h"

namespace mlash::UiTheme {
// Compatibility bridge for existing components. The specialization is the
// sole source of values; applyAppearance updates these roles as one snapshot.
inline const char *Window=design::ShadowrocketStyle::colors(false,false).window, *Sidebar=design::ShadowrocketStyle::colors(false,false).sidebar, *Surface=design::ShadowrocketStyle::colors(false,false).surface;
inline const char *Ink=design::ShadowrocketStyle::colors(false,false).ink, *Secondary=design::ShadowrocketStyle::colors(false,false).secondary, *Muted=design::ShadowrocketStyle::colors(false,false).muted, *Disabled=design::ShadowrocketStyle::colors(false,false).disabled;
inline const char *Border=design::ShadowrocketStyle::colors(false,false).border, *Separator=design::ShadowrocketStyle::colors(false,false).separator, *Brand=design::ShadowrocketStyle::colors(false,false).brand, *Selection=design::ShadowrocketStyle::colors(false,false).selection;
inline const char *Primary=design::ShadowrocketStyle::colors(false,false).primary, *OnPrimary=design::ShadowrocketStyle::colors(false,false).onPrimary, *Hover=design::ShadowrocketStyle::colors(false,false).hover, *Focus=design::ShadowrocketStyle::colors(false,false).focus;
inline const char *Control=design::ShadowrocketStyle::colors(false,false).control, *Pressed=design::ShadowrocketStyle::colors(false,false).pressed, *PrimaryPressed=design::ShadowrocketStyle::colors(false,false).primaryPressed;
inline const char *Success=design::ShadowrocketStyle::colors(false,false).success, *Warning=design::ShadowrocketStyle::colors(false,false).warning, *Error=design::ShadowrocketStyle::colors(false,false).error, *Track=design::ShadowrocketStyle::colors(false,false).track;
inline constexpr auto ArtworkSurface=design::ShadowrocketStyle::artworkSurface;
inline constexpr int ControlHeight = design::ShadowrocketStyle::geometry.controlHeight, TableRow = design::ShadowrocketStyle::geometry.tableRow, PanelGap = design::ShadowrocketStyle::geometry.panelGap, PopupRadius = design::ShadowrocketStyle::geometry.popupRadius;
inline constexpr int ContentInset = design::ShadowrocketStyle::geometry.contentInset, ContentTop = design::ShadowrocketStyle::geometry.contentTop, PanelInset = design::ShadowrocketStyle::geometry.panelInset, SidebarWidth = design::ShadowrocketStyle::geometry.sidebarWidth;
inline constexpr int TopbarHeight = design::ShadowrocketStyle::geometry.topbarHeight, SidebarItemHeight = design::ShadowrocketStyle::geometry.sidebarItemHeight, RowHeight = design::ShadowrocketStyle::geometry.rowHeight, NodeRowHeight = design::ShadowrocketStyle::geometry.nodeRowHeight;
inline constexpr int SettingRowHeight = design::ShadowrocketStyle::geometry.settingRowHeight, ProfileRowHeight = design::ShadowrocketStyle::geometry.profileRowHeight, NodeCardHeight = design::ShadowrocketStyle::geometry.nodeCardHeight, DrawerWidth = design::ShadowrocketStyle::geometry.drawerWidth;
#ifdef Q_OS_MACOS
inline constexpr double BodyPoints=design::ShadowrocketStyle::typography.bodyPointsMac;
#else
inline constexpr double BodyPoints=design::ShadowrocketStyle::typography.bodyPointsWindows;
#endif
inline constexpr int DialogPoints=design::ShadowrocketStyle::typography.dialogPoints, SectionPoints=design::ShadowrocketStyle::typography.sectionPoints, ValuePoints=design::ShadowrocketStyle::typography.valuePoints, TitlePoints=design::ShadowrocketStyle::typography.titlePoints;
inline QFont rowCaptionFont(QFont font) { font.setPointSizeF(qMax(design::ShadowrocketStyle::typography.captionFloor,font.pointSizeF()*design::ShadowrocketStyle::typography.captionRatio)); return font; }
inline int twoLineRowHeight(const QFont& font,int minimum=NodeRowHeight) {
    return qMax(minimum,QFontMetrics(font).height()+QFontMetrics(rowCaptionFont(font)).height()+4);
}
class AppearanceNotifier final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
signals:
    void changed();
};
AppearanceNotifier* notifier();
void install();
void applyAppearance(Qt::ColorScheme scheme,bool highContrast=false);
void setPreferences(const QString& appearance,int percent);
int textScale();
bool isDark();
QFont bodyFont();
QPalette palette();
QString styleSheet();
}
