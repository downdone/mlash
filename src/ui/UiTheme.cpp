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
    font.setFamilies({"Microsoft YaHei UI","Segoe UI","Noto Sans CJK SC"});
#endif
    font.setPointSizeF(BodyPoints*percent/100.0); font.setStyleHint(QFont::SansSerif); return font;
}
void setPreferences(const QString& appearance,int scale) { if (!validAppearance(appearance,scale)) return; if (preferred==appearance&&percent==scale) { if (!installed) install(); return; } preferred=appearance; percent=scale; if (!installed) install(); else updatePreferred(); }
void applyAppearance(Qt::ColorScheme scheme,bool highContrast) {
    dark=scheme==Qt::ColorScheme::Dark;
    // 高对比是第四套并行取值（分层设计 6.3 规则 3）：不只加深边框，表面分级、
    // 选中、控件填充与焦点环同时增强。HC 下表面不再「逐级变亮」，而是靠边框与
    // 更大的明度差区分；取值经 WCAG 相对亮度复算，见 ui-r9-tokens.md。
    if (highContrast) {
        Window=dark?"#000000":"#FFFFFF"; Sidebar=dark?"#000000":"#FFFFFF"; Surface=dark?"#101010":"#FFFFFF";
        Ink=dark?"#FFFFFF":"#000000"; Secondary=dark?"#FFFFFF":"#1A1A1A"; Muted=dark?"#E6E6E6":"#333333"; Disabled=dark?"#8C8C8C":"#6B6B6B";
        Border=dark?"#FFFFFF":"#000000"; Separator=dark?"#FFFFFF":"#000000";
        Brand=dark?"#FFD60A":"#0B3D91"; Primary=dark?"#FFD60A":"#0B3D91"; OnPrimary=dark?"#000000":"#FFFFFF";
        Selection=dark?"#3A2F00":"#DCE6F7"; Hover=dark?"#262626":"#EDEDED";
        Focus=dark?"#FFD60A":"#0B63CE";
        Control=dark?"#1C1C1C":"#F2F2F2"; Pressed=dark?"#333333":"#D9D9D9"; PrimaryPressed=dark?"#FFE44D":"#082C6B";
        Success=dark?"#5CFFA8":"#00552E"; Warning=dark?"#FFD60A":"#6B3A00"; Error=dark?"#FF8A9A":"#A30018"; Track=dark?"#FFFFFF":"#4A4A4A";
    } else {
        // K 版（U08 §7.3）：深色直接取 GPT 原型 token；浅色按同一层级关系取，
        // 不复制原型的暗色值。高对比仍是第四套独立取值（上一分支）。
        Window=dark?"#000000":"#F2F2F7"; Sidebar=dark?"#202020":"#F5F5F7"; Surface=dark?"#1C1C1E":"#FFFFFF";
        Ink=dark?"#F5F5F7":"#1D1D1F"; Secondary=dark?"#A1A1AA":"#63636B"; Muted=dark?"#868d99":"#5f6877"; Disabled=dark?"#5c6069":"#A7ADB8";
        Border=dark?"#343436":"#E3E3E8"; Separator=dark?"#343436":"#E3E3E8";
        // U11: shared prototype roles; primary fill remains separate from link/icon accent.
        Brand=dark?"#4DA3FF":"#0066CC"; Primary=dark?"#075FBE":"#0066CC"; OnPrimary="#FFFFFF";
        Selection=dark?"#132B43":"#E7F0FA"; Hover=dark?"#29292C":"#ECEEF3"; Focus=dark?"#3798ff":"#0066CC";
        Control=dark?"#242529":"#F0F1F4"; Pressed=dark?"#333538":"#E2E5EA"; PrimaryPressed=dark?"#3798ff":"#0066CC";
        Success=dark?"#64C98A":"#18724A"; Warning=dark?"#EFB66A":"#865000"; Error=dark?"#FF8A96":"#B32636"; Track=dark?"#696970":"#8B8B91";
    }
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
        QWidget#websiteSurface, QWidget#pageSurface, QWidget#mainSurface, QWidget#settingsSurface, QWidget#settingsCanvas, QWidget#profilesSurface, QStackedWidget, QScrollArea { background: @Window; }
        QStackedWidget#nodeViews { background: @Surface; border-radius: 8px; }
        QDialog#importDialog, QDialog#profilePolicyDialog { background: @Window; }
        QWidget#importSource { background: @Surface; }
        QFrame#profileImportGroup, QFrame#websiteCard, QFrame#overview, QFrame#panel, QFrame#homeControlCard, QFrame#settingsGroup, QFrame#trafficChart { background: @Surface; border: 1px solid @Border; border-radius: 12px; }
        /* K1：内容区首行固定 72px（原型 topbar）。左标题/副标题，右状态胶囊与图标钮。 */
        QWidget#topbar { background: @Sidebar; border: none; border-bottom: 1px solid @Separator; }
        QPushButton[directoryAction="true"] { text-align: left; background: transparent; border: none; border-bottom: 1px solid @Separator; border-radius: 0; padding: 8px 6px; }
        QPushButton[directoryAction="true"]:hover { background: @Hover; }
        QPushButton[directoryAction="true"]:focus[keyboardFocus="true"] { border: 1px solid @Focus; border-radius: 5px; }
        QFrame#remoteProfileGroup, QFrame#localProfileGroup, QFrame#prototypeGroup { background: @Surface; border: none; border-radius: 10px; }
        QLabel#remoteProfileHeading, QLabel#localProfileHeading, QLabel#prototypeSection { padding: 4px 12px 0px 12px; font-weight: 500; }
        QPushButton[prototypeRow="true"] { text-align: left; background: transparent; border: none; border-bottom: 1px solid @Separator; border-radius: 0; padding: 0; }
        QPushButton[prototypeRow="true"]:hover { background: @Hover; }
        QPushButton[prototypeRow="true"][keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QPushButton[backLink="true"] { background: transparent; border: none; color: @Brand; padding: 3px 0; }
        QLabel[caption="true"] { font-size: 9pt; }
        QToolButton[prototypeIcon="true"] { background: transparent; border: none; padding: 5px; color: @Brand; }
        QToolButton#browseGroup { color: @Ink; padding: 5px; }
        QToolButton[prototypeIcon="true"]:hover { background: @Hover; }
        QWidget#settingsCanvas, QWidget#settingsDirectory, QWidget#profileDetailsPage { background: @Window; }
        QFrame#panel { border: none; }
        QFrame#homeControlCard { border: none; border-radius: 10px; }
        QFrame#homeControlSeparator { background: @Separator; border: none; margin-left: 35px; }
        QTabBar#taskTabs, QTabBar#profileTaskTabs { background: @Sidebar; border-radius: 7px; }
        QTabBar#taskTabs::tab, QTabBar#profileTaskTabs::tab { border: none; background: transparent; color: @Secondary; padding: 4px 10px; margin: 1px; border-radius: 5px; }
        QTabBar#taskTabs::tab:selected, QTabBar#profileTaskTabs::tab:selected { background: @Surface; color: @Ink; }
        QDialog[embeddedTask="true"] { background: @Window; }
        QTabBar#dataTabs { background: @Sidebar; border-radius: 7px; }
        QTabBar#dataTabs::tab { border: none; background: transparent; color: @Secondary; padding: 4px 12px; margin: 2px; border-radius: 5px; }
        QTabBar#dataTabs::tab:selected { background: @Surface; color: @Ink; }
        QLabel#topbarTitle { font-size: @TitlePoints; font-weight: 700; }
        QLabel#topbarSubtitle { color: @Secondary; }
        QWidget#topbarStatus { border: 1px solid @Border; border-radius: 15px; background: @Surface; }
        QLabel#topbarDot[state="success"] { color: @Success; }
        QLabel#topbarDot[state="warning"] { color: @Warning; }
        QLabel#topbarDot[state="error"] { color: @Error; }
        QLabel#topbarDot[state="active"] { color: @Brand; }
        QLabel#topbarDot[state="idle"] { color: @Secondary; }
        /* K6：数据页四个 tab 用同一套胶囊样式（QTabBar 基础规则已给出）。 */
        QTabBar#dataTabs::tab { padding: 4px 12px; }
        QLabel#sectionHeading { font-size: @SectionPoints; font-weight: 600; }
        QLabel#sectionCount { color: @Secondary; }
        QWidget#sidebar { background: @Sidebar; border: none; border-right: 1px solid @Border; }
        QLabel#brand { font-size: @SectionPoints; font-weight: 600; }
        QLabel#dialogTitle { font-size: @DialogPoints; font-weight: 600; }
        QLabel#overviewTitle { font-size: @SectionPoints; font-weight: 600; }
        QLabel#homeControlTitle { font-weight: 500; }
        QLabel#homeControlValue { color: @Secondary; }
        QLabel#homeControlChevron { color: @Secondary; font-size: @SectionPoints; }
        QWidget#homeControlRow[first="true"] { border: none; }
        /* K2：第 2 行是按钮（点开浮层菜单），其余两行是普通行；同一套行样式用 property 选中。 */
        QPushButton[controlRow="true"] { background: transparent; border: none; border-radius: 6px; padding: 0; text-align: left; }
        QPushButton[controlRow="true"]:hover { background: @Hover; }
        QPushButton[controlRow="true"]:pressed { background: @Pressed; }
        QPushButton[controlRow="true"][expanded="true"] { background: @Selection; }
        QPushButton[controlRow="true"][keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QWidget#homeInlineStat { border: none; }
        QToolButton#homeStatDetails { color: @Brand; }
        QLabel#currentMember { font-size: @BodyPoints; font-weight: 600; }
        QLabel#secondaryText, QLabel[secondary="true"] { color: @Secondary; font-size: 9pt; }
        QLabel#statusDot[state="success"] { color: @Success; }
        QLabel#statusDot[state="warning"] { color: @Warning; }
        QLabel#statusDot[state="error"] { color: @Error; }
        QLabel#statusDot[state="active"] { color: @Brand; }
        QLabel#statusDot[state="idle"] { color: @Secondary; }
        QLabel#logEmptyState { color: @Secondary; }
        QListView#nodeCards[flat="true"] { border: none; background: transparent; outline: none; }
        QLabel#websiteDelay[state="ok"] { color: @Success; font-weight: 600; }
        QLabel#websiteDelay[state="failed"] { color: @Error; font-weight: 600; }
        QLabel#websiteDelay[state="cancelled"] { color: @Secondary; font-weight: 600; }
        QLabel#websiteDelay[state="pending"] { color: @Brand; font-weight: 600; }
        QLabel#sidebarStatusDot[state="success"] { color: @Success; }
        QLabel#sidebarStatusDot[state="warning"] { color: @Warning; }
        QLabel#sidebarStatusDot[state="error"] { color: @Error; }
        QLabel#sidebarStatusDot[state="secondary"] { color: @Secondary; }
        QToolButton#sidebarStatus { background: transparent; border: none; border-top: 1px solid @Separator; border-radius: 0; text-align: left; }
        QToolButton#sidebarStatus:hover { background: @Hover; }
        QLabel#sidebarStatusLine1 { color: @Secondary; }
        QLabel#sidebarStatusLine2 { color: @Secondary; }
        QLabel[role="headline"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel#settingsSubheading { font-size: @BodyPoints; font-weight: 600; padding-bottom: 3px; }
        QPushButton#proxyManagement::menu-indicator { image: none; width: 0; height: 0; }
        QLabel#sectionTitle { font-size: @SectionPoints; font-weight: 600; }
        QLabel#settingName { font-weight: 500; }
        QWidget#settingRow[located="true"] { background: @Selection; border-radius: 6px; }
        QLabel#websiteName { font-weight: 600; }
        QPushButton[quiet="true"], QToolButton[quiet="true"] { background: transparent; border-color: transparent; }
        QPushButton[quiet="true"]:hover, QToolButton[quiet="true"]:hover { background: @Hover; }
        QPushButton[quiet="true"]:pressed, QToolButton[quiet="true"]:pressed { background: @Pressed; }
        QPushButton[quiet="true"][keyboardFocus="true"]:focus, QToolButton[quiet="true"][keyboardFocus="true"]:focus { border-color: @Focus; }
        QLabel#websiteMark { background: @ArtworkSurface; border-radius: 9px; }
        QLabel#websiteDelay { font-size: @ValuePoints; font-weight: 600; }
        QLabel#statValue[tier="primary"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel[metricValue="true"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel#statValue[tier="secondary"] { font-size: @BodyPoints; font-weight: 600; }
        QListWidget#proxyGroups { border: none; background: transparent; outline: none; }
        QLabel#statValue { font-size: @BodyPoints; font-weight: 600; }
        QListWidget#navigation { border: none; background: transparent; outline: none; }
        QListWidget#navigation::item { padding: 5px 8px; margin: 0px; border-radius: 7px; }
        QListWidget#navigation::item:hover { background: @Hover; }
        QListWidget#navigation::item:selected { background: @Primary; color: @OnPrimary; }
        QPushButton, QToolButton { min-height: 22px; padding: 3px 10px; border: 1px solid transparent; border-radius: 7px; background: @Control; }
        QPushButton:hover, QToolButton:hover { background: @Hover; }
        QPushButton:pressed, QToolButton:pressed { background: @Pressed; }
        QPushButton[keyboardFocus="true"]:focus, QToolButton[keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QToolButton::menu-indicator { image: none; width: 0px; }
        QListView#nodeList { background: @Surface; border: none; border-radius: 10px; outline: none; }
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
        /* K8：设置页三组平铺行，52px 高，行间只用分隔线（原型 .setting-row）。 */
        QWidget#settingRow { border-bottom: 1px solid @Separator; min-height: @SettingRowHeight; }
        QWidget#settingRow[last="true"] { border-bottom: none; }
        QLabel#settingIcon { background: transparent; border-radius: 7px; }
        QLabel#settingChevron { color: @Muted; font-size: @BodyPoints; }
        QLabel#settingValue { color: @Muted; }
        QWidget#settingsActions { border-top: 1px solid @Separator; padding-top: 10px; }
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
        /* 控制区的失败原因行（V5）：用告警色，与普通说明文字区分；成功时不显示。 */
        QLabel#homeControlHint[state="error"] { color: @Error; }
        QLabel#nodeGroupHint { color: @Secondary; }
        /* 菜单分组标题（V4）：禁用 action 默认会被画成灰色，这里统一压低对比并缩小上下留白。 */
        QMenu::item:disabled { color: @Muted; }
        QStatusBar QLabel#operationAlarm { padding-left: 10px; color: @Error; }
        QStatusBar QLabel#operationAlarm[state="warning"] { color: @Warning; }
        QToolButton#websiteResultDetails { min-height: 0; padding: 3px; }
        QToolButton#contextHelp, QToolButton#websiteMethod { min-width: 0; min-height: 0; padding: 0; border-radius: 13px; color: @Secondary; }
        QToolButton#websiteResultDetails[keyboardFocus="true"]:focus { padding: 3px; }
        QToolButton#statusDetails { min-height: 0; padding: 2px 8px; border-color: transparent; background: transparent; }
        QToolButton#statusDetails:hover { background: @Hover; }
        QToolButton#statusDetails[keyboardFocus="true"]:focus { border-color: @Focus; padding: 1px 7px; }
        QPushButton[mode] { min-width: 35px; }
        /* K7：配置页行高 76px（原型 .profile-row），图标 37px 方形。 */
        QListView#remoteProfileList, QListView#localProfileList { border: none; background: transparent; outline: none; }
        QListView#remoteProfileList::item, QListView#localProfileList::item { border-bottom: 1px solid @Separator; }
        QLabel#profileRowMark { background: @Control; border-radius: 10px; }
        /* K3：右侧抽屉（410px），原型 .drawer。 */
        QDialog#nodeDetailsDrawer { background: @Surface; border-left: 1px solid @Border; }
        QLabel#drawerTitle { font-size: @SectionPoints; font-weight: 700; }
        QLabel#drawerSub { color: @Secondary; }
        QWidget#drawerKeyValue { border-bottom: 1px solid @Separator; min-height: 47px; }
        QLabel#drawerKey { color: @Secondary; }
    )";
    sheet.replace("@UiFamily",QString('"')+bodyFont().family()+QString('"'));
#ifdef Q_OS_MACOS
    sheet.replace("@MonoFamily","\"Menlo\", \"PingFang SC\"");
#else
    sheet.replace("@MonoFamily",QString('"')+QFontDatabase::systemFont(QFontDatabase::FixedFont).family()+QString('"'));
#endif
    sheet.replace("@BodyPoints",QString::number(BodyPoints)+"pt");
    sheet.replace("@DialogPoints",QString::number(DialogPoints)+"pt");
    sheet.replace("@SectionPoints",QString::number(SectionPoints)+"pt");
    sheet.replace("@ValuePoints",QString::number(ValuePoints)+"pt");
    sheet.replace("@TitlePoints",QString::number(TitlePoints)+"pt");
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
    sheet.replace("min-height: 22px",QString("min-height: %1px").arg(ControlHeight-8));
    const QRegularExpression sizes("font-size:\\s*([0-9]+(?:\\.[0-9]+)?)pt"); auto iterator=sizes.globalMatch(sheet); QList<QRegularExpressionMatch> matches; while (iterator.hasNext()) matches.append(iterator.next()); for (int i=matches.size()-1;i>=0;--i) { const auto& match=matches[i]; sheet.replace(match.capturedStart(1),match.capturedLength(1),QString::number(match.captured(1).toDouble()*percent/100.0,'f',1)); }
    return sheet;
}
}
