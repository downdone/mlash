# R9 施工记录：Token 检查与硬编码清零

> 归属：[界面分层设计](ui-layered-design.md#实施切片) 的 R9 片，规则见 [6.3 应用规则](ui-layered-design.md#63-应用规则)。

## 1. 实施内容

### 1.1 `scripts/check_ui_tokens.py`

按规则 7 落地检查脚本，报告四类绕过 `UiTheme` 的硬编码：

| 类别 | 规则 | 说明 |
|---|---|---|
| `literal-color` / `literal-hex` | `QColor("#…")`、`QColor(Qt::white)`、`QColor(0x…)` | 字面色值 |
| `font-size` | `setPointSize(n)`、`setPixelSize(n)`、内联 `font-size: n` | 代码设定字号 |
| `focus-padding` | QSS 里用 padding 变化代替焦点环 | 防跳写法 |
| `inline-stylesheet` | 每次刷新 `setStyleSheet(...)` | 应用属性 + QSS 状态选择器代替 |

白名单按规则 7：`UiTheme.*`、`UiIcons.*`、`WidgetStyle.*`、`NodePresentation.*`、`MainWindowParts.*`、`CompactComboBox.*`、`ProxyTableModel.*`、`tests/`，以及订阅二维码的 `Qt::white/black` 单行豁免（功能性黑白，必须高反差才能扫描）。

返回码 0 = 无命中，1 = 有命中，可直接进 CI。

### 1.2 迁移

脚本在改动前的分支上跑出 6 处命中；迁移后为 **0 处**：

| 位置 | 原实现 | 现实现 |
|---|---|---|
| `MainWindow.cpp` 侧栏状态点 | `setStyleSheet("color:%1;")` 拼颜色 | `setProperty("state", …)` + QSS `[state="…"]` |
| `MainWindow.cpp` 首页状态点 | `setStyleSheet("color:%1; font-size:18px")` 写死颜色与 px 字号 | 同上，字号走 `@SectionPoints` token |
| `MainWindowProxy.cpp` 节点卡片 | 内联 QSS 覆盖列表背景与边框 | `setProperty("flat", true)` + QSS |
| `WebsiteTestsPage.cpp` 延迟文字 | 每次刷新 `setStyleSheet` 重设颜色 | 状态属性 + QSS，颜色同时表达成功/失败/取消/等待 |
| `OutboundDialog.cpp` | `setPointSize(17)` 写死、不随文字比例缩放 | `UiTheme::ValuePoints` |

## 2. 验收点与结果

| 验收点 | 结果 |
|---|---|
| 脚本对改动前代码跑出命中清单 | 6 处（字面量/字号/内联样式），与设计点名的位置一致 |
| 迁移后零命中 | `python scripts/check_ui_tokens.py` → `0 hits` |
| 迁移不改变行为 | 完整 ctest 34 项中 33 通过、0 失败、1 跳过 |
| 状态仍由文字与颜色双通道表达 | 是：状态点的 `state` 属性同时驱动颜色，文本行照常给出语义 |

### 1.3 高对比第四套取值

`applyAppearance` 原先把 HC 只用于边框，其余角色沿用普通取值。现在 HC 是**独立的第四套并行取值**（浅/深 × 普通/高对比），表面分级、选中底、开关轨道与焦点环同时改写。HC 下表面不再"逐级变亮"，改靠边框与明度差区分。

逐对复算（WCAG 2.x 相对亮度，脚本实算）：

| 配对 | HC 浅 | HC 深 | 门槛 |
|---|---|---|---|
| 主文字 / 表面 | 21.00 | 19.03 | 4.5 |
| 次文字 / 表面 | 17.40 | 19.03 | 4.5 |
| 弱文字 / 表面 | 12.63 | 15.25 | 4.5 |
| 边框 / 表面 | 21.00 | 19.03 | 3.0 |
| 主色底上文字 | 10.04 | 14.88 | 4.5 |
| 焦点环 / 底色 | 5.69 | 14.88 | 3.0 |
| 选中竖条 / 选中底 | 7.99 | 9.38 | 3.0 |
| 开关轨道 / 表面 | 8.86 | 19.03 | 3.0 |
| 成功色 / 表面 | 8.98 | 14.79 | 4.5 |

`semanticContrastAndArtwork` 由原来的「4 套外观 × 7 前景 × 3 背景」扩展为同时断言焦点环、悬停底，并在 HC 下额外断言边框、轨道与选中底。

### 1.4 图标绘制器合并

`navigationIcon(NavKind)` 不再自绘描边，改为调用 `UiIcons::icon()`；`UiIcons` 新增 `Home` / `Chart` / `List` 三个分区字形，映射为 首页→Home、配置→Folder、数据→Chart、设置→Settings。两套绘制器由此收敛为一套（6.2c），描边宽度与尺寸一致。

## 2. 验收点与结果（补充）

| 验收点 | 结果 |
|---|---|
| HC 逐对复算达标 | 9 对全部达标，值记入上表 |
| HC 有自动断言 | `semanticContrastAndArtwork` 扩展后通过 |
| 图标只有一套绘制器 | `navigationIcon` 已改调 `UiIcons` |
| 完整 ctest | 34 项中 33 通过、0 失败、1 跳过 |
| token 扫描 | `0 hits` |

**未验**：HC 与 125%/150% 缩放下的人工视觉复核；读屏播报；真实 Windows 实机。
