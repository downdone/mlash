# U05 界面系统重构方案

2026-10-29 · 评审结论，未实施。本文只记录发现与提案；实际后果由 [验证报告](verification-report.md) 维护，当前行为仍以 [交互契约](ui-interactions.md) 为准。决策意图见 [ADR 0045](adr/0045-ui-system-rebuild.md)。

## 方法

以 `UiTheme.cpp`、`MainWindow*.cpp` 等实际实现为证据，对照 [交互契约](ui-interactions.md) 与 Apple HIG（[布局](https://developer.apple.com/design/human-interface-guidelines/layout)/[排版](https://developer.apple.com/design/human-interface-guidelines/typography)/[颜色](https://developer.apple.com/design/human-interface-guidelines/color)/[按钮](https://developer.apple.com/design/human-interface-guidelines/buttons)）。两条独立评审线分别覆盖信息架构与视觉系统，结论交叉核对；对比度按 WCAG 2.x 静态计算，未在 Windows 实机、120% 文字或系统高对比下截图核对。

## 总体判断

四分区/八视图在契约层成立，代码层仍是八页索引驱动：分区只是页号映射，标题位被页签劫持，导航语义随页面变化。视觉系统有正确的语义角色词汇，主文字对比（16.8:1 / 12.8:1）扎实，但只有三层真正生效的表面，悬停/按下/选中对比仅 1.05–1.35，高对比只改两个边框值。

两份评审独立得出的结论一致：问题不在个别控件，而在四条结构性决策。

## 四条共同根因

| 根因 | 现象 | 证据 | 视觉与结构后果 |
|---|---|---|---|
| 标题槽被页签劫持 | 同一左上位置在不同分区代表不同层级 | `MainWindow.cpp:480-487` 的 `replaceWidget`；`UiTheme.cpp:88` 的 `#pageTitle` 样式基本不可见 | 应用里没有页面标题；规则/连接出现两层同形页签 |
| 首页六卡是第二套导航 | 六卡用 `i+1` 当页号，恰好是旧 1–6 页序 | `MainWindowHome.cpp:45-47,62` | 侧栏、页签、卡片三套同目的地；主次卡只差 10px |
| 每个容器都描 1px 边框 | 一条规则统一八种容器 | `UiTheme.cpp:85` | 页面由「线框」堆成，分组只靠间隙区分 |
| 状态无单一答案 | 解释性文本计算后隐藏；TUN 文本与开关不同源 | `MainWindowHome.cpp:106`；`MainWindow.cpp:331,343` | 在任何非首页无法回答「我是否在被代理」 |

## 结构问题

### N1 导航由页号驱动【推翻】

侧栏只存页号 `{0,1,4,6}`（`MainWindow.cpp:107`），`syncSectionNavigation` 用三元表达式把八页映射为四分区（`MainWindow.cpp:470`）。页序调整会静默错链；启动页下拉直接列旧八页名（`MainWindowSettings.cpp:56`）；托盘「订阅管理」打开标题为「订阅」的页。

方案：单一导航模型（稳定 view ID、分组、标签、默认页），侧栏/页面标题/托盘/启动页/搜索全部从它取值；视图代码禁止页号算术。

### N2 标题槽可变页签【推翻】

见根因一。分区最后视图挂在导航项数据上（`MainWindow.cpp:472`），轻量关闭重建后 `GuiHost` 只恢复一个页面号（`GuiHost.cpp:9-25`）。

方案：固定导航，页面标题恢复可见；规则/连接的子视图降为页内分段控件，不再与主导航同形。

### N3 词表不统一【重排】

「首页/状态」「数据/连接+日志」「订阅管理/订阅」三处不一致（`MainWindow.cpp:106,470,476,117`；`TrayController.cpp:168`）。

方案：单一词表，托盘与搜索从导航模型取标签。

### N4 全局状态只在首页且被隐藏【重排】

`coreLabel_` 永久 `hide()`（`MainWindowHome.cpp:106`）；`homeSelection_` 的完整接管说明在 109 行隐藏、160–161 行仍在计算。侧栏与标题区无核心/代理/TUN 状态。

方案：侧栏底部常驻状态条（核心、代理归属、TUN、当前订阅），点击回状态页；首页只留一条结论句。

### N5 首屏不给下一步【重排】

已选订阅但内核未就绪时只有「内核已停止」与「启动内核」（`MainWindow.cpp:332-333`）；失败原因只进状态栏（`MainWindow.cpp:349`）；六卡中四卡又被 `homeSummaryMask` 隐藏（`MainWindowHome.cpp:66,96-98`）。

方案：状态卡直接给出「流量路径 + 下一步动作 + 失败原因/日志入口」；删除六卡网格。

### N6 高频任务被入口分裂【重排】

节点选择有首页快捷、节点页、托盘组菜单、最近选择四个入口；「使用并启动/应用更新」在订阅页与节点页各有一套；`coreAction_` 在四种文案间切换（`MainWindow.cpp:401`）。

方案：常驻状态条 + 唯一节点选择入口；统一 use/apply 组件与文案；core 按钮固定为启动/停止。

### N7 「前往首页」只导航不执行【微调】

节点/连接/规则运行/网站测试四处「下一步」都只 `setPage`（`MainWindowProxy.cpp:63`；`MainWindow.cpp:267,382`；`WebsiteTestsPage.cpp:44,112`）。

方案：按钮直接执行下一步，文案写动作而非目的地。

### N8 设置即时/应用边界不一致【重排】

`testUrl_` 在 `editingFinished` 提交（`MainWindowSettings.cpp:126`），但 `settingsDirty()` 只算重启类；未失焦就关窗会静默丢失。按钮名「应用网络设置」与实际范围不符。

方案：行内标注「即时生效/需重启」；testUrl 纳入草稿检测或在关窗前统一收集。

### N9 托盘单击绕过确认【微调】

托盘单击直接 `setSystemProxy`（`TrayController.cpp:52,57`），菜单项与首页却有外部代理确认（`TrayController.cpp:115-122`；`MainWindowHome.cpp:34-41`）。

### N10 状态栏被 lastError 占用【重排】

`updateState` 无条件用 `state.lastError` 覆盖状态栏（`MainWindow.cpp:349-351`）；「处理」菜单固定四项、仅在失败时出现。

方案：状态栏分「当前告警/最近操作」；「处理」改为按错误类型路由的具体动作。

### N11 页面级错误写进全局状态栏【微调】

策略组循环错误（`MainWindowProxy.cpp:207`）、连接详情创建规则的拒绝（`MainWindow.cpp:294-305`）都只写状态栏，而对话框可能遮住它。

### N12 意图与观测混写【微调】

TUN 文本用 `settings.tunEnabled`、开关用 `tunObserved`（`MainWindow.cpp:331,343`）；托盘在 `tunObserved` 时把整体标题改成「TUN 已接管」（`TrayController.cpp:28,40-42`）。

### N13 设置分类靠运行期重排【重排】

「网络与内核」含 TUN、端口、排除、日志、IPv6、延迟、测试地址、内核路径、内核更新，又通过 `takeAt` 把环境变量、Geo、当前配置、内核目录搬进来（`MainWindowSettings.cpp:99-102`），109 行还有 catch-all。

方案：固定分类，行在最终父容器直接构建；Geo/备份/存储移出。

### N14 长流程任务以设置行存在【重排】

本地备份、恢复、自动备份、WebDAV、Geo、目录共七个入口分散在两类设置行里。

方案：独立「维护」分类；继续增长时升级为侧栏页面（新视图 ID，不改 Controller/schema）。

### N15 工具窗口生命周期不一致【重排】

TUN/内核更新/本地与远程备份用 `findChild` 复用（`MainWindowSettings.cpp:67,77,97-98`）；DNS/节点编辑/快捷键等每次 `new`。

方案：统一 tool-window 注册表，按「工具+对象」键复用并 raise；破坏性流程用模态 sheet。

### N16 设置搜索名不副实【重排】

只索引 `settingRow` 与 7 条硬编码路由（`SettingsSearch.cpp:18-23`）；分类名取隐藏的 `sectionTitle`，中间分类标题「内核与本地代理」与可见页签「网络与内核」不符；96 条静默截断；跨页路由不定位。

### N17 节点页工具带堆叠【重排】

上下文行混订阅/使用/切换组/恢复自动/筛选/节点选项；底部再放七项（`MainWindowProxy.cpp:16-27,43-44,82-86`）；无 `last_group` 时静默选成员最多的组（`MainWindowProxy.cpp:146-152`）。

方案：三层（上下文/工具/提交）；无记忆时显示「选择策略组」空态。

### N18 订阅页动作与菜单边界模糊【重排】

「来源与更新…」同时在对象条与管理菜单（`MainWindowProfiles.cpp:48,77`）；`bottom` 布局创建后从未填充（`MainWindowProfiles.cpp:28,72`）。

## 视觉问题

### V1 双份调色板【重设 token】

`UiTheme.h:9-16` 的 inline 默认值与 `applyAppearance` 运行时值不同（Surface `#FFFFFF` vs 深色 `#2B2C30`；Brand `#0068D9` vs `#0062CC`），且 `PanelGap`/`PanelInset`/`PopupRadius` 在 `src/` 中零引用（`UiTheme.h:15-16`）。「唯一来源」承诺不成立。

### V2 高对比只加深边框【推翻】

`UiTheme.cpp:35` 仅 Border/Separator 变化。高对比下卡片边框从 1.34 跳到 5.94，选中行仍是 1.13，形成「重边框 + 轻状态」的反向层级。

### V3 状态阶梯挤在 1.05–1.35【重设 token】

浅色：Hover 对 Surface 1.18、Selection 1.13、Pressed 对 Control 1.15；深色 Selection 1.35。悬停几乎无反馈，选中与悬停难区分（1.05）。

方案：accent 叠色阶梯（hover 6%、pressed 12%、selection 14% + 2px accent 条）。

### V4 主按钮 hover 与 pressed 同色【推翻】

`UiTheme.cpp:136-137` 两处都写 `@PrimaryPressed`；danger 只有文字色（`UiTheme.cpp:133`）；`ProxySwitch` 无 hover/press 分档（`MainWindowParts.h:264-270`），knob 硬编码白色。

### V5 深色模式硬编码漏色【推翻】

`WidgetStyle.cpp:118` 下拉滚动条写死 `#d0d0d7`（深色弹层上对比 9.09，是近白亮条）；`WebsiteTestsPage.cpp:130`、`MainWindow.cpp:338` 内联 `setStyleSheet`；`NodePresentation.cpp:98-99` 徽标写死四色与浅底；服务图标主体 RGB(22,22,22)，深色下对比约 1.5:1。

### V6 状态仅靠颜色【微调】

状态点是同形状纯色圆（`MainWindow.cpp:338`；`WebsiteTestsPage.cpp:127`），与契约「配合文本、图标或状态点」不符。

方案：形状编码（成功实心圆/警告三角/失败方/进行中空心环）+ 描边。

### V7 字号阶断层【重设 token】

Windows 为 10/11/18/20，中间缺 13–15pt；`HomeMetricsView.cpp:30` 的 `setPointSize(SectionPoints)` 被 QSS 的 `@ValuePoints` 覆盖成死代码；`OutboundDialog.cpp:22` 硬编码 17pt 且不随比例缩放；`NodePresentation.cpp:100` 用 `setPixelSize(9)`。

方案：7 阶（caption 9 / footnote 10 / body 11 / label 11·500 / title3 13 / title2 15 / display 18）。

### V8 全局强制单字体族【微调】

`UiTheme.cpp:80` 对所有 QWidget 设 `font-family: @UiFamily`，而 `@UiFamily` 只取一个族名，覆盖了 `setFamilies` 回退链与 `setApplicationFallbackFontFamilies`。

### V9 无网格【重设 token】

间距出现 19 种组合，圆角 10 种（3–13px）；同一「面板」在首页/网站/设置/日志四套内边距。

### V10 设置页「单列」名不副实【推翻】

`MainWindowSettings.cpp:111` 声明 `singleColumn`，但 79 行的「备份与工具」并排两列，与 ADR 0042 冲突。

### V11 四套焦点环【推翻】

QSS 描边并改 padding 防跳（`UiTheme.cpp:129,132,178,181,187`）；节点卡片虚线（`MainWindowParts.h:228`）；开关方角矩形（`MainWindowParts.h:270`）；菜单抑制 `PE_FrameFocusRect`（`WidgetStyle.cpp:170`）。

方案：统一 `FocusRing` 绘制器（外围 2px accent + 1px 间隙，不参与布局）。

### V12 按钮层级实现分裂【推翻】

对话框默认按钮由 `InputSource` 提升为 `visualPrimary`（`WidgetStyle.cpp:37-43`），主窗口靠手写 `primary` property；行内按钮强制 `quiet`（`MainWindowParts.h:163`）；`ChevronToolButton` disabled 用 `Muted` 而非 `Disabled`；chevron 有五处实现、三种描边、三种位置。

### V13 输入边界与开关轨道不足 3:1【重设 token】

输入框边框对 Surface 1.34；开关轨道 1.87，白 knob 对轨道 1.87。

### V14 表格/列表选择四种表达【重设 token】

表格淡底（`UiTheme.cpp:147,149` 还有重复规则）、卡片淡底+蓝边、菜单淡底、下拉 hover=selected（`WidgetStyle.cpp:129`）。

### V15 深色页签反向抬升【重设 token】

`UiTheme.cpp:159-161` 深色选中 `@Surface` 比底座 `@Control` 更暗，与 macOS 分段控件方向相反。

### V16 图标集混用【推翻】

`UiIcons.cpp:21` 的 `Play` 是实心，其余描边 1.7；`UiIcons` 无 Selected 模式，导航另用 `navigationIcon`；服务图标 alpha bbox 从 96×96 到 96×44 不等，光学大小不一。

### V17 120% 文字只放大字号【推翻】

`UiTheme.cpp:221` 的正则只改 `font-size`；开关轨道 20px < 12pt 行高 21.3px；网站卡片最小高 108 面对 24pt 值只剩 4px；`setPixelSize(9)` 与 `setPointSize(17)` 完全无视缩放。

### V18 说明文只对两个字面量名字显示【推翻】

`MainWindowParts.h:159` 的 `explain` 只匹配「启动时恢复代理开关」与「关闭窗口时释放界面」，其余说明全塞进 tooltip。

## 建议的 token 表

### 颜色

三套平行：浅色 / 深色 / 高对比。语义角色名沿用现有代码，便于替换。对比值为静态计算，深色高对比列未逐项列出。

| 角色 | 浅色 | 深色 | 高对比-浅 | 高对比-深 |
|---|---|---|---|---|
| `canvas` | `#F2F2F7` | `#1C1C1E` | `#FFFFFF` | `#000000` |
| `surface` | `#FFFFFF` | `#2C2C2E` | `#FFFFFF` | `#000000` |
| `surfaceRaised` | `#FFFFFF` | `#36363B` | `#FFFFFF` | `#111111` |
| `sidebar` | `#E5E5EA` | `#26262A` | `#EDEDED` | `#141414` |
| `controlFill` | `#EFEFF3` | `#38383C` | `#E8E8E8` | `#2E2E2E` |
| `hoverFill` | `#E6E6EA` | `#3E3E43` | `#E0E0E0` | `#292929` |
| `pressedFill` | `#D5D5DC` | `#4A4A50` | `#C9C9C9` | `#474747` |
| `selectionFill` | `#D8E6F7` | `#263C54` | `#C7DBFF` | `#1B3A66` |
| `selectionBar` | `#0057B8` | `#7DB4FF` | `#000000` | `#FFFFFF` |
| `border` | `#CFCFD6` | `#48484D` | `#1D1D1F` | `#FFFFFF` |
| `borderStrong` | `#84848C` | `#82828C` | `#1D1D1F` | `#FFFFFF` |
| `separator` | `#E4E4E9` | `#3A3A3E` | `#5F5F66` | `#B0B0B8` |
| `textPrimary` | `#1D1D1F` | `#F2F2F7` | `#000000` | `#FFFFFF` |
| `textSecondary` | `#58585E` | `#B9B9C0` | `#2C2C2E` | `#D6D6DB` |
| `textTertiary` | `#5E5E66` | `#9C9CA4` | `#3A3A3E` | `#C0C0C6` |
| `textPlaceholder` | `#76767E` | `#8E8E96` | `#4A4A4F` | `#A8A8B0` |
| `textDisabled` | `#A6A6AD` | `#6E6E76` | `#5F5F66` | `#9A9AA0` |
| `accent` | `#0B63CE` | `#0E70DC` | `#003FA3` | `#8CC2FF` |
| `accentHover` | `#0A57B8` | `#1274E0` | `#00327A` | `#A8D0FF` |
| `accentPressed` | `#08479A` | `#08509E` | `#002B73` | `#6FA8E0` |
| `accentText` | `#0057B8` | `#7DB4FF` | `#003FA3` | `#8CC2FF` |
| `onAccent` | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` | `#000000` |
| `focusRing` | `#0B63CE` | `#0A84FF` | `#000000` | `#FFD60A` |
| `success` | `#0C6B45` | `#6ADBA1` | `#0C5132` | `#8FF0BE` |
| `warning` | `#8A5400` | `#F0C070` | `#6B3D00` | `#FFD27F` |
| `danger` | `#B3261E` | `#FF8E86` | `#A30000` | `#FF8A80` |
| `switchTrackOff` | `#8F8F97` | `#7A7A83` | `#5F5F66` | `#A8A8B0` |
| `scrollHandle` | `#B0B0B8` | `#7A7A82` | `#5F5F66` | `#B0B0B8` |
| `badgeGroup`/`badgeReject`/`badgeDirect`/`badgeGeneric` | `#5E4C8A`/`#8A3B3B`/`#2C6B4F`/`#3F6078`（实心 + 白字） | 同左 | 加 1px 白描边 | 加 1px 白描边 |

### 字号（pt，Windows 基准；macOS 各 +2）

| role | 字号 | 字重 | 用途 |
|---|---|---|---|
| `caption` | 9 | 400/500 | 版本号、元信息、徽标 |
| `footnote` | 10 | 400 | 行内说明、次要列 |
| `body` | 11 | 400 | 正文、列表、表头 |
| `label` | 11 | 500 | 设置行标题 |
| `title3` | 13 | 600 | 分组标题、网站延迟 |
| `title2` | 15 | 600 | 页面标题、卡片主值、出口 IP |
| `display` | 18 | 600 | 每页最多一处大数字 |

### 间距与圆角

`space-1..8 = 4/8/12/16/20/24/32/48`；`radius-sm 6 / md 8 / lg 12 / pill 999`。容器内边距 `16,12`；行内 gap `8`；组间 `16`；页面纵向间距 data `12` / form `16`。

### 控件几何（随 textScale 线性缩放）

| token | 100% | 120% |
|---|---|---|
| `control-sm` | 24 | 28 |
| `control-md` | 30 | 36 |
| `control-lg` | 36 | 44 |
| `row-data` | 32 | 38 |
| `row-form` | 48 | 56 |
| `icon-hit` | 28 | 34 |
| `switch-track` | 42×24 | 48×28 |
| `statusbar` | 34 | 40 |

## 目标信息架构

推荐单层分组侧栏，每页一次点击可达，取消分区页签。

| 分组 | 页面 | 任务归属 |
|---|---|---|
| 运行 | 状态（首页/状态合并） | 核心启停与就绪、代理/TUN 归属、当前订阅与成员、流量、失败原因 |
| 运行 | 节点 | 组浏览、成员选择、延迟、筛选、显示偏好 |
| 配置 | 订阅 | 来源、版本/候选、节点/组、DNS、上游链、Provider |
| 配置 | 规则 | 优先/追加/屏蔽规则、规则源、内核实际规则 |
| 诊断 | 网站测试 | 目标响应、出口/服务诊断 |
| 诊断 | 连接 | 活动/已结束会话、断开 |
| 诊断 | 日志 | 阅读、脱敏导出 |
| 设置与维护 | 设置 | 偏好、网络、内核/TUN（固定三类） |
| 设置与维护 | 维护 | 备份/恢复/WebDAV、Geo、存储、诊断导出 |
| 全局 | 状态条 | 核心/代理/TUN/当前订阅；点击回状态页 |

## 实施阶段

| 阶段 | 内容 | 风险 | 是否需 ADR |
|---|---|---|---|
| S1 地基 | token 重设（三态颜色、字号、间距、圆角）、删硬编码色、统一焦点环、补 pressed 态 | 低，不改结构 | 否 |
| S2 结构 | 删标题槽、首页六卡、设置重排；导航模型单一化 | 高 | 是（0045） |
| S3 一致性 | 控件状态补全、tool-window 注册表、托盘去重、工具带分层 | 中 | 否 |
| S4 收尾 | 图标归一化、120% 几何、服务图标深色变体、搜索索引扩展 | 中 | 否 |

## 验收边界

每阶段按 [交互契约](ui-interactions.md) 的完整任务链跑隔离 UI 测试：空/加载/成功/失败/取消、候选/运行版本、窄窗 840×560、90–120% 文字、浅/深/高对比、鼠标/键盘、轻量重建。默认测试不修改系统代理；`--system-proxy-test` 仅限受控测试 VM。

本方案为只读代码评审，**未运行应用、未截图、未做系统代理或 TUN 实测**。对比度为静态计算，几何判断需人工在 Windows 实机复核。未执行的真实系统测试保持未验。
