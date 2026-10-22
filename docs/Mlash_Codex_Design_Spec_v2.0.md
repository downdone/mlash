# Mlash — 产品需求、系统架构、交互与 Codex 实施规格

> **版本**：V2.0 · 2026-10-13<br>
> **项目名称**：**Mlash**（官方建议读音：**Em-lash**）<br>
> **产品描述**：A lightweight, cross-platform mihomo client.<br>
> **设计基线**：**C++20 + Qt 6 Widgets + Qt Network + mihomo（独立内核进程）**<br>
> **交付目标**：Windows 10/11 x64 V1.0；在架构上预留 macOS / Linux。<br>
> **文档用途**：Codex 直接读取并逐阶段实现；兼做设计评审、开发和验收依据。<br>
> **状态**：需求与架构已收敛；API 细节、性能目标及跨平台行为须以所锁定版本的官方资料和实测确认。<br>

---

# 0. Codex 执行总指令（必须首先阅读）

你是本项目的 **C++/Qt 桌面客户端架构师、开发工程师、测试工程师**。目标是在当前仓库完成一个可运行、可构建、可测试、可发布的 **Mlash V1.0**，而非仅生成新的方案或一组 TODO。

## 0.1 执行约束

1. **先检查实际环境**：检查代码仓库、CMake、编译器、Qt 版本及模块、yaml-cpp、测试工具和可用 mihomo 文件；列出已存在/缺失的依赖。不得假设环境中有 Windows SDK 或已安装 Qt。
2. **技术基线固定**：使用 **C++20 + Qt 6 Widgets / Core / Network + yaml-cpp + CMake**。禁止把 WTL、WinForms、Slint、Tauri、Electron、QML、Qt Quick、Qt WebEngine、数据库或微服务引回主方案。Windows 原生 API 仅限 platform/windows 层，并可按需使用 WIL；其他平台不依赖 Windows 头文件。
3. **品牌全部统一为 Mlash**：用户可见名称 `Mlash`，程序 `Mlash.exe`，项目目录/包名 `mlash`。不遗留旧名称 `MiniProxy` / `Mini Proxy`（迁移说明中允许出现）。
4. **按 M0→M1→M2→M3→M4→M5 实施**。先给出仓库现状、实施计划与测试路径，然后动手编码；优先完成核心控制和系统恢复，再完成 GUI。阶段间必须有真实的验收结果。
5. **先做能运行的 Windows 版本**：抽象 `platform` 接口，但 macOS/Linux 仅交付清晰的适配规范和可编译的受控 `Unsupported` 行为（若必要），禁止为凑“跨平台”而写出伪实现或宣称其他平台功能可用。
6. **所有 UI 非阻塞**：网络、配置验证、文件 I/O、内核启停均异步处理；不在 Qt GUI 主线程中调用阻塞性等待或长任务，不使用嵌套 `QEventLoop` 假装异步。
7. **状态必须真实**：UI 区分用户期望、内核实际状态、系统代理实际状态及节点可用性。超时后回读状态；禁止“点击成功就认为操作成功”。
8. **优先可恢复性与安全**：更新配置时保留 Last Known Good；系统代理改动前写恢复日志，恢复时做所有权比对；不覆盖其他软件新写入的代理设置。
9. **严格区分系统代理与 TUN**：V1.0 仅对遵循系统代理的应用生效。不提供 TUN、不要求常驻管理员权限、不承诺所有流量被接管。
10. **真实验证要求**：只记录已实际运行的命令与结果。非 Windows 环境可以编写代码、执行适用的静态检查/跨平台单测，但不得声称已通过 Windows 代理切换、托盘、安装包等实机验收。
11. **外部程序安全**：不可从未知站点自动下载/执行 mihomo；缺少用户提供或官方核验的二进制时，通过 Fake Core / Mock HTTP 服务器进行测试。正式分发前核验实际内核发行标签及许可证。
12. **源码和测试优先**：保持代码规模与依赖最小，禁止提前引入插件化、复杂 DI 容器、任意脚本执行、云端服务、自动更新框架。
13. **维护验证报告**：每一阶段更新 `docs/verification-report.md`，包含命令、环境、通过/失败/未执行项、已知问题、下一阶段工作。不能编造性能数据。

## 0.2 验收总原则

- 首次启动、无订阅、无内核、网络离线、代理端口占用、订阅异常、内核崩溃等场景均有确定行为。
- Windows V1.0 的主使用链：**URL/文件导入 → 配置验证 → 启动内核 → 启用系统代理 → 策略组与节点切换 → 订阅更新 → 关闭/退出并恢复**。
- GUI 符合 **原生、清晰、少装饰、低后台开销**。允许使用 Qt 标准控件和少量布局；不画流量图、不制作大面积节点卡片、不做动态动画。
- 每个里程碑给出可演示结果与明确 DoD；不能把占位按钮或 Fake Core 冒充真实实现。

---

# 1. V2.0 相对于旧版的设计变更

| 维度 | 旧版 Mini Proxy V1.0 | 新版 Mlash V2.0 | 决策 |
|---|---|---|---|
| 名称 | Mini Proxy | **Mlash（Em-lash）** | 正式统一 |
| 主语言 | C++20 | C++20 | 保持 |
| GUI | WTL + Win32 | **Qt 6 Widgets** | 替换 |
| 网络 | WinHTTP | **Qt Network / QNetworkAccessManager** | 统一异步网络接口 |
| 原生系统 API | 全局大量直接使用 | **隔离到 Platform Adapter** | 预留跨平台 |
| 平台 | 仅 Windows | **Windows 首发，macOS/Linux 规划** | 分阶段 |
| 主界面 | 托盘 + 1~2 个对话框 | **四个简洁功能页 + 托盘** | 功能扩充 |
| 代理页 | 基本节点选择 | **策略组、搜索、排序、测速** | P0 增强 |
| 订阅页 | URL/YAML 导入 | **URL + 本地文件 + 多订阅 + 切换/更新/回滚** | P0 保留 |
| 诊断 | 基本日志 | **P0 日志，P1 活动连接/规则查看** | 可按需加载 |
| 设置 | Win32 简单对话框 | **Qt Widgets 单页设置** | 精简保留 |
| TUN / 脚本 / WebUI | 暂不支持 | 暂不支持 | 保持 |

**重要**：用户确认采用 Qt 是为了多页面管理效率和未来跨平台，而非要求花哨 UI。无需把 Clash Verge 的全部功能复制进来。

---

# 2. 需求与设计目标

## 2.1 业务背景与产品定位

成熟 mihomo 内核已实现代理协议、策略组、DNS、规则、节点测速和外部控制 API。Mlash 将其包装为低资源、交互高效、可靠的桌面客户端。目标用户希望日常只用几次点击完成订阅、模式和节点管理；异常时能定位问题，无需掌握内核复杂配置。

**产品一句话**：**Mlash：简洁、稳定、跨平台演进的 mihomo 图形客户端。**

## 2.2 用户需求（UR）到可验证系统需求（SR）

| UR | 需求 | SR | 转换后的工程要求 | 验收依据 |
|---|---|---|---|---|
| UR-01 | 轻量、快速 | SR-01 | Qt Widgets 最小模块、按需页面/刷新，托盘常驻 | CPU/内存/启动时间实测 |
| UR-02 | 简洁、直观 | SR-02 | 四页面；高频操作在代理页或托盘；配置字段最少 | 操作路径/可用性测试 |
| UR-03 | 核心功能完整 | SR-03 | URL/文件订阅、多 Profile、策略组/节点/测速/模式 | API 与 UI 端到端测试 |
| UR-04 | 稳定运行 | SR-04 | 内核状态机、有限重试、错误可见 | 故障注入、72h 稳定性 |
| UR-05 | 代理设置可靠 | SR-05 | 系统代理所有权、快照、日志、冲突恢复 | 系统实机测试 |
| UR-06 | 可维护、易部署 | SR-06 | 单 GUI/Controller 进程 + 内核进程；本地文件；CMake | 干净机器部署 |
| UR-07 | 后续跨平台 | SR-07 | Platform Adapter 隔离 OS；Qt UI/Core 可复用 | 非 Windows 编译与适配评估 |
| UR-08 | 必要诊断 | SR-08 | 可读日志、错误复制、按需连接查询（P1） | 指定异常定位场景 |

## 2.3 非功能指标：研发预算，非已测结论

| 指标 | 初始门槛 | 口径 |
|---|---|---|
| GUI+Controller 空闲内存 | 优先争取 P95 ≤ 55 MiB，验收前实测修订 | 不含 mihomo；记录 Private Bytes 和 Working Set |
| 空闲 CPU | 10 分钟平均 ≤ 0.5% | 无窗口交互、托盘运行 |
| 托盘初次可操作 | Windows 冷启动目标 ≤ 1.5 s | 进程创建至菜单可操作 |
| 内核就绪 | 本地有效配置、无首次下载目标 ≤ 5 s | API 与代理监听状态就绪 |
| 托盘交互 | P95 ≤ 100 ms | 菜单响应，不包含网络完成时间 |
| 长时间运行 | ≥ 72 h，无崩溃和持续增长趋势 | 同一配置、规则和测试环境 |
| 依赖 | 不要求数据库、WebView、系统级常驻服务 | 安装/解压可运行 |

此处不再沿用 WTL 版本 Controller 的 35 MiB 目标作为 Qt 的必达值；必须与 Clash Verge Rev 在同一内核、同一配置、同一系统下对照测试。记录 Controller 和内核占用，勿混为一个值。

---

# 3. 功能范围（P0/P1/P2）

## 3.1 P0：Windows V1.0 必须具备

| 功能域 | ID | 功能 | DoD / 行为 |
|---|---|---|---|
| 内核 | F-CORE-01 | 启停、状态监测、异常恢复 | 能区分正常停止与崩溃、健康检查真实 |
| 代理 | F-PROXY-01 | 系统代理启停 | 接管前快照；退出恢复；冲突保护 |
| 代理 | F-PROXY-02 | Rule / Global / Direct | 操作后回读内核实际模式 |
| 代理 | F-PROXY-03 | 策略组/成员选择 | 保留多个策略组，不错误扁平化 |
| 代理 | F-PROXY-04 | 搜索、名称/延迟排序 | 大量节点仍可快速使用 |
| 代理 | F-PROXY-05 | 单节点与按需组测速 | 异步、取消、超时，标识未测/失败 |
| 订阅 | F-PROFILE-01 | URL 导入完整 YAML | URL 安全、下载、格式/内核校验 |
| 订阅 | F-PROFILE-02 | 本地 YAML 文件导入 | `.yaml`/`.yml`，导入快照 |
| 订阅 | F-PROFILE-03 | 多 Profile | 切换、命名、删除、当前态显示 |
| 订阅 | F-PROFILE-04 | 手动更新与安全回滚 | 更新失败不破坏 Last Known Good |
| 诊断 | F-DIAG-01 | 应用/内核日志查看 | 按需显示、搜索、复制、定位文件 |
| 设置 | F-SETTING-01 | 开机自启、静默启动 | 系统状态与 UI 一致 |
| 设置 | F-SETTING-02 | 启动时代理状态恢复 | 首次启动默认不接管；冲突不覆盖 |
| 设置 | F-SETTING-03 | Mixed 端口、日志等级 | 配置校验与变更事务 |
| 设置 | F-SETTING-04 | 版本信息、配置/日志目录 | 诊断基础入口 |
| 桌面 | F-DESKTOP-01 | 系统托盘、快捷菜单 | 关闭窗口≠退出；真退出安全恢复 |
| 存储 | F-STATE-01 | 应用设置与恢复日志 | 原子更新、损坏恢复、单实例 |

## 3.2 P1：V1 通过后，价值明确再增加

- 自动定时更新订阅、订阅到期/流量信息（仅当源返回标准字段）。
- 活动连接只读查看（目标、进程、链路、规则、上传/下载），关闭单连接需单独授权操作。
- 规则只读检索；**不实现规则编辑器**。
- 简单节点连通性测试；诊断导出时自动脱敏。
- 最近使用节点、批量延迟测试进度、日志暂停。
- macOS 适配验证（不与 Windows V1.0 交付捆绑）。

## 3.3 P2 / 明确不做

- TUN、系统 VPN、透明代理、管理员常驻服务。
- Merge/Script 等自定义执行脚本、复杂 YAML 可视化编辑器、通用订阅格式转换。
- 流量曲线/仪表盘、完整连接分析器、服务“解锁”矩阵和联网统计分析。
- 主题引擎、CSS 注入、QML、Web Dashboard、浏览器嵌入、WebDAV。
- V1.0 不实现自动升级、远程控制开放、局域网代理共享。

**需求边界**：首版支持 **mihomo 可解析的常用完整 YAML**；不承诺支持所有第三方订阅格式或所有高级配置特性。导入不可信 YAML 需要安全约束，详见第 9 节。

---

# 4. 已有能力与技术栈

## 4.1 复用基础

- **mihomo**：本地代理入口、节点与策略组、规则、DNS、Provider、API、延迟测试、日志/连接查询。
- **Qt 6 Core/Widgets/Network**：跨平台事件循环、系统托盘、原生风格 UI、Model/View、HTTP、JSON、进程、配置路径、异步信号。
- **Windows**：WinINet 系统代理、登录自启、进程/job、DPAPI 与权限管理。
- **macOS/Linux**：未来通过平台适配实现，不在 V1 直接启用系统更改。

## 4.2 技术选型与禁止项

| 层 | 方案 | 说明 |
|---|---|---|
| 语言 | C++20 | Qt 主应用与业务代码统一 |
| UI | Qt 6 Widgets | QMainWindow、QStackedWidget、QTableView、QDialog |
| 托盘 | QSystemTrayIcon + QMenu | Qt 管理界面事件循环 |
| 网络 | Qt Network | QNetworkAccessManager 异步 HTTP；需要流式诊断时按文档处理 |
| Core 进程 | QProcess + OS Adapter | 统一生命周期，Windows 特殊控制下沉 |
| 数据 | QJsonDocument / QJsonObject | 简单 JSON 文件及 API |
| YAML | yaml-cpp | 结构化覆盖受控字段；验证锚点/别名兼容性 |
| 线程 | QObject 线程亲和性 + QThread（必要时） | 优先异步事件，不无谓开线程 |
| Windows 系统接口 | Win32 + WIL（可选） | 仅 `platform/windows` 中使用 |
| 构建 | CMake + Qt CMake API | `AUTOMOC/AUTOUIC/AUTORCC` |
| 测试 | Qt Test + Fake Core + 真实内核集成测试 | GUI、API、状态与故障测试 |
| 部署 | Windows `windeployqt` + portable ZIP | 正确打包 Qt 平台插件 |

**Qt 版本**：将 Qt **6.12.x 作为当前候选基线**，在构建环境实际可用且验证通过后锁定精确 patch 版本。Qt 官方说明 Qt 6.12 支持 Windows 10 1809+、Windows 11，并且是计划中最后支持 Windows 10 的 Qt 分支。不要使用缺少依据的 `find_package(Qt6)` 无限制升级策略。参见官方资料部分。

**许可证**：Qt Widgets 的开源许可涉及 LGPLv3 等条款；公开/商业分发必须评估动态链接、许可证通知、替换/重新链接权利等适用义务。mihomo 需检查**所捆绑版本的实际发布标签和源码**的许可证，不要被 GitHub 默认分支不相关许可证文件误导。Mlash 与 mihomo 作为独立进程的组合分发关系，需要按具体实现评估；不得断言“进程分开就不用遵循任何 GPL 义务”。

---

# 5. GUI 交互与原型（简约而不简单）

## 5.1 信息架构

正式主窗口 **四个导航页面**：

1. **代理**：系统代理、模式、当前 Profile、策略组/节点、搜索、测速。
2. **订阅**：URL/文件导入、多 Profile、切换、手动更新、删除、错误提示。
3. **诊断**：P0 应用/内核日志；P1 活动连接与规则只读查询。
4. **设置**：启动、自启、端口、日志级别、目录与版本。

原则：**优先系统原生风格、紧凑布局、避免视觉噪声**。不做每个节点独立 QWidget 卡片、不做图表、不做大量分组卡片、不做首页和代理页重复内容。

### 主窗口布局草图

```text
┌──────────────────────────────────────────────────────────────────┐
│  Mlash                                               ─   □   ×  │
├─────────────┬────────────────────────────────────────────────────┤
│             │                                                    │
│  ● 代理     │  当前订阅 [ Daily                      ▼ ] [更新]  │
│    订阅     │  内核 ● 运行中        系统代理 [✓ 启用]             │
│    诊断     │  运行模式 (● 规则  ○ 全局  ○ 直连)               │
│    设置     │                                                    │
│             │  策略组 [ 节点选择                ▼ ] [延迟测试]    │
│             │  搜索 [ 搜索名称 / 地区...                  ]       │
│             │ ┌──────────────────────────────────────────────┐ │
│             │ │  节点名称               类型           延迟    │ │
│             │ │ ● 香港 01                SS            42ms    │ │
│             │ │   日本 02                SS            68ms    │ │
│             │ │   美国 03                Trojan       180ms    │ │
│             │ │   自动选择               URLTest        —      │ │
│             │ └──────────────────────────────────────────────┘ │
│             │                                                    │
├─────────────┴────────────────────────────────────────────────────┤
│  mihomo <实际版本>  ·  127.0.0.1:<实际端口>  ·  无错误               │
└──────────────────────────────────────────────────────────────────┘
```

> 注意：列表中类型、延迟等均为**占位示例**，真实 UI 必须通过 mihomo API 与当前配置获取。Rule 模式中的策略组可能分别选择不同成员，不能把页面当作“一个全局节点”。

## 5.2 代理页

- 顶部：订阅选择与更新、内核状态、系统代理开关、规则/全局/直连。
- 中部：策略组 `QComboBox`；节点 `QTableView + QAbstractTableModel + QSortFilterProxyModel`；按组搜索/排序。
- 支持高亮当前选中成员；对自动策略组区分 **当前实际选择** 与 **可手动修改的配置**。
- 点击节点可直接选择，异步确认后更新 UI；处理中显示轻量状态，不阻塞其他只读查询。
- 仅用户手动触发测速，未测为 `—`，超时/失败与 `0 ms` 明确区分。
- 避免对策略组不停全量重载；API 查询在页可见时或状态变化时发生。

## 5.3 订阅页

```text
订阅管理
────────────────────────────────────────────────────────
[ 添加订阅 ▼ ]   [ 更新选中 ]   [ 删除 ]
                  ├─ 从 URL 导入...
                  └─ 从配置文件导入...

名称                  来源          最近更新        状态
● Daily                URL           10-13 10:00     当前
  Backup               YAML 文件     —              可用
  Work                 URL           10-10 08:00     更新失败(保留旧版)

[ 激活订阅 ]                                      [打开目录]
```

- URL 导入：名称（可自动推导）、URL、添加；默认只接受 http/https 且优先 HTTPS，验证来源策略。
- 文件导入：`QFileDialog` 选择 `.yaml/.yml`，保存导入快照；源文件被移动/删除后仍可使用导入版本。
- 编辑/删除：支持修改名称，删除当前激活订阅前必须先切换或安全停止代理。
- 更新：仅 URL 来源默认支持“更新”；本地文件提供“重新从文件导入/替换”，不要假装是远程自动更新。
- 显示活动版本、上次更新、失败原因；失败时继续使用 Last Known Good。
- 页面切换过程中“更新”和“激活”是不同动作；更新成功**默认不擅自覆盖当前运行态**，由策略控制（可在候选配置通过验证后提示激活）。

## 5.4 诊断页

V1.0 仅提供 **日志**（应用日志 + 可按需订阅的内核实时日志）：

- 过滤级别、搜索文本、复制选中、打开日志目录、清空当前视图。
- 日志量有上限，历史记录采用滚动文件（例如 5×5 MiB，可配置）；敏感信息脱敏。
- 打开该页时才获取较详细的内核日志；页面隐藏后停掉实时流（除明确需要的异常状态检查）。

P1 增加内部 Tab：**活动连接 / 规则（只读）**。

- 活动连接：目标、进程（如果 API 提供）、链路、命中规则、上下行；只在可见时刷新。
- 规则：只读查询，不能直接通过 UI 修改订阅规则。
- 用户点击“关闭连接”需明确操作且优先单连接，不默认做“一键关闭全部连接”。

## 5.5 设置页

建议一个紧凑页面，设置如下：

| 分组 | 控件/字段 | 默认值 | 生效方式 |
|---|---|---|---|
| 应用 | 开机自启 | 关 | OS 适配器立即应用 |
| 应用 | 静默启动 | 开 | 下次启动生效 |
| 应用 | 启动时恢复上次系统代理期望 | 开（首次运行仍默认关闭代理） | 启动时安全检查后执行 |
| 代理 | 本地 Mixed Port | 17890（候选） | 校验端口；必要时重启内核并更新系统代理 |
| 诊断 | 日志等级 | Warning | 更新配置或重启方式按所锁内核版本确认 |
| 信息 | 内核/客户端版本 | 实际值 | 只读 |
| 信息 | 打开配置目录/日志目录 | — | 按钮 |
| 信息 | 一键复制本地代理地址 | — | 按钮 |

内部自动管理：API Controller 端口、随机 Secret、局域网访问关闭、TUN 关闭、日志轮转、LKG 版本。DNS/IPv6/统一延迟原则上尊重订阅或采用经过验证的受控策略，**不能静默改变原始网络语义**。

## 5.6 系统托盘

```text
Mlash · 运行中
当前：Daily
──────────────────
✓ 系统代理
代理模式 ▶ 规则 / 全局 / 直连
策略组   ▶ 节点选择 / AI 服务 / ...
订阅     ▶ Daily / Backup / 更新当前订阅
──────────────────
打开主窗口
退出 Mlash
```

- 托盘使用 `QSystemTrayIcon`、`QMenu`，主窗口可延迟创建。
- 默认点击托盘图标唤出主窗口，右键打开快捷菜单；平台差异需要适配。
- 关闭主窗口默认隐藏到托盘；真正退出通过托盘菜单“退出”或设置的显式操作执行清理。
- 发生异常必须可发现：提示气泡仅用于重要异常，不持续骚扰。
- macOS 和 Linux 对托盘激活及菜单展示细节不同；使用 `QSystemTrayIcon::isSystemTrayAvailable()` 验证环境能力，不能承诺所有 Linux 桌面都有系统托盘。

## 5.7 体验约束

- 原生布局优先：QFormLayout、QVBoxLayout、QSplitter、QTableView、QDialog。
- 支持中文 Unicode，窗口缩放按 Qt DPI 机制处理；首次默认中文，国际化 P1 扩展。
- 正常成功操作不滥用弹窗；失败提示可定位到订阅/节点/系统层并支持复制错误。
- UI 空状态必须设计：无订阅、无节点、离线、Core 无法运行、代理被外部修改、系统托盘不可用。
- 不在网络请求后直接操作已经析构的 QObject；所有回调、取消和线程归属必须可靠。

---

# 6. 总体架构与模块设计

## 6.1 逻辑分层

```text
┌─────────────────────────────────────────────────────────┐
│ Qt 6 Widgets UI                                         │
│ Tray / ProxyPage / ProfilesPage / Diagnostics / Settings │
└─────────────────────────┬───────────────────────────────┘
                          │ AppCommand / AppStateChanged
┌─────────────────────────▼───────────────────────────────┐
│ Application Controller                                  │
│ 唯一写状态协调者 / 命令串行化 / 操作取消与 generation      │
├─────────────────────────────────────────────────────────┤
│ CoreManager   MihomoClient   ProfileManager   Settings  │
│ RecoveryManager / Diagnostics                           │
└──────────────┬──────────────────────┬───────────────────┘
               │                      │ IPlatformServices
   REST / Process / Files             ▼
┌──────────────▼────────┐   ┌────────────────────────────┐
│ mihomo 独立进程        │   │ Windows / macOS / Linux    │
│ 代理/规则/DNS/Provider│   │ 系统代理 / 自启 / 进程保护   │
└───────────────────────┘   └────────────────────────────┘
```

**UI 不直接调用 Windows API 或控制 mihomo。** AppController 协调所有影响全局运行状态的动作，系统平台接口与 Core API 分离。

## 6.2 核心模块

| 模块 | 主要职责 | 不负责 |
|---|---|---|
| AppController | 单一命令执行、状态收敛、重试与操作结果 | 不直接画 UI |
| CoreManager | 进程启停/退出监听、就绪检查、运行状态同步 | 不解析订阅格式 |
| MihomoClient | 官方 REST API 的 HTTP 适配、状态对象 | 不自行代理流量 |
| ProfileManager | URL/文件导入、下载、校验、激活和回滚 | 不直接修改 OS 代理 |
| ConfigBuilder | 构造可控的运行 YAML | 不修改 source.yaml |
| SystemProxyAdapter | OS 当前用户代理状态、所有权判断、恢复 | 不负责策略组 |
| RecoveryManager | 恢复日志、操作记录、启动恢复 | 不在 UI 线程做同步阻塞 |
| SettingsStore | 本地持久化、版本迁移 | 不存复杂数据库 |
| DiagnosticManager | 应用日志、按需 Core 日志/连接查询 | 不无限制常驻采集 |
| Qt Widgets UI | 视图、原生交互、错误反馈 | 不直接控制系统设置 |

## 6.3 平台适配契约

概念接口（具体返回类型和异步封装可按代码需要调整）：

```cpp
struct ProxyEndpoint {
    QString host;  // 仅允许 loopback
    quint16 port;
};

struct ProxySnapshot;          // OS 特定代理设置的完整快照
struct OperationResult;         // code, message, details

class ISystemProxyAdapter {
public:
    virtual ~ISystemProxyAdapter() = default;
    virtual OperationResult readSnapshot(ProxySnapshot& out) = 0;
    virtual OperationResult enableIfAllowed(const ProxyEndpoint& endpoint) = 0;
    virtual OperationResult restoreIfOwned() = 0;
    virtual OperationResult inspectOwnership() = 0;
};

class IAutoStartAdapter {
public:
    virtual ~IAutoStartAdapter() = default;
    virtual OperationResult setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
};

class IProcessSupervisor {
public:
    virtual ~IProcessSupervisor() = default;
    virtual OperationResult configureChildLifecycle(qint64 pid) = 0;
};
```

建议通过 `PlatformServices` 聚合上述适配接口，实现由 CMake 按平台选择源码文件。主业务不出现 `#ifdef Q_OS_WIN` 分支和 Windows 类型。

## 6.4 各平台支持策略

| 平台 | 首版交付 | 系统代理实现方向 | 自启/守护方向 | 风险 |
|---|---|---|---|---|
| Windows 10/11 x64 | **V1.0 实现** | WinINet 当前用户连接代理选项 | HKCU Run / Windows Job 机制 | PAC、第三方接管、强杀恢复 |
| macOS | V1.1 评估 | SystemConfiguration / 网络服务代理偏好（按服务） | LaunchAgent 等 | 权限、网络服务切换、签名公证 |
| Linux Desktop | V1.2 评估 | GNOME/GSettings、KDE/网络环境适配 | XDG autostart / systemd --user 按需 | 无统一系统代理、托盘差异、桌面环境碎片化 |

Linux/macOS 不存在“一套 Qt API 即可强制全系统代理”的通用保证，跨平台功能完整性必须按平台实测并标示兼容矩阵。

## 6.5 Qt 异步模型与线程规则

- GUI 主线程运行 `QApplication::exec()`；`QSystemTrayIcon`、Widgets 均在 GUI 线程创建和更新。
- `QNetworkAccessManager` 默认异步；每个管理器及其 Reply 属于各自的 QObject 线程，不跨线程直接调用。
- Core 进程使用 `QProcess` 异步 `started`、`errorOccurred`、`finished` 信号处理。
- 文件下载、REST 请求使用 Qt 异步；复杂 YAML 解析/构建与外部配置校验放入受控工作任务，不阻塞 UI。
- 对影响运行状态的命令使用**逻辑串行协调**，不要求把每个简单操作都强行排到线程池；引入 `generation`/取消令牌识别过时回调。
- 重要状态变更触发 `AppStateChanged`，UI 订阅信号更新；不存在两套随意可写的全局状态。
- 本地 Core API 的网络管理器**显式 NoProxy**；订阅下载使用独立网络策略，避免客户端请求经过自身系统代理形成死循环。

---

# 7. 状态机与生命周期

## 7.1 三类状态不得合并

```cpp
enum class CoreState { Stopped, Starting, Running, Stopping, Failed };
enum class SystemProxyState { Disabled, OwnedByMlash, External, Unknown };
enum class ProxyMode { Rule, Global, Direct };

struct AppState {
    CoreState core;
    SystemProxyState systemProxy;
    ProxyMode mode;
    bool desiredProxyEnabled = false; // 用户期望，非 OS 实际状态
    QString activeProfileId;
    QString lastError;
    quint64 generation = 0;
};
```

系统代理开关反映 **系统实际设置**；应用中保留 `desiredProxyEnabled` 用于自动恢复意图。若 Core 崩溃而 Windows 仍指向其已停服务端口，界面必须显示故障并执行恢复程序。

## 7.2 Core 状态转换

```text
Stopped --启动--> Starting --就绪--> Running --停止--> Stopping --> Stopped
                        |                 |
                        +--失败--> Failed <--异常退出--+
                                     |
                                     +--有限重试--> Starting
                                     +--停止重试--> Stopped
```

就绪判断至少包括：进程已启动、`GET /version` 有效、`GET /configs` 可读、预期代理监听入口就绪。远端节点是否连通是独立指标，不作为 Core 启动成功的强制判据。

## 7.3 退出/崩溃边界

- 正常退出：停止新状态变更 → 安全恢复已接管的系统代理 → 停止 Core → 清理托盘、持久化结束。
- Core 非预期退出：第一时间将系统代理恢复到安全状态，然后有限次数重试；恢复后检查用户期望并且确认无第三方接管，才可重新启用。
- Controller 被强制结束：**无法保证立刻恢复系统代理**，依赖下次启动的恢复日志检测；子进程约束可使用 Windows Job，但 Job 本身不会替崩溃的主程序恢复系统代理。
- 操作系统注销/关机：尽最大努力清理，不承诺任意长时间同步完成；再次启动执行遗留状态检查。
- 普通网络不通/节点故障：不触发 Core 无限重启。

---

# 8. Windows 系统代理及恢复设计（P0 风险重点）

## 8.1 V1 范围

- 仅管理当前用户由 WinINet 提供的系统代理连接设置，不主动修改 WinHTTP 全局默认代理。
- 主要影响遵循系统代理设置的应用；不承诺所有 TCP/UDP 流量被接管。
- 初次启动时默认系统代理关闭；若已有企业 PAC、手动代理或其他客户端代理，默认不抢占。

## 8.2 接管前快照

`ProxySnapshot` 需要保存：当前连接范围/标识、手动代理配置、绕过列表、自动检测、PAC URL、相关标志与原始值、时间戳及本次 `ownership_token`（应用自己的日志标识，不是操作系统原子锁）。Windows 端按实际 API 结构持久化。

## 8.3 原子性边界与流程

```text
用户启用
 -> 核验 Core Running / 代理端口可连接
 -> 查询系统代理（包括 PAC/自动检测）
 -> 判断冲突 / 展示必要确认
 -> 持久化写前恢复日志（必须先落盘）
 -> 应用 Mlash 代理设置
 -> Windows 设置变更通知 / 刷新
 -> 回读确认 OS 实际设置
 -> 成功提交或失败恢复、告警
```

关闭时：先查当前 OS 设置是否仍**等于 Mlash 上次写入的受控状态**；一致才恢复快照。不同说明用户或其他软件接管，不覆盖新状态，报告“外部修改”。Windows 接口无法提供整组代理字段的跨进程原子 CAS，必须在读写后再次验证并接受无法完全消除的竞争窗口。

必要情形：端口切换、Core 重启、Profile 切换、应用退出/崩溃恢复都按相同所有权规则处理。

## 8.4 安全性与异常设计

- 发现企业策略限制/代理配置读取失败：拒绝或标注受限，不采用强制覆盖。
- 操作失败后保留恢复日志，允许 UI 提示打开系统代理设置。
- 启动时发现旧恢复日志：只恢复可确认属于本程序的设置；已经被外部更改时保留新状态。
- 单实例下也需要考虑其他代理程序和 Windows 设置应用同时修改的竞态。
- Windows 原生实现使用 WinINet 连接设置相关 API，文档中的示例不代替真实用户/网络配置测试。

---

# 9. 订阅与配置事务设计

## 9.1 两类来源，统一 Profile

```cpp
enum class ProfileSourceType { Url, LocalFile };
struct ProfileMeta {
    QString id;           // UUID，禁止用显示名拼路径
    QString displayName;
    ProfileSourceType sourceType;
    QString sourceValue;  // 敏感 URL 保密存储/展示脱敏
    QDateTime lastUpdated;
    QString lastGoodHash;
};
```

- **URL**：下载完整 YAML 到候选文件；支持显式更新。
- **本地文件**：复制一次快照到 Mlash 数据目录；使用该快照运行。文件变更自动监测 P1；V1 允许重新导入替换。
- 输入只支持完整 mihomo/Clash YAML；不提供 V2Ray/Base64/分享链接等格式转换。

## 9.2 三层配置模型

```text
source.yaml (原始订阅，不覆盖)
  + managed settings（仅受控系统字段）
  -> candidate.yaml（生成运行配置）
  -> mihomo 校验 -> 临时激活/运行检查
  -> active.yaml + last-good.yaml（成功提交）
```

受控字段至少包括 `mixed-port`、`external-controller`、`secret`、`allow-lan`、`tun` 及其他监听入口；必须定义未知配置字段的白名单/黑名单处理与告警。禁用未经授权的 `external-controller-*`、`external-doh-server`、`external-ui`、`port`、`socks-port`、`redir-port`、`tproxy-port` 等额外入口，避免下载配置重新开放管理面/本地服务。

不要随意改变远端订阅的 DNS、IPv6、路由规则和 Provider 语义；与安全边界冲突时应**明确提示不兼容或要求授权**，不可静默假装导入无损。

**重要**：yaml-cpp 对序列化顺序、注释、锚点、合并键/特殊标签的保真性可能不满足所有 YAML 需求。必须用真实订阅样本验证；对于无法可靠变换的配置，拒绝并报告，禁止静默破坏行为。

## 9.3 更新/切换工作流

1. 下载/读取源文件到临时候选；设置内容大小、时间、重定向和编码限制。
2. 使用 YAML 解析器检查语法和结构，构造受控运行配置。
3. 使用**实际捆绑版本** mihomo 的配置校验命令/CLI 参数验证；命令及 flags 必须先检查 `--help`，不能凭空假设。
4. 校验通过后保留候选、现用 active 与 Last Known Good。
5. 用户激活配置：先将本程序接管的 Windows 代理安全恢复，再停止/切换 Core；按新配置启动并验证 API 与代理端口。
6. 成功提交 active Profile；必要时恢复用户期望代理开关。
7. 失败：恢复旧配置并尝试重启旧 Core；仅在确认系统代理仍可安全接管时恢复；保留明确错误日志。

不要求无缝切换；**短时中断 + 可验证回滚**是 V1 的目标。

## 9.4 订阅 URL 与文件安全

- 优先 HTTPS，验证证书；限制下载大小、重定向跳数、超时和 URL Scheme。
- 为降低 SSRF 风险，默认拒绝远程订阅 URL 指向回环/内网地址以及重定向到这些地址；确有合法需求时通过单独明确授权处理，避免破坏合法本地测试场景。
- 下载 URL 的认证参数、响应敏感字段不进入普通日志；错误显示时脱敏。
- Provider/Rule Provider 的远程访问、缓存路径、本地路径和自动下载仍属于不可信配置输入，必须隔离到受控工作目录并限定能力。
- 在激活前检查不存在不预期的端口开放、外部控制或 TUN 接管。

---

# 10. 接口设计

## 10.1 GUI → Controller 命令

```cpp
enum class AppCommandType {
    StartCore, StopCore, SetSystemProxy,
    SetMode, SelectGroupMember, TestProxy,
    ImportProfileUrl, ImportProfileFile,
    UpdateProfile, ActivateProfile, DeleteProfile,
    UpdateSetting, Exit
};

// 实现时建议用强类型 payload（例如 std::variant），
// 保留 operationId 与 generation 进行异步回调关联。
```

命令完成统一返回：操作 ID、是否成功、错误码/可读说明、实际新状态。UI 控件在请求发出时可显示忙碌，但不直接更改实际运行状态。

## 10.2 内部接口（建议）

```cpp
#include <functional>
#include <QJsonObject>
#include <QString>
#include <QUrl>

struct ApiResult {
    bool ok = false;
    int httpStatus = 0;
    QString errorCode;
    QString errorMessage;
    QJsonObject payload;
};
using ApiCompletion = std::function<void(ApiResult)>;

struct ProfileResult {
    bool ok = false;
    QString profileId;
    QString errorCode;
    QString errorMessage;
};
using ProfileCompletion = std::function<void(ProfileResult)>;

class IMihomoClient {
public:
    virtual ~IMihomoClient() = default;
    virtual void fetchVersion(ApiCompletion completion) = 0;
    virtual void fetchConfig(ApiCompletion completion) = 0;
    virtual void fetchGroups(ApiCompletion completion) = 0;
    virtual void selectGroupMember(const QString& group,
                                   const QString& member,
                                   ApiCompletion completion) = 0;
};

class IProfileManager {
public:
    virtual ~IProfileManager() = default;
    virtual void importUrl(const QUrl& url, ProfileCompletion completion) = 0;
    virtual void importFile(const QString& path, ProfileCompletion completion) = 0;
    virtual void update(const QString& profileId, ProfileCompletion completion) = 0;
    virtual void activate(const QString& profileId, ProfileCompletion completion) = 0;
};
```

以上为可落到源码的**职责接口示例**。回调必须遵守 Qt 对象线程亲和性，并明确对象退出时的取消行为；实际工程可改用 `QObject` 信号，但不得留下占位方法或违反唯一 Controller 状态写入原则。

## 10.3 mihomo REST API（按所捆绑版本复核）

| 用途 | Method | Endpoint | 用法/注意 |
|---|---|---|---|
| 版本与就绪 | GET | `/version` | 核心就绪探测 |
| 运行配置 | GET | `/configs` | 查询当前模式/端口 |
| 修改运行模式 | PATCH | `/configs` | `{"mode":"rule"}` 等 |
| 配置载入 | PUT | `/configs` | 对照当前文档检查 path / payload / force 等参数 |
| 节点与策略组 | GET | `/proxies` | 类型、成员、当前选择 |
| 策略组列表 | GET | `/group` | 构建组选择器 |
| 单组/节点 | GET | `/proxies/{name}` | 名称按路径段百分号编码 |
| 切换组成员 | PUT | `/proxies/{name}` | body `{"name":"Japan"}`；回读确认 |
| 节点延迟 | GET | `/proxies/{name}/delay` | URL、timeout 参数按文档 |
| 策略组延迟 | GET | `/group/{name}/delay` | 可能影响自动组固定选择，手动触发 |
| Provider | GET | `/providers/proxies` | 内核 Provider 列表 |
| Provider 更新 | PUT | `/providers/proxies/{name}` | **不用于任意完整订阅 URL 更新** |
| 实时日志 | GET / WS | `/logs` | P0 按需启用流；具备级别、格式参数 |
| 活动连接 | GET / WS | `/connections` | P1 仅打开诊断时使用 |
| 规则查询 | GET | `/rules` | P1 只读 |

接口要求：

- `external-controller` **仅绑定回环地址**，Secret 随机生成，使用 `Authorization: Bearer ...`。
- 本地 Core API 的 QNetworkAccessManager 明确设置 `QNetworkProxy::NoProxy`；远程订阅使用独立 QNetworkAccessManager/策略，不能继承错误的系统代理路径。
- 建立受控进程身份与控制端口关联机制，避免固定端口被其他进程占据时误发送带 Secret 的管理指令。
- 支持 HTTP 204 空响应，非 2xx、超时、无效 JSON、网络断开分别识别。
- 请求超时后首先回读状态，避免无脑重试有副作用的命令。
- 动态组名和节点名必须作为**独立 URL path segment**正确编码；不得做全 URL 二次编码。
- 每个 API 以当前 mihomo 文档与所选版本实测为准；字段差异时记录适配，不杜撰响应。

### 示例

```http
PATCH /configs HTTP/1.1
Host: 127.0.0.1:19090
Authorization: Bearer <redacted-secret>
Content-Type: application/json

{"mode":"rule"}
```

```http
PUT /proxies/GLOBAL HTTP/1.1
Host: 127.0.0.1:19090
Authorization: Bearer <redacted-secret>
Content-Type: application/json

{"name":"Japan"}
```

## 10.4 Qt 与平台 API

| 能力 | 首选 |
|---|---|
| 托盘与菜单 | QSystemTrayIcon / QMenu |
| 多页面 | QMainWindow / QStackedWidget |
| 节点列表 | QTableView / QAbstractTableModel / QSortFilterProxyModel |
| 订阅与设置 | QListView、QDialog、QFormLayout、QFileDialog |
| 网络 | QNetworkAccessManager / QNetworkReply / QNetworkProxy |
| Core 子进程 | QProcess |
| JSON | QJsonDocument / QJsonObject |
| 单实例 | QLockFile 或合适的本地进程锁，验证多用户/会话边界 |
| 文件位置 | QStandardPaths |
| Win 系统代理 | WinINet（平台适配层） |
| Win 自启 | HKCU Run（平台适配层） |
| Win 子进程约束 | Job Object（按实际 QProcess 集成可行性验证） |
| Win 机密存储 | DPAPI + ACL（平台适配层） |

---

# 11. 数据模型、存储与部署

## 11.1 推荐结构

```text
Mlash/                             # 发布包
  Mlash.exe
  cores/mihomo.exe                 # Windows；其他平台换成对应二进制
  platforms/                      # Qt platform 插件（按部署工具生成）
  Qt6*.dll                         # 依实际动态部署结果
  licenses/
  README.md

%LOCALAPPDATA%/Mlash/              # Windows 用户数据
  settings.json
  profiles.json
  profiles/<uuid>/
    source.yaml
    candidate.yaml
    active.yaml
    last-good.yaml
    work/                          # 内核缓存、Provider 等
  recovery/proxy-journal.dat
  logs/app.log
```

macOS/Linux 使用 `QStandardPaths::AppLocalDataLocation` 等平台路径，不允许把 Windows 目录字符串写死在跨平台的 Storage 逻辑中。

## 11.2 Settings 示例

```json
{
  "schema_version": 2,
  "app_name": "Mlash",
  "auto_start": false,
  "silent_start": true,
  "start_core_on_launch": true,
  "restore_proxy_on_launch": true,
  "desired_proxy_enabled": false,
  "active_profile_id": "",
  "mixed_port": 17890,
  "controller_port": 19090,
  "log_level": "warning"
}
```

端口为**示例候选默认值**，必须检查端口占用；若用户修改，涉及 Core 和系统代理的事务切换。敏感 Token/Secret 不以明文保存在普通 JSON 或日志中。内核运行配置可能仍需含明文控制 Secret，需通过权限隔离降低暴露面。

## 11.3 持久化规则

- 使用 `QSaveFile` 等机制进行关键设置文件原子替换，并测试断电/进程退出时的数据完整性。
- 修改系统代理前必须将恢复日志持久化到可靠文件；普通 UI 查询不得频繁写盘。
- Profile 的 `last-good.yaml` 必须可运行，不只是 YAML 语法合法。
- 应用日志滚动，避免磁盘无限增长；订阅 URL 的 query token 等敏感数据必须脱敏。

## 11.4 部署策略

Windows 首版：Release x64 + `windeployqt` 收集 Widgets/Network 必需 DLL、platform 插件，打包 portable ZIP，干净 Windows 10/11 虚拟机测试。不要假设仅拷贝 `Mlash.exe` 就足以运行 Qt 动态程序。

后续 macOS：应用包 `.app`、Qt macOS 部署工具、签名/公证；Linux：选择 AppImage/包管理或其他成熟分发形式，需要单独研究目标发行版依赖与桌面集成。

---

# 12. 业务时序和异常处理

## 12.1 首次启动

```text
读取设置/单实例检查
 -> 检查恢复日志
 -> 检测平台系统代理与接管冲突
 -> 创建 Qt 托盘（如果可用）
 -> 加载或延迟显示主窗口
 -> 没有订阅：提示 URL/文件导入，保持系统代理不变
 -> 有有效 Profile：按设置启动 Core，确认就绪
 -> 如用户此前明确要求自动恢复代理且状态安全，再启用
```

## 12.2 节点切换

```text
产品要求某个 Selector 策略组成员
 -> Controller 校验 group/member 与 Core Running
 -> PUT /proxies/{group}
 -> GET 回读最新 now
 -> 更新全局 AppState / UI
```

## 12.3 订阅切换

```text
选择 Profile -> 校验已有有效 candidate/LKG
 -> 暂时解除 Mlash 对 OS 系统代理的接管
 -> Core 停止并切换工作目录/运行配置
 -> API/端口检查 -> 更新 Profile Active 状态
 -> 若原用户期望代理开启，且无冲突则再次接管
失败 -> 旧配置/旧 Core 尝试恢复 -> OS 设置安全确认 -> 明确错误
```

## 12.4 正常退出

```text
停止接收状态写命令 -> 取消/收敛后台操作
 -> RestoreIfOwned() -> 停止 Core -> 刷盘日志/设置
 -> 移除托盘图标 -> QApplication 退出
```

## 12.5 应明确处理的异常

- Core 未启动但用户点击启用代理；端口被占用；Core API 端口被占据。
- 订阅 HTTP 404/超时/TLS 失败/YAML 非法/配置核验失败。
- 更新中用户切换订阅或退出；多个请求先后返回错乱。
- 托盘运行时关闭主窗口；Explorer 重启；托盘不可用。
- Windows PAC/企业策略/第三方代理变动；应用强杀后再次启动。
- 日志数据过快、长期运行、休眠唤醒、屏幕 DPI 切换。
- 首次启动无 Profile、Linux 无托盘环境、macOS 代理设置权限不同。

---

# 13. 测试与验收规格

## 13.1 测试分层

1. 单元测试：AppState、配置读写、YAML 管理字段、错误映射、日志脱敏、URL/path 编码。
2. Fake Core 测试：REST API 模拟（正常、204、超时、错误 JSON、旧响应覆盖）；QProcess 启停模拟。
3. 真实内核集成：配置校验、策略组、模式、延迟测试、异常退出。
4. 系统实机：WinINet PAC 快照与恢复、第三方代理冲突、用户权限、自启、Explorer 重启。
5. GUI 自动化/人工验收：搜索排序、策略组切换、订阅 URL/文件导入、错误状态与屏幕缩放。
6. 跨平台预研：Qt UI 编译和托盘能力探测；OS 修改层暂不宣称完成。

## 13.2 P0 验收矩阵

| Case | 场景 | 预期结果 |
|---|---|---|
| A01 | 无订阅首次运行 | 可进入导入页，不动 OS 代理 |
| A02 | URL 导入正常 YAML | 下载、验证、导入/激活成功 |
| A03 | 导入本地 YAML | 生成快照，原文件删除不影响已导入配置 |
| A04 | URL 返回 HTML/损坏 YAML | 明确失败，不破坏当前配置 |
| A05 | 内核配置非法 | 不激活，保留旧 Core/LKG |
| A06 | 多订阅切换 | 运行配置/显示 Profile 一致 |
| A07 | 节点选择 | 指定组 `now` 经过 API 回读确认 |
| A08 | 规则/全局/直连 | `/configs` 实际模式匹配 UI |
| A09 | 节点测速超时 | 显示失败/超时，不显示 0ms |
| A10 | 正常开启系统代理 | WinINet 设置实测正确；回读 |
| A11 | 关闭/退出 | 安全恢复原 PAC/代理/绕过配置 |
| A12 | 外部程序修改代理 | 不覆盖外部新设置，提示冲突 |
| A13 | Core 崩溃 | 发现并恢复代理，有限重启 |
| A14 | Controller 强杀 | 下次启动检测/恢复遗留代理 |
| A15 | Core 代理端口占用 | 不错误接管 OS 代理 |
| A16 | 双实例 | 不发生双重管理/双重代理写入 |
| A17 | 订阅更新中退出/断网 | 不损坏设置与 LKG |
| A18 | 打开诊断后关闭窗口 | 实时流停止，不持续占资源 |
| A19 | Explorer 重启 | 系统托盘能恢复或可重新创建 |
| A20 | Windows 登录自启 | 静默托盘，状态符合设置 |
| A21 | 100~500 节点 | 列表滚动搜索排序流畅，无一节点一 Widget |
| A22 | Release 干净机器 | 缺少开发环境也可运行；Qt 插件完整 |
| A23 | 休眠与唤醒 | 重新校验状态，界面不假报正常 |
| A24 | 隐私检查 | 日志不含 Token、Secret 等认证参数 |

## 13.3 性能测量

- 用同一 mihomo 二进制、同一 YAML 订阅、相同系统和条件对比 Mlash 与 Clash Verge Rev。
- Controller 和 Core 分别统计：Private Bytes、Working Set、CPU、句柄、线程数；记录版本与环境。
- 同时测量安装包/解压后大小、托盘可用耗时、内核就绪时间、500 节点列表操作响应。
- Release 构建取多轮统计，明确冷/热启动；不拿 Debug 与 Release 对比。
- 如 Qt 的常驻成本超出目标，先排查无效轮询、UI 长期持有的大数据和无谓线程；不要立刻重写成 WTL。

---

# 14. 研发里程碑及 DoD

## M0 — 环境和工程骨架

- 探测平台、工具链、Qt 版本、mihomo 是否可用。
- 建立 CMake、Qt Widgets 可启动窗口、系统托盘、日志、单元测试框架。
- 建立 `src/core`、`src/app`、`src/platform` 的边界，Windows 编译路径与平台条件构建。
- **DoD**：真实构建命令记录；最小窗口/托盘可运行（若环境具备）；无工具链时明确未执行。

## M1 — Controller + Core API

- MihomoClient GET/PATCH/PUT、错误处理、群组查询、节点选择和测试。
- CoreManager QProcess 生命周期、就绪/退出事件、有限重试、状态机。
- Fake Core 与真实 Core（若可用）的分层测试。
- **DoD**：可以在无完整 GUI 条件下验证 Core 启停、模式和组成员选择，并回读真实状态。

## M2 — Platform/Windows 系统代理与恢复

- ISystemProxyAdapter、WinSystemProxyAdapter、恢复日志、所有权检测、端口切换安全处理。
- 自启、单实例、Windows 子进程控制（根据可验证的 Job Object 方案）。
- **DoD**：正常开关和恢复、PAC、外部接管、Core 崩溃、Controller 强杀再启动等关键测试有真实结果或明确未执行原因。

## M3 — Profile 事务

- URL 和本地 YAML 导入、多 Profile、配置合成、校验、更新、激活、回滚。
- 下载/配置安全边界、LKG/工作目录、流式任务取消、故障注入。
- **DoD**：至少两份合法 Profile 成功切换，损坏/超时/断网场景不丢失旧有效版本。

## M4 — 完整 Qt Widgets UI

- 主窗口四页面、节点表格/筛选、托盘、订阅管理、设置、日志诊断。
- 页面按需刷新、异常状态可见、GUI 取消、隐藏到托盘、DPI 行为。
- **DoD**：不使用命令行即可完成日常代理使用；各按钮有效，异常态不假报成功。

## M5 — 发布、性能与跨平台准备

- Windows Release/portable ZIP、Qt 动态依赖检查、许可证文件、使用说明。
- 可靠性/性能基线，清理过度依赖；导出 Windows 适配限制。
- 补充 macOS/Linux 适配设计与接口测试（不把其他平台误标为 V1 已支持）。
- **DoD**：满足 P0 验收矩阵，产出实测报告、版本锁定文件、安装包与已知限制。

---

# 15. 推荐工程目录

```text
mlash/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSES/
├── resources/
│   ├── icons/
│   └── app.qrc
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── AppController.h/.cpp
│   │   ├── AppState.h
│   │   └── Commands.h
│   ├── core/
│   │   ├── CoreManager.h/.cpp
│   │   └── MihomoClient.h/.cpp
│   ├── profile/
│   │   ├── ProfileManager.h/.cpp
│   │   └── ConfigBuilder.h/.cpp
│   ├── platform/
│   │   ├── IPlatformServices.h
│   │   ├── windows/
│   │   │   ├── WindowsSystemProxy.cpp
│   │   │   ├── WindowsAutoStart.cpp
│   │   │   └── WindowsProcessSupervisor.cpp
│   │   ├── macos/             # V1.1 待实现
│   │   └── linux/             # V1.2 待实现
│   ├── storage/
│   │   ├── SettingsStore.h/.cpp
│   │   └── RecoveryManager.h/.cpp
│   ├── diagnostics/
│   │   └── DiagnosticManager.h/.cpp
│   └── ui/
│       ├── MainWindow.h/.cpp
│       ├── TrayController.h/.cpp
│       ├── ProxyPage.h/.cpp
│       ├── ProfilesPage.h/.cpp
│       ├── DiagnosticsPage.h/.cpp
│       ├── SettingsPage.h/.cpp
│       └── ProxyTableModel.h/.cpp
├── tests/
│   ├── unit/
│   ├── fake_core/
│   └── integration/
└── docs/
    ├── architecture.md
    ├── requirements-traceability.md
    ├── api-contracts.md
    ├── ui-interactions.md
    ├── compatibility-matrix.md
    └── verification-report.md
```

平台文件按实际进度创建，不要求 V1 空文件塞满整个仓库。保留明确的接口并优先减少不必要抽象。

## 15.1 CMake 骨架参考

```cmake
cmake_minimum_required(VERSION 3.24)
project(Mlash VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

# 下面仍需补齐 executable、target_sources、target_link_libraries、测试 target。
# 可用的 Qt 精确版本由受控环境/版本锁定配置规定。
# Windows 特定文件通过 if(WIN32) 编译；其他平台不得编译 Win32 源码。
```

Codex 应在实际工程中修正上面的骨架、加上 `target_link_libraries` / 源码分组 / 测试配置，完成真正可编译的 CMakeLists。上面只是参考，不是完整交付。

---

# 16. 开发过程硬性质量规则

1. 禁止 UI 直接调用系统代理 API 或操作 mihomo 子进程。
2. 禁止 Core API 请求因使用系统默认代理配置而循环进入自己。
3. 禁止 YAML 拼接替换、静默丢失未知字段或运行配置覆盖原始订阅。
4. 禁止订阅下载自动开放监听到 `0.0.0.0` 或启用额外控制入口。
5. 禁止异常退出场景静默声称“系统代理已恢复”。
6. 禁止在窗口析构后调用悬空 QObject 或跨线程直接修改控件。
7. 禁止长时间阻塞 Qt GUI 线程；避免无界重试、无界日志和高频轮询。
8. 禁止在资源指标未测量时宣称 C++/Qt 更轻或更快。
9. 禁止把“Qt 可跨平台”作为“macOS/Linux 系统代理已可用”的证据。
10. 禁止违反实际第三方许可证；发布前检查 Qt、mihomo、yaml-cpp 和其他直接/传递依赖。
11. 禁止编译失败时仅靠注释屏蔽核心功能蒙混过关。
12. 改动跨模块接口时同时更新测试、文档、错误码、状态契约。

---

# 17. 官方参考资料与需核实事项

## Qt

- [Qt 6.12 支持平台](https://doc.qt.io/qt-6.12/supported-platforms.html)：Windows 10（1809+）、Windows 11、macOS、Linux；Qt 6.12 是最后支持 Windows 10 的 Qt 版本。
- [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html)
- [QSystemTrayIcon](https://doc.qt.io/qt-6/qsystemtrayicon.html)：Linux 托盘能力需运行时检测。
- [QNetworkAccessManager](https://doc.qt.io/qt-6/qnetworkaccessmanager.html)：异步 HTTP、QObject 线程归属、代理配置。
- [Qt 许可](https://doc.qt.io/qt-6/licensing.html)

## mihomo

- [mihomo API 文档](https://wiki.metacubex.one/en/api/)
- [mihomo 全局配置](https://wiki.metacubex.one/en/config/general/)
- [mihomo 官方项目](https://github.com/MetaCubeX/mihomo)
- [mihomo 官方发布](https://github.com/MetaCubeX/mihomo/releases)

**注意**：对 `GET /logs` 流式响应、`GET /connections` 响应、配置重载参数、节点测速副作用、CLI 语法及配置字段的行为，以锁定发行版本实测为准。

## Windows

- [WinINet InternetSetOption](https://learn.microsoft.com/en-us/windows/win32/api/wininet/nf-wininet-internetsetoptionw)
- [WinINet InternetQueryOption](https://learn.microsoft.com/en-us/windows/win32/api/wininet/nf-wininet-internetqueryoptionw)
- [Windows Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)

## 授权与品牌

- 对 Mlash 的源码许可证须由项目维护者确定；不要误以为使用 Qt 自动决定整个项目许可证。
- 若捆绑 mihomo，核实所使用发行标签的完整源代码许可证、版权与第三方 Notices，并记录合规方式。
- 品牌 Mlash 与 Clash 名称关联可能引发误认，README 中明确“Independent third-party client; not affiliated with Clash or MetaCubeX”。

---

# 18. Codex 每个阶段交付格式

完成每个里程碑必须输出：

1. **变更概览**：新增/修改文件、设计调整及原因。
2. **构建验证**：环境、真实命令、实际结果及日志摘要。
3. **测试结果**：通过/失败/未执行数量，未执行的明确障碍。
4. **演示步骤**：如何从当前仓库启动和验证这一阶段。
5. **安全/恢复结果**：影响系统代理与配置状态的测试结果。
6. **已知限制**：不能隐藏或用“预计可行”替代验证。
7. **下一里程碑**：根据本规格继续执行，不重复需求澄清。

结束时提交 `README.md`、`docs/verification-report.md`、可编译源码、测试、Windows x64 Release 包（如当前环境允许）和完整依赖/许可证清单。

---

# 19. 最终发布 DoD

- [ ] 所有 P0 功能可使用，四个页面/托盘操作流程完整。
- [ ] URL 和本地 YAML 两种导入均通过合法、非法和断网测试。
- [ ] 真实 API 查询/切换策略组和模式，结果回读与 UI 一致。
- [ ] Windows 系统代理正常启停、PAC 快照、外部冲突与退出恢复通过。
- [ ] Core 崩溃检测及下次启动的遗留恢复逻辑可验证。
- [ ] 修改配置和订阅更新不会破坏 Last Known Good。
- [ ] 空闲无连接日志监测/高频测速等无谓后台资源消耗。
- [ ] Windows 10/11 x64 干净系统可以部署、启动并正确退出。
- [ ] Qt/mihomo 等发行依赖及许可证完整。
- [ ] 性能预算经真实测试，明确与 Clash Verge Rev 的对照条件。
- [ ] macOS/Linux 未实现功能在产品 UI 与文档中清楚标识。
- [ ] 全项目可见名称为 **Mlash**；启动图标、程序信息、README、包名统一。

**最终架构结论**：**Mlash = Qt Widgets 跨平台 GUI + C++20 单一 Controller + 平台系统代理适配层 + 独立 mihomo 内核。** Windows V1.0 先交付稳定的完整日常使用闭环，之后以实测成本和真实需求决定 macOS/Linux 及诊断增强范围。
