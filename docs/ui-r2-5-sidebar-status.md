# R2.5 施工记录：侧栏状态条与共用状态句

> 归属：[界面分层设计](ui-layered-design.md#实施切片) 的 R2.5 片，落实原则 P1（状态先于导航）与 P2（一个事实只有一个权威来源）。实施后在同一文件追记结果。

## 1. 要解决的问题

P1 要求全局状态**常驻**：核心 / 代理归属 / TUN / 当前订阅。现状只有两处半实现：

- 侧栏底部只有一个版本号 `QLabel`。
- 托盘 tooltip 拼了一段近似文案（`statusTooltip`），但主窗口没有任何等价文本；切换页签后用户看不到这四项。
- 主窗口首页控制区有 `coreLabel_` / `proxyLabel_`，但那是**首页**的详情位置，不是常驻位置。

因此「当前是什么状态」在离开首页后不可见，违反 P1；两处文案若各自演化，会违反 P2（同一事实两处独立计算）。

## 2. 采用方案

### 2.1 一个函数产出四段

新增 `StatusSentence.h/.cpp`（`src/ui`，因为它是展示层措辞，不是 Controller 契约）：

```cpp
struct StatusFacts { CoreState core; bool ready; ProxyState proxy; bool tunObserved; bool tunRequested; QString activeProfile; QString profileName; };
struct StatusSentence {
    QString core;    // 「内核运行中」/「内核启动中」/…
    QString takeover;// 「系统代理已接管」/「TUN 已开启」/「未接管流量」/「由其他程序管理」/「未确认」
    QString tun;     // 「TUN 已开启」/「TUN 未开启」/「TUN 未确认」
    QString profile; // 订阅名，或「未选择订阅」
    QColor accent;   // 状态点颜色角色（Success / Warning / Error / Secondary）
};
StatusSentence statusSentence(const AppState&, const Settings&);
```

- **纯函数**：只读 `AppState` 与 `Settings`，无缓存、无计时器。主窗口与托盘都调用它，满足 P2「摘要由同一来源派生」。
- **不新增字段、不改 Controller**。`tunObserved` 仍按现状（意图的影子）使用，但**文案不得把它说成独立观测**：`tunObserved && tunRequested` → 「TUN 已开启」；`tunRequested && !tunObserved` → 「TUN 未确认」（与首页控制区现有措辞一致）；两者皆假 → 「TUN 未开启」。
- 「接管」段的优先级：系统代理 `Owned` → 「系统代理已接管」；否则 TUN 已开启 → 「TUN 已接管」；内核就绪但都没接管 → 「未接管流量」；系统代理 `External` → 「系统代理由其他程序管理」；未知 → 「状态未知」。**「内核运行中 · 未接管流量」是 ADR 0048 要求的独立状态，不得并入「已关闭」。**

### 2.2 侧栏状态条

放在侧栏底部、版本号之上（`margin-top:auto` 之前的部分保持现状）：

```
● 内核运行中 · 系统代理已接管
  TUN 未开启 · 订阅 A
```

- 结构：`QToolButton`（整行命中，`objectName=sidebarStatus`），内部两行 `QLabel`（`sidebarStatusLine1/2`）+ 一个状态点。行高按内容，`840×560` 与 120% 文字下最多两行，超出用 `ElidedLabel` 省略，hover 显示完整文本。
- 状态点颜色取 `StatusSentence::accent`（Success / Warning / Error / Secondary），**同时**靠文字表达状态——不单靠颜色（P6 规则 6）。
- 单击 → `navigate(Route{0, -1, HomeAnchor::Control})`（回首页控制区）。`Ctrl+Shift+S` 不新增快捷键（R8 统一处理）。
- 可见性与刷新：`updateState()` 里更新；`GuiHost` 轻量重建后由新窗口的第一次 `updateState()` 重建，不持久化。
- 版本号行保留在状态条**下方**，仍用 `secondaryText`。

### 2.3 托盘复用同一句

`TrayController::statusTooltip` 改为：

```
Mlash · <line1>
<line2>
内核模式：规则
```

第一行直接用 `statusSentence` 的两行文案（与侧栏**逐字相同**），第三行保留现有的「内核模式」信息（侧栏空间不足，不重复）。托盘菜单顶部禁用的状态项同样改用 `line1`。

### 2.4 不做的事

- 不给状态条加快捷动作（托盘已承担）；不把「开启/关闭代理」放进侧栏（ADR 0048 说首页与托盘是入口，侧栏只展示）。
- 不改首页控制区文案（那是详情位置），只把它的颜色/措辞与 `statusSentence` 对齐到同一来源，避免两套「TUN 未确认」判断。
- 不改 `tunObserved` 的赋值语义（TUN 真观测是另一项工作，见分层设计 P2 表备注）。

## 3. 状态转换与边界

| 场景 | 期望 |
|---|---|
| 内核已停止 | 点 grey；「内核已停止 · 未接管流量」/「TUN 未开启 · <订阅>」 |
| 启动中 / 停止中 | 点 warning；「内核启动中」/「内核停止中」 |
| 就绪 + 系统代理 Owned | 点 success；「内核运行中 · 系统代理已接管」 |
| 就绪 + 仅 TUN | 点 success；「内核运行中 · TUN 已接管」 |
| 就绪 + 都没接管 | 点 brand/secondary；「内核运行中 · 未接管流量」（ADR 0048 六态之一） |
| 系统代理被外部接管 | 点 warning；「… · 系统代理由其他程序管理」 |
| 内核 Failed | 点 error；「内核启动或运行失败」 |
| 未选择订阅 | 「未选择订阅」 |
| 忙碌（启停/写操作） | 文案不变（状态条只报事实，不报忙碌；忙碌在状态栏） |
| 设置页 / 任意页 | 状态条始终可见（它是侧栏的一部分） |
| 轻量重建 | 新窗口首次 `updateState()` 重建，无需持久化 |
| 840×560、120% 文字 | 两行不溢出；超长订阅名省略且 hover 可看全文 |

## 4. 验收点

1. 每个视图（含设置三分类）侧栏都能看到状态条，且文本非空。
2. 托盘 tooltip 的前两行与侧栏两行**逐字相同**（测试直接比较字符串）。
3. 六个状态（已接管 / 仅 TUN / 内核运行未接管 / 已停止 / 启动中 / 失败）各自产生预期的两行文本与状态点角色。
4. 单击状态条回到首页控制区（`page()==0`）。
5. 840×560 与 120% 文字下状态条仍是两行、不出现横向滚动。
6. 既有测试全绿。

## 5. 实际结果

**已实施并自动测试（2026-10-29）。**

实现：

- `src/ui/StatusSentence.{h,cpp}`：`StatusFacts` + `statusSentence()` 纯函数，产出 `line1`（内核 + 接管）、`line2`（TUN + 订阅）与 `accent`（`success`/`warning`/`error`/`secondary`）。`tunObserved` 与 `tunRequested` 分列，`tunRequested && !tunObserved` 只说「TUN 未确认」。
- 侧栏新增 `QToolButton#sidebarStatus`（`sidebarStatusLine1/2` + `sidebarStatusDot`），位于导航与版本号之间；单击 `navigate(Route{0, kNoCategory, HomeAnchor::Control})`；`updateState()` 每次刷新；轻量重建由新窗口构造期的 `updateState()` 重建。
- `TrayController::statusTooltip(state, settings, profileName)` 前两行直接取 `statusSentence`，第三行保留「内核模式」。签名从单参数扩为三参数（带默认值），供测试注入。
- 样式：`#sidebarStatus` 透明 + `separator` 上边框 + hover `@Hover`；两行 `@Secondary`。

复核：

- 新增 `mlash_ui_tests sidebarStatusIsPermanentAndSharesTrayWording`：六个状态的 `line1/line2/accent` 逐条比对（含「内核运行中 · 未接管流量」与「内核已停止」必须不同、「TUN 未确认」）；八个视图里状态条均可见且文本非空；托盘 tooltip 第 2/3 行与侧栏两行逐字相等；点击回入口 0 且 anchor 为 Control；840×560 截图。
- 更新两处旧断言：`UiTests::trayAppearancePriorityAndResources` 与 `TunTests` 的 `statusTooltip` 调用补 `Settings()` 实参。
- 完整 `ctest`：34 项中 33 通过、0 失败、`system_test_guard` 跳过，用时 116 秒。
- 截图复核：840×560 下状态条两行、状态点、上分隔线与版本号共存，无横向溢出。

**偏差说明**：设计稿曾写 `StatusSentence` 携带 `QColor`，实现改为 `QString accent` 角色名，由 `MainWindow` 映射到 `UiTheme::Success/Warning/Error/Secondary`——保持 `statusSentence` 不依赖 Qt 绘制、便于托盘复用与测试比较。

**未验**：真实 Windows 实机的托盘 tooltip 显示、人工读屏、120% 文字下的实际换行（自动测试仅覆盖 840×560 默认字号）。

