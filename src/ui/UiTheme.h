#pragma once
#include <QPalette>
#include <QString>
#include <QObject>

namespace mlash::UiTheme {
// Semantic roles are updated together by applyAppearance, never by a page.
inline const char *Window="#F7F8FA", *Sidebar="#EEF0F3", *Surface="#FFFFFF";
inline const char *Ink="#1D1D1F", *Secondary="#60646C", *Muted="#686C73", *Disabled="#979BA3";
inline const char *Border="#DDE1E7", *Separator="#ECEEF2", *Brand="#0068D9", *Selection="#E9F2FF";
inline const char *Primary="#0068D9", *OnPrimary="#FFFFFF", *Hover="#F2F5FA", *Focus="#0068D9";
inline const char *Success="#08745B", *Warning="#965A00", *Error="#BF303C", *Track="#B8BEC7";
inline constexpr int ControlHeight = 32, TableRow = 30, PanelGap = 12, PopupRadius = 8;
inline constexpr int BodyPoints=10, PagePoints=15, SectionPoints=11, ValuePoints=15;
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
QPalette palette();
QString styleSheet();
}
