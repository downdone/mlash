# U07 工作计划：原型补全与视觉结构对齐

> 归属：[界面分层设计](ui-layered-design.md) 的实施切片之后。地基（R1–R10）已落地，本文规划下一阶段：先把原型补到覆盖全部页面，再按原型把实现的视觉结构对齐。
> 决策意图：用户 2026-10-30 决定「都要做；原型要更新」。

## 1. 起点审计（2026-10-30）

### 1.1 地基完成情况

| 项 | 状态 | 证据 |
|---|---|---|
| 路由模型 `Route{view, category, anchor}` | 完成 | 所有入口走 `navigate()`；设置分类可深链 |
| 单一标题槽、窗口标题随路由 | 完成 | `titleSlotIsSingleAndWindowTitleFollowsRoute` |
| 侧栏状态条与托盘同源 | 完成 | `statusSentence()` 一处产出 |
| 首页合并 + 代理总开关 | 完成 | `setProxyEnabled()` 单事务，失败回滚 |
| 十态呈现 | 完成 | 三页各自可区分 |
| 空态可聚焦动作 | 完成 | 四处按原因给下一步 |
| 反馈路由 | 完成 | `OperationId` + 归属表 + 状态栏两段 |
| Token 单一来源 | 完成 | `check_ui_tokens.py` → `0 hits` |

**四项缺口**，本计划的前两片处理前两项，后两项单独立项：

| 缺口 | 现状 | 归入 |
|---|---|---|
| 页面生命周期未对象化 | `navigate()` 里 8 处 `if (view == N)`；设计文档的 `onEnter`/`onLeave`/`PageMemory` 从未建立 | S1 |
| 工具窗口未收敛 | 13 个窗口里 5 个用 `findChild` 复用；`BackupDialog` 有 4 个 `new` 点 | S2 |
| TUN 意图/观测同一值 | `AppControllerTransaction.cpp:84,154` 把 `settings.tunEnabled` 写回 `tunObserved` | S3（另立 ADR） |
| 状态只有颜色编码 | `statusDot` 是换颜色的 `●` 字符，没有形状第二通道 | V1 |

### 1.2 原型覆盖缺口

现有原型 9 屏：01 首页 / 02 开关六态 / 03 更多运行信息 / 04 订阅 / 05a·05b 设置 / 06 菜单托盘 / 06b 关闭代理后 / 07 状态呈现。

**缺四页**：连接、日志、规则、网站测试。另有三处原型已画但实现没跟上：订阅用量条、设置行内当前值、菜单分组标题。

## 2. 分片计划

依赖：`P1 → P2 → S1 → S2 → V1 → V2 → V3 → V4 → V5`。S3 独立。

### P1 · 原型补四页（先于实现）

新增 08–11 四屏，沿用既有 `.window` 骨架与 token：

| 屏 | 内容 | 关键结构 |
|---|---|---|
| 08 连接页 | 页内分段（活动/已结束）→ 工具条 → 表格 → 提交条 | 状态行同时表达「采样时间 / 过期 / 失败」；空态与失败态各一张 |
| 09 日志页 | 工具条 → 文本流 → 结果摘要 | 空/无匹配/未就绪/已暂停四态各画一张小卡 |
| 10 规则页 | 上下文条 → 分段（优先规则/内核实际规则）→ 列表 → 提交条 | 内核实际规则的「读取失败带重试」单独画 |
| 11 网站测试页 | 目标卡片网格 → 结果行 | 卡片按 `--card-min-website: 108px` |

验收：四屏与既有屏共用同一套 CSS 变量；浏览器打开无横向滚动；每屏标注「对应实现页与差异」。

### P2 · 原型补三处未跟上的细节

| 屏 | 补充 |
|---|---|
| 04 订阅 | 加用量条列（勾选/名称/更新时间/用量条） |
| 05a/05b 设置 | 每行下方显示「当前值」次要文字 |
| 06 菜单 | 五个分组标题 + 组内 `trail` 状态尾巴 |

验收：三处在原型里可见，并回写为 V2/V3/V4 的验收依据。

### S1 · 页面生命周期对象化

- 新增 `src/ui/PageLifecycle.h`：`struct PageMemory { QVariantMap filters; int tab = 0; }`，`class PageLifecycle { virtual void onEnter(const Route&); virtual void onLeave(); virtual void onRestore(const PageMemory&); }`
- `MainWindow` 持有 7 个生命周期对象，`navigate()` 只调用 `leave(old)` / `enter(new)`
- 把 `navigate()` 里现有的轮询、采样、诊断订阅、规则刷新搬进各页对象
- 验收：新增测试断言「切换到任一视图只触发该视图的 `onEnter` 一次」；现有 35 项全绿
- 风险：低，不改变可见行为

### S2 · 工具窗口注册表

- 新增 `src/ui/ToolWindowRegistry.h`：`QWidget* show(ToolId, const QString& objectKey, std::function<QWidget*()> factory)`
- 13 个窗口逐个迁移；`BackupDialog` 的 4 个 `new` 点收敛
- 验收：同一工具+对象第二次打开返回同一实例并 `raise()`；对象被删时窗口关闭；现有测试全绿
- 风险：中，需逐个核对模态/非模态与关闭语义

### V1 · 状态形状编码与骨架屏

- `UiIcons` 新增 `Glyph::{DotFilled, Triangle, Square, Ring}`，`statusDot`/`sidebarStatusDot` 由字符改为绘制
- 颜色 + 形状双通道；token 扫描仍 0 命中
- 加载态显示三行骨架块，替换纯文字「正在读取…」
- 验收：四态形状两两不同（像素断言）；HC 下形状仍可辨

### V2 · 订阅列表用量条

- 表格新增自绘 `UsageDelegate`，数据来自已有 `SubscriptionInfo`
- 无额度的订阅整列隐藏
- 验收：有额度显示条，无额度列隐藏且不留空白

### V3 · 设置行内当前值

- `settingRow()` 增加可选 `currentValue` 参数
- 只对「值不在控件里」的行生效（启动页面、TUN 状态、内核路径等）
- 验收：设置搜索仍命中；840×560 无横向滚动

### V4 · 菜单分组标题与状态尾巴

- `QMenu` 加分组标题动作（次要色、禁用态、不可聚焦）
- 订阅菜单五组各加标题；托盘「代理」组加状态尾巴
- 验收：菜单顶层项数不变；分组标题不可聚焦

### V5 · 首页控制区对齐原型

- 节点行补协议标签；组标题右侧补「自动选路 / 手动单卡生效」
- 开关失败在控制区显示**原因行**，不再只有单行提示
- 验收：840×560 下控制区之后仍可见 ≥3 行节点（既有断言不得退让）

### S3 · TUN 意图/观测分离（独立）

`core_.tunObservation()` 真实写回 `state_.tunObserved`，需要新 ADR，因为它改变 `statusSentence` 与 `TunDialog` 的事实来源。不在本计划的排队内。

## 3. 约束

- 每片先写设计记录再实现（AGENTS.md 的 UI 先审视、先设计、后修改）。
- 每片结束跑完整 `ctest` 与 `check_ui_tokens.py`。
- 原型不是验收依据（[文档导航](README.md) 已注明），验收以测试与验证报告为准。
- 不宣称 Windows 10、干净机器、72 小时稳定性或性能预算。

## 4. 状态

| 片 | 状态 |
|---|---|
| P1 原型补四页 | 已完成：08 连接 / 09 日志 / 10 规则 / 11 网站测试；另补了原型引用却从未定义的基础类（`control-*`、`node-*`、`home-drawer` 等），否则 01/03/06b 三屏在浏览器里退化为纯文本 |
| P2 原型补三处细节 | 已完成：用量条 CSS、菜单五组标题、`switch-btn.busy` |
| S1 生命周期 | 已完成：`PageRouter` + `PageLifecycle`；`navigate()` 只剩顺序 |
| S2 窗口注册表 | 已完成：`ToolWindowRegistry`；13 个窗口收敛，订阅删除时关闭指向它的窗口 |
| V1 状态形状与骨架屏 | 已完成：`StatusShape` 五态五形状；`SkeletonBlock` 接在「内核实际规则」读取中（事后核实发现初版只建了类没接界面，已补） |
| V2 订阅用量条 | 已完成：`UsageDelegate`；无额度整列隐藏 |
| V3 设置行内当前值 | 已完成：`settingRow` 可选 `currentValue`；实际接到 TUN 与内核两行（事后核实删掉了两处指向不存在标签的死代码） |
| V4 菜单分组标题 | 已完成：`menuSection()`；托盘路由模式带尾巴 |
| V5 首页控制区 | 已完成：卡片协议标签、组选路说明、控制区失败原因 |
| S3 TUN 意图/观测分离 | 未开始（需独立 ADR） |
