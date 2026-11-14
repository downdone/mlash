# R2 施工记录：标题槽统一与图标收敛

> 归属：[界面分层设计](ui-layered-design.md#实施切片) 的 R2 片。本文件记录**实施前**的采用方案与验收点（[工程方法](engineering.md#ui-先审视先设计后修改)），实施后在同一文件追记实际结果。ADR 口径见 [ADR 0047](adr/0047-website-tests-and-title-removal.md)。

## 1. 要解决的问题

现状（R1 后）：

- 每个页面自己创建 `pageTitle` QLabel，`syncSectionNavigation` 用 `replaceWidget` 把标题换成 `sectionTabs_`，再把标题 `hide()` 但保留指针（`sectionTitle_`）。设置页不走 `replaceWidget`，而是「页签宿主」被复用为设置分类页签——同一处位置由两个不同的 `QTabBar` 轮流填充。
- 因此存在**三份**摆放逻辑：各页 `pageTitle`、共享 `sectionTabs_`、设置 `settingsCategories_`。任何一页改动首行都会与另两处错位。
- 窗口标题恒为「Mlash」，任务栏与 Alt+Tab 无法区分当前视图。
- `navigationIcon(int kind)` 保留旧八页的 8 种 `kind` 魔法值，实际只有 4 个分区在用它。

## 2. 采用方案

### 2.1 一个页签宿主

保留**一个** `sectionTabs_`（QTabBar，objectName 仍为 `sectionNavigation`，以免破坏既有测试与样式），把它从「替换某页标题」改为「内容区固定第一行的独立控件」：

- 它不再由 `replaceWidget` 插入页面布局，而是由 `MainWindow` 的中央布局在 `pages_` **之上**固定放置，`content->addWidget(sectionTabs_)` 后接 `content->addWidget(pages_, 1)`。
- 页面自身的 `pageTitle` 全部删除；`sectionTitle_` 成员删除；`replaceWidget` 调用删除。
- 设置页的三分类页签（`settingsCategories_`）**保持在设置页内容区内**（它承载的是分类视图名，位置由设置页骨架决定），`sectionTabs_` 在设置分区隐藏——与现状一致。两者不再互相替换，因为已经不存在「标题位」这个共享槽。
- 首页没有页签：`sectionTabs_` 隐藏（ADR 0048）。

页签行的可见性规则（单一函数决定）：`section == 6 || section == 首页 → 隐藏；否则显示`。页签内容仍由「分区 → 视图列表」表生成，与 R1 的 `Route` 一致；首页不生成任何页签。

### 2.2 窗口标题

`setWindowTitle("Mlash")` 改为随路由更新：`Mlash · <当前视图名>`。

- 视图名取自路由表（首页 → 首页；订阅 → 订阅；规则 → 规则；连接 → 连接；日志 → 日志；网站测试 → 网站测试；设置 → 设置 › <分类名>）。
- 设置页的分类变化（`selectSettingsCategory`）也更新标题。
- 更新点集中在 `MainWindow::navigate` 与 `selectSettingsCategory`，不散落到各页。
- 标题只用文本，不进入状态句，不报告运行状态（避免与 ADR 0048 的「状态只呈现实际观测」混淆）。

### 2.3 图标收敛

`navigationIcon(int kind)` 改为 `navigationIcon(NavKind)`，`enum class NavKind { Home, Config, Data, Settings }`，映射固定为
首页→`Glyph::Home`、配置→`Glyph::List`、数据→`Glyph::Chart`、设置→`Glyph::Settings`。

- 首页 6 张摘要卡片中的 4 张（规则、连接、日志、设置）称 `navigationIcon(i+1)`，参数恰好与旧八页 ID 相同（3–6），是导航图标复用；订阅卡与节点卡不画图标。收敛后按**去向分区**取图标：规则→Config、连接→Data、日志→Data、设置→Settings。卡片图标从此表达「去哪个分区」，与侧栏一致。
- `MainWindow.cpp` 的侧栏初始化同样改用 `NavKind`。
- 不新增绘制规格：内部继续调用现有 `UiIcons::icon(Glyph,…)`，颜色与尺寸沿用当前实现。

### 2.4 不做的事

- 不合并 `UiIcons` 与 `navigationIcon` 的绘制器（那是 6.2c 的整体目标，需要新的 Glyph 与描边参数，归 R9 的视觉片）。
- 不改页签的样式、间距、字重（R9）。
- 不改任何页面内部骨架顺序（R3 起逐页调整）。

## 3. 状态转换与边界

| 场景 | 期望 |
|---|---|
| 首页（0 / 2） | 无页签行；窗口标题「Mlash · 首页」 |
| 分区切换（侧栏或页签） | 页签行显示该分区视图，选中项与路由一致 |
| 设置页 | `sectionTabs_` 隐藏；设置分类页签在内容区第一行；标题「Mlash · 设置 › <分类>」 |
| 设置搜索跳转到其它页 | 标题与页签同时更新（走同一 `navigate`） |
| 轻量重建 | `GuiHost` 保存 `Route`（R1 已做），重建后标题与页签随之恢复 |
| 窄窗 840×560 | 页签行不换行、不与页面动作重叠；标题栏由系统绘制，不参与布局 |
| 文字 90–120% | 页签行高度随字号增长，内容区不裁切 |
| 隐藏窗口 | 标题保持最后视图名，隐藏不重置 |

## 4. 验收点（可观察）

1. `findChildren("pageTitle")` 在任何页面为空。
2. 任一视图内 `sectionNavigation` 的位置在所有页面相同（同一 y 与左缘）。
3. `windowTitle()` 随 `navigate` 变化，覆盖 7 个视图 + 设置 3 分类。
4. `navigationIcon` 只有 4 个取值；侧栏 4 项与首页 4 张导航卡都不再传 3–6 的旧 ID。
5. 既有测试全绿（含 `taskEntryPointsAndSettingsStayCoherent`、`appearance_tests` 的全部页面遍历）。
6. 截图对比：首页、订阅、设置三页的第一屏顶部结构一致（无标题行、页签行同高）。

## 5. 实际结果

**已实施并自动测试（2026-10-29，分支 `feature/r1-route` 之后）。**

实现：

- `MainWindow` 中央布局固定为 `sectionTabs_` 在上、`pages_` 在下；`replaceWidget`、`sectionTitle_` 成员与各页 `pageTitle` 全部删除（`MainWindowHome/Proxy/Profiles`、`diagnosticsPage`、`WebsiteTestsPage`，以及 `heading()` 辅助函数本身）。
- `QLabel#pageTitle` 样式改名 `#dialogTitle`，`UiTheme::PagePoints` 改名 `DialogPoints`；仅 `ImportDialog` 的对话框标题使用它——页级标题已不存在，token 不随页面骨架回归。
- `syncSectionNavigation` 的可见性规则改为 `section != 6 && section != 0`，原「状态/节点」两个首页页签删除，网站测试归入数据分区（ADR 0047）。侧栏点「首页」恒回入口 0。
- 窗口标题由 `updateWindowTitle()` 统一生成（`routeTitle(route, category)`，例：`Mlash · 设置 › 备份与工具`），在 `navigate` 与 `selectSettingsCategory` 两处触发。
- `navigationIcon(int)` → `navigationIcon(NavKind)`，`navKindOfView(int)` 把视图 ID 映射到 4 个分区；侧栏 4 项与首页 4 张导航卡都走它。

复核：

- 新增 `mlash_ui_tests titleSlotIsSingleAndWindowTitleFollowsRoute`：`findChildren("pageTitle")` 为空；首页无页签；窗口标题随 8 个视图与 3 个设置分类变化；订阅/规则/连接/日志/网站测试五个视图的页签宿主几何完全一致；数据分区页签为 3 个。
- 修正既有断言 `AppearanceTests taskEntryPointsAndSettingsStayCoherent`：首页分区页签回落到入口 0（与 ADR 0048 一致），并断页面栈为索引 0。
- 完整 `ctest`：34 项中 33 通过、0 失败、`system_test_guard` 跳过，用时 109 秒。
- 截图复核（`mlash_appearance_tests allPagesAndDialogsAcrossAppearances`，light/dark × 三个尺寸）：订阅、规则、连接、日志、网站测试的页签行位置一致；首页从控制区首行开始、无页签；设置页分类页签仍在内容区首行。

**未验**：真实 Windows 实机的 Alt+Tab/任务栏标题、人工读屏、页签行的键盘焦点顺序（R9 焦点环统一时复核）。

