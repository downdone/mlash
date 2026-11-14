#pragma once
#include <QPalette>
#include <QString>
#include <QObject>
#include <QFont>
#include <QFontMetrics>

namespace mlash::UiTheme {
// Semantic roles are updated together by applyAppearance, never by a page.
inline const char *Window="#F7F8FA", *Sidebar="#EEF0F3", *Surface="#FFFFFF";
inline const char *Ink="#1D1D1F", *Secondary="#60646C", *Muted="#686C73", *Disabled="#979BA3";
inline const char *Border="#DDE1E7", *Separator="#ECEEF2", *Brand="#0068D9", *Selection="#E9F2FF";
inline const char *Primary="#0068D9", *OnPrimary="#FFFFFF", *Hover="#F2F5FA", *Focus="#0068D9";
inline const char *Control="#F0F1F3", *Pressed="#E2E5EA", *PrimaryPressed="#0057B8";
inline const char *Success="#08745B", *Warning="#965A00", *Error="#BF303C", *Track="#B8BEC7";
inline constexpr const char* ArtworkSurface="#F2F2F7";
inline constexpr int ControlHeight = 30, TableRow = 30, PanelGap = 8, PopupRadius = 8;
inline constexpr int ContentInset = 12, ContentTop = 12, PanelInset = 12, SidebarWidth = 188;
// U13 density baseline; actual font metrics expand rows at larger text scales.
inline constexpr int TopbarHeight = 44, SidebarItemHeight = 32, RowHeight = 38, NodeRowHeight = 38;
inline constexpr int SettingRowHeight = 38, ProfileRowHeight = 40, DrawerWidth = 350;
#ifdef Q_OS_MACOS
inline constexpr int BodyPoints=11, DialogPoints=18, SectionPoints=12, ValuePoints=20, TitlePoints=14;
#else
inline constexpr int BodyPoints=11, DialogPoints=18, SectionPoints=11, ValuePoints=20, TitlePoints=13;
#endif
inline QFont rowCaptionFont(QFont font) { font.setPointSizeF(font.pointSizeF()*0.82); return font; }
inline int twoLineRowHeight(const QFont& font,int minimum=NodeRowHeight) {
    return qMax(minimum,QFontMetrics(font).height()+QFontMetrics(rowCaptionFont(font)).height()+6);
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
