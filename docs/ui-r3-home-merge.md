# R3 施工记录：首页合并与代理总开关

> 归属：[界面分层设计](ui-layered-design.md#实施切片) 的 R3 片，按 [ADR 0048](adr/0048-home-merged-proxy-switch.md) 实施。实施前记录方案与验收点，实施后在同一文件追记结果。

## 1. 本片要解决的两件事

1. **首页合并**：状态与节点不再并列，合并为一个滚动视图（控制区在上、节点列表在下），删除 `pages_` 索引 2 的控件实例；入口 2 改为「滚到节点列表」。
2. **主控件换成代理总开关**：按钮语义从「启动/停止内核」改为「让所选接管方式生效/失效」，并新增 Controller 命令 `setProxyEnabled(bool)`。

第 2 项是 Controller 契约的**唯一例外**（ADR 0048「Controller 影响」）。理由已在 ADR 0048 用五行代码事实固定：UI 层编排 `startCore → setSystemProxy → setTunEnabled` 无法保证「失败回到点击前快照」。

## 2. `setProxyEnabled(bool)` 的设计

### 2.1 语义

| 调用 | 含义 | 内核 | 接管 |
|---|---|---|---|
| `setProxyEnabled(true)` | 让**当前设置允许的接管方式**生效 | 未运行则先启动并等待就绪；已在运行则复用 | 按 `settings.tunEnabled` 决定是否启用 TUN，其余走系统代理 |
| `setProxyEnabled(false)` | 撤销接管 | **继续运行**（ADR 0048：关闭代理不停内核） | 关闭 TUN 与系统代理，恢复点击前的系统代理快照 |

- 「当前设置允许的接管方式」指 `settings.tunEnabled`：为真时 TUN 是接管手段，系统代理保持独立设置不被强开；为假时走系统代理。
- 不做「偏好开关」判断——设置里没有「允许接管」开关（ADR 0048 已确认）。
- `state_.tunObserved` 仍按现有语义使用，不在本命令里改成真观测。

### 2.2 事务结构

一次 `begin(kOperationProxy)` 包住整个流程，完成后一次 `finish()`：

1. 记录**点击前快照**：`settings.desiredProxy`、`settings.tunEnabled`、`state_.proxy`、`state_.core`、`state_.ready`、代理协调器已保存的快照。
2. `true` 路径：若内核未运行 → 启动；等待就绪事件的回调里继续 → 需要 TUN 则 `updateTunOptions + prepareActivation`，否则直接 `setSystemProxy(true)`；每一步都回读。
3. `false` 路径：需要 TUN 则先关 TUN（它内部会 release 系统代理），再 `setSystemProxy(false)`；`settings.desiredProxy=false`。
4. 任一步失败 → 按快照回滚：恢复 `desiredProxy`，TUN 回到点击前值，系统代理用协调器的 `restorePrevious`（只在「当前完整快照 == 我们最后写入的快照」时执行，AGENTS.md 要求），然后 `finish(false, 原因)`。
5. 成功 → `finish(true, "代理已开启" / "代理已关闭")`。

并发与取消：沿用 `begin()` 的互斥（忙碌时直接拒绝并给出「上一个操作未完成」）；本命令**不可取消**（与现有启停一致），UI 只显示「正在开启…／正在关闭…」。

### 2.3 不做的事

- 不新增 `OperationId`（ADR 0046 的可选参数尚未实施，本片维持现有 `operationFinished(bool, QString)`）。
- 不改 `startCore/stopCore/setSystemProxy/setTunEnabled` 的既有语义；`setProxyEnabled` 是它们的组合与回滚包装。
- 不改托盘的一次性开关行为：托盘「系统代理 / TUN」仍是各自独立开关；总开关只在首页与托盘主项使用。

## 3. 首页布局

```
[总开关行]   ● 内核运行中 · 系统代理已接管          [开启代理 / 关闭代理]
[当前代理]   香港 01 · 自动选择                     [策略组 ▾]
[路由方式]   规则 / 全局 / 直连
[连通性]     上次结果 · 时间                        [测试]
─────────────────────────────────────────────
订阅选择 ▾   搜索…   排序 ▾   测速   筛选
节点与策略组列表（分组可折叠、行内延迟、当前成员标记）
```

- 控制区 **≤6 个可交互控件**（ADR 0048 的新预算口径）：总开关、策略组选择、模式分段、连通性测试、更多运行信息、订阅选择。节点列表与其工具带不计入。
- 「更多运行信息」保留为收纳入口（只展开不执行），内核单独启停移入此处与托盘。
- 节点区迁移：把 `proxyPage()` 的节点面板迁到 `homePage()` 的滚动视图内；`pages_` 索引 2 删除，`QStackedWidget` 变为 7 个索引；`fromLegacyPage(2)` 仍返回 `{0,kNoCategory,Nodes}`，`applyHomeAnchor` 改为滚动 `homeScroll_` 到节点区首行。
- 迁移后必须同步的既有依赖：`QualificationRun` 的节点测量、`UiTests` 里 `setPage(2)` 的节点操作、`WorkflowDepthTests` 的节点书签恢复、`MainWindowSettings` 启动页下拉的「节点」项显示名。

## 4. 状态与失败

| 场景 | 总开关 | 状态句 |
|---|---|---|
| 未选订阅 | 禁用 | 「添加并使用订阅后即可开启」 |
| 内核停止 + 未接管 | 「开启代理」可用 | 「内核已停止 · 未接管流量」 |
| 内核运行 + 接管 | 「关闭代理」 | 「内核运行中 · 系统代理已接管」 |
| 内核运行 + 未接管 | 「开启代理」 | 「内核运行中 · 未接管流量」 |
| 开启中 / 关闭中 | 禁用 + 文案替换 | 步骤进度，不改状态句 |
| 失败 | 恢复点击前位置 | 状态句保留观测，状态栏给原因与重试 |

## 5. 验收点

1. 840×560 下控制区可交互控件 ≤6，且控制区之后至少看到 3 行节点。
2. 六态可复现（用注入的 `PlatformServices`/TUN 假实现驱动，**禁止真实写系统代理**）。
3. 注入「系统代理写入失败」后：开关回到点击前位置、`desiredProxy` 回到点击前值、状态栏给原因；注入「TUN 失败」时系统代理若已写入则按快照恢复。
4. 关闭代理后内核仍在运行（`state.core` 不变），且 `desiredProxy=false`。
5. 自动策略组行内无切换动作；手动组单击即生效、失败回滚。
6. 入口 2（启动页/托盘/搜索）滚动到节点列表首行。
7. `pages_->count()==7`，不再存在节点视图实例；`page()` 对首页仍返回 0/2。
8. 既有测试全绿（含 `QualificationRun` 相关）。

## 6. 实际结果

**已实施并自动测试（2026-10-29）。**

### 6.1 `setProxyEnabled(bool)`（`src/app/AppControllerProxySwitch.cpp`）

- 一次 `begin()` 事务包住整个流程。开启且内核未运行时走 `prepareActivation`（内核启动 + `resumeDesiredProxy` 从既有日志恢复接管）；内核已在运行时直接 `acquire`；关闭只 `release()` 并把 `desiredProxy` 置假，**内核继续运行**。
- 涉及 TUN 时复用既有 `release → stop → start → resume` 与 `failActivation → restorePrevious`，不另写回滚。
- 失败路径：`acquire` 失败后把 `desiredProxy` 恢复到点击前值再 `finish(false, 原因)`，开关不会虚报成功；系统代理的恢复仍由 `ProxyCoordinator::release()` 的「当前完整快照 == 我们最后写入的快照」前提把关。
- 未新增 `OperationId`，`operationFinished(bool, QString)` 未变。

### 6.2 首页合并

- `proxyPage()` 改名为 `nodeSection()`，只返回节点区控件，不再注册为栈页；`homePage()` 在滚动内容里按序追加「控制区 → … → 节点区」，`nodeSection_` 首次被赋值。
- **新增视图 ID → 栈索引映射 `MainWindow::stackIndex(int)`**：删除原索引 2 后 ID 3–7 各前移一位。这是本片最关键的实现细节——直接 `setCurrentIndex(route.view)` 会让网站测试页（ID 7）落在 7 页栈之外而被静默忽略，设置页则错位到网站测试页。所有 `pages_->currentIndex()` 比较随之改为比较 `route_.view`。
- `applyHomeAnchor` 改为 `homeScroll_->ensureWidgetVisible(nodeSection_)`（`QTimer::singleShot(0)` 等布局完成）。
- 首页主控件为「开启代理/关闭代理」，进行中显示「正在开启…/正在关闭…」。

### 6.3 复核

- 新增 `mlash_tun_tests proxyMasterSwitchRollsBackAndKeepsCoreRunning`：无订阅拒绝且零系统写入；接管成功且内核进程不变；关闭后系统代理还原、内核仍在运行、`desiredProxy` 为假；重复关闭幂等；注入写入失败后开关失败、意图复原、仍可重新开启；退出还原。全部走注入的 `PlatformServices`，`autoWrites==0`。
- 新增 `mlash_ui_tests homeOwnsNodeSectionInOneScrollView`：栈 7 页、首页含节点区、设置/网站测试映射到正确栈页、入口 2 回到首页并请求滚动。
- 改写五处几何前提：`refinedTaskGeometryAndSemantics`（改为「节点区被滚入视口」+ 视口高度占比）、`currentGroupSelectionAndNativePreview`（展开策略组不再挤压节点视口，改为不增高）、`OutboundTests` 的 `homeMetrics` 高度对比（改为最小尺寸一致）、`WebsiteTests` 两处、`AppearanceTests`/`UiTests` 中 `homeCoreControl` → `homeProxySwitch`。
- 完整 `ctest`：34 项中 33 通过、0 失败、`system_test_guard` 跳过，用时 107 秒。
- 截图复核：940×620 与 840×560 下首页为单一滚动视图，控制区在上、节点区在下，页签行位置不变。

**未验**：真实 Windows 实机的 TUN 接管与代理切换（自动测试全部走注入替身）、人工读屏、性能预算与 72 小时稳定性；`QualificationRun` 的节点测量在合并后未实跑（仅确认其按 objectName 查找仍可命中）。
