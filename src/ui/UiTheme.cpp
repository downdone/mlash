#include "UiTheme.h"
#include "storage/Storage.h"
#include <QRegularExpression>
#include <QColor>
#include <QApplication>
#include <QStyleHints>
#include <QAccessibilityHints>
#include <QPointer>
#include <QStyle>
#include <QFont>
#include <QWidget>
#include <QFontDatabase>

namespace mlash::UiTheme {
namespace { QPointer<AppearanceNotifier> changes; bool dark=false,installed=false; QString preferred="system"; int percent=100;
void updatePreferred() { const auto* hints=qApp->styleHints(); const bool high=hints->accessibility()->contrastPreference()==Qt::ContrastPreference::HighContrast; const auto scheme=high||preferred=="system"?hints->colorScheme():preferred=="dark"?Qt::ColorScheme::Dark:Qt::ColorScheme::Light; applyAppearance(scheme,high); }
}
AppearanceNotifier* notifier() { if (!changes) changes=new AppearanceNotifier(qApp); return changes; }
bool isDark() { return dark; }
int textScale() { return percent; }
QFont bodyFont() {
    auto font=QFontDatabase::systemFont(QFontDatabase::GeneralFont);
#ifdef Q_OS_MACOS
    font.setFamilies({".AppleSystemUIFont","Helvetica Neue","PingFang SC"});
#else
    font.setFamilies({"Segoe UI","Microsoft YaHei UI","Noto Sans CJK SC"});
#endif
    font.setPointSizeF(BodyPoints*percent/100.0); font.setStyleHint(QFont::SansSerif); return font;
}
void setPreferences(const QString& appearance,int scale) { if (!validAppearance(appearance,scale)) return; if (preferred==appearance&&percent==scale) { if (!installed) install(); return; } preferred=appearance; percent=scale; if (!installed) install(); else updatePreferred(); }
void applyAppearance(Qt::ColorScheme scheme,bool highContrast) {
    dark=scheme==Qt::ColorScheme::Dark;
    Window=dark?"#202124":"#F5F5F7"; Sidebar=dark?"#292A2D":"#EBEBED"; Surface=dark?"#2B2C30":"#FFFFFF";
    Ink=dark?"#F5F5F7":"#1D1D1F"; Secondary=dark?"#C0C1C7":"#606067"; Muted=dark?"#B1B2B9":"#65656D"; Disabled=dark?"#86878F":"#96969E";
    Border=highContrast?(dark?"#B8BECA":"#60646C"):(dark?"#45464B":"#DEDEE3"); Separator=highContrast?Border:(dark?"#3B3C41":"#EBEBEF");
    Brand=dark?"#91BEFF":"#0062CC"; Primary=dark?"#0064CB":"#0068D9"; OnPrimary="#FFFFFF";
    Selection=dark?"#30415B":"#EAF2FF"; Hover=dark?"#383A40":"#EAECEF"; Focus=dark?"#9BCBFF":"#0068D9";
    Control=dark?"#36373C":"#F1F1F4"; Pressed=dark?"#484A52":"#DFE2E7"; PrimaryPressed=dark?"#0056B0":"#0057B8";
    Success=dark?"#7AD7B5":"#08745B"; Warning=dark?"#F2BF75":"#965A00"; Error=dark?"#FF9BA5":"#BF303C"; Track=dark?"#606979":"#B8BEC7";
    qApp->setFont(bodyFont());
    qApp->setPalette(palette()); qApp->setStyleSheet(styleSheet()); qApp->style()->setObjectName("mlash-refined"); emit notifier()->changed();
    for (auto* widget:QApplication::topLevelWidgets()) widget->update();
}
void install() {
    if (installed) return;
    installed=true;
#ifdef Q_OS_MACOS
    QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han,{"PingFang SC","Heiti SC"});
#else
    QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han,{"Microsoft YaHei UI","Microsoft YaHei","Noto Sans CJK SC"});
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
    colors.setColor(QPalette::Highlight,QColor(Selection));
    colors.setColor(QPalette::HighlightedText,QColor(Ink));
    colors.setColor(QPalette::Disabled,QPalette::Text,QColor(Disabled));
    colors.setColor(QPalette::Disabled,QPalette::ButtonText,QColor(Disabled));
    return colors;
}
QString styleSheet() {
    QString sheet=R"(
        QWidget { font-family: @UiFamily; font-size: @BodyPoints; color: @Ink; }
        QWidget#websiteSurface, QWidget#pageSurface, QWidget#mainSurface, QWidget#settingsSurface, QWidget#settingsCanvas, QStackedWidget, QScrollArea { background: @Window; }
        QStackedWidget#nodeViews { background: @Surface; border-radius: 8px; }
        QDialog#importDialog, QDialog#profilePolicyDialog { background: @Window; }
        QWidget#importSource { background: @Surface; }
        QFrame#homeOnboarding, QFrame#websiteCard, QFrame#overview, QFrame#panel, QFrame#homeMetrics, QFrame#settingsApplication, QFrame#settingsCore, QFrame#settingsTools { background: @Surface; border: 1px solid @Border; border-radius: 12px; }
        QWidget#sidebar { background: @Sidebar; border: none; border-right: 1px solid @Border; }
        QLabel#brand { font-size: @SectionPoints; font-weight: 600; }
        QLabel#pageTitle { font-size: @PagePoints; font-weight: 600; }
        QLabel#overviewTitle { font-size: @SectionPoints; font-weight: 600; }
        QLabel#currentMember { font-size: @BodyPoints; font-weight: 600; }
        QLabel#secondaryText, QLabel[secondary="true"] { color: @Secondary; }
        QLabel[role="headline"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel#settingsSubheading { font-size: @BodyPoints; font-weight: 600; padding-bottom: 3px; }
        QToolButton#homeDetailsDisclosure { border-color: transparent; background: transparent; color: @Secondary; }
        QToolButton#homeDetailsDisclosure:hover { background: @Hover; color: @Brand; }
        QPushButton#proxyManagement::menu-indicator { image: none; width: 0; height: 0; }
        QLabel#sectionTitle { font-size: @SectionPoints; font-weight: 600; }
        QLabel#settingName { font-weight: 500; }
        QWidget#settingRow[located="true"] { background: @Selection; border-radius: 6px; }
        QLabel#websiteName { font-weight: 600; }
        QPushButton#dashboardCard { text-align: left; padding: 0; }
        QPushButton#dashboardCard { border-radius: 12px; min-height: 104px; }
        QPushButton#dashboardCard[tier="primary"] { min-height: 114px; }
        QFrame#dashboardCard { background: @Surface; border: 1px solid @Border; border-radius: 12px; }
        QPushButton[quiet="true"], QToolButton[quiet="true"] { background: transparent; border-color: transparent; }
        QPushButton[quiet="true"]:hover, QToolButton[quiet="true"]:hover { background: @Hover; }
        QPushButton[quiet="true"]:pressed, QToolButton[quiet="true"]:pressed { background: @Pressed; }
        QPushButton[quiet="true"][keyboardFocus="true"]:focus, QToolButton[quiet="true"][keyboardFocus="true"]:focus { border-color: @Focus; }
        QLabel#websiteMark { background: @Control; border-radius: 9px; }
        QLabel#websiteDelay { font-size: @ValuePoints; font-weight: 600; }
        QLabel#dashboardIcon { background: @Selection; border-radius: 7px; }
        QLabel#statValue[tier="primary"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel[metricValue="true"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel#statValue[tier="secondary"] { font-size: @BodyPoints; font-weight: 600; }
        QListWidget#proxyGroups { border: none; background: transparent; outline: none; }
        QLabel#statusDot { color: @Muted; font-size: 18px; }
        QLabel#statValue { font-size: @BodyPoints; font-weight: 600; }
        QListWidget#navigation { border: none; background: transparent; outline: none; }
        QListWidget#navigation::item { padding: 8px 10px; margin: 2px 0px; border-radius: 7px; }
        QListWidget#navigation::item:hover { background: @Hover; }
        QListWidget#navigation::item:selected { background: @Primary; color: @OnPrimary; }
        QPushButton, QToolButton { min-height: 22px; padding: 3px 10px; border: 1px solid transparent; border-radius: 7px; background: @Control; }
        QPushButton:hover, QToolButton:hover { background: @Hover; }
        QPushButton:pressed, QToolButton:pressed { background: @Pressed; }
        QPushButton[keyboardFocus="true"]:focus, QToolButton[keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QToolButton#testGroupButton { padding-right: 22px; }
        QToolButton#testGroupButton::menu-button { width: 20px; border: none; background: transparent; }
        QToolButton#testGroupButton::menu-arrow { image: none; }
        QToolButton#testGroupButton[keyboardFocus="true"]:focus { padding: 3px 21px 3px 8px; }
        QPushButton#recentNodes { padding-right: 26px; }
        QPushButton#recentNodes::menu-indicator { image: none; width: 0; height: 0; }
        QPushButton#recentNodes[keyboardFocus="true"]:focus { padding: 3px 25px 3px 8px; }
        QPushButton[danger="true"]:enabled, QToolButton[danger="true"]:enabled { color: @Error; }
        QLabel#emptyState { color: @Secondary; padding: 24px; }
        QPushButton[primary="true"]:enabled, QToolButton[primary="true"]:enabled, QPushButton[visualPrimary="true"]:enabled { background: @Primary; color: @OnPrimary; border-color: @Primary; }
        QPushButton[primary="true"]:hover:enabled, QToolButton[primary="true"]:hover:enabled, QPushButton[visualPrimary="true"]:hover:enabled { background: @PrimaryPressed; }
        QPushButton[primary="true"]:pressed:enabled, QToolButton[primary="true"]:pressed:enabled, QPushButton[visualPrimary="true"]:pressed:enabled { background: @PrimaryPressed; border-color: @Focus; }
        QPushButton:checked, QToolButton:checked { background: @Selection; color: @Brand; border-color: @Brand; }
        QLineEdit, QComboBox, QSpinBox { min-height: 22px; padding: 3px 8px; border: 1px solid @Border; border-radius: 7px; background: @Surface; }
        QLineEdit[keyboardFocus="true"]:focus, QComboBox[keyboardFocus="true"]:focus, QSpinBox[keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QLineEdit QToolButton { min-width: 0; min-height: 0; padding: 0; margin: 0; border: none; background: transparent; }
        QLineEdit QToolButton:hover { background: @Hover; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox[customChevron="true"]::down-arrow { image: none; }
        QHeaderView::section { background: @Surface; color: @Secondary; padding: 7px 6px; border: none; border-bottom: 1px solid @Separator; }
        QTableView { border: none; border-radius: 5px; selection-background-color: palette(highlight); selection-color: palette(highlighted-text); }
        QTableView::item:selected { background: @Selection; color: @Ink; }
        QTableView::item:hover { background: @Hover; }
        QTableView::item:selected { background: @Selection; color: @Ink; }
        QTableView::item { padding: 0px 4px; border-bottom: 1px solid @Separator; }
        QPlainTextEdit { background: @Surface; border: 1px solid @Separator; border-radius: 7px; padding: 10px; font-family: @MonoFamily; }
        QPlainTextEdit[formField="true"] { background: @Surface; border: 1px solid @Border; border-radius: 7px; padding: 6px; font-family: @UiFamily; }
        QPlainTextEdit[keyboardFocus="true"]:focus { border: 1px solid @Focus; border-radius: 7px; padding: 10px; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: @Track; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QTabWidget::pane { background: @Surface; border: none; border-radius: 7px; }
        QTabBar { background: @Control; border-radius: 8px; }
        QTabBar::tab { padding: 5px 12px; margin: 3px; background: transparent; border: 1px solid transparent; border-radius: 6px; }
        QTabBar::tab:selected { background: @Surface; color: @Ink; border-color: @Border; }
        QTabBar::tab:hover:!selected { background: @Hover; }
        QWidget#settingRow { border-bottom: 1px solid @Separator; }
        QFrame#settingsApplication, QFrame#settingsCore, QFrame#settingsTools { border-radius: 12px; }
        QDialog { background: @Window; }
        QDialogButtonBox QPushButton { min-width: 64px; }
        QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
        QScrollBar::handle:horizontal { background: @Track; border-radius: 4px; min-width: 24px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
        QWidget:disabled { color: @Disabled; }
        QToolTip { background: @Surface; color: @Ink; border: 1px solid @Border; padding: 6px; }
        QStatusBar { background: @Sidebar; border-top: 1px solid @Border; }
        QStatusBar::item { border: none; }
        QStatusBar QLabel#operationMessage { padding-left: 10px; color: @Secondary; }
        QToolButton#websiteResultDetails { min-height: 0; padding: 3px; }
        QToolButton#contextHelp, QToolButton#websiteMethod { min-width: 0; min-height: 0; padding: 0; border-radius: 13px; color: @Secondary; }
        QToolButton#websiteResultDetails[keyboardFocus="true"]:focus { padding: 3px; }
        QToolButton#statusDetails { min-height: 0; padding: 2px 8px; border-color: transparent; background: transparent; }
        QToolButton#statusDetails:hover { background: @Hover; }
        QToolButton#statusDetails[keyboardFocus="true"]:focus { border-color: @Focus; padding: 1px 7px; }
        QPushButton[mode] { min-width: 35px; }
        QFrame#modeControl { background: @Control; border-radius: 7px; }
        QFrame#modeControl QPushButton { border-color: transparent; background: transparent; padding: 3px 8px; }
        QFrame#modeControl QPushButton:checked { background: @Surface; border-color: @Border; color: @Brand; }
        QFrame#modeControl QPushButton:pressed { background: @Pressed; color: @Brand; }
        QFrame#modeControl QPushButton[keyboardFocus="true"]:focus { border-color: @Focus; padding: 2px 7px; }
    )";
    sheet.replace("@UiFamily",QString('"')+bodyFont().family()+QString('"'));
#ifdef Q_OS_MACOS
    sheet.replace("@MonoFamily","\"Menlo\", \"PingFang SC\"");
#else
    sheet.replace("@MonoFamily",QString('"')+QFontDatabase::systemFont(QFontDatabase::FixedFont).family()+QString('"'));
#endif
    sheet.replace("@BodyPoints",QString::number(BodyPoints)+"pt");
    sheet.replace("@PagePoints",QString::number(PagePoints)+"pt");
    sheet.replace("@SectionPoints",QString::number(SectionPoints)+"pt");
    sheet.replace("@ValuePoints",QString::number(ValuePoints)+"pt");
    sheet.replace("@PrimaryPressed",PrimaryPressed);
    sheet.replace("@Control",Control); sheet.replace("@Pressed",Pressed);
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
    sheet.replace("min-height: 22px",QString("min-height: %1px").arg(ControlHeight-8));
    const QRegularExpression sizes("font-size:\\s*([0-9]+(?:\\.[0-9]+)?)pt"); auto iterator=sizes.globalMatch(sheet); QList<QRegularExpressionMatch> matches; while (iterator.hasNext()) matches.append(iterator.next()); for (int i=matches.size()-1;i>=0;--i) { const auto& match=matches[i]; sheet.replace(match.capturedStart(1),match.capturedLength(1),QString::number(match.captured(1).toDouble()*percent/100.0,'f',1)); }
    return sheet;
}
}
