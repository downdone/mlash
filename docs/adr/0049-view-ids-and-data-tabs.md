# ADR 0049：视图编号收敛为 5 个，数据页合并为四页签

2026-11-02 · 已接受（切片 K1、K6 落地）。本 ADR 取代 [ADR 0046](0046-navigation-routability-and-feedback-routing.md) 中「设置三分类均为可寻址视图」与「分区页签承载视图名」的适用范围，以及 [ADR 0048](0048-home-merged-proxy-switch.md) 中「`QStackedWidget` 保留索引 2 映射到首页」的实施细节；ADR 0047「网站测试归数据」不变，但网站测试不再是独立视图，而是数据页的第一个页签之一。分区名、四分区数量、持久化 schema 与 Controller 命令集合**不变**。

## 原因

用户 2026-11-02 的指令要求按 GPT 原型逐屏照做。原型的事实是：

1. **侧栏只有四个入口，数据页是一个页面带四个页签**（流量 / 连接 / 测试 / 日志），不是三个并列视图。旧模型把「连接」「日志」「网站测试」当作三个可寻址视图，导致「数据」分区下的三个页签在 `QStackedWidget` 里占三个索引，切换页签等于切换视图——页签的语义（同页内换一块内容）和实现（换页面）不符。
2. **设置页没有分类页签**，三组卡片同屏平铺。旧模型把三个分类当作可寻址视图，深链需要 `(6, category)` 复合键，`ensureWidgetVisible` 还要做视口定位——把这三组留在同一个滚动面里，深链只需滚动到对应卡片。
3. **视图编号与旧页号混在一起**是上一轮多起缺陷的共同根因：`ViewIds::Rules == 2` 与旧编号「2 = 首页节点」相撞，`navigate()` 与 `PageRouter::go()` 各留了一次历史猜测；侧栏条目存视图 ID（0/1/3/4），点击处理却交给按旧编号解释的 `setPage()`，把「数据」送到规则页、「设置」送到数据页。

## 决定

**视图编号收敛为 5 个，按侧栏顺序编号。**

| 视图 | ID | 旧编号（兼容入口） |
|---|---|---|
| 首页 | 0 | 0（控制区）、2（节点锚点） |
| 配置 | 1 | 1 |
| 规则 | 2 | 3 |
| 数据 | 3 | 4（连接）、5（日志）、7（测试） |
| 设置 | 4 | 6 |

- 旧编号**只在** `ViewIds::fromLegacy()` 里出现一次，用于 `startupPage`、托盘、`GuiHost::show(int)`、订阅链接与设置搜索这些整数入口；任何代码都不得对视图 ID 做页号算术或历史猜测（`navigate()` / `PageRouter::go()` 中原本的「view==2 → 首页」已删除）。
- 数据页的四个页签用 `Route::tab` 表达（`-1` 沿用当前页签），**非数据页的 `tab` 恒为 `-1`**。设置的分组用 `Route::category` 表达（`kNoCategory = -1` 沿用当前分类），分组不是视图。
- 页面栈只有 5 个索引（`ViewIds::Count`）；`stackIndex(int view)` 恒等返回，所有调用点仍经它。
- 侧栏条目存视图 ID，点击直接 `navigate(Route{视图 ID})`，不再经过 `setPage()` 的旧编号解释。

## 影响与约束

- **可见性按页签 OR 合成。** 数据页只有真值来源：进入数据视图时按当前页签调用 `setConnectionsVisible` / `setDiagnosticsVisible` / `setWebsiteTestsVisible`，切换页签时同步三者，离开时三者全 false。旧编号 4/5/7 仍可到达对应页签，只是不再各自启动采样。
- **旧数据不迁移。** `startupPage` 的 0–7 取值原样保留，读入时经 `fromLegacyPage()` 归一化；`toLegacyPage()` 负责反向回显。因此旧编号的 `Route` 不会被写进新状态。
- **设置深链不再要求独立视图。** 托盘/搜索以 `(Settings, category)` 直达，落地行为是「选中该分类并滚到对应卡片」；分类保留在路由里，同一视图内的下一次导航不会把视野拨回顶部。
- **规则视图没有侧栏条目。** 它只能经托盘、设置搜索与订阅链接到达；此时侧栏保持上一条可归属分区的选中态（`syncSectionNavigation`），不额外造第五个入口。
- **本 ADR 不改 Controller，不改命令集合，不改 YAML/持久化 schema**（`startupPage` 的数值域不变）。

## 验证

- `ViewIds` 与 `fromLegacyPage` / `toLegacyPage` 的往返由 `UiTests::routesCarryHomeAnchorAndSettingsCategory` 固定：`navigate(Route{2})` 落规则，`setPage(2)` 落首页 + `HomeAnchor::Nodes`。
- 侧栏逐行导航由 `WebsiteTests::controllerNavigationAndGuiLifetime` 固定：点第 n 行后 `currentRoute().view == item(n)->data(Qt::UserRole)`。
- 数据页页签由 `UiTests::routesCarryHomeAnchorAndSettingsCategory` 与 `WorkflowDepthTests` 固定：`setPage(5)` 落数据页日志页签、`setPage(7)` 落测试页签。
- 未验：真实 Windows 实机上侧栏与页签的键盘焦点顺序、读屏播报。
