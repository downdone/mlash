# R10 施工记录：反馈路由

> 归属：[界面分层设计](ui-layered-design.md#p41-反馈路由) 的 P4.1 原则，对应 [ADR 0046](adr/0046-navigation-routability-and-feedback-routing.md) 的「反馈路由」一半。R1–R9 只落地了路由一半，本片把另一半补完。

## 1. 施工前审计

`operationFinished(bool ok, const QString& message)` 不带操作标识，因此 UI 无法判断某次结果属于哪个页面、哪个对象。实际后果：

| 现象 | 位置（改动前） |
|---|---|
| 所有失败都进全局状态栏，页面内没有归属 | `MainWindow.cpp:148` 的单次连接 |
| 内核实际规则读取失败同时进状态栏和页面（重复） | `AppController.cpp:407` |
| 批量更新逐项结果只靠弹窗内的局部 hint，离开弹窗即丢失 | `MainWindowProfiles.cpp:191` |
| 订阅更新成功没有页面级结论，只剩表格「状态」列 | `MainWindowProfiles.cpp:191` |
| 状态栏被 `state.lastError` 无条件覆盖，「最近一次操作」与「当前告警」互相冲掉 | `MainWindow.cpp:389` |

设计（P4.1）要求六类反馈各有承载位置，且 `operationStarted`、`delayProgress`、`settingsFeedback_`、`rulesHint_`、托盘失败气泡逐条归类，不得留在状态栏兜底。

## 2. 设计

### 2.1 操作标识

```cpp
enum class OperationId : quint8 {
    Unspecified, ProfileUpdate, ProfileActivate, ProfileBatch, ProfileImport,
    ConnectionClose, ConnectionCloseAll, ProxyToggle, TunToggle, ModeSwitch,
    MemberSelect, CoreStart, CoreStop, CoreUpdate, Backup, Restore, SettingsApply
};
void operationStarted(const QString& message, OperationId id = OperationId::Unspecified);
void operationFinished(bool ok, const QString& message, OperationId id = OperationId::Unspecified);
```

**为什么用 `quint8` 而不是裸 `enum class`**：信号签名进入 moc 元对象，`QSignalSpy` 取值与跨 TU 一致都要求它是具名的注册类型；`quint8` 基类型让 `Q_DECLARE_METATYPE`/`qRegisterMetaType` 只注册一次，且 `QSignalSpy::at(i)[2].value<OperationId>()` 可直接读。

**默认参数保证兼容**：现有 99 处 `emit`、34 处 `connect` 全部可继续编译；先跑通全绿，再逐处补标识。这一点与设计原文「加默认参数保证旧调用点继续编译，新增调用点应显式传值」一致。

**归属范围**：只标注**页面级**与**跨页级**两类操作。对话框内部的动作（DNS、代理链、快捷键、备份窗口等）已经有自己的 `hint_`，它们在对话框里断开连接、自带 pending 标志，再补标识只会重复；这些保持 `Unspecified`。判断标准：**结果是否需要落到某个页面或对象上**——需要才给标识。

### 2.2 三种承载位置

| 类 | 承载 | 生命周期 | 本片的操作 |
|---|---|---|---|
| 全局「最近操作」 | 状态栏第一段 | 成功 4 秒后消失，失败保留到下一次操作 | 导入、批量更新、启用/停止内核、内核更新、备份/恢复、外观、排除列表 |
| 全局「当前告警」 | 状态栏第二段 | 直到条件解除 | `state.lastError`、只读保护 |
| 页面提示 | 页面内 hint 行 | 直到页面状态改变 | 订阅更新/激活、规则保存、连接关闭、设置应用、代理总开关、路由方式、节点切换 |
| 聚合结果 | 页面内 hint，含「n 成功 / m 失败」+「查看」 | 直到关闭或下一次批量 | 批量更新 |

**状态栏两段并存**：`state.lastError` 不再覆盖最近操作，两者同时显示时用「告警」前缀区分，且告警段点击打开「处理」菜单。条件解除（`lastError` 清空）时告警段自动消失——这就是 ADR 0046 写的「不再无条件占用状态栏」。

**页面提示只在目标页可见时显示**：操作由托盘或快捷键触发时用户可能不在该页，此时结果仍进「最近操作」，页面提示同时缓存，切回该页时可见（缓存即页面状态，不是新增的第二个事实来源）。

### 2.3 首页「对象内结果」与「意图/观测」

代理总开关是 P4.1 表里的「对象内结果」：意图由 `proxyMasterSwitch_->setProperty("pending", true)` 表达（已有的 R3 机制），结果由 `ProxyToggle` 标识回填到控制区提示行，不写状态栏。这与 ADR 0048「开关六态」一致，不新增状态。

节点切换（`MemberSelect`）结果落在首页节点区提示行；失败时成员已由 Controller 回滚，提示写明「已恢复为 <原成员>」。路由方式（`ModeSwitch`）同理落在控制区。

### 2.4 托盘

托盘失败气泡原本对**所有**失败弹出，包括用户正在对话框里看到的那条。改为：只有非页面级（`Unspecified`）且窗口不可见时弹气泡；页面级失败交给页面提示，避免同一句话出现两次。

### 2.5 内核实际规则读取失败

`refreshRules` 的失败已经由页面空态承担（R5）。这里去掉它额外的 `operationFinished(false, ...)`：同一事实只留一个承载位置。

## 3. 实施顺序

1. 信号与枚举（默认参数），全量 ctest 必须仍为 33 通过。
2. 状态栏拆两段 + 状态机（最近操作 / 当前告警）。
3. 逐命令补 `OperationId`：导入、更新、激活、批量、连接关闭、代理总开关、TUN、模式、节点、内核、备份/恢复、设置。
4. 页面提示接线（订阅、规则、连接、设置、首页控制区）。
5. 托盘气泡收窄；去掉重复的 rules 失败 emit。
6. 测试：新增 `FeedbackRoutingTests`，覆盖「页面级失败进页面不到状态栏」、两段并存、聚合结果、托盘不重复弹窗、默认参数兼容。

## 4. 验收点与结果

| 验收点 | 结果 |
|---|---|
| 页面级失败只出现在所属页面 | 已实施：`operationResultsGoToTheirOwnPage` 断言订阅更新失败写订阅页且状态栏不变 |
| 跨页结果进「最近操作」段 | 已实施：内核更新失败写状态栏 |
| 两段互不覆盖 | 已实施：`operationAlarm_` 独立于 `message_`，`state.lastError` 不再覆盖最近操作（N10） |
| 首页对象内结果 | 已实施：代理总开关失败写到控制区 `homeControlHint_`，不写状态栏 |
| 默认参数兼容旧调用 | 是：99 处 `emit` 中未标注的仍为 `Unspecified`，编译与既有 34 项测试全绿 |
| 归属表单测 | `feedbackTargetsStayExplicit` 逐条固定 |
| 完整 ctest | **35 项中 34 通过、0 失败、`system_test_guard` 跳过**（新增 `feedback_routing_tests`） |
| token 扫描 / 文档检查 | `0 hits`；文档 0 findings |

### 4.1 实施中修正的设计细节

1. **标识不能由 `begin()` 全权决定**。最初设想在 `begin()` 里读「当前命令」的标识，但拒绝路径（参数校验失败、只读保护）根本不会进 `begin()`，那些结果仍然没有归属。改为 `OperationScope`：命令开头声明，`refuseOperation`/`reportOperation`/`begin`/`finish` 都读同一个作用域值，作用域退出即复位。
2. **「无写入的成功」不是失败**。重复关闭代理原本以成功返回「代理已关闭」；第一版把它当拒绝处理（恒返回失败），`proxyMasterSwitchRollsBackAndKeepsCoreRunning` 立即转红。改为 `reportOperation(true, …)`。这是测试抓到的，不是推演出来的。
3. **`TunToggle` / `ProxyToggle` 属首页**，不属于设置页。首页 TUN 开关是控制区的一行，结论应落在那里。
4. **设置应用的「成功」不作为页面提示**。「设置已应用」若写进 `settingsFeedback_`，会一直压住「有 n 项需要重启的修改」并破坏既有断言；成功时清空草稿提示即可，失败才保留原因。
5. **规则保存是独立标识 `RulesSave`**，不是 `SettingsApply`——它有自己的结论行 `rulesHint_`。
6. **首页控制区提示不能放进 `overview` 面板**。第一版放进去后，「请先激活订阅」这类结论恰好出现在 `overview` 被隐藏的时候，提示跟着一起消失。测试用可见性断言抓到了这个真实缺陷，已移到面板外层。

## 5. 未验

- 真实 Windows 托盘气泡与页面提示同时出现时的观感；
- 读屏对两段状态栏的播报顺序；
- 21 个对话框内部 hint 是否还有重复（它们保持 `Unspecified`，本次未逐个复核）。
