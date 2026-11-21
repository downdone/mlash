#include "ShadowrocketStyleSheet.h"

namespace mlash::design::ShadowrocketStyle {
QString templateStyleSheet() {
    return QString::fromUtf8(R"(
        /* The application font owns the fallback list; a global QSS family would discard it. */
        QWidget { color: @Ink; }
        QWidget#websiteSurface, QWidget#pageSurface, QWidget#mainSurface, QWidget#settingsSurface, QWidget#settingsCanvas, QWidget#profilesSurface, QStackedWidget, QScrollArea { background: @Window; }
        QScrollArea#connectionColumnScroll, QWidget#connectionColumnRows { background: transparent; border: none; }
        QStackedWidget#nodeViews { background: @Surface; border-radius: 8px; }
        QDialog#importDialog, QDialog#profilePolicyDialog { background: @Window; }
        QDialog#importDialog[inlineImport="true"] { background: transparent; }
        QFrame#homeMetrics { background: @Surface; border: 1px solid @Border; border-radius: 10px; }
        QDialog#proxyEditorDialog[embeddedTask="true"] { background: @Window; }
        QTabWidget#proxyEditorTabs::pane { background: transparent; border: none; top: 4px; }
        QListWidget#editableNodes, QListWidget#editableGroups { background: @Surface; border: 1px solid @Border; border-radius: 9px; outline: none; }
        QScrollArea#importScroll { background: transparent; border: none; }
        QWidget#importBody, QTabWidget#importTabs, QTabWidget#importTabs::pane { background: @Surface; border: none; }
        QLabel#profileCollectionHeading { font-size: @SectionPoints; font-weight: 600; }
        QLabel#profileCollectionCount { color: @Secondary; font-size: @CaptionPoints; padding-left: 4px; }
        QWidget#importSource { background: @Surface; }
        QFrame#profileImportGroup, QFrame#backupForm, QFrame#overview, QFrame#panel, QFrame#homeControlCard, QFrame#settingsGroup, QFrame#trafficChart { background: @Surface; border: @PanelBorder; border-radius: 8px; }
        QFrame[summaryCard="true"] { background: @Surface; border: 1px solid @SummaryBorder; border-radius: 8px; }
        /* K1：内容区首行固定 72px（原型 topbar）。左标题/副标题，右状态胶囊与图标钮。 */
        QWidget#topbar { background: @Window; border: none; border-bottom: 1px solid @Separator; }
        QToolButton[windowChromeButton="true"] { background: transparent; border: none; border-radius: 5px; color: @Secondary; font-size: 14px; padding: 0; }
        QToolButton[windowChromeButton="true"]:hover { background: @Hover; color: @Ink; }
        QToolButton[windowChromeButton="true"]:pressed { background: @Pressed; }
        QToolButton[destructiveChrome="true"]:hover { background: @Error; color: @OnPrimary; }
        QPushButton[directoryAction="true"] { text-align: left; background: transparent; border: none; border-bottom: 1px solid @Separator; border-radius: 0; padding: 8px 6px; }
        QPushButton[directoryAction="true"]:hover { background: @Hover; }
        QPushButton[directoryAction="true"]:focus[keyboardFocus="true"] { border: 1px solid @Focus; border-radius: 5px; }
        QFrame#remoteProfileGroup, QFrame#localProfileGroup, QFrame#prototypeGroup { background: @Surface; border: @PanelBorder; border-radius: 8px; }
        QLabel#remoteProfileHeading, QLabel#localProfileHeading, QLabel#prototypeSection { padding: 4px 12px 0px 12px; font-weight: 500; }
        QPushButton[prototypeRow="true"] { text-align: left; background: transparent; border: none; border-radius: 0; padding: 0; }
        QPushButton[taskBack="true"] { background: @Control; color: @Ink; padding: 0; min-width: 30px; min-height: 30px; }
        QPushButton[taskBack="true"]:hover { background: @Hover; }
        QPushButton[backLink="true"] { background: transparent; border: none; color: @Brand; padding: 3px 0; }
        QWidget#settingsSearchStrip { background: @Window; }
        QLineEdit#settingsSearch { min-height: 18px; padding-top: 3px; padding-bottom: 3px; }
        QPushButton#settingsSearchResults { background: transparent; border: none; color: @Secondary; padding: 3px 4px; }
        QPushButton#settingsSearchResults:hover { background: @Hover; color: @Ink; }
        QListWidget#settingsInlineResults { background: @Surface; border: 1px solid @Border; border-radius: 9px; }
        QListWidget#settingsInlineResults::item { min-height: @SettingRowHeight; padding: 0 12px; border-bottom: 1px solid @Separator; }
        QListWidget#settingsInlineResults::item:hover { background: @Hover; }
        QListWidget#settingsInlineResults::item:selected { background: @Selection; color: @Brand; }
        QLabel[caption="true"] { font-size: @CaptionPoints; }
        QToolButton[prototypeIcon="true"] { background: transparent; border: none; padding: 5px; color: @Brand; }
        QToolButton#browseGroup { color: @Ink; background: @Control; border: none; border-radius: 6px; padding: 4px 8px; text-align: left; }
        QToolButton#browseGroup:hover { background: @Hover; }
        QToolButton[prototypeIcon="true"]:hover { background: @Hover; }
        QWidget#settingsCanvas, QWidget#settingsDirectory, QWidget#profileDetailsPage { background: @Window; }
        QFrame#panel { border: none; }
        QFrame#homeControlCard { border: @PanelBorder; border-radius: 8px; }
        QFrame#homeControlSeparator { background: @Separator; border: none; margin-left: 35px; }
        QLabel#taskTitle { font-size: @TitlePoints; font-weight: 600; }
        QDialog[embeddedTask="true"] { background: @Window; }
        QLabel#topbarTitle { font-size: @TitlePoints; font-weight: 600; }
        QLabel#topbarSubtitle { color: @Secondary; }
        QWidget#topbarStatus { border: 1px solid @Border; border-radius: 15px; background: @Surface; }
        QLabel#topbarDot[state="success"] { color: @Success; }
        QLabel#topbarDot[state="warning"] { color: @Warning; }
        QLabel#topbarDot[state="error"] { color: @Error; }
        QLabel#topbarDot[state="active"] { color: @Brand; }
        QLabel#topbarDot[state="idle"] { color: @Secondary; }
        QLabel#sectionHeading { font-size: @SectionPoints; font-weight: 600; }
        QLabel#sectionCount { color: @Secondary; }
        QWidget#sidebar { background: @Sidebar; border: none; border-right: 1px solid @Border; }
        QLabel#brand { font-size: @SectionPoints; font-weight: 600; }
        QLabel#dialogTitle { font-size: @DialogPoints; font-weight: 600; }
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
        QLabel#secondaryText, QLabel[secondary="true"] { color: @Secondary; font-size: @CaptionPoints; }
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
        QLabel#websiteDelay[state="untested"] { color: @Secondary; font-weight: 600; }
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
        QLabel[summaryTitle="true"] { font-size: @BodyPoints; font-weight: 600; }
        QPushButton[quiet="true"], QToolButton[quiet="true"] { background: transparent; border-color: transparent; }
        QPushButton[quiet="true"]:hover, QToolButton[quiet="true"]:hover { background: @Hover; }
        QPushButton[quiet="true"]:pressed, QToolButton[quiet="true"]:pressed { background: @Pressed; }
        QPushButton[quiet="true"][keyboardFocus="true"]:focus, QToolButton[quiet="true"][keyboardFocus="true"]:focus { border-color: @Focus; }
        QLabel[summaryMark="true"] { background: @Control; border-radius: 9px; }
        QLabel#websiteMark[summaryMark="true"] { background: @ArtworkSurface; }
        QLabel#websiteDelay { font-size: @ValuePoints; font-weight: 600; }
        QLabel#statValue[tier="primary"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel[metricValue="true"] { font-size: @ValuePoints; font-weight: 600; }
        QLabel#statValue[tier="secondary"] { font-size: @BodyPoints; font-weight: 600; }
        QListWidget#proxyGroups { border: none; background: transparent; outline: none; }
        QLabel#statValue { font-size: @BodyPoints; font-weight: 600; }
        QFrame#sidebarDivider { background: @Separator; border: none; }
        QListWidget#navigation { border: none; background: transparent; outline: none; }
        QToolButton#sidebarSettings, QToolButton#toggleSidebar { min-height: @NavButtonInnerHeight; max-height: @NavButtonInnerHeight; padding: 0; border: 1px solid transparent; border-radius: 7px; }
        QToolButton#sidebarSettings { background: transparent; }
        QToolButton#sidebarSettings:hover { background: @Hover; }
        QToolButton#sidebarSettings:checked { background: @Selection; color: @Brand; border-color: transparent; font-weight: 600; }
        QToolButton#sidebarSettings:checked:hover { background: @Selection; }
        QToolButton#sidebarSettings[keyboardFocus="true"]:focus { border-color: @Focus; }
        QToolButton#nodeSearchToggle:enabled, QToolButton#nodeLocateButton:enabled, QToolButton#nodeToolsButton:enabled { color: @Secondary; }
        QLabel#nodeHeadingTitle { font-size: @SectionPoints; font-weight: 600; }
        QLabel#nodeHeadingCount { color: @Secondary; font-size: @CaptionPoints; }
        QPushButton, QToolButton { min-height: @ControlMinimum; padding: 3px 10px; border: 1px solid transparent; border-radius: 6px; background: @Control; }
        QPushButton#homeRouteRow { min-height: @HomeControlRowMinimum; padding: 0; }
        QPushButton:hover, QToolButton:hover { background: @Hover; }
        QPushButton:pressed, QToolButton:pressed { background: @Pressed; }
        QPushButton[keyboardFocus="true"]:focus, QToolButton[keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QPushButton[prototypeRow="true"]:hover, QPushButton[prototypeRow="true"]:pressed { background: transparent; border: none; }
        QPushButton[prototypeRow="true"][keyboardFocus="true"]:focus { border: none; }
        QToolButton::menu-indicator { image: none; width: 0px; }
        QListView#nodeList { background: @Surface; border: none; border-radius: 8px; outline: none; }
        QListWidget#groupChoices { background: transparent; border: none; outline: none; }
        QListWidget#groupChoices::item { padding: 3px 8px; border-radius: 5px; }
        QListWidget#groupChoices::item:hover { background: @Hover; }
        QListWidget#groupChoices::item:selected { background: @Selection; color: @Ink; }
        QToolButton#testGroupButton { padding-right: 22px; }
        QToolButton#testGroupButton::menu-button { width: 20px; border: none; background: transparent; }
        QToolButton#testGroupButton::menu-arrow { image: none; }
        QPushButton#recentNodes { padding-right: 26px; }
        QPushButton#recentNodes::menu-indicator { image: none; width: 0; height: 0; }
        QPushButton[danger="true"]:enabled, QToolButton[danger="true"]:enabled { color: @Error; }
        QLabel#emptyState { color: @Secondary; padding: 0px; }
        QPushButton[primary="true"]:enabled, QToolButton[primary="true"]:enabled, QPushButton[visualPrimary="true"]:enabled { background: @Primary; color: @OnPrimary; border-color: @Primary; font-weight: 600; }
        QPushButton[primary="true"]:hover:enabled, QToolButton[primary="true"]:hover:enabled, QPushButton[visualPrimary="true"]:hover:enabled { background: @PrimaryPressed; }
        QPushButton[primary="true"]:pressed:enabled, QToolButton[primary="true"]:pressed:enabled, QPushButton[visualPrimary="true"]:pressed:enabled { background: @PrimaryPressed; border-color: @Focus; }
        QPushButton[primary="true"][keyboardFocus="true"]:focus:enabled, QToolButton[primary="true"][keyboardFocus="true"]:focus:enabled, QPushButton[visualPrimary="true"][keyboardFocus="true"]:focus:enabled { border-color: @OnPrimary; }
        QPushButton:checked, QToolButton:checked { background: @Selection; color: @Brand; border-color: @Brand; }
        QLineEdit, QComboBox, QSpinBox { min-height: @ControlMinimum; padding: 3px 8px; border: 1px solid @Border; border-radius: 6px; background: @Surface; }
        QLineEdit[keyboardFocus="true"]:focus, QComboBox[keyboardFocus="true"]:focus, QSpinBox[keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QLineEdit QToolButton { min-width: 0; min-height: 0; padding: 0; margin: 0; border: none; background: transparent; }
        QLineEdit QToolButton:hover { background: @Hover; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox[customChevron="true"]::down-arrow { image: none; }
        QHeaderView::section { background: @Surface; color: @Secondary; padding: 7px 6px; border: none; border-bottom: 1px solid @Separator; }
        QTableView { border: none; border-radius: 5px; selection-background-color: palette(highlight); selection-color: palette(highlighted-text); }
        QTableView::item:selected { background: @Selection; color: @Ink; }
        QTableView::item:hover:!selected { background: @Hover; }
        QTableView::item { padding: 0px 4px; border-bottom: 1px solid @Separator; }
        QPlainTextEdit { background: @Surface; border: 1px solid @Separator; border-radius: 6px; padding: 8px; font-family: @MonoFamily; }
        QTextEdit#versionChangesDetails { background: @Surface; border: 1px solid @Separator; border-radius: 6px; padding: 8px; }
        QTextEdit#versionChangesDetails[keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QPlainTextEdit[formField="true"] { background: @Surface; border: 1px solid @Border; border-radius: 6px; padding: 6px; }
        QPlainTextEdit[keyboardFocus="true"]:focus { border: 1px solid @Focus; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: @Track; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QTabWidget::pane { background: @Surface; border: none; border-radius: 6px; }
        QTabBar { background: @Control; border-radius: 6px; }
        QTabBar::tab { padding: 5px 12px; margin: 2px; min-height: 18px; background: transparent; color: @Secondary; border: 1px solid transparent; border-radius: 5px; }
        QTabBar::tab:selected { background: @Surface; color: @Ink; font-weight: 600; }
        QTabBar::tab:hover:!selected { background: @Hover; }
        QTabBar[keyboardFocus="true"]::tab:selected { border-color: @Focus; }
        /* K8：设置页三组平铺行，52px 高，行间只用分隔线（原型 .setting-row）。 */
        QWidget#settingRow { min-height: @SettingRowHeight; }
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
        QFrame#statusAlarm { background: @Window; border: 1px solid @Border; border-radius: 6px; }
        QLabel#operationAlarm { color: @Ink; font-size: @CaptionPoints; font-weight: 600; }
        QTextEdit#statusAlarmReason { background: @Window; border: 1px solid @Border; border-radius: 6px; padding: 6px; }
        QTextEdit#statusAlarmReason[keyboardFocus="true"]:focus { border-color: @Focus; }
        QToolButton[statusAction="true"] { min-height: 0; padding: 1px 5px; background: transparent; color: @Secondary; font-size: @CaptionPoints; }
        QToolButton[statusAction="true"]:hover { background: @Hover; color: @Ink; }
        QToolButton[statusAction="true"]:pressed { background: @Pressed; }
        QToolButton[statusAction="true"][keyboardFocus="true"]:focus { border-color: @Focus; }
        QToolButton#websiteResultDetails { min-height: 0; padding: 3px; }
        QToolButton#contextHelp, QToolButton#websiteMethod { min-width: 0; min-height: 0; padding: 0; border-radius: 13px; color: @Secondary; }
        QToolButton#websiteResultDetails[keyboardFocus="true"]:focus { padding: 3px; }
        QToolButton#statusDetails { min-height: 0; padding: 2px 8px; border-color: transparent; background: transparent; }
        QToolButton#statusDetails:hover { background: @Hover; }
        QToolButton#statusDetails[keyboardFocus="true"]:focus { border-color: @Focus; }
        QPushButton[mode] { min-width: 35px; }
        /* 配置卡片内的名称、来源和动作。 */
        QWidget#profileQuickImport { background: @Selection; border-radius: 8px; }
        QWidget#profileImportField { background: @Window; border: 1px solid @Border; border-radius: 7px; }
        QWidget#profileImportField[editing="true"] { border-color: @Focus; }
        QLineEdit#profileImportInput, QLineEdit#profileImportInput[keyboardFocus="true"]:focus {
            min-height: 24px; padding: 3px 0; background: transparent; border: none;
        }
        QToolButton#profileBrowseImport { min-height: 24px; min-width: 26px; padding: 0; }
        QPushButton#profileQuickSubmit, QPushButton#importSubscription { min-height: 24px; padding: 3px 10px; border-radius: 7px; }
        QPushButton#profileQuickSubmit { min-width: 44px; }
        QPushButton#importSubscription { min-width: 37px; }
        QPushButton#importSubscription:enabled { background: @Window; border-color: @Border; }
        QPushButton#importSubscription:hover:enabled { background: @Hover; }
        QPushButton#importSubscription:pressed:enabled { background: @Pressed; }
        QPushButton#importSubscription[keyboardFocus="true"]:focus:enabled { border-color: @Focus; }
        QListView#profileList { border: none; background: transparent; outline: none; }
        QListView#profileList::item { border: none; background: transparent; }
        QFrame#profileCard { margin: 8px; border-color: @Border; border-radius: 10px; }
        QFrame#profileCard:hover { border-color: @Track; }
        QFrame#profileCard[selected="true"] { border-color: @Brand; }
        QListView#profileList[keyboardFocus="true"]:focus QFrame#profileCard[selected="true"] { border: 1px solid @Focus; }
        QLabel#profileCardMark[summaryMark="true"] { background: @Selection; }
        QLabel#profileCardTitle[summaryTitle="true"] { font-size: @SectionPoints; }
        QLabel[profileCardMetadataTag="true"] { background: @Control; border-radius: 10px; color: @Secondary; padding: 2px 8px; font-size: @CaptionPoints; }
        QWidget[profileCardStateTag="true"] { background: @Control; border-radius: 10px; }
        QWidget[profileCardStateTag="true"][tone="success"] { background: @ProfileActiveFill; }
        QLabel[cardState="true"] { color: @Secondary; font-size: @CaptionPoints; }
        QLabel[cardState="true"][tone="success"] { color: @Success; }
        QLabel[cardState="true"][tone="warning"] { color: @Warning; }
        QLabel[cardState="true"][tone="error"] { color: @Error; }
        QFrame#proxyReadStatus { background: @Surface; border: 1px solid @Separator; border-radius: 8px; }
        QToolButton[profileCardIcon="true"] { padding: 0; background: @Window; border-color: @Border; border-radius: 8px; }
        QToolButton[profileCardIcon="true"]:hover:enabled { background: @Hover; }
        QToolButton[profileCardIcon="true"]:pressed:enabled { background: @Pressed; }
        QToolButton[profileCardIcon="true"]:disabled { background: transparent; border-color: @Separator; }
        QPushButton[profileCardAction="true"] { font-size: @CaptionPoints; }
        QLabel#profileRowMark { background: @Control; border-radius: 10px; }
        /* K3：右侧抽屉（410px），原型 .drawer。 */
        QDialog#nodeDetailsDrawer { background: @Surface; border-left: 1px solid @Border; }
        QLabel#drawerTitle { font-size: @SectionPoints; font-weight: 600; }
        QLabel#drawerSub { color: @Secondary; }
        QWidget#drawerKeyValue { border-bottom: 1px solid @Separator; min-height: 38px; }
        QLabel#drawerKey { color: @Secondary; }
    )");
}
} // namespace mlash::design::ShadowrocketStyle
