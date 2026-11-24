# 需求与验证映射

下表提供实现与测试入口；实际运行状态由 [验证报告](verification-report.md) 维护。“自动”指隔离测试，“模拟”不等于实际系统写入。

## Windows 功能实现闭环

实现完成与发布资格分别记录。下表覆盖原规格 17 项 P0；对应 A 场景的真实环境缺口见后表。

| 功能 ID | 实现入口 | 隔离验证依据 |
|---|---|---|
| F-CORE-01 | CoreManager / initialize/startCore/stopCore/handleCrash | fakeCoreLifecycleAndValidation、fakeCoreUnexpectedExit、realCoreApi；明确非零意外退出/故障状态回读，实际健康回读、两次有限重启；宿主 OS CrashExit 不由该夹具代替 |
| F-PROXY-01 | WindowsServices / ProxyCoordinator | proxyOwnershipAndPacRestore、externalProxyNeverOverwritten、journalPrecedesOsWrite、damagedJournalNeverWritesProxy |
| F-PROXY-02 | setMode / MihomoClient.config | 真内核 UI 三模式写后回读；pendingCandidateSurvivesRuntimeChanges 检查实际模式 |
| F-PROXY-03 | selectMember/clearAutomaticSelection / 策略组表 | 中文/路径编码 API、真内核选择记忆/恢复、嵌套子组与只读组边界 |
| F-PROXY-04 | ProxyTableModel / MemberSortProxyModel | largeNodeModelSorting、真内核 UI 默认/名称/延迟排序、过滤与选择保持 |
| F-PROXY-05 | testNodes/cancelTests / delayProgress | API 超时/取消、三并发/500 上限、单个/本组入口与失败/未测标识 |
| F-PROFILE-01 | SubscriptionDownloader / ConfigBuilder / import | HTTP 重定向/体积/响应头/URL 地址边界、YAML 拒绝、真实内核校验 |
| F-PROFILE-02 | ProfileStore.prepare / ImportDialog | 文件快照保存、删除来源仍运行、原子文件维护保持 active |
| F-PROFILE-03 | activateProfile/renameProfile/deleteProfile / profilesPage | controllerImportActivateRollback、真实两 Profile 切换及 UI 当前状态 |
| F-PROFILE-04 | updateProfile/activateProfile(lastGood) / transact | 失败保存旧索引与 LKG、端口占用回滚、候选跨重启/端口修改仍待激活 |
| F-DIAG-01 | Diagnostics / diagnosticsPage | logStreamStopsWhenHidden、logPauseClearAndDiagnosticPrivacy、过滤/复制/目录/导出 |
| F-SETTING-01 | WindowsServices.autoStart/readAutoStart / updateSettings | 读取冲突保护、自启实际观测与索引失败独立；登录写入待受控实机 |
| F-SETTING-02 | initialize/startCore / desiredProxy/restoreProxy | 首次无 Profile 不接管、完整所有权模拟恢复；真实 OS 恢复待受控实机 |
| F-SETTING-03 | updateSettings / prepareExisting/transact | 端口、日志等级校验与真实失败回滚；未保存草稿不被后台更新覆盖 |
| F-SETTING-04 | settingsPage / 目录入口 | 版本/Qt/内核信息与目录入口；当前八页冒烟 |
| F-DESKTOP-01 | GuiHost / TrayController / requestExit | 轻量释放/快速重开/无托盘退出、真实通道保持、13 种实际状态映射、恢复失败可重试 |
| F-STATE-01 | ProfileStore / ProxyCoordinator / QLockFile | DPAPI/原子替换、类型/大小损坏拒绝、日志先写后改/冲突保留、只读仍可诊断 |

Windows P1 已实现：默认关闭的 URL 定时/文件监测、标准用量与到期、活动连接与明确单连接关闭、规则只读检索、可配置 HTTP 连通测试、脱敏导出、最近手动成员、批量进度/取消及日志暂停。规则编辑为 ADR 0009 接受的范围扩展。自动刷新只在指定可见页启用，后台维护仅按明确开启的策略运行。
ADR 0017 补充可见会话总量/速率/离开记录、详情/排序/确认关闭全部；启动页/托盘单击/环境变量、IPv6/统一延迟真实回读和 GUI Geo 导入/脱敏只读配置。会话记录不落盘、隐藏清空，字段旧索引兼容；当前资格见验证报告。

macOS 适配与国际化扩展不与本次 Windows 交付捆绑；平台返回 Unsupported。原规格的部分P2排除项经 ADR0029 扩展，Windows TUN 经 ADR0036 明确纳入；当前状态及验收入口由 project-status 维护，未获授权的独立架构项继续排除；不用扩展范围替代原有功能验证。

## U40 用户任务闭环

U40 提案把用户任务编号为 `F01`–`F35`（见 [U40 功能清单](design/u40-feature-inventory.md)）。它们与上表共用同一编号空间，**35 条全部**登记为 `F-U40-01`–`F-U40-35`，第二列是清单原编号。每条任务只命名一个主容器，映射见下节。

其中 `F14`（拨号链）、`F32`（快捷键）、`F33`（外观）最初因缺少容器标识而暂缓，已核对代码后补齐：拨号链是配置详情的真实任务页（`openProfileTask(id,"chain")` → `ProxyChainDialog`），快捷键由设置目录的「配置…」打开独立对话框，外观在设置目录行内编辑。据此在 [ContainerIds.h](../src/ui/ContainerIds.h) 新增 `profile:chain` 与 `settings:hotkeys`，`F33` 落到设置视图本身。

| 功能 ID | U40 编号 | 用户任务 |
|---|---|---|
| F-U40-01 | F01 | 连接接管状态：查看配置、内核健康、代理归属与 TUN 实际状态，开启/关闭代理，处理外部代理冲突 |
| F-U40-02 | F02 | 切换规则、全局、直连路由模式并理解各自含义 |
| F-U40-03 | F03 | 浏览并切换策略组、子组与手动成员，查看自动选择，恢复自动与已记忆的选择 |
| F-U40-04 | F04 | 搜索节点，按地区、协议、状态、延迟、收藏筛选，切换表格/卡片、排序，定位当前成员并看详情 |
| F-U40-05 | F05 | 测单个节点、所选成员与当前组，看进度并取消；管理组测试地址、常用组与叶子探测 |
| F-U40-06 | F06 | 管理切换后按组清理受影响连接的偏好，查看选路与清理结果 |
| F-U40-07 | F07 | 导入订阅 URL、本地 YAML 与节点链接，预览去重，处理拖放/剪贴板/外部协议，设置下载参数 |
| F-U40-08 | F08 | 浏览、使用、重命名、排序、多选、删除配置；显式展示可分享来源与私人二维码 |
| F-U40-09 | F09 | 更新订阅、同步本地源文件，管理自动更新/监测、用量到期与下载策略，批量更新看逐项结果 |
| F-U40-10 | F10 | 识别活动、待应用与上次良好版本，看候选差异，应用候选、安全重载与回滚 |
| F-U40-11 | F11 | 添加、编辑、覆盖、移除节点和策略组，管理成员与引用，向现有配置加入节点链接 |
| F-U40-12 | F12 | 查看来源规则，编辑前置、追加、屏蔽规则与规则集声明，切换表单/高级文本并看改动影响 |
| F-U40-13 | F13 | 查看并编辑配置 DNS，继承、清除或覆盖来源，使用表单与高级 YAML |
| F-U40-14 | F14 | 查看、设置或清除节点拨号上游，检查引用与循环 |
| F-U40-15 | F15 | 查看节点源、规则源与版本快照，单项/同类/全部刷新，调整继承的下载策略与自动更新计划 |
| F-U40-16 | F16 | 查看兼容性检查、受控覆盖与拒绝能力，预览脱敏运行配置 |
| F-U40-17 | F17 | 查看上下行速率、累计量、连接数、图表与采样状态 |
| F-U40-18 | F18 | 查活动及已结束连接，暂停/恢复、搜索筛选、排序、定制列，看快照并断开单条/全部 |
| F-U40-19 | F19 | 按指定节点重测网站、编辑测试目标、比较同会话结果，看详情、停止或清空 |
| F-U40-20 | F20 | 检查出口 IP、地区、ASN 与服务响应，选传输路径、取消并看结果依据 |
| F-U40-21 | F21 | 查看内核当前实际规则并测试 DNS 查询，从连接或诊断定位所属配置 |
| F-U40-22 | F22 | 读应用/内核日志，筛选、暂停、复制、换行、跟随、清空、开目录并导出脱敏诊断 |
| F-U40-23 | F23 | 创建加密备份，预览并选择性恢复，选择范围，处理恢复前内核/代理要求 |
| F-U40-24 | F24 | 配置自动本地备份、变更前保护与保留策略，立即备份、预览档案，停用并清除存储密码 |
| F-U40-25 | F25 | 配置 WebDAV 凭据与局域网策略，按需/定时备份，看远端版本、取消或下载归档，清除凭据 |
| F-U40-26 | F26 | 核对运行内核，检查/下载/导入内核、预检、应用、切换历史与清理副本 |
| F-U40-27 | F27 | 检查、导入和更新 Geo 文件及其来源，区分默认数据与运行版本使用的数据 |
| F-U40-28 | F28 | 查看应用版本、运行环境与数据位置，检查/下载/取消更新并打开已校验候选包 |
| F-U40-29 | F29 | 改本地代理端口、手动测试地址与系统代理排除，复制代理地址和环境变量 |
| F-U40-30 | F30 | 配置系统代理/TUN 接管方式、栈与路由选项，看网卡服务状态并安装/修复/卸载、重试清理 |
| F-U40-31 | F31 | 管理登录启动与冲突、启动页/隐藏/内核/恢复代理、关闭释放、托盘手势与协议关联 |
| F-U40-32 | F32 | 录制、修改、清除快捷键，查看注册失败与冲突 |
| F-U40-33 | F33 | 调整明暗、增强对比、字阶与窗口标题栏 |
| F-U40-34 | F34 | 托盘显示状态、开关代理与接管方式、切模式、快速换组/节点/配置、停留测速与安全退出 |
| F-U40-35 | F35 | 从失败或只读保护恢复，看告警详情与处理入口，区分隐藏/关闭/退出，轻量重建后找回位置 |

## 主容器映射

功能与界面容器的双向映射（原则 12）。容器标识的唯一定义处是 [ContainerIds.h](../src/ui/ContainerIds.h)；`infra` 表示非界面能力（持久化、恢复、生命周期）。检查脚本 `scripts/check_feature_outline.py` 校验未登记标识、孤儿容器与重复映射。

| 功能 | 主容器 |
|---|---|
| F-CORE-01 | `proxy:run` |
| F-PROXY-01 | `proxy:run` |
| F-PROXY-02 | `proxy:run` |
| F-PROXY-03 | `proxy:members` |
| F-PROXY-04 | `proxy:members` |
| F-PROXY-05 | `proxy:members` |
| F-PROFILE-01 | `view:profiles` |
| F-PROFILE-02 | `view:profiles` |
| F-PROFILE-03 | `view:profiles` |
| F-PROFILE-04 | `profile:versions` |
| F-DIAG-01 | `data:logs` |
| F-SETTING-01 | `settings:startup` |
| F-SETTING-02 | `settings:startup` |
| F-SETTING-03 | `settings:network` |
| F-SETTING-04 | `settings:about` |
| F-DESKTOP-01 | `tray:menu` |
| F-STATE-01 | `infra` |
| F-U40-01 | `proxy:run` |
| F-U40-02 | `proxy:run` |
| F-U40-03 | `proxy:members` |
| F-U40-04 | `proxy:members` |
| F-U40-05 | `proxy:members` |
| F-U40-06 | `proxy:members` |
| F-U40-07 | `view:profiles` |
| F-U40-08 | `view:profiles` |
| F-U40-09 | `profile:source` |
| F-U40-10 | `profile:versions` |
| F-U40-11 | `profile:nodes` |
| F-U40-12 | `profile:rules` |
| F-U40-13 | `profile:dns` |
| F-U40-14 | `profile:chain` |
| F-U40-15 | `profile:providers` |
| F-U40-16 | `profile:compatibility` |
| F-U40-17 | `data:traffic` |
| F-U40-18 | `data:connections` |
| F-U40-19 | `data:tests` |
| F-U40-20 | `data:tests` |
| F-U40-21 | `view:rules` |
| F-U40-22 | `data:logs` |
| F-U40-23 | `settings:backup` |
| F-U40-24 | `settings:backup` |
| F-U40-25 | `settings:backup` |
| F-U40-26 | `settings:engine` |
| F-U40-27 | `settings:tools` |
| F-U40-28 | `settings:about` |
| F-U40-29 | `settings:network` |
| F-U40-30 | `settings:tun` |
| F-U40-31 | `settings:startup` |
| F-U40-32 | `settings:hotkeys` |
| F-U40-33 | `view:settings` |
| F-U40-34 | `tray:menu` |
| F-U40-35 | `infra` |

同一容器承载多条任务是允许的：`proxy:run` 收拢连接页的接管、模式与内核状态，`proxy:members` 收拢节点与策略组，`settings:backup` 收拢备份的三个同级页签，`data:tests` 收拢网站测试与出口诊断。

仍未被任何功能引用的容器在检查脚本的 `RESERVED` 中登记原因与去向；`RESERVED` 只保留结构性父容器、待定的总览视图和 U40 清单未收录的隧道工作区，已映射的条目在合并时逐条删除。`F14`、`F32`、`F33` 的落点需要新增容器标识后才能登记，见上节说明。

## 原验收场景

| 原验收 | 实现位置 | 当前证据 / 缺口 |
|---|---|---|
| A01 无订阅 | Controller.initialize / MainWindow | 首次使用空态与当前八页隔离冒烟，默认不接管 |
| A02 URL YAML | Downloader / ConfigBuilder / ProfileStore | Mock HTTP 下载/重定向；公开真实订阅整链待测 |
| A03 本地快照 | ProfileStore / Controller | 删除源文件后仍成功真实激活 |
| A04 损坏来源 | ConfigBuilder / Controller | HTML、错误 YAML 拒绝且旧 candidate/active 保留 |
| A05 Core 校验失败 | CoreManager.validate | Fake Core 非零退出；真实正常配置校验 |
| A06 多订阅 | Controller.transact | 两份本地 Profile 真内核切换、当前订阅安全删除；端口变更回滚 |
| A07 节点 | MihomoClient / selectMember | 真 v1.19.32 PUT/GET now |
| A08 模式 | setMode | 真 PATCH/GET mode |
| A09 测速失败 | delay / ProxyTableModel | 超时/取消 API；失败与未测状态；公网节点待测 |
| A10 代理接管 | WindowsServices / ProxyCoordinator | 模拟写前日志与回读；OS 实际写入未执行 |
| A11 退出/PAC | ProxyCoordinator / requestExit | 模拟 PAC 完整恢复，GUI 退出；OS PAC 未执行 |
| A12 外部接管 | ProxyCoordinator | 模拟不覆盖、保留日志 |
| A13 Core 崩溃 | CoreManager / handleCrash | Fake Core 崩溃信号；完整 OS 恢复待实机 |
| A14 主程序强杀 | journal / initialize | 日志持久化与所有权模拟；实际强杀/代理待受控 VM |
| A15 端口占用 | PID identity / rollback | 真 Core 新端口占用失败后旧 Core/索引恢复 |
| A16 单实例 | QLockFile | 同数据目录锁；本机双实例运行验收另记 |
| A17 更新取消 | Downloader / busy / exitPending | API/下载限制；原子索引、旧快照保留 |
| A18 按需日志 | MihomoClient / UI show/hide | Mock chunked 日志接收，隐藏后连接确实关闭 |
| A19 Explorer | QSystemTrayIcon | 实现由 Qt 负责；Explorer 重启未执行 |
| A20 登录自启 | HKCU Run / settings | 实现回读与冲突保护；实际写入/登录未执行 |
| A21 500 节点 | QAbstractTableModel / filter | 排序/搜索/失败延迟/稳定索引回归；短期采样不代表资源预算达标，OS 交互/冷启动/长期资格仍需独立验证 |
| A22 无开发环境 | package.ps1 | 本机清理 PATH 的便携冒烟；全新 Windows 未执行 |
| A23 休眠/DPI | health timer / native Qt | 原生 DPI 截图；休眠唤醒未执行 |
| A24 隐私 | DPAPI / ACL / redaction | 索引加密、URL/password 脱敏测试；真实源审计待验收 |

P1 连接与规则查询已经实现；规则编辑器由 ADR 0009 明确接受，替代原规格只读限制。定时更新/本地监测与标准用量按 ADR 0013 补齐；其余诊断增强列在 project-status。Windows TUN 由 ADR0036 纳入；显式内核更新由 ADR0033/0039 纳入。任意脚本、静默程序自更新及 macOS/Linux 系统集成仍不属于当前支持范围。
资源与长期指标需要 Release 对照实测，不能从语言选型推断完成。
