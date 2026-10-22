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
