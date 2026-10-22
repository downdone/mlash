# Clash Verge Rev 与 Mlash 功能差异及跟进清单

> 历史归档：仅用于追溯当时的评审、阶段方案和决策过程。当前行为以 [交互契约](../ui-interactions.md)、[架构](../architecture.md) 和 [项目状态](../project-status.md) 为准；文中阶段状态不代表当前版本。

2026-10-15 · 源码静态分析 · 建议清单，尚未批准为开发排期。

后续状态（2026-10-16）：上句及下表保留初次分析时的判断；G01–G22 后续已获授权并完成实现，TUN/权限服务另由用户显式授权、ADR0036 取代原排除项。当前实施与验证阶段以 project-status.md、verification-report.md 为准。

1.13.0实现后的功能深度/操作链复核见[细粒度对照](clash-fine-grained-review-1.13.md)，本页保留1.11.0历史分析。

## 1. 对照范围与结论

- 参考对象：产品要求 `Clash Verge Rev 2.5.9 参考源码`，`package.json` 与 `src-tauri/Cargo.toml` 均声明 **2.5.9**。此目录没有 `.git`，无法提供分支、提交或确认它与公开发行版一致；本文只描述这份本地源码。
- Mlash：**1.11.0**，实现基线提交 `清理前代码基线`。参考架构、Controller 命令、持久化字段、配置构建器及页面实现。
- 方法：沿页面/操作入口追踪到配置、命令和处理实现。没有编译或运行 Verge，没有测试真实订阅、真实网站或修改系统配置。因此“参考源码具备”表示存在实现路径，不表示已在本机验收。
- 覆盖主要用户任务、设置、配置增强和桌面集成；不是逐个函数/API 或每个样式偏好的完整审计。下面共 22 项建议，另列范围差异与验证欠账。
- 本次没有修改任何产品代码、构建物、用户配置或参考源码。可见界面、源码声明和实际运行验收是不同证据层次。

**主要差距在配置兼容和管理工具：下载路径选择、Provider 管理、备份迁移、配置覆盖、诊断信息。日常代理主链已经具备，不需要重新开发一遍。**

两者都是 mihomo 客户端，不能把内核协议、DNS、嗅探和策略组能力全部算成 Verge 独有功能。Mlash 接受范围内的这些配置可交给内核执行；缺少可视化编辑入口，与直接拒绝某种配置，要分别处理。

优先级：**P1 建议下一阶段跟进；P2 有具体需求再跟进；边界项需要先决定产品方向**。S/M/L 为相对工作量，小/中/大，不是工期承诺。本文 P1/P2 是本次建议排序，不改写原规格的同名等级。

## 2. 已有能力：不重复列入缺口

| 领域 | Mlash 当前情况 | 证据 |
|---|---|---|
| 内核与代理 | 启停/就绪检查、规则/全局/直连、系统代理接管和恢复、崩溃有限重启 | M1、M4 |
| 订阅 | URL/完整 YAML 文件、多订阅、手动/定时更新、本地监测、标准用量/到期、候选与使用分离、回滚 | M1、M2 |
| 节点 | 策略组/子组、表格/卡片、搜索、默认/名称/延迟排序、单卡/本组测速、最近选择、选择记忆 | M1、M5 |
| 托盘与生命周期 | 托盘选节点/模式/订阅、展开组按需测速、小象状态图标；轻量模式释放主窗口 | M1、M8 |
| 规则 | 每订阅前置自定义规则；原始 YAML 保留；当前内核规则搜索；保存校验/失败保留 | M3、M5 |
| 连接 | 活动/已离开、规则/链路/进程、累计/差值速率、排序/搜索/详情/复制、关闭单条/全部 | M1、M5 |
| 测速 | 指定叶子节点测 12 个常见 HTTPS 目标、单卡重测、取消、清空、图标/时间/结果 | M6 |
| 设置与诊断 | 登录自启、静默启动、启动页面、托盘点击、IPv6/统一延迟覆盖、Geo 离线导入、脱敏预览/导出、日志过滤/暂停 | M1、M2、M5 |
| 外观 | 跟随系统浅/深/高对比、统一弹出层、键盘焦点、DPI 布局、内置国旗和服务图标 | M7、M8 |

上述是覆盖情况，不代表逐项等同 Verge 的全部深度，也不代表所有 Windows 系统场景已经人工验收。

## 3. P1：建议先补的六项

| ID | 差异与用户影响 | Verge 源码 / Mlash 现状 | 建议与可观察验收 | 工作量/依赖 |
|---|---|---|---|---|
| G01 | **导入兼容性与错误说明**：同一订阅在 Verge 可用，Mlash 可能直接拒绝，用户不知道如何调整 | R1/R2 保留配置并做增强；M3 有明确能力白名单，拒绝 MRS、外部文件 Provider、额外监听等；部分错误未给出具体字段位置 | 先显示字段路径、拒绝原因、可替代办法、将被覆盖的字段；用匿名合法样本建立兼容矩阵，再逐项决定扩展。拒绝时原 active/candidate 均不被破坏 | M；不直接放开白名单。通过此项确认 G03/G07/G08 的真实需求 |
| G02 | **订阅下载设置**：直连不可达或服务端依赖 UA 时，更新失败 | R3 支持系统代理/本地核心代理、UA、超时；M2 固定 NoProxy 与 Mlash UA，设置字段没有下载路径选项 | 每订阅提供直连/经当前本地代理、UA 和有界超时；明确失败原因与实际路径；代理不可用时不静默改路由。仍只生成候选，不自动替换运行版本 | M–L；需重审代理路径下 DNS/IP 检查、凭据和重定向边界，不能直接删除现有安全检查 |
| G03 | **Provider 可见性与独立更新**：节点/规则提供者的更新时间、数量、错误不可见，更新粒度过大 | R4 有节点/规则 Provider 列表和单个/全部更新；M3 将 HTTP Provider 转为本版本 file 快照，当前由订阅更新带动下载，没有独立管理命令 | 增加只读提供者列表、来源类型、数量、快照时间、最近错误；独立刷新仍创建包含新快照的候选，并明确“使用更新”。错误保留旧快照；不对被冻结 file 简单调用内核更新来假装下载成功 | L；先补版本内 Provider 元数据与事务契约，保持可重复回滚；MRS 另见 G08 |
| G04 | **本地备份/恢复与迁移**：现有索引备份不能让用户完整迁移订阅/覆盖/偏好 | R5 有本地备份、导入/导出/恢复、自动备份与 WebDAV；M2 的 DPAPI 索引绑定账户，last-good 是恢复机制，没有用户备份包入口 | 先做显式本地备份/恢复：显示包含的订阅、规则、设置和机密选项；可移植机密备份需独立加密设计；恢复先校验/预览，失败保持原状态。排除日志、恢复日记、旧 OS 所有权和自动接管意图 | L；账号绑定数据不可直接复制宣称可迁移；WebDAV 不在此项范围 |
| G05 | **测速自定义目标**：只能测内置 12 个站点，无法加入用户自己的域名/服务 | R6 可增删/编辑/排序并保存测试目标；M6 定义编译内置，仅测试夹具可注入目标 | 增删改目标名称/HTTPS 地址、恢复默认、持久化；采用通用本地图标，无需在线取 favicon；保留指定叶子、并发/超时/取消限制。不得把“已响应”改称“已解锁” | M；新增存储字段、旧索引兼容、URL 边界与取消回归 |
| G06 | **规则编辑深度**：现有文本前置规则可用，但删除订阅原规则、追加规则和规则集引用不方便 | R7 有 prepend/append/delete、原规则展示及 RULE-SET 等类型；M3/M5 是 13 种类型的前置文本列表，原规则只读 | 先做结构化查看/编辑/上下移动和逐条错误；再增加受控的追加、原规则屏蔽和引用已有 rule-provider。显示来源及生效顺序，保存失败留草稿；源文件保持不变 | M–L；需 schema/合成规则契约与 ADR；不直接开放所有逻辑表达式 |

这六项中，G01 是风险最低的起点；G02 解决“更新不下来”，G03 解决“更新的是什么”，G04 解决“换电脑/误操作如何恢复”。G05 延续当前测速使用链，G06 改善规则维护。

## 4. P2：按实际需求安排

| ID | 功能差异 | 源码对照与跟进建议 | 工作量/取舍 |
|---|---|---|---|
| G07 | **每订阅 DNS 覆盖与诊断** | R8 有 DNS 编辑/校验及按订阅确认；M3 保留接受的源 DNS 配置，但没有专门覆盖/诊断入口，且禁止独立 DNS listen。建议先显示有效 nameserver/policy 与来源，再做受控字段覆盖、还原；不要把“无编辑器”说成“不支持 DNS” | L；涉及配置有效性/启动，优先不新增监听或修改 OS DNS |
| G08 | **MRS 等规则 Provider 格式兼容** | R1/R2 的配置管线保留 rule-providers 交给内核，未见 Mlash 的 yaml/text-only 限制；具体 MRS 行为仍需运行内核样本验证。M3 明确拒绝 MRS/file Provider/proxy 下载。建议用实际失败订阅决定先后，先支持有界远程 MRS 快照、哈希与回滚，不开放任意本地路径 | L；若 G01 证明常用订阅被阻塞，可升 P1。扩展的是输入/快照兼容，不重写 MRS 引擎 |
| G09 | **GeoData 版本/更新管理** | R9 有显式更新入口；M1/M5 有本地 Geo 导入和版本复用，无对应在线管理界面。M3 同时保留 geo-auto-update 等源字段，不能笼统说内核完全不会联网更新。建议展示当前文件/哈希/来源、官方受控手动下载与失败保留，说明源自动更新对可重复性的影响 | M–L；数据更新不直接覆盖 active，需明确冻结/内核自动更新策略 |
| G10 | **首页实时流量、连接数、核心内存** | R10 以可见性控制 traffic/memory 读取与图表；Mlash 总量/速率仅在连接观察会话中，首页没有独立实时样本。建议先做可见首页的紧凑指标及短内存曲线，隐藏即停止；区分核心内存、GUI 内存和订阅用量 | M；原规格排除完整流量分析，应新增有限范围 ADR；不加数据库或全天历史 |
| G11 | **出口 IP/地区/ASN 验证** | R11 有 IP 信息卡；M5 国旗按节点名称推断，不是出口地址。建议手动检测并标明请求目标、时间、实际代理路径；规则模式不同目标可走不同出口，不展示成“全局唯一 IP” | M；新的外部请求默认按需，隐藏停止；不要用名字推断结果充当检测 |
| G12 | **服务地区/解锁检测** | R12 有 ChatGPT/Claude/Gemini/Netflix/YouTube 等专用判定；M6 仅做延迟/响应。适合独立的可选诊断，返回支持/受限/未知及检测依据 | L、长期维护；原规格排除解锁矩阵，需新 ADR。R12 经 Mixed 端口受规则影响，不能当作指定节点结果；其关闭证书/主机名验证的写法不应沿用 |
| G13 | **全局快捷键** | R13 注册显示窗口、切模式、切代理等系统快捷键；Mlash 有窗口内键盘操作，无全局注册。建议先做显示窗口/打开代理页，冲突要明确反馈，代理切换仍遵守外部接管确认 | M；Windows 平台层实现，退出注销，不留窗口引用 |
| G14 | **连接高级检索/列配置** | R14 有大小写/整词/正则搜索、列管理和表格/列表布局；M5 是不区分大小写的固定字符串、多字段搜索及固定列。建议列显示/宽度记忆、正则错误反馈，详情继续复用现有数据 | S–M；不是重新做连接采集；默认搜索保持简单 |
| G15 | **外观手动选择与文字缩放** | R15 有 light/dark/system、字体、主题和布局定制；M7 已自动跟随系统浅/深/高对比，没有用户外观偏好。建议仅补浅/深/系统与有限文字比例，兼容高对比和小窗口；无需复制 CSS/主题编辑器 | M；不能把现有深色模式当作缺失项 |
| G16 | **节点/策略组声明式编辑** | R1 的增强链支持 proxies/groups，R7 类编辑器组合覆盖；Mlash 可浏览/选择源组，但不能在 GUI 新建节点、增删组或调整组成员。可先做新建本地 YAML 配置/受控组覆盖，保留来源/候选分离 | L；高级入口，需契约而非任意 YAML merge；普通机场订阅用户价值低于 P1 |
| G17 | **链式代理编辑** | R16 通过有序节点写 dialer-proxy；M3 不一概删除源中的 dialer-proxy，但没有链编辑器。仅在确有多跳需求时补图形入口、循环检测和失败回滚，不把已有源链运行能力算作全缺失 | L；依赖 G16/版本事务；不默认切换全部连接 |
| G18 | **首页当前节点周期探测** | R17 的 current-proxy-card 有可选周期检测；Mlash 节点页手动、托盘展开按需，内核自己仍可执行 URLTest/Fallback。可选“首页可见时刷新当前叶子”，不做关窗后全节点循环测试 | S–M；与按需/轻量原则协调；自动测速不等于自动选节点 |
| G19 | **协议链接一键导入** | R18 处理 clash://、clash-verge:// 的订阅 URL；Mlash 仅对话框 URL/文件。若需要从网站跳转，新增 Mlash 自有协议并展示导入确认，避免抢占别的客户端关联或收到链接自动下载/激活 | M；Windows 协议注册/卸载与输入验证；不等于支持 ss/vmess 单节点链接转换 |
| G20 | **系统代理排除列表** | R19 有自定义 bypass/PAC/guard；M4 能保存/恢复完整 PAC/bypass，但自身写入使用固定本地排除列表。先提供受控 bypass 字段及回读/恢复测试；PAC 服务与主动代理守护另列边界 | M；需要扩展完整快照/目标与恢复回归，不能只改 UI 字符串 |
| G21 | **手动更新 mihomo 的完整工作流** | R20 有稳定/Alpha 选择、下载/暂存/替换/失败回滚；Mlash 允许指定本地核心文件并校验/安全重启，没有在线下载入口。先补“选本地新版→显示版本/兼容性→验证→应用→失败恢复”，已有工具路径继续可用 | M–L；在线官方来源校验及版本矩阵另立 ADR，不默认追 Alpha。原规格不引入自动更新框架 |
| G22 | **WebDAV/定时备份** | R5 有实现；Mlash 没有。这是 G04 之后的可选扩展，不应为了开发验证先增加云端账号/凭据/后台同步。先验证用户是否真的需要多设备同步 | L；WebDAV 是原规格明确排除项；未获新范围决定前不执行 |

G08 的完整 Provider 路径和 G21 的新内核兼容性，不可只凭参考前端选项就宣布支持；需要扩展后的真实内核失败/回滚用例。

## 5. 差异存在，但当前不建议照搬

| 项目 | Verge 依据 | 对 Mlash 的判断 |
|---|---|---|
| TUN/透明代理/常驻高权限服务 | `src-tauri/src/feat/tun.rs`、`enhance/tun.rs`、`core/service.rs` | 能覆盖不遵循系统代理的程序，但属于架构变化。现有工程契约明确禁止；若将来游戏/UDP/全流量接管成为核心需求，再独立设计。不能承诺当前系统代理覆盖这些场景 |
| LAN 共享/多端口/额外隧道监听 | R2、`setting-clash.tsx`、`mods/tunnels-viewer.tsx` | Mlash 强制回环 Mixed、关闭额外监听，是范围选择。共享给手机等需求出现时单独设计认证、绑定、访问范围，不默认打开 |
| 任意 JavaScript 增强/启动脚本 | R1、`enhance/script.rs`、R15 | 扩展性强，也扩大执行边界和维护负担；当前不做。受控声明式覆盖不需要引入 JS 引擎 |
| 代理守护自动重新抢占 | R19、`core/proxy_control.rs:575` | 与 Mlash “外部修改后尊重所有权”的约定冲突。可提示冲突并提供用户明确重新接管，不能后台持续覆盖其他软件 |
| PAC 服务、外部控制开放/WebUI | R19、`mods/controller-viewer.tsx`、`mods/web-ui-viewer.tsx` | 现有 API 只给本机客户端，PAC 只恢复原状态。没有需求不新增服务/暴露 API；不为复制网页管理入口引入 WebView |
| CSS 注入、可编程主题、首页任意布局 | R15 | 产品要求精细极简，先保证统一语义/文字缩放/无障碍，不做主题平台 |
| 程序自身静默在线更新 | `src-tauri/src/core/updater.rs:428`、`:683` | 开发验证先保留手动 ZIP 更新；自动更新需要来源验证、替换/回滚及 Windows 锁文件方案，不是当前缺一颗按钮 |
| UWP Loopback 豁免工具 | `src-tauri/src/core/win_uwp.rs:9` | 有 UWP 访问本地代理的问题时再做明确诊断/工具入口；Verge 这里实际启动单独工具并可能提权，不是普通设置开关 |
| macOS/Linux 完整系统适配 | 各处平台分支及系统服务 | 这是平台支持差距，当前用户目标为 Windows。现有 Unsupported 不升级为“可用”，不与 Windows 功能补齐捆绑 |
| 多语言界面 | R15 的语言切换及 `src/locales/` | Verge 有多语资源，Mlash 目前以中文界面为主；当前中文产品范围暂不优先引入完整翻译维护链 |

## 6. 推荐实施顺序与交互原则

1. **第一批：G01 + G02。** 先解决订阅能否导入/更新及失败解释，建立实际订阅兼容样本。
2. **第二批：G03 + G04。** Provider 版本可见、可更新，数据能备份/恢复；这两项优先落实事务与失败保护，再设计界面。
3. **第三批：G05 + G06。** 补测速目标编辑、规则维护，不增加新的顶级导航。
4. **随后按需求选 G07/G08/G09。** DNS/规则格式/Geo 影响可用性时优先；否则选 G10/G11/G13/G14 改善日常观察与操作。
5. G12/G16/G17/G19/G21/G22 单独评估，不与前几批打包承诺。

界面承载建议：订阅下载策略放在订阅属性；Provider 放在现有代理/规则页；本地备份放设置“工具与数据”；测速目标管理放测速页。首页只显示有效摘要和少量实际指标；低频高级功能留在各自任务内，不继续增加平级菜单和大段技术说明。

所有新增采样遵循“页面可见/用户明确触发”，关闭窗口释放 GUI；联网功能有取消、并发/大小/时间上限。配置类写入继续走 Controller 和候选校验/事务，观察结果不由按钮意图代替。

## 7. 验证欠账：与功能缺口分开

- 系统代理写入、强杀恢复、登录自启、Explorer 重启、休眠/网络切换，需要实际受控 Windows 场景。已有模拟测试不能证明上述全都通过。
- Windows 10、干净机器、72 小时稳定性、完整性能预算、屏幕阅读器尚无对应完整验收；不因 Verge 源码更完整而推断其在本机已通过。
- 原生剪贴板环境限制仍由现有验证报告管理；不重复立一个“新增复制功能”来掩盖。
- 这些是可靠性验证工作，不是要求用户先准备发行环境；开发迭代可继续，OS 写入测试仍遵守受控 VM 门禁。

## 8. 源码证据索引

以下为分析时参考快照的源码路径和文件指纹，非当前仓库内可点击文件；快照不据此宣称与上游最新提交一致：

- `package.json`：`e5dd5babd7d00c91043e621532d6f3c7ea4a99b982a8d298d541bd4471a22ff4`
- `src-tauri/src/enhance/mod.rs`：`559e27fde34ddea34dddbc883b88324ba45634b4312316bd503c8fa78822d1fe`
- `src-tauri/src/config/verge.rs`：`acd20e809b2ddd46d637f40b42bc8df004a397b050cc0bcdac477b4871142a5a`

| ID | 参考快照源码入口 |
|---|---|
| R1 | 增强合成（`src-tauri/src/enhance/mod.rs`）、增强类型（`src-tauri/src/enhance/chain.rs`）、前置/追加/删除（`src-tauri/src/enhance/seq.rs`） |
| R2 | 配置字段传递（`src-tauri/src/enhance/field.rs`）、核心设置（`src/components/setting/setting-clash.tsx`） |
| R3 | 订阅字段与下载（`src-tauri/src/config/prfitem.rs`）（99–120/260–288）、订阅属性入口（`src/components/profile/profile-viewer.tsx`）（342–454） |
| R4 | 节点 Provider（`src/components/proxy/provider-button.tsx`）（59/87）、规则 Provider（`src/components/rule/provider-button.tsx`）（50/84）；调用 mihomo 插件并请求 runtime provider 同步；插件依赖源码不在本目录，不宣称其内部已经审计 |
| R5 | 备份实现（`src-tauri/src/core/backup.rs`）（271）、恢复/导入导出（`src-tauri/src/feat/backup.rs`）（138/171/211）、定时备份（`src-tauri/src/core/auto_backup.rs`）、命令（`src-tauri/src/cmd/backup.rs`） |
| R6 | 自定义目标列表（`src/components/home/test-card.tsx`）（84/94/108）、配置字段（`src-tauri/src/config/verge.rs`）（146/230） |
| R7 | 规则编辑器（`src/components/profile/rules-editor-viewer.tsx`）（67/281/433）、序列合成（`src-tauri/src/enhance/seq.rs`） |
| R8 | DNS 编辑器（`src/components/setting/mods/dns-viewer.tsx`）、每订阅覆盖事务（`src-tauri/src/feat/dns.rs`）（32）、来源指纹（`src-tauri/src/config/dns.rs`）（27） |
| R9 | Geo 更新操作（`src/components/setting/setting-clash.tsx`）（76/344）；updateGeo 来自外部 mihomo 插件，本文只确认客户端入口及调用 |
| R10 | 首页指标（`src/components/home/enhanced-traffic-stats.tsx`）（130/142/147）、traffic 读取（`src/hooks/use-traffic-data.ts`）、memory 读取（`src/hooks/use-memory-data.ts`） |
| R11 | IP/地区/ASN 卡片（`src/components/home/ip-info-card.tsx`）（114/270/327） |
| R12 | Mixed 代理检测入口及 TLS 选项（`src-tauri/src/cmd/media_unlock_checker/mod.rs`）（12/31/41）、服务判定调度（`crates/clash-verge-media-unlock/src/lib.rs`）（79/110/143）、ChatGPT 判定（`crates/clash-verge-media-unlock/src/chatgpt.rs`） |
| R13 | 全局快捷键（`src-tauri/src/core/hotkey.rs`）（24/211/313） |
| R14 | 搜索模式（`src/utils/search-matcher.ts`）、连接列管理（`src/components/connection/connection-column-manager.tsx`）、连接布局/过滤（`src/pages/connections.tsx`）（100/114） |
| R15 | 主题/语言/启动脚本（`src/components/setting/setting-verge-basic.tsx`）、主题编辑（`src/components/setting/mods/theme-viewer.tsx`）、布局偏好（`src/components/setting/mods/layout-viewer.tsx`） |
| R16 | 链式代理界面（`src/components/proxy/proxy-chain.tsx`）、dialer-proxy 合成（`src-tauri/src/config/runtime.rs`）（55）、写命令（`src-tauri/src/cmd/runtime.rs`）（103） |
| R17 | 当前节点周期检测（`src/components/home/current-proxy-card.tsx`）（549 起）、检测偏好（`src/components/setting/mods/misc-viewer.tsx`） |
| R18 | 协议解析与订阅导入（`src-tauri/src/utils/resolve/scheme.rs`）（13/40/86）、注册入口（`src-tauri/src/lib.rs`）（81） |
| R19 | bypass/PAC 设置（`src/components/setting/mods/sysproxy-viewer.tsx`）（109/305）、系统代理实现（`src-tauri/src/core/sysopt.rs`）（288/428）、PAC 输出（`src-tauri/src/utils/server.rs`）（253） |
| R20 | 核心升级流程（`src-tauri/src/feat/core_upgrade.rs`）（43/66）、核心选择/升级 UI（`src/components/setting/mods/clash-core-viewer.tsx`）（27/111） |

| ID | Mlash 对照依据 |
|---|---|
| M1 | [Controller 公开命令](../../src/app/AppController.h)、[状态/设置/订阅字段](../../src/app/Types.h)、[Controller 实现](../../src/app/AppController.cpp) |
| M2 | [存储校验与版本](../../src/profile/ProfileStore.cpp)、[下载器](../../src/profile/SubscriptionDownloader.cpp)（10/62）、[维护策略](../../src/app/ProfileMaintenance.cpp)、[架构](../architecture.md) |
| M3 | [配置/规则/Provider 限制与合成](../../src/profile/ConfigBuilder.cpp)（12/62/82/94/110/113） |
| M4 | [代理快照/恢复/所有权](../../src/platform/ProxyCoordinator.cpp)、[固定代理目标](../../src/platform/windows/WindowsServices.cpp)（59） |
| M5 | [页面/规则/连接/设置/首页](../../src/ui/MainWindow.cpp)（333/530/571/606/903）、[节点呈现](../../src/ui/NodePresentation.cpp)、[连接会话](../../src/app/ConnectionSession.cpp) |
| M6 | [目标定义与调度](../../src/app/WebsiteTests.cpp)、[测速页面](../../src/ui/WebsiteTestsPage.cpp)、[API delay](../../src/core/MihomoClient.cpp)、[响应语义决策](../adr/0027-on-demand-website-tests.md) |
| M7 | [系统外观跟随](../../src/ui/UiTheme.cpp)（32）、[视觉决策](../adr/0028-apple-hig-adapted-visual-system.md) |
| M8 | [托盘](../../src/ui/TrayController.cpp)、[按需主窗口](../../src/ui/GuiHost.cpp)、[交互契约](../ui-interactions.md) |
| M9 | [原始规格](../Mlash_Codex_Design_Spec_v2.0.md)（技术/产品边界及3.3排除项）、[现有验收界限](../verification-report.md)、[需求映射](../requirements-traceability.md) |

本清单不声称单节点分享链接/自动订阅格式转换、带宽测速或完整历史统计是双方已经具备的能力；参考入口不足或仅见配置字段时，不凭字段名称推断端到端支持。
