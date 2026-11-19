#include "UiTheme.h"
#include "design/ShadowrocketStyleSheet.h"
#include "storage/Storage.h"
#include <QColor>
#include <QApplication>
#include <QStyleHints>
#include <QAccessibilityHints>
#include <QPointer>
#include <QStyle>
#include <QFont>
#include <QWidget>
#include <QFontDatabase>

static QString bundledFontFamily() {
    static const QString family=[] {
        Q_INIT_RESOURCE(fonts);
        const int id=QFontDatabase::addApplicationFont(":/fonts/NotoSansSC-variable.ttf");
        return id>=0 ? QFontDatabase::applicationFontFamilies(id).value(0) : QString{};
    }();
    return family;
}

namespace mlash::UiTheme {
namespace { QPointer<AppearanceNotifier> changes; bool dark=false,high=false,installed=false; QString preferred="system"; int percent=100;
void updatePreferred() { const auto* hints=qApp->styleHints(); const bool high=hints->accessibility()->contrastPreference()==Qt::ContrastPreference::HighContrast; const auto scheme=high||preferred=="system"?hints->colorScheme():preferred=="dark"?Qt::ColorScheme::Dark:Qt::ColorScheme::Light; applyAppearance(scheme,high); }
}
AppearanceNotifier* notifier() { if (!changes) changes=new AppearanceNotifier(qApp); return changes; }
bool isDark() { return dark; }
int textScale() { return percent; }
QFont bodyFont() {
    auto font=QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    const auto bundled=bundledFontFamily();
#ifdef Q_OS_MACOS
    font.setFamilies({bundled,".AppleSystemUIFont","Helvetica Neue","PingFang SC"});
#else
    font.setFamilies({bundled,"Microsoft YaHei UI","Segoe UI","Noto Sans CJK SC"});
#endif
    font.setPointSizeF(BodyPoints*percent/100.0); font.setStyleHint(QFont::SansSerif); return font;
}
void setPreferences(const QString& appearance,int scale) { if (!validAppearance(appearance,scale)) return; if (preferred==appearance&&percent==scale) { if (!installed) install(); return; } preferred=appearance; percent=scale; if (!installed) install(); else updatePreferred(); }
void applyAppearance(Qt::ColorScheme scheme,bool highContrast) {
    dark=scheme==Qt::ColorScheme::Dark;
    high=highContrast;
    const auto& c=design::ShadowrocketStyle::colors(dark,highContrast);
    Window=c.window; Sidebar=c.sidebar; Surface=c.surface;
    Ink=c.ink; Secondary=c.secondary; Muted=c.muted; Disabled=c.disabled;
    Border=c.border; Separator=c.separator; Brand=c.brand; Selection=c.selection;
    Primary=c.primary; OnPrimary=c.onPrimary; Hover=c.hover; Focus=c.focus;
    Control=c.control; Pressed=c.pressed; PrimaryPressed=c.primaryPressed;
    Success=c.success; Warning=c.warning; Error=c.error; Track=c.track;
    SwitchOn=c.switchOn;
    qApp->setFont(bodyFont());
    qApp->setPalette(palette()); qApp->setStyleSheet(styleSheet()); qApp->style()->setObjectName("mlash-refined"); emit notifier()->changed();
    for (auto* widget:QApplication::topLevelWidgets()) widget->update();
}
void install() {
    if (installed) return;
    installed=true;
#ifdef Q_OS_MACOS
    QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han,{bundledFontFamily(),"PingFang SC","Heiti SC"});
#else
    QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han,{bundledFontFamily(),"Microsoft YaHei UI","Microsoft YaHei","Noto Sans CJK SC"});
#endif
    qApp->setFont(bodyFont());
    const auto update=[] { updatePreferred(); };
    QObject::connect(qApp->styleHints(),&QStyleHints::colorSchemeChanged,notifier(),[update] { update(); });
    QObject::connect(qApp->styleHints()->accessibility(),&QAccessibilityHints::contrastPreferenceChanged,notifier(),[update] { update(); });
    update();
}
QPalette palette() {
    QPalette colors;
    colors.setColor(QPalette::Window,QColor(Window));
    colors.setColor(QPalette::Base,QColor(Surface));
    colors.setColor(QPalette::AlternateBase,QColor(Window));
    colors.setColor(QPalette::PlaceholderText,QColor(Secondary));
    colors.setColor(QPalette::Link,QColor(Brand));
    colors.setColor(QPalette::LinkVisited,QColor(Brand));
    colors.setColor(QPalette::ToolTipBase,QColor(Surface));
    colors.setColor(QPalette::ToolTipText,QColor(Ink));
    colors.setColor(QPalette::WindowText,QColor(Ink));
    colors.setColor(QPalette::Text,QColor(Ink));
    colors.setColor(QPalette::Button,QColor(Surface));
    colors.setColor(QPalette::ButtonText,QColor(Ink));
    colors.setColor(QPalette::Mid,QColor(Track));
    colors.setColor(QPalette::Highlight,QColor(Selection));
    colors.setColor(QPalette::HighlightedText,QColor(Ink));
    colors.setColor(QPalette::Disabled,QPalette::Text,QColor(Disabled));
    colors.setColor(QPalette::Disabled,QPalette::ButtonText,QColor(Disabled));
    return colors;
}
QString styleSheet() {
    QString sheet=design::ShadowrocketStyle::templateStyleSheet();
    sheet.replace("@UiFamily",QString('"')+bodyFont().family()+QString('"'));
#ifdef Q_OS_MACOS
    sheet.replace("@MonoFamily","\"Menlo\", \"PingFang SC\"");
#else
    sheet.replace("@MonoFamily",QString('"')+QFontDatabase::systemFont(QFontDatabase::FixedFont).family()+QString('"'));
#endif
    const auto points=[](qreal value) { return QString::number(value*percent/100.0,'f',2)+"pt"; };
    sheet.replace("@CaptionPoints",points(qMax(design::ShadowrocketStyle::typography.captionFloor,BodyPoints*design::ShadowrocketStyle::typography.captionRatio)));
    sheet.replace("@BodyPoints",points(BodyPoints));
    sheet.replace("@DialogPoints",points(DialogPoints));
    sheet.replace("@SectionPoints",points(SectionPoints));
    sheet.replace("@ValuePoints",points(ValuePoints));
    sheet.replace("@TitlePoints",points(TitlePoints));
    sheet.replace("@PanelBorder",high?QString("1px solid ")+Border:QStringLiteral("none"));
    sheet.replace("@ControlMinimum",QString::number(ControlHeight-8)+"px");
    // Qt's QPushButton style adds its vertical padding to min-height.
    sheet.replace("@HomeControlRowMinimum",QString::number(RowHeight+2)+"px");
    sheet.replace("@SettingRowHeight",QString::number(SettingRowHeight)+"px");
    sheet.replace("@PrimaryPressed",PrimaryPressed);
    sheet.replace("@Control",Control); sheet.replace("@Pressed",Pressed);
    sheet.replace("@ArtworkSurface",ArtworkSurface);
    sheet.replace("@OnPrimary",OnPrimary);
    sheet.replace("@Primary",Primary);
    sheet.replace("@Separator",Separator);
    sheet.replace("@Disabled",Disabled);
    sheet.replace("@Hover",Hover);
    sheet.replace("@Focus",Focus);
    sheet.replace("@Track",Track);
    sheet.replace("@Window",Window);
    sheet.replace("@Sidebar",Sidebar);
    sheet.replace("@Surface",Surface);
    sheet.replace("@Ink",Ink);
    sheet.replace("@Secondary",Secondary);
    sheet.replace("@Muted",Muted);
    sheet.replace("@Border",Border);
    sheet.replace("@Brand",Brand);
    sheet.replace("@Selection",Selection);
    sheet.replace("@Success",Success);
    sheet.replace("@Error",Error);
    sheet.replace("@Warning",Warning);
    return sheet;
}
}
