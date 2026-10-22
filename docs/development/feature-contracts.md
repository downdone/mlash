# 功能模块契约

当前有界数据、并发、事务和取消细则；总体状态与模块归属见 [架构](../architecture.md)。实现/测试状态由 [验证报告](../verification-report.md) 维护。

## 视觉组件与生命周期

UiTheme拥有语义视觉；WidgetStyle以单一应用级过滤器记录输入来源和普通按钮键盘行为，焦点重polish按控件合并排队，销毁自动取消。主题重应用仅配置Combo已有view，不在Qt遍历时替换其对象。WrapLayout只服务有界工具栏，节点仍虚拟绘制；ElidedLabel保留纯文本原值、固定单行绘制和完整提示。页面/托盘/维护窗布局收敛不改变Controller命令、线程、网络并发、缓存、持久化schema或恢复事务。

## 订阅批量维护

订阅批量更新由 Controller 拥有最多 256 项的串行队列，在单一 busy 周期内逐项复用下载/不可变候选/内核校验/原子索引事务；单项失败保存维护结果后继续，GUI 可关闭但任务结果保留至下次批量任务。取消停止后续队列，当前已开始的事务安全完成；退出也停止后续项，不将取消伪装为回滚。来源编辑带名称/来源/周期/下载策略的期望快照，来源变化先校验新内容，成功才原子提交新来源与候选，失败保留旧来源/active/candidate；元数据编辑不使用独立下载候选。订阅向量顺序即持久顺序，不增加重复排序字段。维护状态由调度器只读给出实际下次时间/退避次数。剪贴板/拖入只预填确认窗，不自动读取文件、请求网络或激活。二维码采用固定源码/哈希的 Nayuki 1.8.0 静态库，用户明确展示后本地编码，最多 2048 UTF-8 字节；无在线二维码服务、默认显示或自动导出。没有供应方主页元数据时不推测官方主页。

## 节点偏好与筛选

Profile.node_preferences 为有界声明式偏好（256KiB，最多256组/512收藏或常用项）：组测试URL/视图/排序、收藏成员、常用组、最近组与可选受影响连接清理。严格校验未知字段、名称、URL、整数枚举；旧索引缺省空，随加密备份保留。偏好经Controller期望快照+preferences事务保存，不重启内核。延迟测试携带组地址，缓存键含节点和测试URL，保存实际时间/60秒新鲜度；不同URL不互相显示为刚测结果。NodeFilter在同一代理模型上组合名称、名称推测地区、类型、结果状态、延迟和收藏，表格/卡片共用；筛选不改变选择。GUIHost仅保留无QObject的会话书签，轻量销毁后恢复组/视图/排序/筛选/预选/滚动，不启动隐藏采样。

## 选节点后的连接处理

选择成员默认保留连接。明确启用后，先快照经过所选组的连接ID，再选择/回读/保存，然后最多32条、三并发、每请求1.5秒局部关闭并回读；其他组和切换后新建的连接不在清理范围。超量/失败/无法回读以保留或未确认明确反馈，不将节点切换成功冒充连接全部关闭。没有任意关闭全部、隐式开启TUN或系统代理。

## 规则草稿

RuleDraft是受控CSV类型/字段编解码、目标/规则集/选项检查和规则覆盖快照的唯一实现，ConfigBuilder、文本页和列表编辑器共用；核心仍校验网段/域名等完整负载语义。列表的类型/目标/规则集/选项为真实值选择器，中英展示不写入配置；错误就近定位行/单元格。来源引用按需worker读取、最多3项在途，普通页面或隐藏GUI不为元数据更新发起来源读取。文本的待保存规则可带入模态列表，取消不改原文本，保存成功才同步主草稿；比较前置/追加/屏蔽快照拒绝旧窗口覆盖。失败保留草稿与提示；原始来源、独立候选和运行事务继续由原管道保护。

## 规则集声明

每订阅以有界rule_provider_edits保存HTTPS规则集的名称、行为、格式和明确移除标记；声明和前置/追加/屏蔽规则作为一个Controller事务下载、静态及内核校验后提交。原始source.yaml不改写。版本目录的overrides.json保存该版本完整规则/DNS/节点/规则集覆盖及来源SHA；旧版缺失元数据在首次声明变更前冻结旧意图。Provider缓存清单同时绑定内容SHA和kind/name/url/behavior/format的来源签名，声明变化强制重新下载，离线重建只从当前引用版本复用签名和内容都匹配的快照。Provider更新不改变订阅来源更新时间。备份按各版本的覆盖集合导出/恢复，旧档缺省沿用其原共享覆盖行为。GUI加入声明草稿不联网，最终保存失败保留编辑内容与旧索引；原始来源的复杂引用仍由内核做完整检查。

## Provider 计划

provider_policies按源生成的冻结ID保存，每订阅最多64项/64KiB，可显式指定DownloadPolicy或缺省继承订阅；自动更新默认关，周期1–168小时。Controller仅在存在自动项时启动60秒粗定时器，在有界worker读取候选快照的实际下载时间，串行选取到期项，失败按5分钟起、最多1小时退避，不启动内核或回退下载路径。后台只生成候选，来源更新时间不变；GUI不持有计划，退出停调度。失败/最近尝试为有界会话状态，重启后重新依据冻结时间判断，不伪造下载成功。已不在候选的策略仍可见并可明确清除。备份保留下载策略，恢复关闭所有Provider自动计划。

## 共享 YAML 表单

YamlFields只在worker处理单映射、64KiB/32层/5000节点预算的解析与指定键补丁，拒绝多文档、重复/merge键和自定义tag，错误仅字段/行号，不回显内容。YamlFormEditor拥有一份QPlainTextEdit草稿、单在途任务及有界待合并字段，来源变化使旧结果失效；未触及键/复杂值保留，无法用表单表达的值只允许高级文本编辑，不静默转为空。GUI只保存草稿，最终写入仍经Controller既有事务和内核校验。

## 节点链接导入

NodeLinks在既有有界worker离线解码/合成YAML，不读文件、解析DNS或请求网络。NodeLinksEditor只保留当前输入/单份预览，通过Controller命令获取；关闭用QPointer丢弃接收，不留GUI引用或新增调度。新建快照复用stageSource和原候选事务，已有订阅导入复用节点覆盖/版本快照/内核校验。原始链接不写入索引，生成配置及凭据沿既有私有版本目录与加密备份管理，无新schema/注册表协议或系统权限。

## TUN 会话

Windows TUN 权限扩展由 ADR0036 维护。CoreManager 在普通 QProcess/Job 与 TunSession 服务会话之间选择；两者共用 API/Mixed PID 归属、就绪和配置事务。服务内核固定随包 SHA，客户端发送冻结工作快照路径，服务模拟账户读取、重新合成受控配置并复制到受保护会话。服务拥有独立私有内核凭据；TunApiGate 在回环端口仅转发允许的查询/节点选择/模式/连接操作，禁止升级、重启、完整配置与 Provider 写入。GUI 的 API 端口由服务 PID 拥有，Mixed 端口由服务子内核 PID 拥有。原生管道连接/SCM/文件在 worker 执行，句柄以 RAII 跨线程转交；取消不会泄漏句柄。关窗不影响 Controller 的会话；断线后的停止须重新连接服务取得 idle/cleanup 确认，未确认不会按成功提交关闭意图或启动替代内核。

## 固定状态栏

全局反馈控件从页面内容布局移至常驻QStatusBar，单行省略但保留完整文本，字体变化时调整固定高度；消息生命周期仍只在MainWindow的局部3秒定时器和既有Controller信号内，不增加业务状态/存储/后台任务。交互由ADR0035维护。

## WebDAV 备份

RemoteBackup负责有界worker归档/本机缓存/下载校验，WebDavClient负责单在途DNS固定IP、严格HTTPS和DAV版本提交；Controller串行拥有任务/计划，窗口释放不影响定时备份。remote_backup为本机DPAPI设置，便携备份移除、恢复保留。只读目录/下载仍使用beginPreferences串行，但不改变内核generation。协议、预算、回读和失败保留由ADR0034维护。

## 内核副本维护

CoreUpdate负责有界文件副本/PE架构/哈希/显式清理，均在worker执行；CoreVersionProbe仅异步版本进程，取消/超时不提交。Controller串行预检与应用，应用时保留旧副本并以新程序执行配置校验，运行后核对API版本。独立候选、失败恢复和本机previous_core_path见ADR0033；GUI只呈现版本/SHA并提交token。

## 系统代理排除

系统代理排除项由单一normalizeProxyBypass规范化，存储只接受规范字符串；Windows目标仍包含flags/server/bypass/pac四字段。所有Controller接管/恢复意图使用该设置。更新当前受控目标及崩溃恢复边界见ADR0032；偏好提交失败恢复旧目标，外部所有权不强行夺回，不为编辑排除列表隐式启用代理。

## 协议导入

自有协议、关联归属、账户管道与确认边界由ADR0031维护。ProtocolInbox只校验/转交，不调用业务写命令；ProtocolImportQueue仅在Controller初始化和空闲后恢复GUI并显示既有ImportDialog，URL默认隐藏，最终确认才调用importUrl。关联检查为单在途worker；写入/实际回读由Controller串行处理，非Windows明确Unsupported，不随启动注册。

## 首页节点探测

Controller拥有HomeNodeProbe；首页显式选择关闭/60秒/5分钟，默认关闭。只在首页可见、内核就绪/空闲且有可解析叶子时单在途GET/proxies/{leaf}/delay；首次开启立即测，完成后再计周期。规则模式检测首页标注的浏览组实际叶子，全局使用GLOBAL叶子，直连用DIRECT，负载均衡等不猜叶子。节点/核心代次/测试地址变化、离页/隐藏/写命令取消本请求并清空会话，不调用全局cancelAll、不锁busy、不改选择；结果共享有界延迟显示，不保存检测历史。

## 拨号链

proxy_edits.dialers为静态节点名→上游节点/组名，空字符串显式清除来源dialer-proxy，缺项继承。最多512项，和G16共享256KiB限制；先合成节点/组，再应用拨号覆盖，组成员边与节点拨号边联合检查缺失/保留策略/自环/间接循环和64层预算。动态Provider节点不提供本地拨号改写。保存经同一候选/运行/失败恢复事务，不主动改组成员、关闭连接或更改模式。内核API不暴露完整拨号字段，不将普通proxies回读冒充逐链验证；实际本地双CONNECT路径作为行为证据。

## 节点与策略组覆盖

每订阅proxy_edits为声明式nodes/groups名称→单项YAML及remove_nodes/remove_groups列表。合计256KiB、单定义64KiB、最多512节点/256组覆盖，名称不可为保留内建策略。worker按来源顺序替换/移除、追加新项，校验名称冲突、成员与Provider引用和64层有界组图循环，之后沿用完整兼容性检查/真实内核-t；禁止额外文件/监听的边界继续适用。运行事务在提交索引前GET/proxies核验编辑项存在和移除项消失，失败恢复旧版本；来源和独立下载候选不写。编辑器仅在主动编辑时显示所选定义（可能含凭据），不进日志；旧窗口保存检查覆盖快照，避免覆盖其他编辑窗口结果。加密备份/恢复保留覆盖。

## 外观偏好

外观意图为system/light/dark，文字比例仅90/100/110/120，旧索引默认system/100。操作留在设置草稿，应用后由Controller串行原子保存；纯外观使用beginPreferences，不重启内核、不取消网站测试或覆盖其他草稿。UiTheme统一字体与样式字号，系统高对比优先于手动主题；跟随系统才响应普通系统深浅变化。首页可滚动，小窗与有限放大不强迫扩大窗口；不加载外部字体或任意主题代码。

## 连接视图偏好

`connection_view`只保存大小写/Unicode整词/正则开关和列的显示/宽度/顺序（兼容原七列及当前十五列），不含搜索文本/连接条目。列ID/唯一性/24–1000宽度/目标列可见严格校验；默认自适应目标列，用户调整后按保存布局。400ms去抖，隐藏/释放前提交未保存偏好；Controller忙碌时只保留最新待写快照，空闲后串行worker原子保存。纯视图偏好使用独立beginPreferences，不改变核心generation、不取消在途只读探测；失败保留原索引并统一反馈。正则输入≤256，搜索字段≤4096，PCRE匹配/递归预算1000/64，不接受用户控制指令；非法表达式保留最后有效结果并就近提示。

## 全局快捷键

全局快捷键由独立平台适配器拥有，默认空配置不创建线程或注册。启用后最多一个Qt Core事件线程异步RegisterHotKey/UnregisterHotKey，窗口释放不影响；线程没有GUI引用/轮询，应用退出在该线程清理并结束。F08/S09将固定动作扩为show-window/show-proxy/show-connections/cycle-mode/toggle-system-proxy/toggle-tun/reload-active，新增动作默认不绑定；录制由Controller的单一token会话拦截实际注册键，失焦按事件尾取消、隐藏/任务开始立即取消，结束后300ms拒绝排队激活。网络动作忙时忽略，不接管外部代理或安装服务，重载仅使用active并保留candidate；注册使用preferences事务及期望快照，不改变Core generation。每次重注册递增平台epoch，旧排队消息不能执行新动作；Ctrl/Alt至少一个，可加Shift/字母/数字/F1–F24，排除F12/Win保留组合。固定动作ID和键/修饰符核验丢弃旧键消息，MOD_NOREPEAT防重复。新配置冲突先撤销部分新键并恢复旧键；索引保存失败也异步恢复已观测映射。注册结果与持久意图区分，启动失败显示原因，可原样重试。备份恢复保留本机快捷键，避免迁移时隐式注册；原设置主提交不改此字段，单独Controller命令负责。Qt6.10.2工作线程的线程消息走windows_generic_MSG，兼容dispatcher_MSG；源码核对：https://raw.githubusercontent.com/qt/qtbase/v6.10.2/src/corelib/kernel/qeventdispatcher_win.cpp ，Win32契约：https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey 。

## 服务诊断

出口诊断的服务页按需串行检测ChatGPT/YouTube Premium/Netflix/Claude/Gemini固定公开端点，每项10s/2MiB；共用G11传输路径和归属核验。下载器的显式inspectResponse模式返回完整HTTP状态/有界内容，不跟随重定向；TLS/超时/大小限制不改变，普通订阅非2xx仍失败。判定只输出固定依据、地区、状态、目标/时间/目的IP，不保存原始页面。明确地区错误/页面限制为受限，预期特征只能为疑似支持，验证码/未知结构/网络证书错误为未知；不维护猜测的国家黑名单，不用普通200/403推断账号/播放可用，Claude trace仅报告边缘地区。取消/隐藏/写操作清空队列，单项重测也不改变节点或解除固定。

## 出口诊断

出口诊断为显式的会话内请求，默认经当前受控Mixed端口，也可选NoProxy直连；core路径先worker核验PID/端口归属，不回退。固定IP.SB `/geoip`（字段定义：https://ip.sb/api/），本地DNS全地址检查、固定目的IP、保留Host/TLS peer；15s/64KiB。只接受合法公网IP，地区/ASN缺失为未知；保存本次目标、最终主机/目的IP、观测模式/端口和时间，不以节点名字推断链。此IP仅代表该次请求，目的以经检查IP的HTTP代理请求发出，不能假装所有域名规则共用此出口。结果不进入索引或日志；关闭对话框/隐藏GUI/写操作/内核停止取消并清空，Controller仍拥有任务。每次请求按需创建下载器，旧代次不能写新结果。

## 首页指标

Controller拥有HomeMetrics，首页可见且内核就绪/空闲时2s读取内核连接总计和数量，1.5s单在途；不保留连接明细。内存以worker中的Windows进程工作集读取，不是GUI内存或订阅用量。首次/计数回退/超过10s缺口的速率未知；失败清空实时值，关闭/离页/写操作/内核代次变化局部取消并清空。曲线内存最多30个/60s单调时间样本，按实际时间绘制，无磁盘历史或关窗采样。窗口销毁仅清理GUI，采样器不持有窗口。

## Geo 管理

Geo管理区分默认快照与当前运行目录文件；按需worker读取大小/时间/哈希及来源，并说明来源自定义geox-url和geo-auto-update。在线只允许MetaCubeX发布的country.mmdb/geoip.dat/geosite.dat/GeoLite2-ASN.mmdb固定地址（来源清单：https://raw.githubusercontent.com/MetaCubeX/meta-rules-dat/master/README.md）；直连、目的地址检查/TLS/重定向共用下载器，显式128MiB/120s预算不扩大订阅默认8MiB。数据先入私有暂存目录，以仅引用该文件的配置做15s内核-t，校验配置将自动补下载地址固定为不可用本机端口，避免数据损坏触发公网修复。校验后SHA未变才合并既有默认文件并执行旧目录保留/重命名事务；既有文件哈希变化或任何失败不替换。当前active工作目录不写，下次重建无自定义Geo来源的版本优先复制默认快照。本地导入仍是受控原始文件导入，内核使用时再校验，不冒充全部数据格式已验证。

## MRS 快照

远程rule-provider支持yaml/text及MRS domain/ipcidr，仍转换为受控file快照。MRS保持原始二进制/哈希，Provider列表使用头部规则数；下载、复用、备份/恢复均校验行为一致。Zstandard固定1.5.7源提交与SHA，静态链接且不新增DLL；输入8MiB、窗口/解压输出64MiB，先检查版本/长度/位图树结构或IP范围，再执行真实内核配置验证。不开放classical MRS或任意本地路径，不实现规则匹配引擎。

## DNS 覆盖

每订阅保存可选 `dns_override_enabled` / `dns_override`（64KiB）。ConfigBuilder 在来源之上覆盖开放字段，策略映射按键合并，未指定字段沿用来源；禁止新增DNS监听。与规则覆盖共用候选校验/运行切换事务，运行中修改不激活独立下载候选。DNS预览在有界worker读取，凭据及URL参数脱敏；解析使用内核当前配置，不能宣称指定叶子出口或写入系统DNS。查询句柄和token独立取消，窗口销毁及任何写操作使旧查询失效。

## 共用视觉层

UiTheme 管理共用浅/深/高对比语义色、排版/尺寸和应用级样式，WidgetStyle 管理无动画的独立圆角弹出层。UI 保留日志阅读选区、草稿计数和短成功反馈；此共享视觉层不直接发出业务或系统写入。任务书适配与边界见 ADR 0025、ui-ux-changelog。

## 网站测试生命周期

常见目标测速通过 Controller 所有的 WebsiteTests 读取指定节点 API；默认组到具体叶子冻结、三并发/5秒核心超时、局部 abort/代次与销毁保护，状态和生命周期见 ADR 0027。结果独立于节点/托盘缓存；核心 URLTest 仍会更新其自身历史，HTTP 响应不等于解锁。启动页内部 ID 0–6 保留，新增测速为7，显示顺序用显式映射。

## 外观与资源

UiTheme 在 QApplication 层统一安装，Qt ColorScheme/AccessibilityHints 变化一起更新语义角色与样式，再通知窗口刷新导航/按钮图标和数据视图；不写系统外观。主题管理对象只由应用拥有，无页面引用或轮询；页面/图标仍随GUI释放。ProxyTableModel只读取共用语义色，独立引擎测试不链接Widgets。Windows排版采用Segoe UI和显式Han脚本微软雅黑UI回退。UiIcons提供统一线条操作图形及内置服务PNG；源和哈希由resources/services维护，运行时不取favicon/不加载SVG。构造AppController可注入有界WebsiteTarget定义供隔离HTTP夹具，生产默认12个HTTPS地址不变。详细决策见ADR0028。

## 移除引用分析

ProxyImpact在Controller的按需单worker中合并原始节点/组定义与当前局部草稿，并读取当前已保存有效规则，允许未保存定义尚不完整。只返回有界的引用身份和分析边界；动态Provider、隐式包含、复杂逻辑及损坏定义不被当作完整图。来源版本/代次/已保存覆盖在回调时复核，GUI加入草稿前再次比较；取消分析不写。节点移除同时去掉自身失效的独立拨号覆盖，不替用户改其他引用，最终提交继续由原ConfigBuilder/真实核心/原子版本事务负责。

## 连接暂停与列兼容

连接暂停属于Controller的可见会话状态，取消当前轮询reply、递增读代次并停止timer，不影响已授权写操作。ConnectionSession冻结活动条目并清差值基线，恢复首个样本不从暂停间隙推断离开；当前快照与最后成功接收时间分开，失败清当前数据但保留上次时间，隐藏/离页/核心更换清会话。UI冻结代理自动重排，字段查询/主动排序仍可用，关闭命令在Controller也验证当前样本。列ID为append-only，持久文档兼容完整旧七列/新十五列，仅存宽度/顺序/可见及搜索flags，不存流量/搜索文本/暂停快照。主连接页的搜索错误使用既有底部状态位，不插入额外布局行。

## 诊断摘要

MainWindow在可见GUI内只缓存出口结果的七个有界显示字段，并绑定Controller generation/就绪观察。隐藏/上下文变化/新诊断清除；完整OutboundDiagnostics结果仍随诊断关闭清空，摘要不进入索引、日志或备份。Website/服务/连接详情均为打开时不可变GUI快照，动作通过Controller已有命令和期望快照，字段复制需显式点击。具体生命周期与显示边界由ui-interactions/API契约拥有。

## 选择性恢复

BackupArchive接受预览SHA绑定的有界唯一整数索引，Undefined兼容全选，空集合只允许明确恢复偏好。worker复用全档案元数据校验，按原档案顺序只重建所选版本/Provider，所选数计入256上限。restoreProfileNames为预览与提交共用纯命名器，忽略大小写避开已有名称，120字符内追加（恢复 N），不修改现有ID/文件。实际恢复仍先停止核心/释放本程序代理，提交失败只清理本次UUID。UI勾选和本地名称预览不写入，文件/密码变化使SHA预览失效；正文有界滚动、固定错误和提交，名称不再在摘要重复。独立本地计划/重大变更备份及远端历史摘要见下文维护闭环与ADR0039。

## 固定发布内核

发布内核由 dependencies.lock.json、cores/manifest.json 与 TunCoreSha256 绑定同一版本和 SHA256。bootstrap 验证固定产物，package 拒绝不符内核；TUN 仅运行固定哈希并要求显式安装/升级。

## 半关闭回收默认策略

ConfigBuilder在来源缺失方向时补client300/server30秒；已开放并校验来源字段，显式0/自定义值保持。普通/TUN二次合成和旧版本从source重建共用该策略，不写回source、不新增持久设置或GUI资源，不变更内核二进制SHA。默认只作用半关闭后的空闲等待，完整连接和持续成功转发由内核原策略处理。

## 自动备份与变更前保护

新增本机local_backup有界设置，开启计划后Controller每60秒检查到期，只有空闲且可写时执行；关闭GUI不停止。密码保存在原有账户加密索引中，不进入便携备份/日志，恢复保留本机计划。每个快照沿用密码认证加密BackupArchive并核验，独立元数据只含时间/原因/数量/哈希。仅清理名称、元数据和内容哈希全部匹配的本程序快照；先成功生成新备份，再按保留数清理，失败不删旧版本。重大变更在既有串行事务内先备份当前settings/profiles，再执行运行切换/删除/恢复/核心更新；每个操作最多一次，失败终止该变更，退出不继续变更。Controller备份与内核维护命令分文件实现，公开命令和统一状态所有权不变。

## Controller 与页面拆分

AppControllerBackup.cpp拥有备份/远端计划和重大变更保护命令，AppControllerCoreUpdate.cpp拥有本地及官方更新协调；仍是同一Controller实例和串行状态，无第二套写入状态。MainWindowHome.cpp和MainWindowSettings.cpp分别拥有首页/设置构建及视图更新，MainWindowParts.h仅为内部绘制与布局助手，不调用业务写入。实际验证由验证报告维护。

## 维护职责拆分

重构继续按职责归并：AppControllerProfiles.cpp拥有订阅来源、批量/Provider维护与元数据命令；MainWindowProxy.cpp与MainWindowProfiles.cpp拥有对应页面构建和视图状态。MainWindow仍统一导航/全局反馈/窗口生命周期，Controller仍统一状态与运行事务，未建立平行的持久化或写入通道。

## 就绪探测

API端口预留、Mixed/API预留监听和Mixed TCP探测显式NoProxy，避免应用层代理污染本机归属/连通检测。每次TCP探测由单一完成守卫停止定时器/断开信号，并在写错误或转为Running前检查代次及Starting状态；仍保持700ms单次与12秒总时限、API实际版本/配置和PID/端口归属核验。历史偶发超时未据此宣称全部根因确定。

## 事务阶段与视图更新

保持AppController为唯一命令/状态所有者，将启动/切换/回滚事务的阶段表达改为命名成员函数和持有受控快照的事务上下文，减少跨多层lambda复制；不引入第二个Controller、写入通道或更宽时限。视图按输入身份增量同步，不把GUI缓存当作状态真相。NodePresentation的缓存按有限语义键/有界短名管理，ProxyTableModel仅通知实际变化及延迟新鲜度转换；线程池仍最多两worker。具体实现/性能与一小时长稳结果以verification-report为准。

## 激活与回滚事务

AppControllerTransaction.cpp持有ActivationTransaction（Release→Stop→Start→Verify→Select→Save→Resume→Completed）和RollbackTransaction快照；原异步工作仍由同一Controller/CoreManager/MihomoClient执行。每个激活阶段只接受预期阶段的单次完成，失败进入原版本恢复；只有实际核心选项/规则/节点回读并完成选择恢复后才提交索引。恢复停止、准备、启动和提交分别具名；释放失败/停止失败的终止边界、来源/候选/活动/lastGood、代理恢复意图及完整快照冲突规则保持。未增加全局重试、强制GC、系统工作集裁剪或放宽超时。
