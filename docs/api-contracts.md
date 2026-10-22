# 内核/API 契约

F09：`previewNodeLinks(text,reserved,done)`为单在途只读worker；最多64KiB/64条/每条8192字符及20000保留名称，严格解码与受控字段转换。items只含名称/类型/端点；definitions/yaml含凭据，仅供私人编辑器内存使用，禁止日志/诊断。输入变化即丢旧预览，同名生成新名称。未知/冲突参数、坏编码、任一坏行整批拒绝，错误仅行号/固定原因。`importNodeLinks(name,text)`重新解析并生成本地快照（既有file/空source语义），走stageSource/真实内核校验/索引原子提交，不自动应用。已有编辑器先并入草稿，再走updateProxyEdits的覆盖及active/candidate期望快照；取消不写。无source的本地快照不能自动更新；元数据编辑仅允许继续保留空source且automatic=false，附加文件先校验，不允许清空已有文件来源静默解除关联。

F08/S03：新增可选tun_mtu（1280–9000，默认1500）、tun_dns_hijack（默认true）、tun_exclude_routes（规范CIDR字符串数组，最多64项/4096字符，拒绝/0）。旧索引默认兼容，Settings保存/加载及权限服务prepareTunRuntime均严格复核；设备仍固定Mlash。updateTunSettings仅复制TUN字段并可核对expected旧值，未启用TUN时只存偏好，不重启普通内核。启用/修改后核对核心API的MTU、DNS和排除网段；原生观察能取得接口MTU时也必须一致。旧服务不支持高级字段则启用前拒绝，不能静默丢弃。

权限服务owner记录增加版本和advanced_options能力；只读诊断返回安装内核匹配、当前会话/恢复标记及选中内核匹配，不暴露私有凭据。显式修复通过一次UAC在确认旧会话清理后移除并重装，清理失败不删标记；重试清理使用绑定身份的idle确认，不能结束其他客户端会话。网络接口观察只读查询接口和目标1.1.1.1的系统IPv4路由，不发送探测包，不将TUN接口称为物理出口。上述新增真实SCM/路由行为仍需VM验收。

F08/S07链预览：`inspectProxyEntries`在既有私有编辑描述中增加source_dialers与group_members。前者保留节点原定义（包括节点局部覆盖）里的上游，排除独立dialers覆盖；后者仅含静态组成员名称。UI将当前dialers草稿覆盖source_dialers后展示流向，并检查已知组/链引用中的循环及64层上限；组节点不解析为某个运行叶子。读取/YAML仍在worker，返回范围受原配置预算限制，不返回新的网络状态、不新增采样。

F08/S06维护：`updateProviderPolicy(id,providerId,policy,expected)`是独立preferences事务，不重启核心；expected只比较该项配置，快照ID须来自候选HTTP提供者。空对象明确清除此项，可清理失去来源的策略。`provider_policies`最多64项/64KiB，键为providers/<24hex>.cache，字段download（严格DownloadPolicy、缺省继承）、automatic（缺省false）、hours（1–168整数、缺省12）；拒绝未知字段。`inspectProviders`窄行额外返回policy、maintenance与next_update；已失去候选引用的策略以orphan行返回，只允许清除、不允许刷新。会话maintenance含last_attempt/failures/retry_at/error，冻结时间为updated_utc，自动计划只向候选写入。

F08/S06：`updateRuleDefinitions(id,prepend,append,disabled,providerEdits,expected)`一次提交规则和规则集声明；expected比较candidate/active/规则/Provider编辑，旧编辑器拒绝覆盖。`rule_provider_edits`最多64项/128KiB，名称1–128字符，值为null移除或精确url/behavior/format对象；HTTPS无userinfo/fragment，行为domain/ipcidr/classical、格式yaml/text/mrs（MRS不支持classical）。声明变化指定对应冻结ID重新下载；同路径新URL不能复用旧来源。`inspectRuleSource`额外返回原始声明的窄描述，仅供私人编辑窗显示，URL不进入日志/状态消息。`versionProfile`只读恢复版本的七项覆盖；overrides.json绑定原始来源SHA。`source_sha256`为kind/name/url/format/behavior规范JSON哈希，独立于payload的sha256；版本重建/备份不将不同来源的同名缓存混用。

F08/S02：`inspectProfileChanges(id)`在有界worker比较已使用/候选的受控work配置与Provider清单，仅返回五类数量/最多各100个名称、时间、失效/动态待核选择，不返回YAML、节点定义或凭据。候选/active/generation变化使回调作废，比较单在途，不写版本或索引。`inspectProviders(id,done,runningVersion=false)`默认仍候选，true为已使用版本，返回scope/version；候选更新错误不混入已使用版本。变化预览确认使用前检查候选仍与预览相同，继续走原激活校验/回滚事务。

F08/S01：`AppController::homeNode()`按当前模式和首页浏览组解析到唯一叶子；首页卡片及周期探测共用，不把规则模式一个组的叶子解释成所有目标出口。没有唯一叶子返回空，界面明确未知。规则摘要统计前置/追加/屏蔽，连接摘要只读取既有HomeMetrics的active_count，不新增后台采样。UI的DialogDraftGuard仅保护内存草稿/在途提交；窗口释放和托盘退出经GuiHost/MainWindow确认，Controller不持有GUI引用，显式取消不写配置。

F07 TUN：Settings 可选 tun_enabled/tun_stack/tun_strict_route，默认 false/mixed/false，非法类型/栈拒绝加载；备份导出和设置恢复关闭 TUN 意图。Controller updateTunOptions 先检查服务所属账户和锁定内核哈希，复用 prepareActivation/transact；core.start 的 tun 参数决定后端。CoreManager 仅在 API tun.enable、服务接口/路由与两个监听 PID 均确认后完成启动，Controller 再核对设备/栈/自动路由/出口检测/严格路由。TunSession 协议 1 使用有界 JSON 行：start（work）、status、stop；断线后新连接 idle 只确认没有运行 Job 和未处理恢复标记，不会结束其他客户端的会话。stop/idle 的 cleanup_confirmed 是停止成功前提，普通 status 不能冒充停止确认。恢复标记只在实际读回无接管路由且接口不再运行时清理，否则保留且拒绝再次启用。

TUN 服务持有真正的内核 API Secret；回环 TunApiGate 校验 GUI Secret、单请求帧/长度，转发到私有端口并替换认证，屏蔽整个配置上传、upgrade/restart/Provider 写入及未知路径。公开 GET 仅 version/configs/proxies（含单节点/延迟）/rules/connections/dns/query/logs，写操作仅模式 PATCH、成员 PUT/清除及关闭连接。禁止 chunked 请求、升级协议、重复认证/长度、管线及越界内容；普通响应16MiB、发送积压512KiB、最多24连接。日志流按关闭下游取消上游；没有 WebSocket 或任意代理转发。

私有 API 连接建立后，先按已建立 TCP 四元组核对服务子内核 PID，再发送私有认证；停止时关闭 TUN 的本机请求遵循同一顺序，防止端口复用取走凭据。进程仅向绑定账户授予 limited-query/synchronize，不授予 VM_READ，工作集观测沿用有限查询权限。TUN 重新合成关闭 geo-auto-update，使用受保护的冻结 Geo/Provider 快照。TUN 关闭时只修改栈或严格路由偏好走独立偏好提交，不重启普通内核。

G22 WebDAV：仅HTTPS目录，PROPFIND Depth:1要求207；PUT临时文件要求201/204，GET要求200，MOVE Overwrite:F要求201；手动重定向策略一律拒绝3xx。临时及最终GET字节SHA须一致。目录使用DAV multistatus命名空间且成功propstat，只列同源直接子项，最多200份。控制器先核验core路由归属，不隐式回退；账户/密码不进入日志、便携归档或错误正文。完整边界见ADR0034。

G19：parseProtocolLink只返回已验证的url/name/host，错误为固定文本，不回显token。inspectUrlProtocol和setUrlProtocol分别只读/显式写当前账户关联，设置意图不持久化到备份。协议CLI不能组合其他参数，冷启动通过suppressAutomaticStartup抑制内核自动运行；正常手动启动命令仍可使用。收到链接不自动下载/激活，确认后调用原有importUrl命令，受相同SSRF/TLS/候选校验约束。

G17：updateProxyDialers(id,dialers,expected)比较已保存拨号快照，合并到最新proxy_edits，保留同窗口之外的其他节点编辑；冲突拒绝并保留草稿。inspectProxyEntries附有效静态节点名、可选目标及当前已保存拨号映射。字段从受控合成配置读取，显示配置意图；不是实时观测链。

G16：inspectProxyEntries(id)单个worker只读当前待编辑版本的来源节点/组定义，QPointer接收者负责界面生命周期，generation变化丢弃；结果仅供编辑器内存，不写诊断。updateProxyEdits(id,edits,expected)执行完整版本事务；可选expected比较当前覆盖，冲突不提交。保存使用最终合成配置验证引用和运行回读，不将编辑草稿作为有效来源；原字段缺失沿用来源。

G06：`updateRuleOverlay(id,prepend,append,disabled)`沿用版本校验/运行事务，旧`updateCustomRules`只更新前置并保留追加/屏蔽。`inspectRuleSource`单个在途worker只读取将编辑的active（运行中）或candidate，最多返回5000原规则和规则集名称/版本/总数，旧代次丢弃。ConfigBuilder拒绝未知规则集、无效目标/选项与越界列表，返回具体覆盖列表位置；PreparedProfile保存expectedRuleCount，启动后GET/rules核验总数与前后边界，结果不一致不提交新索引。

G05：设置可选`website_targets`为空沿用编译默认/测试注入默认，否则为1–24个id/name/url/mark对象；id唯一且受限字符、名称≤64、HTTPS无凭据/片段、URL≤2048、mark≤8，非法类型或内容拒绝加载/覆盖索引。Controller `updateWebsiteTargets`只提交此字段；提交/恢复设置后WebsiteTests比较定义，变化则取消并清空旧会话、发布targetsChanged，不发测速或选择请求。旧索引缺失字段保持原12目标；测试注入的初始目标仍只用于隔离测试。

G04：`exportBackup`为串行写命令，`previewBackup`为单个在途只读worker，`restoreBackup(path,password,expectedHash,restoreSettings)`要求核心停止/代理已释放。预览只返回名称/创建时间/加密文件SHA，不回传明文包。恢复重检SHA和所有索引/配置/快照，成功原子保存新订阅集合；提交失败异步删除本次生成UUID后才完成命令。密码不持久化，G22的显式定时凭据设计另立契约。

G03：`inspectProviders(id)`返回所选候选版本的`providers`数组（id/name/kind/type/format/count/updated_utc/status/sha256/source_host/error）及version，只读、一个在途worker、不回显URL查询。`refreshProviders(id, providerId)`接受源生成的相对快照ID、`kind:proxy-providers`/`kind:rule-providers`或`*`；非来源ID不能构造任意路径。刷新由客户端建立新候选，不对冻结file Provider调用内核更新冒充远程下载。清单落盘和复制校验均在worker，成功后仍需用户明确使用更新。

G02：`DownloadPolicy` 持久化到每订阅可选 `download_policy`（route、user_agent、timeout_seconds）；默认值兼容旧索引。`SubscriptionDownloader.download` 返回 `download_route`，错误标注实际路径；同实例仅一个完整下载在途。`updateProfilePolicy` 可选下载策略参数，旧调用不覆盖已有策略；`importUrl` 可传初始策略。代理不可用、PID归属不明或不安全目的地直接失败，不试其他路由。core代理仅开放Qt TunnelingCapability，使明文HTTP也经CONNECT固定已检查IP；不允许HTTP缓存代理再次按Host解析。依据[Qt 6.10.2实际代理选择源码](https://raw.githubusercontent.com/qt/qtbase/v6.10.2/src/network/access/qnetworkreplyhttpimpl.cpp)。

G01 兼容性检查：`ConfigBuilder::build` 的明确能力拒绝返回 `Result.data.code/path/reason/suggestion`，YAML解析错误仅带 `code=YAML_SYNTAX` 和1起始行列。成功的 `managed_fields` 只列源中被覆盖/移除的字段名，不含值。`AppController.inspectProfileCompatibility` 在有界worker读取所选候选（缺失则active）的source并静态构建；不联网、不创建版本、不写入。代次变化报告过期；最多一个在途检查。内核实际校验/激活仍独立执行。

基线：mihomo v1.19.32，固定源码与实际 `-v` 对照；Qt Network NoProxy。
官方参考：[API](https://wiki.metacubex.one/en/api/)；最终行为以所带二进制为准。

| 操作 | 请求 | 完成条件 |
|---|---|---|
| 版本 | GET /version | JSON version 非空且端口属于子进程 |
| 配置 | GET /configs | 实际 mixed-port/mode/ipv6/unified-delay；显式覆盖启动回读后提交 |
| 模式 | PATCH /configs `{mode}` | 支持 204；GET 回读实际 mode |
| 组/节点 | GET /proxies | proxies map；all/now/type 按组保留 |
| 组选择 | PUT /proxies/{group} `{name}` | 204 后 GET 该组检查 now |
| 延迟 | GET /proxies/{name}/delay | url、timeout(ms)；失败不显示 0ms |
| DNS 查询 | GET /dns/query?name=&type= | 手动 A/AAAA，域名标签校验、4s、单在途；局部取消/写操作代次丢弃旧回调 |
| 日志 | GET /logs?level={debug/info/warning/error} | 按行 JSON，type/payload；级别过滤，暂停/隐藏 abort；相同级别不重开流 |
| 规则 | GET /rules | type/payload/proxy；自定义前缀的类型和目标在提交前回读；列表视图最多 5000 条 |
| 连接 | GET /connections | uploadTotal/downloadTotal + connections（空可为 null），metadata/rule/rulePayload/chains/upload/download/start；GUI连接页2s刷新/1.5s/单请求/最多5000条；G10首页另只读计数不保留连接条目 |
| 关闭单连接 | DELETE /connections/{id} | 仅当前 GUI 快照 ID；204 后 GET /connections 确认消失 |
| 关闭全部 | DELETE /connections | 用户确认后发送；GET 验证原快照 ID 消失，允许同时产生新 ID，不自动执行 |
| 配置验证 | mihomo -t -d work -f config | 实际退出码 0，受 360s 总限时 |
| 启动 | mihomo -d work -f config | PID 端口身份、API、代理 TCP 全部满足 |

随机 API 端口，每次运行随机 256-bit Secret，Authorization Bearer。API不接受重定向。
路径名按独立 segment 百分号编码，中文、空格、`/ ? # %` 均不能拼为原始路径。
普通 API 限制 16 MiB，对非 2xx、空响应、坏 JSON、超时、取消分别识别。
修改超时回读，不盲重试写操作；旧内核 generation 的响应按取消处理。

单节点 delay 源码 timeout 用 signed 16-bit 解析，客户端固定 5000ms，避免超界。
url 来自已保存的 test_url，默认 https://www.gstatic.com/generate_204；必须是有界 HTTP/HTTPS，无用户凭据或片段，作为 query 项编码，不拼接原始查询字符串。连通性仅限该次 HTTP 测试。
单节点测速的内核任务使用后台 context，客户端取消不保证立刻停止其内核任务；由 5s 上限约束。
组 delay 会清除非 Selector 自动组的固定选择。因此客户端批量测试用最多三并发的节点 delay，
不调用有此副作用的组测速接口。最多 500 个条目，用户可取消。
Controller 在队列准备后发布 0/total 的初始进度，testingNodes 仅描述当前测速队列/在途请求，不能代替 ready 或节点健康；UI 浏览保持可用，写命令仍由 busy 串行拒绝。
明确 cancelTests 清空客户端队列/取消请求并结束当前任务；operationFinished(true,"测速已取消") 为正常用户操作，清除普通错误状态且保留已有结果，不记录成故障。仍不保证内核已开始的 5s 测试立即终止。
托盘按需测速同样使用单成员 `/proxies/{name}/delay`，最多三个客户端请求、5s 目标超时；不调用会解除自动组固定的 `/group/{name}/delay`。独立只读队列不占 busy，写命令优先取消它；只 abort 本队列借用的 QNetworkReply，不能调用全局 cancelAll。request/delay 返回请求句柄（未配置时 nullptr），所有权仍属 MihomoClient/Qt，调用者不 delete；completion 的格式/取消/上限不变。结果缓存按操作代次和单调时钟 60 秒，旧回调不写缓存、不通知菜单，自动失败仅就近显示，不作为应用操作失败。

Selector、URLTest、Fallback 支持手动选择；后二者手动选择属于固定当前成员，UI 提示。
DELETE /proxies/{name} 解除自动组固定；GET 返回 fixed，客户端核验为空后删除本地记忆。
手动选择通过实际 now 确认后按 Profile 保存；源 store-selected=false 时尊重不保存策略。
LoadBalance 等组只读当前行为，不伪造手动可选状态。
完整 Profile 更新由客户端重新下载来源 YAML；不能拿 `/providers/proxies/{name}` 代替。
文件 Profile 可显式重新读取原来源，内容未变不生成候选；读取失败保留旧版本。手动与自动刷新采用同一事务，不自动激活。
运行 Provider 使用冻结 file 快照，自动 Provider 联网更新不在首版范围。

PUT /configs 支持 payload 或安全的绝对 path、force query，返回 204；首版采用安全停止/启动事务，
不把重载返回码当作完整就绪证据。
U06 增加按页可见性获取规则/连接（ADR 0009）。连接 GET 返回的是活动快照，不能据此推断 TCP 握手、远端可用性或历史断开原因；界面状态写“活动”，无记录表示当前快照无活动项。连接失败清空旧快照并显示错误，不显示过期记录为实时。
不使用WebSocket或磁盘历史，不采集系统全局网络流量。ADR 0017接受同一连接页可见会话内最多1000条离开快照记录；差值速率遇首次/回退/超过10s缺口为未知。隐藏/离页清空并丢弃旧响应，不据此推断TCP原因。
规则视图只在规则页的“当前内核规则”页签可见时读取；隐藏/离页/切回编辑器/GUI 销毁清空视图并丢弃旧可见性代次响应。规则保存的事务回读是独立的 Controller 操作，不依赖 GUI 是否继续存在。

## 常见目标响应测试（U18）

WebsiteTests 仅对具体叶子调用现有 `/proxies/{name}/delay`，url为内置HTTPS目标、timeout=5000，客户端6s；并发3、定义上限24，成功要求delay为0–65535整数。每目标结果/错误独立，不发nodeDelay；取消局部abort，代次和QPointer丢弃旧回调。无选择PUT、无组测速/解除固定DELETE。

锁定1.19.32的URLTest为HEAD，不跟随跳转；统一延迟可请求两次。返回只有delay、没有HTTP状态，403等仍会有成功延迟，因此显示已响应，不推断服务解锁/登录/账号地区可用。URLTest更新核心自身历史/健康，自动组仍由核心管理。取消API连接不保证核心探测立停，已开始的测试可继续到5s。具体决策见ADR0027，实际证据见验证报告。

F08/S07表单草稿：常用字段通过worker的inspectYamlFields/patchYamlFields读写同一局部YAML文本，不调用OS/Core写接口。允许字段集合由YamlFields唯一拥有，保留未触及键，拒绝多文档/重复/merge键及超预算文本，结果仅返回表单值/不适配字段/安全错误。`updateDnsOverride`新增可选expected={yaml,enabled,candidate,active}；`updateProxyEdits`在原expected覆盖对象外增加可选sourceExpected={candidate,active}，旧调用省略仍兼容，GUI使用完整期望防止覆盖来源更新后的草稿。

S07组选项：inspectProxyEntries额外提供provider_names，仅返回当前静态构建中的节点Provider名称。单项组编辑器用来源＋父窗口当前草稿合并可选节点/组（去掉已移除及自身），Provider来源仍只读；加入操作仅修改当前草稿，不建立新Provider或触发网络。

S07移除影响：`inspectProxyImpact(id,draft,name,sourceExpected,done)`按需单worker，无网络/OS/Core变更，返回最多100个`references[{kind,name}]`、`total`（按类别/引用对象去重的数量）与布尔`incomplete`，不返回规则原文。复用proxyPreviewPending门禁；入队比较sourceExpected，完成比较代次、candidate/active和已保存proxyEdits；GUI加入草稿前再次比较原保存覆盖，拒绝旧来源。分析使用原始来源合并传入草稿，不要求整个未保存图有效；无法解析的其他定义计入不完整边界。已保存规则在实际有效构建中分析，仅受控14类可静态确定目标，复杂逻辑/子规则不声称覆盖。此预览不替代保存时全图/真实核心校验。

F08/S08第一切片契约：连接列ID0–6保持，7进程、8来源、9目标IP、10开始时间、11上传量、12下载量、13上传速率、14下载速率，默认后八列隐藏。connection_view.columns兼容旧完整七列或新完整十五列，必含可见ID0、无重复/未知ID、宽24–1000；旧七列保持原顺序/宽度，新列只在GUI追加默认隐藏，之后偏好写出十五列。

`setConnectionsPaused(bool)`仅可见会话读状态，不持久化/不开始mutation；暂停递增连接读取代次、取消自身在途轮询reply（不取消写命令）、停timer、保留快照、清除速率和差值基线，恢复第一采样不推断间隙离开。ConnectionSession在成功接收时为行/总量标记_observed_at，暂停快照标记_paused；`connectionsCurrent()`仅可见、非暂停且已获得有效新样本，关闭命令也在Controller拦截暂停/旧快照。隐藏重置暂停及所有会话记录；晚到读取或其他关闭命令的回读不能覆盖冻结快照。

S08采样时间语义补充：ConnectionSession另保留最后成功接收时间，invalidate只清当前快照/基线，不抹去最后成功时间；reset（隐藏/离页/核心更换）清除全部会话时间。界面在采样失败时显示当前未知及上次成功时间，不能因totals为空显示零连接；最后成功时间不作为connectionsCurrent依据。

S08已实现的`connectionSampleTime()`返回当前可见会话最后成功采样的UTC字符串；与`connectionsCurrent()`分离，失败后仅作为历史时间，不使关闭门禁重新开放。

S08详情切片接口（实现前）：ActivityTableModel的UserRole+3仅为GUI返回当前行QJsonObject；连接详情复制不可变打开快照，不发网络/文件请求。规则建议仅从实际host/IP/process元数据本地生成受控DOMAIN、精确IP-CIDR/6或PROCESS-NAME；拒绝CSV分隔/控制字符/接口域，目标无法确认则留空待选。详情发出browseGroup/createRule界面意图，主窗再次比较来源订阅active版本；定位只改浏览，规则通过已有RuleOverlayDialog的prepend/expected草稿入口预填，不调用保存接口。原有保存事务/RuleDraft快照仍是唯一写边界。


F08/S08诊断显示快照：MainWindow仅缓存GUI可见期的出口显示子集（ip/country/region/asn/route/mode/measured_utc，每字符串最多128字符），不保留原响应或Controller任务。Controller generation/就绪状态改变、GUI隐藏及下一诊断开始清除；关闭诊断仍取消并清空完整结果。服务详情冻结实际已有结果，不发请求，缺少HTTP状态明确未知。以上不增加持久化schema或后台调度。


F08/S09首页显示偏好：Settings.homeSummaryMask持久化为home_summary_mask，整数0–15，bit0/1/2/3分别为规则/连接/日志/设置摘要，缺省15；订阅/当前节点/流量不在可隐藏范围。Controller.updateHomeSummary(mask,expected)仅偏好串行事务，expected若提供必须等于当前值；失败或过期不提交，核心generation不变。备份沿Settings保留该有限偏好，不增加页面/诊断记录。


F08/S09快捷键扩展契约（实施切片）：固定目录按顺序为show-window/show-proxy/show-connections/cycle-mode/toggle-system-proxy/toggle-tun/reload-active；持久化仍为action→canonical sequence对象，最多7项，新增默认缺省未绑定。Windows ID=10100+固定目录下标，旧10100/10101保持，未来只追加；键/修饰符核验及MOD_NOREPEAT保持。注册代次使改绑/注销前排队回调失效，注册失败返回实际映射并恢复旧映射。

Controller.beginHotkeyCapture返回非零代次，endHotkeyCapture只结束同代次；hotkeyCaptureChanged/token与hotkeyCaptured(token,sequence)用于单录制会话。已注册键只取activeHotkeys实际映射作为录制结果，不执行动作，未注册键仅UI本地录制；新会话/任务开始/隐藏销毁结束旧会话，结束后300ms丢弃激活。更新绑定接受可选原对象期望，偏好事务/失败恢复/实际映射不变。网络热键忙时不发operationFinished，避免被其他在途编辑当作自己的完成；不强接外部代理，不安装服务，reloadActive只重建active且保留独立candidate，按运行版配置初始化模式，只有原观察Owned才允许原有安全代理恢复。


F08/S10：BackupArchive::prepareRestore及AppController::restoreBackup增加可选selectedProfiles，Undefined兼容旧全选；提供数组时必须为0..档案数-1的唯一整数，按档案顺序恢复。空数组允许仅restoreSettings=true，均未选拒绝。所选数计入256总上限；全档案元数据仍严格检查，所选版本/Provider在原worker完整校验、失败清理。restoreProfileNames统一预览/提交的新增命名，忽略大小写避开当前和本次已分配名称，追加（恢复 N）且120字符上限；绝不替换现有ID/内容。SHA变化仍拒绝，实际提交用当前订阅重新计算同名结果，界面stateChanged同步重算。


H03 半关闭回收（2026-10-19，实施前契约）：ConfigBuilder开放顶层tcp-half-close-timeout单映射，仅client/server两个字段，规范非负十进制整秒0..9223372036。来源未指定的方向分别补client=300、server=30；显式0关闭该方向，其他有效值保留，错误不回显值。默认只在合成运行配置写入，source.yaml/下载内容不改。普通模式、旧active/candidate重建和TUN服务安全重合成都共用此策略，0不会因二次合成被重开；无新设置字段、UI或额外采样。内核仍由既有-t校验与`/configs`实际回读验证，不靠仅文件存在。此次产品要求取代ADR0038初始“客户端不默认开启”，Go二进制/固定SHA保持。

2026-10-20本地备份：local_backup默认空，包含enabled/before_changes布尔、interval_hours整数1–168、retain整数1–30、password字符串8–1024字符，以及Controller维护的last_success_utc/next_due_utc。完整对象校验拒绝未知字段、错误类型和非法时间。保存采用期望用户字段快照；密码仅账户加密索引保存，便携备份移除该对象，恢复保留本机计划。重大变更备份失败阻止删除/恢复/设置及运行配置/内核切换，既有索引和运行态保持；无GUI时同一Controller执行计划，失败15分钟退避。

官方内核：检查只读元数据，下载完成worker验证大小/SHA/ZIP目录和CRC后复用stageBytes/版本预检；官方声明版本须等于实际-v，才返回一次性预检token。应用仍是独立命令。TUN启用时仅接受TunCoreSha256，网络和解包取消不变更当前核心/订阅/系统。源API限流等失败明确返回，不使用镜像或缺失哈希资产。
