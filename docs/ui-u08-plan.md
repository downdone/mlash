# U08 工作计划：对齐 Shadowrocket 风格原型

> 归属：[界面分层设计](ui-layered-design.md) 的实施切片之后。本文记录用户 2026-10-30 追加的第三份参照原型，以及按它修正实现的分片。
> 参照物：`prototypes/Mlash_Shadowrocket_Style_Interactive_Prototype_from_chatgpt.html`（下称「GPT 原型」）。
> 既有 `prototypes/ui-prototype.html`（U06 静态原型）继续有效；两者冲突时以本文「取舍」一节为准。

## 1. 为什么要再做一轮

U07 结束时九片都已落地、回归全绿，但用户随后提供了这份更接近目标形态的原型：四分区（首页 / 配置 / 数据 / 设置），首页只有**一张列表卡片的三行控制区 + 节点行列表**，没有仪表盘卡片、没有独立的订阅上下文面板。与当前实现逐条对照后，首页的信息架构存在实质差异，不是配色或间距问题。

同时本轮复查发现一处**声明与实现不符**：`docs/ui-layered-design.md:389` 「首页六卡删除，不再保留可配置卡片」写成已完成的裁决，`MainWindowHome.cpp:71-91` 实际仍完整构建六张 `dashboardCard`，`homeSummaryMask` 偏好与 `HomeSummaryDialog` 仍然生效。文档声称删除、代码没有删除。

## 2. 原型与实现的差异（逐页）

### 2.1 首页（差异最大）

| 项 | GPT 原型 | 当前实现 |
|---|---|---|
| 控制区形态 | 一张 `list-card`，三行 `action-row`：当前节点（图标 + 名称 + 订阅/选择方式 + 系统代理开关）、路由方式（值 + ›）、连通性测试（说明 + 「开始测试」按钮） | `overview` 面板：状态点 + 大标题「内核运行中」+ TUN 设置按钮 + 「开启代理」主按钮，下面连通性行与两个开关 |
| 主控件 | **系统代理开关**（行尾 switch），文案「系统代理开启 / 系统代理关闭」 | 「开启代理」按钮（代理总开关，ADR 0048） |
| 当前节点 | 行标题即当前节点名，副标题给「订阅 A · 手动选择」 | 当前成员在下方「策略组」卡片里，控制区只有「内核运行中」这类状态标题 |
| 路由方式 | 独立一行，右侧显示「规则模式」+ 箭头，点击展开 | 分段控件（规则/全局/直连）浮在控制区之上，无行标题 |
| 连通性 | 独立一行，右侧「开始测试」按钮 | 一行「连通性 · 结果 · 测试 · 详情」 |
| 节点区 | 标题行「节点选择 N」+ 右侧订阅下拉、搜索框、测速图标、排序图标；下面是**扁平行列表**（区域字母 + 名称 + 协议副标题 + 勾选 + 延迟 + 信息按钮），分「手动选择 / 自动策略组」两组 | 订阅上下文面板（订阅下拉 + 使用并启动 + 切换策略组 + 恢复自动 + 筛选 + 节点选项）→ 表格（成员名称/类型/延迟/状态四列）或卡片视图，组卡片另有一个 110px 高的选择器 |
| 指标 | 列表底部一行 `inline-stat`：内核状态 · 模式 · ↓速率 · ↑速率 · 「数据详情」 | 首页指标卡 `homeMetrics`（曲线 + 出口摘要），另有六张 `dashboardCard` |
| 更多运行信息 | 无独立抽屉（数据在「数据」页） | 「更多运行信息」抽屉 + 摘要卡片行 |
| 六张导航卡 | 无 | 六张 `dashboardCard`（订阅/策略组/规则/连接/日志/设置）|

### 2.2 其它页

配置页 = 远程订阅组 + 本地配置组，每行右侧用量条与详情箭头；数据页 = 四个页签（流量/连接/测试/日志）+ 「复制诊断摘要」；设置页 = 三个分组（常规 / 代理与网络 / 内核与维护）的单列行，行尾为开关、下拉或「值 + 」。

现有实现的订阅/规则/连接/日志/网站测试/设置六页在结构与这些原型基本对应（U06/U07 已对齐），本轮不重做；差异集中在首页。

### 2.3 违规项（不只是「没跟上原型」，而是违反已接受的裁决）

审计逐行核对后发现三处实现与**自家设计文档**冲突，与原型无关：

| 项 | 裁决原文 | 实际代码 |
|---|---|---|
| 首页 TUN 开关 | `ui-layered-design.md:169`「**TUN 开关不放首页**……它归「设置 › 网络与内核」。首页状态句只报告 TUN 的观测结果。」 | `MainWindowHome.cpp:30,64,138` 构建 `ProxySwitch("TUN")` 并随活跃订阅显示 |
| 首页「TUN 设置…」按钮 | 同上 | `MainWindowHome.cpp:28` |
| 抽屉缺内核启停 | `ui-layered-design.md:172` 裁决 5 把「启动/停止内核」列为抽屉固定项 | `extras` 里只有探测行与摘要卡，**全应用首页无内核启停控件** |
| 抽屉被隐藏 | 同条「必须可见」 | `MainWindowHome.cpp:130` `more->setVisible(hasActive)` |
| 六卡 | `ui-layered-design.md:389`「首页六卡删除」 | 六卡完整存在（见 2.1） |
| 控制区预算 | ≤6 个控件（`:195`） | 常态 6、出现「详情」后 7 |

这六项在 H1–H4 里一并修掉；它们不是「原型偏好」，是文档早已承诺、代码没做到。

## 3. 取舍（与已接受决策冲突时）

> **2026-11-02 用户指令覆盖本节。** 用户评价「现在非常难用」并要求「原型怎么做你怎么做」。
> 自本日起，GPT 原型是首页与四页结构的**直接规格**，不再保留下列「保持现状」：原型的交互
> 形态（弹出菜单、右侧抽屉、扁平行列表、顶栏、页签）都要照做；与 `ui-layered-design.md`、
> `docs/README.md`「不是验收依据」注记冲突时，以本节的第二版（见 §7）为准，并在落地后回写
> 设计文档。下述旧取舍只作历史记录，不再生效。

原型不是验收依据（[文档导航](README.md) 已注明）。以下三处**保持现状**，不按 GPT 原型改：

1. **TUN 开关不进首页。** 原型首页只有系统代理开关，与 `ui-layered-design.md:169`「TUN 开关不放首页」一致，无需改动。
2. **代理总开关语义保留。** ADR 0048 明确首页主控件是「让接管生效/失效」，而不是单纯切系统代理。原型行尾的 switch 文案「系统代理开启/关闭」与 ADR 0048 的六态状态句（内核运行中 · 未接管 / 已关闭 / 失败：原因）并不冲突——开关仍是同一个 `setProxyEnabled`，只是**行内呈现**从大按钮换成行尾开关 + 状态句。这一点按原型改。
3. **订阅上下文保留在节点区。** `ui-layered-design.md:598` 规定首页上下文条为「订阅选择（节点来源）」，原型把它压缩成标题行右侧的下拉。两者是同一事实的两种摆法；按原型改成标题行右侧下拉，但保留「使用并启动」这个显式动作（ADR 0048 的「使用并启动」命令不能丢）。
4. **删除首页 TUN 开关与「TUN 设置…」按钮。** 这不是取舍，是回归文档：`ui-layered-design.md:169` 早已裁决不放首页。TUN 的入口留在「设置 › 网络与内核」；首页状态句继续报告 TUN 观测结果。`TunTests.cpp:174` 里「840×560 下 TUN 开关在窗内」的断言随之删除，改为断言 TUN 不出现在首页。

## 4. 分片

依赖：`H1 → H2 → H3 → H4`。V2/V3/V4 已完成，不重开。

### H1 · 首页控制区改为三行卡片

- 用一张 `QFrame#homeControlCard` 卡片替换 `overview` 面板，内部三行 `homeControlRow`：
  1. 当前节点行：图标 + 当前节点名（大）+ 「订阅 · 选择方式」（次）+ 行尾系统代理开关；开关文案随状态变化。
  2. 路由方式行：标题「路由方式」+ 副标题「控制代理请求的分流行为」+ 行尾当前模式文字与 ›；点击展开现有分段控件（保留 `proxyModes_` 三个按钮，改为行内下拉或弹出层）。
  3. 连通性行：标题「连通性测试」+ 副标题给上次结果或「尚未测试 · 测试目标：<url>」+ 行尾「开始测试/取消」按钮。
- `proxyMasterSwitch_` 从控制区移到「更多运行信息」抽屉，**不删除**：ADR 0048 的六态与 `setProxyEnabled` 语义不变，只是首屏主入口变成行尾开关。
- 状态句（内核 / 接管 / 目标可达）作为第一行的第三行次要文本，沿用 `statusSentence()` 的来源，不新造文案。
- **删除首页 TUN 开关与「TUN 设置…」按钮**（回归 `ui-layered-design.md:169`）；TUN 观测仍在状态句里报告，入口在设置。
- **删除首页「系统代理」开关**：行尾开关就是它，同一个 ``setSystemProxy`` 命令只保留一个入口，避免同屏两个含义重叠的开关。
- 失败原因就地显示在第一行的状态句里（六态的「失败：<原因>」），不再另起 `homeControlHint_` 一行；`homeControlHint_` 只保留给「请先激活订阅」这类面板隐藏时的结论。

验收：840×560 下三行完整可见、控制区之后仍可见 ≥3 行节点（沿用 R3 的既有断言）；开关六态各自可复现；`setProxyEnabled` 调用次数与幂等性不变；首页 `findChild("homeTunSwitch")`、`findChild("homeTunSettings")` 均为空；控制区交互控件 ≤6（分段 1 + 系统代理开关 1 + 连通性测试 1 + 路由方式行 1 = 4）。

### H2 · 删除六张导航卡

- 删除 `MainWindowHome.cpp` 六卡循环与 `dashboard` / `homeSummaryRow_` 的构建；删除 `HomeSummaryDialog`、`settings.homeSummaryMask`、`AppController::updateHomeSummary` 及其持久化字段。
- 删除 `UiTheme` 中 `#dashboardCard` 规则与 `navigationIcon` 的 `dashboardIcon` 分支。
- **澄清文档**：`ui-layered-design.md:389` 的裁决本来就要求删除，本次让它与代码一致。
- 迁移数据：`home_summary_mask` 在新版本读入时忽略（不报错），保存时不再写出。

验收：首屏在 940×700 下控制区 + 节点列表连续、无大段空白；`findChildren("dashboardCard")` 为空；旧状态文件（含 `home_summary_mask`）可正常加载。

### H3 · 节点区改为原型行列表

原型（`prototype` 第 78–81、129–131 行）把节点区分成三段：标题行、一张 `list-card` 行列表、一行 `inline-stat`。行列表内部再按「手动选择 / 自动策略组」两个可折叠分组排列，组头右侧给计数与选路说明。落地为：

- **标题行**（`QWidget#nodeHeading`）：「节点选择」+ `secondary` 计数；右侧 `group-tools`：订阅选择 `CompactComboBox#nodeProfile`（原 `profilesCombo_` 移入标题行）、`QLineEdit#nodeSearch`、测速 `QToolButton#nodeTestButton`（菜单里保留「测本组 / 测选中」）、排序/更多 `QToolButton#nodeToolsButton`（溢出菜单）。
- **行列表**：`QListView#nodeList`，`setUniformItemSizes(false)`（组头 34px、成员行 46px 两种高度）+ `NodeRowDelegate`；`QStackedWidget#nodeViews` 改为 `nodeList` 与 `nodeCards_` 两页，表格页删除。
- **两个分组**：`手动选择`（组类型 `Selector`）与 `自动策略组`（`URLTest`/`Fallback`）。组头 `NodeGroupHeader` 用「›/⌄ + 标题 + 计数 + 选路说明」表示，点击折叠；自动组折叠状态默认展开，但组内行不提供切换入口。
- **成员行**：区域字母徽标、名称、协议/子组副标题、当前成员勾选、右对齐延迟（三档色）、行尾信息按钮。
- 删除：`proxyContext` 面板、`proxyGroups_` 组卡片选择器与 `GroupCardDelegate`、底部状态条（`currentMember_` / `locateAction_` / `enterGroup_` / `nodeCount_` / `testProgress_` / `cancelTestButton_`）、`nodeNextStep`、`nodes_`(QTableView) 与 4 列 `ProxyTableModel` 展示。
- 「恢复自动 / 最近选择 / 节点选项 / 筛选」与视图偏好（行列表 ⇄ 卡片）收进 `nodeToolsButton` 溢出菜单（`ui-layered-design.md:622` 的要求）。
- **修缺陷**：`chooseSelectedNode()` 现在不校验组类型，回车会把自动组成员固化成手动选择。改为与 `selectNodeInHome()` 同一条守卫（只有 `Selector` 提交，自动组只移动高亮并给出说明）。

验收：手动组单击即生效、失败回滚；自动组无切换入口（含键盘 Enter 路径）；840×560 下可见 ≥3 行；`findChild("nodesTable")` 为空。

### H4 · 指标与「更多运行信息」收敛

- 首页底部只留原型那行 `inline-stat`（内核状态 · 模式 · 上下行 · 「数据详情」→ 连接页）。
- 「更多运行信息」抽屉保留并**补齐裁决 5 的固定项**：运行版本（订阅名）、内核状态与版本、接管明细（系统代理 + TUN）、会话速率与峰值、最近启动、**内核单独启停按钮**；代理总开关也从首屏移入这里。
- 抽屉在无订阅时仍可见（裁决 5 的「必须可见」）；无订阅时其内容退化为「尚未启动」等只读说明。

验收：首屏不再出现曲线图；抽屉内动作全部可达；首页存在唯一的「停止内核/启动内核」控件；无订阅时 `homeDetailsDisclosure` 仍可见。

## 5. 约束

- 每片先在本文件补设计与验收点，再改代码（AGENTS.md 的「UI 先审视、先设计、后修改」）。
- 每片结束跑 `scripts/build.ps1`、`scripts/test.ps1` 与 `check_ui_tokens.py`。
- 不新增 Controller 命令；H2 是**删除**偏好字段，需同步 `docs/api-contracts.md` 与备份 schema 说明。
- 删除首页 TUN 开关要同步改 `tests/TunTests.cpp:174` 的断言（原断言要求它在 840×560 窗内可见）。
- 不宣称 Windows 10、干净机器、72 小时稳定性或性能预算；原型不是验收依据。

## 6. 状态

| 片 | 状态 | 证据 |
|---|---|---|
| H1 控制区三行卡片 | 已实施、自动测试 | `homeControlCard` / `homeControlRow`（首行 `first="true"` 去分隔线）/ `homeControlTitle`/`Value`/`Chevron` 已在 `UiTheme` 建规则；首页 TUN 开关与「TUN 设置…」已删，`findChild("homeTunSwitch")`、`findChild("homeTunSettings")` 为空由 `ui_refinement_tests` 的 840×560 几何用例固定 |
| H2 删除六卡 | 已实施、自动测试 | `HomeSummaryDialog.{h,cpp}` 已删；`Settings::homeSummaryMask`、`AppController::updateHomeSummary`、`home_summary_mask` 持久化一并移除；旧状态文件读入时忽略该键（`ProfileStore`/`Storage` 注释）。`findChildren("dashboardCard")` 为空 |
| H3 节点行列表 | 已实施、自动测试 | `NodeRowModel`/`NodeRowDelegate`（组头 34px、成员行 46px）在 `MainWindowParts.h`；`QListView#nodeList` + `#nodeCards` 撑起 `QStackedWidget#nodeViews`，四列表格页删除。`chooseSelectedNode()` 缺陷已修：与单击共用 `commitMemberSelection()` 守卫，自动组不再被回车固化 |
| H4 指标收敛 | 已实施、自动测试 | 首屏只剩 `homeInlineStat`（内核 · 模式 · ↓/↑速率 · 连接数）；「更多运行信息」抽屉补齐运行版本、内核状态与版本、接管明细、会话速率、最近启动、内核单独启停与代理总开关；无订阅时 `homeDetailsDisclosure` 仍可见 |

**尚未完成**：真实 Windows 实机观感、读屏播报、高对比与 125%/150% 文字缩放下的人工复核，以及原型在浏览器中的逐屏人工比对。

### 6.1 实施中修掉的缺陷

| 缺陷 | 根因 | 处理 |
|---|---|---|
| 首页段错误 | `coreLabel_` 的构造在 H1 被删，`updateState()` 仍写 `coreLabel_->setText()` | 删写入与成员 |
| 首页段错误 | `cancelTestAction_` 漏建且无 `= nullptr` 初始化，实际值 `0xbaadf00d` | 恢复构建（测速菜单内，接 `cancelTests`）；所有 `QAction*` 成员默认 `nullptr` |
| 构造期段错误 | `updateActions()` 由 `updateState()` 在页面构造中途触发，大量页面指针为空 | 加「页面全部构造完成」前置守卫 + 逐条空指针保护 |
| 自动组可被回车固化 | `chooseSelectedNode()` 不校验组类型 | 与单击共用 `commitMemberSelection()` 守卫 |
| 行列表渲染 O(n²) | `NodeRowModel::data()` 每次按名字线性查源行 | `members_` 与源行同序，直接用 `row.member` 作源行号 |
| 标题行控件越界（x=-142） | `WrapLayout` 无 spacer/拉伸语义，只增长第一个 `Expanding` 项 | 删除 `addStretch`，改让搜索框承担拉伸 |

### 6.2 两个超时用例是既有问题，不是本轮回归

> **后续调整**：2026-11-02 用户同意把 `appearance_tests` 的限时放宽到 100s（见本节末尾）。下表保留当时的实测与判断，作为放宽的依据。

`core_update_tests`（60s 限）与 `appearance_tests`（45s 限）在本机超时。把本轮全部改动暂存后以同一工具链重建基线并复跑：

| 用例 | 基线（未改动） | 本轮改动后 | 限时 |
|---|---|---|---|
| `appearance_tests` | 57.3s 超时 | 49.4s 超时 | 45s |
| `core_update_tests` | 73.5s 超时 | 73.5s 超时 | 60s |

两者在本轮之前就已超限，本轮改动使 `appearance_tests` 略快；结论据此记为环境性性能问题，本轮不调限时。

另有一个**并行争用**而非回归的用例：`appearance_prefs_tests`（45s 限）在 `ctest -j4` 下超时，串行复跑 32.79s 通过、单进程直跑 35s 通过。它的耗时集中在两组 UI 批量截图用例，与本轮改动无关；记为并行度问题，同样不调限时。

第二版结束时以同样方式复核过一次：`core_update_tests` 仍超时（60.09s，限 60s）；`appearance_prefs_tests` 整轮 40.63s 通过、单进程直跑 40.7s。除 `core_update_tests` 外无新增失败。

**2026-11-02 调整（用户决定）**：`appearance_tests` 的限时从 45s 放宽到 **100s**。理由是 45s 从第一版起就低于实测耗时（基线 57.3s、本轮 49.4~64.4s），整轮里它必然撞限被杀，而被杀的是整个用例而不是某条断言——这掩盖了「11 条断言全绿」的事实。调整后完整整轮 **40 通过 / 1 失败（`core_update_tests`）/ 1 跳过**，`appearance_tests` 整轮 64.55s、单跑 64.39s 均通过。限时仍是上限、不是性能声明；`ui_refinement_tests`（非 Windows 分组）的 45s 未动。`core_update_tests` 的 60s **未改**：它超限的成因不同（本机下载/校验慢），需单独判断。
---

## 7. 第二版：按 GPT 原型逐项照做（2026-11-02 起）

### 7.1 触发与结论

用户对第一版结果的评价是「现在非常难用」，指令是「原型怎么做你怎么做」。第一版只搬了
**首页的静态结构**，交互形态仍是旧的：路由方式用行内展开的分段按钮、节点详情用模态对话框、
节点区多一条「使用并启动」、首页底部拖着「更多运行信息」抽屉与曲线图；顶栏、页签式数据页、
原型的三组设置与订阅行列表都没做。§3 的「取舍」据此作废，原型升为**直接规格**。

本版原则（不再逐条权衡）：

1. 原型的每个可交互元素在实现里都要有对应控件，且**交互形态一致**：行点击弹菜单就走菜单，
   详情走右侧抽屉，不做模态框；页内分类走页签，不做多页切换。
2. 原型的每一屏只保留原型有的东西。原型没有的（首页曲线、抽屉、六卡、卡片视图入口）从
   该屏移除；数据不删除，移到原型放置它的地方（流量在「数据 › 流量」）。
3. 深色配色直接取原型 token（§7.3）；浅色与高对比按同一明度关系推导，不复制原型的暗色值。
   **例外（K9 追记）**：原型 `--blue #2488f5` 作主按钮底色时白字只有 3.55:1，不满足正文对比度；
   深色 `Primary` 取同色相深一档 `#1968c2`（白字 5.53:1），`Brand` 保留原型值。原型是交互与
   分区的规格，不覆盖可达性下限。
4. 原型的「模拟」元素（`mock-pill` 提示条、模拟延迟、模拟日志）不搬入实现——产品有真实数据。
   但**真实数据为空时必须说明为什么**（空态文案沿用 R7）。

### 7.2 差异清单与对应切片

| # | 原型 | 当前实现 | 切片 |
|---|---|---|---|
| 1 | 顶栏 72px：页面标题 + 副标题，右侧状态胶囊 + 主题按钮 + 更多按钮 | 无顶栏；页签宿主藏在内容区首行；窗口标题间接反映页面 | K1 |
| 2 | 侧栏：品牌 + 「NETWORK CLIENT」、43px 药丸导航、底部状态 + 两行脚注 | 品牌 + 4 项列表、状态按钮、版本号 | K1 |
| 3 | 路由方式 = 一行，点击弹**浮层菜单**（规则/全局/直连，勾选当前项） | 行内展开三段按钮，再点一次才收起 | K2 |
| 4 | 节点行尾 ⓘ → **右侧抽屉** 410px，含 5 行键值 + 动作 | 模态 `QDialog#nodeDetailsDialog` | K3 |
| 5 | 节点标题行：计数 / 订阅 chip / 搜索 / 测速图标钮 / 排序图标钮；无第二行 | 标题行 WrapLayout + 独立「使用并启动」行；一个「更多」大菜单 | K4 |
| 6 | 首页 = 控制卡 3 行 → 节点标题 → 节点列表 → inline-stat → 提示 | 多出「更多运行信息」抽屉（内核启停、总开关、指标、出口摘要、曲线）、空态卡 | K5 |
| 7 | 数据页 = 一个页面，四个页签（流量/连接/测试/日志）+ 复制诊断摘要 | 连接/日志/网站测试是三个并列页面，无流量页 | K6 |
| 8 | 配置页 = 标题行（添加订阅/导入/更新全部）+ 远程订阅列表 + 本地配置列表，76px 行 | 一个 5 列表格 + 对象操作条 | K7 |
| 9 | 设置页 = 3 组、52px 行、行尾「值 + ›」；无分类页签 | 3 个分类页签 + 3 个面板 + 子标题分组 | K8 |
| 10 | 侧栏 4 项 =首页/配置/数据/设置，无页签宿主 | 与原型一致，但数据页三分页使「数据」下有三个平级视图 | K6 |

### 7.3 视觉取值（新增 token）

暗色直接取原型：`Window=#08090b`、`Sidebar/Card=#1b1c1f`、`CardSoft/Input=#242529`、`Hover=#292b30`、
`Selected=#173b60`、`Border=#2c2e34`、`Separator=#2b2d31`、`Ink=#f3f4f5`、`Secondary=#a2a5ad`、
`Muted=#747982`、`Primary=#2488f5`、成功/警告/错误 `#40bd88 / #f7b255 / #f27070`。
浅色按同一层级关系取：`Window=#F7F8FA`、`Sidebar=#EEF0F3`、`Surface=#FFFFFF`、`Hover=#F2F5FA`、
`Selected=#E8F1FD`、`Border=#E1E4E9`、`Primary=#1677E3`。所有取值经 `UiTheme` 一处维护，
`check_ui_tokens.py` 不改豁免表。

新增常量：`TopbarHeight=72`、`SidebarItemHeight=43`、`SettingRowHeight=52`、`ProfileRowHeight=76`、
`DrawerWidth=410`、`RowHeight=64`（控制区三行）、`NodeRowHeight=56`。

### 7.4 切片与验收

依赖：K1 先做（顶栏与侧栏是其余各屏的容器）；K2/K3/K4/K5 互不依赖；K6 依赖 K1 的页签宿主；
K7/K8 互不依赖。

**K1 · 顶栏与侧栏**
- 内容区首行改成固定 72px 的 `QWidget#topbar`：左「页面标题 18px/700 + 副标题 11px 次要」，
  右「状态胶囊（点 + 文案，border + radius 30）+ 主题图标钮 + 更多图标钮」。
- 标题/副标题由当前视图产出（`viewTitle`），四分区各自固定文案；分区内页签不改变主标题。
- 主题按钮切换浅/深（`updateAppearance`）；更多按钮弹原型的菜单（显示首页、刷新延迟、代理开关、
  切换主题、关于）。
- 页签宿主（订阅/规则、连接/日志/网站测试）并入顶栏右侧下方或保留原行——见 K6 统一。
- 侧栏：品牌行加「NETWORK CLIENT」；导航项高 43px、选中 `Primary` 实心白字；底部保留状态句，
  版本号改为原型脚注样式（`Mihomo · v1.17.1`）。
- 验收：`findChild("topbar")` 存在且高 72；标题随 `navigate()` 变化；840×560 下三者不重叠；
  侧栏导航项 `sizeHint().height() >= 43`。

**K2 · 路由方式改为浮层菜单**
- 删除 `modeControl` 行内分段；`homeRouteRow`（对象名 `homeRouteRow`，样式按 `controlRow` 属性选中）点击在行下方弹出 `QMenu`（规则模式/全局模式/直连模式，
  当前项带勾），选择即 `setMode`。
- 验收：点击行后存在可见菜单且三项可勾选；选「全局」后 `state.mode == "global"`；行内不再有
  `modeControl` 子控件。

**K3 · 节点详情改为右侧抽屉**
- 新增 `DetailsDrawer`（无边框 `QDialog`，`Qt::Tool`，宽 410，贴主窗右侧、等高）：标题 + 副标题 +
  关闭 + 键值行 + 提示 + 底部动作（切换到此节点 / 重新测试）。
- `showNodeDetails()` 改为装配抽屉内容；自动组不提供「切换」并显示原型那条说明。
- 验收：`findChild("nodeDetailsDrawer")` 打开后宽度 410、右边缘贴合主窗右边缘；自动组无
  `nodeDetailsChoose`；关闭后焦点回到行列表。

**K4 · 节点标题行照做、去掉第二行**
- 删除 `#nodeSubscriptionRow`；`#nodeProfile` 下拉改为「选中即 `activateProfile`」，并给对象内结果
  （`homeControlHint_` 复用为节点区提示）。
- 测速与排序拆成两个图标钮：测速（默认动作 = 测本组，菜单里有测所选/取消）、排序（浮层菜单：
  原始顺序/按延迟/按名称）。
- 「更多」菜单保留（原型没有的浏览/筛选/节点选项仍需要一个入口），但排在排序之后、图标化。
- 验收：`findChild("useProxyProfile")` 为空；选择另一个订阅即可触发 `activateProfile`（用假
  controller 或状态断言）；排序菜单选择后 `filter_.sortColumn()` 变化。

**K5 · 首页回到原型构图**
- 删除 `homeDetailsDisclosure` / `homeAdditionalInfo` / `HomeMetricsView` / 曲线 / 出口摘要 /
  探测间隔行 / 代理总开关按钮 / `homeOnboarding` 卡；`homeSelection_`（滚动提示）保留为提示行。
- 首页顺序固定为：控制卡（3 行）→ 节点标题 → 节点列表 → inline-stat → 提示段。
- 内核单独启停从首页移除：内核启停在「设置 › 内核与维护 › Mihomo 内核」管理窗里已有；托盘菜单
  已有代理总开关，二者不再出现在首页。
- 「更多运行信息」里的运行版本、接管明细、出口摘要**移入**数据页（流量页签顶部统计 + 一行出口摘要）。
- 无订阅时首页仍显示控制卡的三行（行值显示「尚未连接 / 未订阅 / 尚未测试」），不再用整卡空态替换。
- 验收：`findChildren("homeDetailsDisclosure")`、`findChild("homeMetrics")`、`findChild("homeOnboarding")`
  均为空；840×560 下控制卡与 ≥3 行节点可见；无订阅时控制卡可见且第一行为「尚未连接」。

**K6 · 数据页合并为四页签**
- 新建 `dataPage()`：`QTabBar#dataTabs`（流量/连接/测试/日志）+ 右侧「复制诊断摘要」；
  `QStackedWidget#dataPanels` 装四块内容。
- 视图编号收敛：数据页 = 视图 4；连接=4/页签1、日志=4/页签3、网站测试=4/页签2；视图 5/7 保留为
  兼容路由，`navigate()` 时映射到数据页并选中对应页签（外部订阅链接、快捷键、托盘仍能到达）。
- **可见性必须按当前页签 OR 合成**：`enterView(4)` 时按页签调用 `setConnectionsVisible` /
  `setDiagnosticsVisible` / `setWebsiteTestsVisible`，切换页签时同步三者的真值；`leaveView(4)`
  三者全 false。迁移 `logsPaused_` 与 `connectionsPaused_` 的现有语义。
- 流量页签：三块统计（下载速率/上传速率/活动连接数）+ 近 60 秒曲线（复用 `HomeMetricsView` 的
  数据源）+ 一行出口摘要（原 `homeOutboundSummary_` 的文案）。
- 日志页签沿用现 `diagnosticsPage()` 的全部工具；连接页签沿用 `connectionsPage()`；测试页签装
  `WebsiteTestsPage`（去掉其自带的一级标题行，避免与页签重复）。
- 验收：`findChild("dataTabs")` 四页签；切到日志时 `controller.diagnosticsVisible()` 为真、
  切走为假；切到连接时连接采样启动、切走清空；`navigate(Route{5})` 落到数据页日志页签。

**K7 · 配置页改为两个列表**
- `QTableWidget#profilesTable` 换成两张 `QListView`（远程订阅 / 本地配置），行高 76：图标 37 +
  标题（当前使用带 `tag`）+ 来源/时间 + 用量条 + 行尾 ›。
- 标题行右侧：添加订阅（primary）/ 导入文件 / 更新全部；选中行后用行尾 › 或双击打开抽屉
  （`profileDetail`：设为当前配置 / 更多操作）。
- 对象操作条（更新/查看变化/来源/编辑/管理/使用并启动）并入抽屉与「更多操作」菜单。
- 验收：两张列表按 `sourceType` 分流；`profilesTable` 为空；点击行打开 410 抽屉；「设为当前配置」
  调用 `activateProfile`。

**K8 · 设置页改为三组单列**
- 删除设置分类页签与三块面板；改为三组（常规 / 代理与网络 / 内核与维护），组内 52px 行，
  行格式「icon + 标签 + 行尾（值 + ›）」。
- 现有设置项按原型归类：常规=外观/文字大小/登录启动/启动时隐藏/关闭窗口时释放/启动页面/托盘单击/
  全局快捷键/链接导入；代理与网络=系统代理/TUN/本地端口/排除/连通测试地址/IPv6/统一延迟；
  内核与维护=内核路径/内核更新/日志等级/备份与恢复/数据目录/日志目录/关于 Mlash。
- 「应用网络设置 / 放弃修改」动作条保留（ADR 0044 的显式提交），固定在组末。
- 验收：`findChild("settingsCategories")` 为空；组数为 3；行高 ≥52；仿真「修改端口 → 动作条出现 →
  应用」链路不变。

### 7.5 约束

- 不新增 Controller 命令；K6 只改可见性合成与路由映射，不改引擎契约。
- 每片先在本文件补验收点再改代码；每片后跑 `scripts/build.ps1`、`scripts/test.ps1`（至少目标用例）、
  `python scripts/check_ui_tokens.py`。
- 被删除控件对应的测试要**改写为断言新结构**（不是删除断言）：例如「更多运行信息可见」改为
  「首页存在 dataTabs 且流量页签可读」。
- 不宣称实机观感、读屏、高对比与缩放的最终结论；这些仍待人工复核。

### 7.6 状态

> 每片先在本节补验收点（见 §7.4），再改代码；实施后回填证据。「自动测试」指 ctest 中的定向用例，
> 不等于实机观感复核。

| 片 | 状态 | 落地与证据 |
|---|---|---|
| K0 主题 token 与基础组件 | 已实施、自动测试 | `UiTheme` 补齐 §7.3 的分级常量与原型暗色取值；浅色 `Brand=#0C5FBF`、`Primary=#0F63C8`（`OnPrimary` 对比 5.77:1，`Brand on Sidebar` ≥4.5）；`MainWindowParts` 提供顶栏、行卡、抽屉等复用件；`check_ui_tokens.py` → `0 hits` |
| K1 顶栏与侧栏 | 已实施、自动测试 | `QWidget#topbar` 固定 72px（`topbarTitle`/`topbarSubtitle`/`topbarStatus`/`topbarTheme`/`topbarMore`）；标题副标题由 `ViewIds::title/subtitle` 随路由更新；侧栏 4 项高 43px；视图编号收敛为 `ViewIds`（`Home=0`…`Settings=4`），旧编号只在 `fromLegacy()` 一处解释 |
| K2 路由方式浮层 | 已实施、自动测试 | `homeRouteRow_` 点击弹出 `QMenu`，三项带勾选；`modeControl` 行内分段已删 |
| K3 节点详情抽屉 | 已实施、自动测试 | `WindowParts::DetailsDrawer`（410px、贴右、等高）；自动组不提供 `nodeDetailsChoose` |
| K4 节点标题行 | 已实施、自动测试 | `#nodeSubscriptionRow` 删除，订阅下拉选中即 `activateProfile`；测速/排序拆为两个图标钮并各自带浮层菜单 |
| K5 首页构图 | 已实施、自动测试 | 顺序固定为控制卡三行 → 节点标题 → 节点列表 → `homeInlineStat` → 提示；`homeDetailsDisclosure`/`homeMetrics`/`homeOnboarding` 均不存在（测试断言为空）；内核启停与代理总开关回落「设置」与托盘 |
| K6 数据页四页签 | 已实施、自动测试 | `QTabBar#dataTabs` + `QStackedWidget#dataPanels`；可见性按当前页签 OR 合成（`setConnectionsVisible`/`setDiagnosticsVisible`/`setWebsiteTestsVisible`）；`navigate(Route{5})` 落到日志页签、`{7}` 落到测试页签 |
| K7 配置页两列表 | 已实施、自动测试 | 两张 `QListView`（`remoteProfileList`/`localProfileList`），行高 76，`ProfileRowModel` 按 `sourceType` 分流；`profilesTable` 为空；行尾 › 或双击打开 410 抽屉 |
| K8 设置页三组 | 已实施、自动测试 | `settingsCategories` 页签删除，改为「通用 / 网络 / 内核与维护」三张卡片；`ensureSettingsGroupVisible()` 按真实几何定位；分类沿用 `kNoCategory` 不再被同视图导航重置 |
| K9 测试改写与文档 | 已实施、自动测试 | 见 §7.7；整轮 `ctest --preset windows-mingw` **40 通过 / 1 失败 / 1 跳过**（唯一失败是既有的 `core_update_tests` 超时；`appearance_tests` 的限时已按用户决定放宽到 100s，整轮 64.55s 通过）；明细见 [验证报告](verification-report.md) |

### 7.7 K9 结果：被删控件对应的测试改写

按 §7.5「改写为断言新结构，不删断言」：

| 原断言 | 改写为 |
|---|---|
| `homeDetailsDisclosure` 可见、首页曲线可读 | `findChild("homeDetailsDisclosure")`/`findChild("homeMetrics")`/`findChild("homeOnboarding")` 为空；指标块在 `setPage(4)`（数据 › 流量）下 `!isHidden()`，回首页后 `currentRoute().view == Home` |
| 首页探测间隔下拉 `homeProbeInterval` 存在 | 该控件为空；`setHomeVisible(true)` 后 `homeProbe()->result()` 含 `delay`（探测仍由可见性驱动） |
| 首页出口摘要 `homeOutboundSummary` | 同名控件在流量页签内，经 `setPage(4)` 打开诊断；断言摘要高度 > 0 且文案来自 `OutboundDiagnostics` |
| 侧栏 `setCurrentRow(3)` 生效 | 逐行对照：侧栏条目存的是视图 ID，点第 n 行后 `currentRoute().view == item(n)->data(Qt::UserRole)`；第 2 行（数据）落在 `ViewIds::Data`，第 3 行（设置）落在 `ViewIds::Settings` 且 `page()==6` |
| 旧编号 2 = 首页节点视图 | `navigate(Route{2})` 落在 `ViewIds::Rules`/`page()==3`；只有整数入口 `setPage(2)` 才回到首页 + `HomeAnchor::Nodes` |
| 启动页/托盘路由期望值 2 | 改为 0（首页 Control），节点深链单独断言 `HomeAnchor::Nodes` |
| 无订阅时 `homeControlCard` 隐藏、由整卡空态替换 | 卡片恒可见，三行各自显示「尚未连接 / 规则模式 / 尚未测试」；`overviewTitle` 文本为「尚未连接」 |
| 首页空态有「前往首页启动」按钮（`homeNextStep`） | 该控件为空；空态由控制区三行承担，导入入口改用配置页 `addSubscription` |
| 路由方式有行内分段按钮（`property("mode")`） | 该控件为空；改用 `homeRouteRow` 整行点击，鼠标焦点与 Tab 焦点两帧不同（焦点环可辨） |
| 指标块在首页、`metrics->isVisible()` 即可断言 | 指标块在「数据 › 流量」：隐藏窗口下先 `navigate(Route{Data,…,Traffic})` 断言 `!isHidden()`，`show()` 后 `isVisible()`，回首页后不可见。注意旧编号 `setPage(4)` 落在 **连接** 页签 |
| 配置页用 `profilesTable` 行操作「策略」「分享二维码」 | 点击 `remoteProfileList` 行打开抽屉 → `profileDetailsMore` → 菜单项 `profilePolicyButton` / `shareProfileQr` |
| 首页探测组由组卡片设置 | K5 删掉组卡片后 `homeProbeGroup_` 无来源；测试改为显式 `setHomeProbeGroup("Test")`，选路语义本身不变 |

K9 中修掉的产品缺陷（均由改写后的断言暴露，不是测试问题）：

| 缺陷 | 根因 | 处理 |
|---|---|---|
| 侧栏「数据」打开规则页、「设置」打开数据页 | `navigation_` 条目存视图 ID（0/1/3/4），点击处理却交给 `setPage()` 当旧编号解释（旧 3=规则、旧 4=数据·连接） | 改走 `navigate(Route{视图 ID})`；视图 ID 与旧编号从此不再互相猜测 |
| 规则视图被 `navigate()` 吞成首页 | `navigate()` 与 `PageRouter::go()` 各自留了一次「view==2 → 首页」的历史归一化 | 两处删除；旧编号只允许在 `setPage()`/`GuiHost` 的整数入口经 `fromLegacyPage()` 解释 |
| 设置深链不滚动 / 标题丢分类 | `ensureWidgetVisible()` 向下多滚半个视口且离屏几何未更新；`Route{Settings}` 的 `kNoCategory` 覆盖了当前分类 | 新增 `ensureSettingsGroupVisible()` 按控件真实几何定位，配 `QTimer::singleShot(0, …)` 重试；`kNoCategory` 时沿用 `route_.category` |
| 本地订阅列表右键「没反应」/ 菜单挡住对话框 | 两张列表共用对象名 `profileContextMenu`，且 `QMenu::popup()` 以模态接管本轮 | 菜单加 `profileKind` 属性、按列表各建一份；打开对话框前先 `menu->close()`；空白处右键先收起已开菜单 |
| 对比度不达标 | `Brand on Sidebar` 4.47、`OnPrimary on Primary` 不足 4.5 | 浅色 `Brand=#0C5FBF`、`Primary/Focus=#0F63C8` |
| 非数据页的路由带上了页签 | `fromLegacyPage()` 把 `DataTab::Traffic` 塞进了首页/规则/设置的 `Route.tab`，同一条路由各字段不等，且 `toLegacyPage()` 往返会变成数据页编号 | 只有 `ViewIds::Data` 才写入 `tab`，其余视图恒为 `-1`（沿用） |
| 深色主按钮白字只有 3.55:1 | K0 把原型 `--blue #2488f5` 同时用作 `Primary`，而原型该色配白字本来就不足 4.5:1 | 深色 `Primary=#1968c2`（白字 5.53:1）；`Brand` 仍取原型链接蓝，只在非文字主色场景使用 |
| 控制区第 2 行没有行背景/焦点环 | 行样式按 `objectName=homeControlRow` 选中；K2 把该行改成 `QPushButton` 并改名为 `homeRouteRow`，QSS 落空 | 行样式改按 `controlRow` 属性选中，并为按钮补 `keyboardFocus` 焦点环；`homeRouteRow` 只保留自己的对象名 |
| 样式表整段失效 | 在 QSS 里写了 `//` 注释，Qt 只认 `/* */`，注释之后直到下一个块结束的规则被静默丢弃（键盘焦点环一直不出现） | 改回 `/* */`；这是本轮唯一一个「代码没报错、效果却消失了」的陷阱，记在此处备查 |

**未验**：真实 Windows 实机观感（悬停、键盘焦点顺序、Alt+Tab）、读屏播报、高对比与 125%/150% 文字缩放下的人工复核、原型在浏览器中的逐屏人工比对。`release_qualified` 仍为 `false`。
