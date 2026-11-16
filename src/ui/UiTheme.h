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
inline constexpr int ControlHeight = 30, TableRow = 28, PanelGap = 6, PopupRadius = 8;
inline constexpr int ContentInset = 12, ContentTop = 12, PanelInset = 12, SidebarWidth = 188;
// U14 compact content density. Font metrics still expand two-line rows at larger text scales.
inline constexpr int TopbarHeight = 42, SidebarItemHeight = 32, RowHeight = 36, NodeRowHeight = 36;
inline constexpr int SettingRowHeight = 36, ProfileRowHeight = 38, NodeCardHeight = 44, DrawerWidth = 350;
#ifdef Q_OS_MACOS
inline constexpr double BodyPoints=10.5;
inline constexpr int DialogPoints=17, SectionPoints=11, ValuePoints=16, TitlePoints=13;
#else
inline constexpr double BodyPoints=10.0;
inline constexpr int DialogPoints=17, SectionPoints=11, ValuePoints=16, TitlePoints=13;
#endif
inline QFont rowCaptionFont(QFont font) { font.setPointSizeF(qMax(8.5,font.pointSizeF()*0.85)); return font; }
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
