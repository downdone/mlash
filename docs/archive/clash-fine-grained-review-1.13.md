# Mlash 1.13.0 与 Clash Verge Rev 细粒度对照

> 历史归档：仅用于追溯当时的评审、阶段方案和决策过程。当前行为以 [交互契约](../ui-interactions.md)、[架构](../architecture.md) 和 [项目状态](../project-status.md) 为准；文中阶段状态不代表当前版本。

2026-10-16 · D02 · 功能与操作链复核，未批准为新的开发排期。

对照对象是本机 `Clash Verge Rev 2.5.9 参考源码` 的 Clash Verge Rev **2.5.9**，不是泛指所有 Clash 客户端，也不是网上最新版。Mlash 基线为 **1.13.0 / 清理前代码基线**。参考目录没有 Git 元数据；package.json SHA256 为 `e5dd5babd7d00c91043e621532d6f3c7ea4a99b982a8d298d541bd4471a22ff4`，config/verge.rs 为 `acd20e809b2ddd46d637f40b42bc8df004a397b050cc0bcdac477b4871142a5a`。

本次静态读取双方入口、状态模型、命令与配置管线；结合已有 Mlash 截图/验证记录提出交互判断。没有编译或运行 Verge，没有新增双方运行比较、性能测量或用户可用性实验。参考代码调用外部插件/服务的地方，只证明存在调用，不证明外部实现已审计或本机行为通过。源码入口见文末。

## 1. 结论与判断标准

Mlash 已补齐上一轮 G01–G22 的主要实现，也新增了 TUN 服务。现在不宜继续用“有没有这个按钮”判断完成度：**功能可进入、用户能顺利完成任务、显示的状态可信、真实系统场景已验收，是四件不同的事。** 不提供缺少分母和运行证据的“完成百分比”。

最值得跟进的是三类：①修正首页和操作后的状态含义；②把已有功能接成完整流程，尤其保存失败、候选更新、规则与订阅管理；③TUN 的实际验收、可诊断性和必要的路由排除。再考虑批量操作、筛选、快捷键、更新和备份的深度。

下表“已有”仅表示 Mlash 存在对应实现，不代表与 Verge 完全同等深度或本机系统级验收。“部分”表示主流程已有但列出的子能力不足；“缺少”限定于当前检查的产品入口/实现；“取舍”是明确产品边界；“待验”不等同没开发。优先级是本次建议，**不会自动变成授权开发任务**。

## 2. 功能逐项对照

### 首页、导航与状态

| ID | 子能力 | 当前 Mlash 与参照差异 | 判断 / 建议 |
|---|---|---|---|
| A01 | 内核启停、系统代理、TUN、代理模式 | 首页/托盘均有；Verge 也有统一入口。[M01] [M03] [R01] | 已有；重点统一过渡状态和失败后的恢复动作 |
| A02 | 当前节点与实际出口 | Mlash 首页卡片读浏览组的直接 `now`；周期探测会按模式解析叶子。Verge 当前代理卡含组/节点控制。[M02] [M16] [R02] | 部分；卡片和探测需共用同一解析结果；规则模式标明“所选组”，不能宣称所有网站同一出口 |
| A03 | 首页快速换组/换节点 | Mlash 卡片跳到代理页；Verge 卡片可直接选择组/成员。[M01] [R02] | 缺少首页快捷选择；可做轻量选择弹层，保留代理页深入操作 |
| A04 | 流量、连接数、内核内存、短曲线 | Mlash 已有首页可见采样和有界曲线。[M17] [R03] | 已有；连接摘要卡应复用已取得的实时数量 |
| A05 | 出口 IP、地区、ASN、服务检测 | 已有按需诊断，但入口在弹窗；Verge 有首页IP卡和独立服务页。[M18] [R04] [R32] | 已有但入口较深；首页增加小型结果摘要/“检测出口”，不默认后台联网 |
| A06 | 首页信息有效性 | Mlash 规则摘要漏追加/屏蔽，连接卡固定文案，底部网络说明忽略TUN。[M02] | 已确认表达问题；先修正，见UX01–03 |
| A07 | 顶级导航/首页布局定制 | Verge 有导航排序、布局配置；Mlash 固定八页，窄窗自适应。[M01] [R05] | 取舍；可先允许隐藏低频首页摘要，暂不做任意拖拽仪表盘 |

### 订阅、导入与版本管理

| ID | 子能力 | 当前 Mlash 与参照差异 | 判断 / 建议 |
|---|---|---|---|
| B01 | URL/文件导入、协议确认导入 | Mlash 已有完整YAML、下载策略和自有协议显式确认；Verge 也有网页协议导入。[M04] [M05] [R06] | 已有；不能把“没有ss/vmess链接转换”当作这两种导入都没有 |
| B02 | 剪贴板/拖入文件快捷导入 | Verge profiles 有读剪贴板和拖入文件入口；Mlash 需打开导入窗填写/选文件。[M04] [R07] | 缺少快捷入口；识别后仍展示确认，不自动下载或激活 |
| B03 | 多选、批量更新、重新排序 | Verge 有批量更新入口和拖拽排序；Mlash 表格单选，没有批量订阅更新命令/顺序字段。[M05] [M06] [R07] | 缺少；批量更新先串行有界队列、逐项结果，不自动应用 |
| B04 | 修改现有 URL/订阅属性 | Verge 属性表单可改URL；Mlash 可重命名/改更新策略，文件可更换，URL没有直接编辑入口。[M05] [M07] [R08] | 部分；增加来源编辑并校验，新来源失败保留旧active/candidate |
| B05 | 自动更新与维护状态 | Mlash 已有URL周期/文件变化维护，1–168小时；Verge 有下一次更新时间显示。[M07] [M08] [R09] | 部分；展示最近尝试、最近成功、下次计划和失败重试，不只“最近更新” |
| B06 | 用量、到期与供应方入口 | Mlash 解析标准用量/到期并放在摘要/更新设置；Verge 订阅项更就近并有供应方主页入口。[M07] [R09] | 部分；独立订阅详情展示剩余/到期/观测时间；无来源数据就显示未知 |
| B07 | 候选、运行、上次可用和回滚 | Mlash 有独立版本/显式应用/失败恢复，是应保留的设计，不为模仿而删除。[M05] [M09] | 已有；缺“本次变化预览”：节点/组/规则/Provider新增、移除和选择失效提示 |
| B08 | 订阅二维码分享 | Verge 有QrViewer；Mlash 无分享入口。[R09] [M05] | 缺少，低优先；URL常含token，必须用户明确显示/导出，不默认展示 |
| B09 | 原文件编辑/增强脚本/全局Merge | Verge 有原文件编辑、全局Merge/Script；Mlash 保留源文件，只开放受控覆盖。[M10] [R10] | 取舍；不复制任意脚本执行；可补脱敏合成预览/差异，不把它伪装成任意配置编辑器 |

### 节点与策略组

| ID | 子能力 | 当前 Mlash 与参照差异 | 判断 / 建议 |
|---|---|---|---|
| C01 | 组切换、嵌套浏览、自动组解除固定 | Mlash 已有组卡、返回、恢复自动及实际回读。[M11] [M09] | 已有；需要更清楚的“自动/手动固定”持续状态 |
| C02 | 表格/卡片、位置国旗、默认/名称/延迟排序 | 已有，共用模型；国旗依据名称，不等同IP定位。[M11] [M12] | 已有；不要再作为缺失能力重新开发 |
| C03 | 单节点/本组测速、点击延迟重测、取消 | 已有，测速不禁用整个浏览列表；托盘展开按需测速也有。[M11] [M03] | 已有；完善结果时间/过期语义，避免旧值看成刚测值 |
| C04 | 节点条件筛选 | Verge 支持 `delay<…`、`type=…`及大小写/整词/正则；Mlash 节点搜索是名称固定字符串，连接搜索才有高级模式。[M11] [R11] | 部分；先提供地区/协议/可用状态/延迟筛选，不要求普通人输入语法 |
| C05 | 每组独立测试地址及视图状态 | Verge 有每组testUrl/headState；Mlash 只有全局延迟测试地址，排序/搜索主要属于当前窗口。[M06] [M11] [M13] [R12] | 缺少每组偏好；可选覆盖默认URL，记住组/视图/排序，隐藏后不继续采样 |
| C06 | 最近节点、收藏、常用组 | Mlash 有最近选择；没有收藏/置顶字段。Verge有组导航，未将其等同已具备收藏。[M03] [M06] [R13] | 部分；收藏/常用组是本次独立建议，非“Verge已存在”的结论 |
| C07 | 切节点后的既有连接处理 | Verge use-proxy-selection 有可选旧代理连接清理；Mlash选择成功不附带该策略。[M09] [R14] | 缺少；默认保留或明确询问，提供“断开受影响连接”选项，避免无差别关闭全部 |
| C08 | 节点/组编辑 | Mlash 可新增/覆盖/删除，但单项依赖YAML；Verge组编辑含结构化类型/成员等表单。[M14] [R15] | 部分；常用字段表单＋高级YAML，同一草稿；显示引用影响后再删除/改名 |
| C09 | 链式代理 | Mlash 静态节点→上游映射、循环检查已有；Verge有有序链界面。[M15] [R16] | 部分；补可视化“应用→上游→节点→目标”及方向，动态Provider节点仍标边界 |
| C10 | 大量组/节点的托盘操作 | Mlash已限高单列滚动/位置图标/延迟；Verge另有组显示模式/内联模式、托盘速率。[M03] [R17] | 部分；优先常用组置前、打开完整代理页/过滤入口；不把百节点菜单全摊到桌面 |

### 规则、DNS 与 Provider

| ID | 子能力 | 当前 Mlash 与参照差异 | 判断 / 建议 |
|---|---|---|---|
| D01 | 自定义前置/追加、屏蔽来源、顺序调整 | Mlash 已有；原始订阅不变。[M19] [M10] [R18] | 已有；首页数量与“沿用”判断需同时覆盖三类 |
| D02 | 规则编辑操作一致性 | 主页面是帮助式添加＋文本；“列表与来源”又有另一套表格，类型/目标是自由文本。[M19] [M20] | 部分；统一一个规则模型与编辑流程，选择中文类型/真实标识、策略/规则集，错误定位行 |
| D03 | 规则类型覆盖范围 | Mlash 自定义规则白名单有限，已有RULE-SET；来源rules/sub-rules部分由内核处理，不等同GUI能新增所有逻辑规则。[M10] [R18] | 部分；先清楚列出支持范围；AND/OR/NOT等按实际样本再扩展 |
| D04 | 规则集声明/新建 | Mlash 可引用已有rule-provider、刷新其快照，缺少创建新远程rule-provider的独立表单。[M10] [M20] [R10] | 部分；若要导入第三方规则集，需“添加规则来源→下载校验→引用策略”的完整事务 |
| D05 | DNS覆盖、来源/有效预览、查询 | Mlash已有，覆盖依赖YAML；Verge有DNS编辑/校验流程。[M21] [R19] | 已有但普通人门槛高；先做上游、模式、IPv6等常见字段表单及来源标记 |
| D06 | 节点/规则Provider列表、独立刷新 | Mlash已有候选快照，Verge有运行Provider入口；两者更新语义不同。[M22] [R20] | 已有但版本切换不直观；增加“运行/候选”切换和就近“应用此次更新”，不能冒充热更新 |
| D07 | Provider格式/本地文件兼容 | HTTP/inline、yaml/text/MRS domain/ipcidr已有；任意外部file路径仍拒绝，来源YAML merge键也拒绝。[M10] | 部分兼容/刻意边界；用实际失败样本决定扩展，不能把安全拒绝统称程序坏了 |
| D08 | Geo在线更新、本地导入、来源哈希 | Mlash已有官方固定来源/快照；TUN冻结Geo，避免会话自动更新。[M23] [M10] | 已有；显示运行与待用版本；不把下载成功显示成运行中已更新 |
| D09 | Provider独立下载代理/定时策略 | Mlash Provider下载复用订阅下载策略，但拒绝来源provider.proxy，冻结版本移除provider.interval；不等同已支持每Provider独立代理/后台更新。[M10] [M09] | 部分；先在兼容报告就近指向订阅下载策略；真正单独策略需扩展版本/维护事务，不直接开放核心热写 |

### 连接、日志与诊断

| ID | 子能力 | 当前 Mlash 与参照差异 | 判断 / 建议 |
|---|---|---|---|
| E01 | 活动/已离开、规则、链、流量/速率、关闭 | Mlash已有；已离开仅本次观察、离页清空是原需求。[M24] [M25] [R21] | 已有；不应照搬常驻历史而破坏轻量模式 |
| E02 | 大小写/整词/正则、列顺序/显示/宽度 | Mlash已有，不是再次缺少列管理。[M26] [R21] | 已有 |
| E03 | 进程/来源/目标IP/时间独立列 | Mlash详情/搜索包含这些数据，但表格只开放固定七列，上传下载合列；Verge有独立进程、来源、时间等列。[M25] [R22] | 部分；新增可选列，默认仍少列，数据缺失不猜进程 |
| E04 | 暂停连接刷新与阅读快照 | Mlash日志能暂停，连接没有独立暂停；详情是打开时快照。[M24] [M27] [R21] | Mlash易用性建议；本次未以Verge存在暂停作为依据。增加暂停/恢复/时间戳，不伪装连接仍活动 |
| E05 | 从连接定位组/创建规则 | Mlash详情主要纯文本，复制/关闭已有。[M24] [M25] | 缺少任务衔接；本次独立建议，不宣称Verge该入口已实现。可预填规则草稿、跳转链上组，仍需确认保存 |
| E06 | 日志筛选/暂停/导出/诊断 | Mlash已有有限历史、脱敏导出和状态栏全文复制。[M27] | 已有；可增加按错误类型就近重试，不扩大原始日志保存范围 |
| E07 | HTTP响应与服务支持判断 | Mlash目标测速和专门服务诊断分离，来源/路径和“未知”已有。[M18] [M28] [R32] | 已有；测速结果就近展示时间/目标/HTTP或错误详情，不能把403或HTTP200直接叫解锁 |

### TUN 与网络控制

| ID | 子能力 | 当前 Mlash 与参照差异 | 判断 / 建议 |
|---|---|---|---|
| F01 | 权限服务/普通GUI、启停、轻量生命周期 | Mlash已实现账户绑定会话和观察回读，原生UI/模拟回归已通过；实际网络未验收。[M29] [M30] [R23] | 实现＋待验；真实服务/驱动/路由/DNS/休眠恢复优先于再加复杂开关 |
| F02 | 栈和严格路由 | Mlash mixed/system/gvisor和严格路由已有；参照栈选项依其版本，不能直接复制未知栈值。[M29] [R24] | 已有 |
| F03 | TUN路由排除网段 | Verge有route-exclude-address输入和CIDR校验；Mlash配置固定，不提供该字段。[M10] [M29] [R24] | 缺少，优先；企业VPN/局域网冲突时可排除指定网段，需服务端重新校验及实际恢复测试 |
| F04 | MTU、设备、DNS劫持可配置 | Verge可编辑；Mlash固定Mlash设备/MTU1500/any:53。[M10] [R24] | 部分；MTU和受控DNS选择可后续加；设备名固定有利恢复归属，不必照抄可改名 |
| F05 | 网卡信息与出口观察 | Verge的network-interface-viewer是网卡/IP/MAC查看器，不能据文件名推断可选出口；Mlash TUN启用自动检测，允许来源顶层interface-name但无相应GUI。[M10] [R25] | 部分；先做只读网卡/实际出口观察。显式绑定作为独立建议，不能把来源配置已能指定说成完全不支持 |
| F06 | 服务诊断、升级/修复流程 | Mlash仅安装/移除/刷新、粗略已启动/已安装显示；无一体升级或诊断步骤。[M29] [M30] [R23] | 部分；展示安装版本/内核匹配/会话/清理状态，UAC取消可重试；失败恢复不能仅靠读日志 |
| F07 | LAN共享、多端口、外部控制/WebUI/PAC | Verge有相应入口；Mlash强制回环Mixed、私有API，未开放。[M10] [R26] | 取舍；个人本机使用不优先；若共享给手机，另做绑定、认证与范围设计 |
| F08 | 自动代理守护/UWP工具 | Verge有代理守护及独立UWP工具；Mlash尊重外部修改，不自动抢回。[M09] [R27] | 取舍；可提示冲突/用户明确接管。UWP应先确认TUN是否解决，避免额外提权工具 |

### 设置、备份、更新与桌面体验

| ID | 子能力 | 当前 Mlash 与参照差异 | 判断 / 建议 |
|---|---|---|---|
| G01 | 深浅系统/有限文字比例/键盘提示 | Mlash已有语义色和高对比适配。[M13] [M31] | 已有；屏幕阅读器、仅键盘全过程仍需实际验收，不能等同已通过 |
| G02 | 全局快捷键覆盖 | Mlash仅打开窗口/代理页，需手写组合；Verge另有模式/系统代理/TUN/重载等动作。[M32] [R28] | 部分；增加按键录制和有限动作，切代理仍走接管/观察契约 |
| G03 | 设置检索与高级功能可发现性 | Mlash两列设置已有，但工具较多、同类配置分散在订阅右键/规则/设置。[M05] [M13] | 本次UX建议；设置搜索＋功能定位，保持分类和就近入口，不新增十几个顶级菜单 |
| G04 | 本地密码备份/预览/恢复 | Mlash已有，恢复新增订阅；可选恢复偏好并保留本机敏感意图。[M33] | 已有；可补选择订阅/同名冲突预览；不需要默认覆盖整套数据 |
| G05 | 纯本地自动备份/变更前快照 | Verge auto_backup支持本地计划和变更触发；Mlash有手动本地备份和远程任务附带本地缓存，缺独立本地自动备份。[M33] [M34] [R29] | 部分；优先本地计划/重大变更前备份、保留数/占用和恢复入口，无需WebDAV账户 |
| G06 | WebDAV/定时/历史 | Mlash已有加密远端版本、下载预览、定时/重试和五份本机缓存。[M34] | 已有；补更直观的最近成功/失败、容量、版本说明，公网账户/跨账户实测单列 |
| G07 | 内核更新 | Mlash有本地预检、副本应用/失败回滚；Verge有官方版本查询/下载、stable/alpha工作流。[M35] [R30] | 部分；优先“官方稳定版→下载校验→预检→确认应用”，TUN服务内核必须同步协调升级，不能绕过SHA固定 |
| G08 | 程序更新、语言、平台、脚本/主题扩展 | Verge有程序更新/多语言/多平台及更自由定制，Mlash未做。[M06] [R05] [R31] | 取舍；优先做好新版目录/自启路径迁移提示。跨平台、任意JS/CSS、全量国际化暂不优先 |

## 3. 已定位的交互问题：先改这些

以下是源码可定位的现状与改进建议，尚未通过本轮交互实验重现；不是仅凭截图评价美丑。

| ID / 优先级 | 现状与用户后果 | 推荐设计与可观察验收 |
|---|---|---|
| UX01 / P1 | 首页当前成员取`selectedGroup_.now`，可能是子组，也不随global/direct切到真实模式出口；下方探测却按模式解析。[M02] [M16] | 同一结果模型驱动卡片/探测/跳转：规则“所选策略组及其叶子”，全局“GLOBAL实际叶子”，直连“DIRECT”；负载均衡无唯一叶子时明确标注。切模式/浏览组后两处不矛盾 |
| UX02 / P1 | `customRules.isEmpty()`即“沿用订阅规则”，追加或屏蔽单独存在时也这样；连接卡固定“尚未观察”，同页已有实时数量。[M02] | 规则显示前置/追加/屏蔽三类数量；连接复用当前可见采样、首次/过期/失败分别未知。不额外持续采集连接明细 |
| UX03 / P1 | TUN已接管但底部仍按系统代理显示“未开启/其他程序管理”；导入对话框仍写“TUN/LAN关闭”。[M02] [M04] | 网络摘要合成内核/系统代理/TUN三个维度；导入文案改为“源TUN字段由Mlash设置管理”等真实描述。覆盖TUN与系统代理四种组合 |
| UX04 / P1 | 订阅更新设置点击保存就`dialog->accept()`，异步保存失败也已离开编辑窗口。[M07] | 只有成功才关闭；失败保留地址/UA/周期草稿，在原窗明确错误与重试。通过注入保存失败验证草稿和旧索引不变 |
| UX05 / P1 | 主窗口释放只检查主规则/主设置dirty；DNS、节点、链、规则列表、测速目标等子窗直接reject，未纳入统一释放保护。[M36] [M14] [M15] [M19] [M21] | 子编辑器注册dirty/在途状态；关窗或轻量释放统一“保留编辑/放弃”，显式取消仍可放弃。不要偷偷将含凭据草稿落盘；验证子窗草稿不被主窗直接销毁 |
| UX06 / P1 | 已有候选隔离，但更新后用户要关Provider窗口再去找“应用更新”，没有变化说明。[M05] [M22] | 完成区提供“查看变化/应用更新”，显示运行与候选时间、数量变化/失效选择；独立候选、验证与回滚保持。更新失败不出现可应用的假状态 |
| UX07 / P1 | 当前选中、正在浏览、实际使用、测试对象分散在不同控件/页脚。[M11] [M28] | 统一“选中”与“使用中”标记；卡片单击预选、双击/Enter使用，延迟点击只测；全流程对象名和时间就近可见，不另加多行常驻技术说明 |
| UX08 / P1 | TUN未安装/内核SHA不符/清理未确认时只有禁用与消息，缺下一步指引。[M29] [M30] | 在相邻位置解释禁用原因，给“安装服务/查看详情/重试清理”；显式安装后引导启用，UAC取消留在原位。不得提供未经归属验证的“强制清理系统路由”按钮 |
| UX09 / P2 | 规则列表目标/类型自由文本，节点组编辑原始YAML，普通人容易写错引用。[M14] [M19] | 常用字段用选择器/搜索，YAML作为高级模式；错误定位具体项，删除显示受影响组/规则，名称变更能预览引用影响 |
| UX10 / P2 | 连接详情是整段快照文本，不能顺手定位组或建立规则。[M24] [M25] | 详情按目标/进程/链/规则分组，单字段复制；“添加规则”只预填草稿，未保存不影响网络。对无进程数据/已离开条目维持正确语义 |
| UX11 / P2 | 大型节点集合靠搜索名、最近选择和全部组；托盘常用对象需多层进入。[M03] [M11] | 常用组/收藏、地区/协议/可用性筛选；托盘仅展示重要快捷项，其余打开完整代理页；键盘可达、长名称完整提示 |
| UX12 / P2 | 轻量重建只记窗口几何、页和表格/卡片，组/排序/搜索位置等缺少成体系恢复。[M37] | 分层：记住用户偏好和浏览组；敏感搜索/连接详情/测速运行不持久化；重建恢复选择但不自动测速。重启和仅重建的行为分别定义 |
| UX13 / P2 | 状态栏已有固定高度/详情，是有效改进；错误主要仍为文字。[M27] | 保留单行状态栏，重要失败给就近“重试/查看设置”，短成功消息自动复位；当前对象不被另一个低优先成功覆盖。不要重新引入随消息变化高度的横幅 |
| UX14 / P2 | 功能越来越多，但首次导入后主要靠用户理解“使用订阅→启内核→接管”。[M01] [M04] [M05] | 空态给一个当前可执行主动作，成功后指向下一步；TUN和系统代理分别解释。已有订阅/熟练用户不重复弹向导，网站/协议请求不自动执行全链 |

## 4. 推荐跟进顺序与完成标准

| 批次 | 建议范围 | 完成标准 |
|---|---|---|
| 0：验收欠账 | F01，原系统代理/自启/休眠等真实系统场景 | 在受控VM记录安装/移除、UAC取消、IPv4/IPv6/DNS、服务/GUI强杀与恢复；暂时没有VM就保持待验，不拿模拟结果替代 |
| 1：修正认知与失败流程 | UX01–06、UX08 | 不改系统接管规则；模式/首页/规则/连接摘要一致，旧文案清理，失败留稿，子窗关闭保护，候选可看懂并可就近应用 |
| 2：补TUN必要可控性 | F03、F05只读观察、F06；F04按实际问题补MTU | CIDR有边界/IPv6校验，服务端受控合成和回读/回滚完整；提供准确服务诊断，真实网卡场景单列验收 |
| 3：日常高频操作 | B02–06、C04/C07、UX07/11/12 | 批量更新逐项结果/取消且不自动应用；修改URL失败保留；筛选/切节点/旧连接反馈可预测，轻量重建偏好明确 |
| 4：高级功能变易用 | C08/09、D02/04/05、E03/05、G02/03/05/07 | 表单/高级编辑共用草稿与事务；完整撤销/错误定位；稳定版内核更新和TUN服务升级协调；本地备份无需云账号 |
| 暂不跟进 | 任意JS/CSS、开放API/WebUI/PAC、自动抢代理、任意仪表盘、多语言/跨平台/静默更新 | 用户有明确场景再独立决策；不能为“选项数量相同”增加长期负担 |

其中候选差异预览、收藏、连接转规则、设置搜索、连接暂停是基于Mlash操作链提出的独立建议；没有把这些全部标成“Verge已有、Mlash落后”。网络切换与休眠本次同样仅列为可靠性验收：参考`network_watch.rs`明确针对macOS，**不能拿它证明Verge在Windows已有相同网络恢复**。

## 5. 源码证据入口

每张表的链接定位实际入口；缺失判断结合相关UI、Controller公开命令、Types/存储字段检查，而非只搜一次中文字符串。进一步开发前需针对所选项细化契约和测试。

[M01]: ../../src/ui/MainWindow.cpp
[M02]: ../../src/ui/MainWindow.cpp
[M03]: ../../src/ui/TrayController.cpp
[M04]: ../../src/ui/ImportDialog.cpp
[M05]: ../../src/ui/MainWindow.cpp
[M06]: ../../src/app/Types.h
[M07]: ../../src/ui/MainWindow.cpp
[M08]: ../../src/app/ProfileMaintenance.cpp
[M09]: ../../src/app/AppController.cpp
[M10]: ../../src/profile/ConfigBuilder.cpp
[M11]: ../../src/ui/MainWindow.cpp
[M12]: ../../src/ui/NodePresentation.cpp
[M13]: ../../src/ui/MainWindow.cpp
[M14]: ../../src/ui/ProxyEditorDialog.cpp
[M15]: ../../src/ui/ProxyChainDialog.cpp
[M16]: ../../src/app/HomeNodeProbe.cpp
[M17]: ../../src/app/HomeMetrics.cpp
[M18]: ../../src/ui/OutboundDialog.cpp
[M19]: ../../src/ui/RuleOverlayDialog.cpp
[M20]: ../../src/ui/MainWindow.cpp
[M21]: ../../src/ui/DnsDialog.cpp
[M22]: ../../src/ui/MainWindow.cpp
[M23]: ../../src/ui/GeoDataDialog.cpp
[M24]: ../../src/ui/MainWindow.cpp
[M25]: ../../src/ui/ActivityTableModel.cpp
[M26]: ../../src/ui/ConnectionOptions.cpp
[M27]: ../../src/ui/MainWindow.cpp
[M28]: ../../src/ui/WebsiteTestsPage.cpp
[M29]: ../../src/ui/TunDialog.cpp
[M30]: ../../src/platform/windows/TunServiceMain.cpp
[M31]: ../../src/ui/UiTheme.cpp
[M32]: ../../src/ui/HotkeysDialog.cpp
[M33]: ../../src/ui/BackupDialog.cpp
[M34]: ../../src/profile/RemoteBackup.cpp
[M35]: ../../src/ui/CoreUpdateDialog.cpp
[M36]: ../../src/ui/MainWindow.cpp
[M37]: ../../src/ui/GuiHost.cpp
[R01]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/shared/proxy-control-switches.tsx
[R02]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/home/current-proxy-card.tsx
[R03]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/home/enhanced-traffic-stats.tsx
[R04]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/home/ip-info-card.tsx
[R05]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/setting/mods/layout-viewer.tsx
[R06]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src-tauri/src/utils/resolve/scheme.rs
[R07]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/pages/profiles.tsx
[R08]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/profile/profile-viewer.tsx
[R09]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/profile/profile-item.tsx
[R10]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/profile/profile-more.tsx
[R11]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/proxy/use-filter-sort.ts
[R12]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/proxy/proxy-group-tools.tsx
[R13]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/proxy/proxy-group-navigator.tsx
[R14]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/hooks/use-proxy-selection.ts
[R15]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/profile/groups-editor-viewer.tsx
[R16]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/proxy/proxy-chain.tsx
[R17]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src-tauri/src/config/verge.rs
[R18]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/profile/rules-editor-viewer.tsx
[R19]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/setting/mods/dns-viewer.tsx
[R20]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/proxy/provider-button.tsx
[R21]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/pages/connections.tsx
[R22]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/connection/connection-table.tsx
[R23]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src-tauri/src/core/service.rs
[R24]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/setting/mods/tun-viewer.tsx
[R25]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/setting/mods/network-interface-viewer.tsx
[R26]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/setting/setting-clash.tsx
[R27]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/setting/mods/sysproxy-viewer.tsx
[R28]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/components/setting/mods/hotkey-viewer.tsx
[R29]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src-tauri/src/core/auto_backup.rs
[R30]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src-tauri/src/feat/core_upgrade.rs
[R31]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src-tauri/src/core/updater.rs

[R32]: https://github.com/clash-verge-rev/clash-verge-rev/tree/v2.5.9/src/pages/unlock.tsx
