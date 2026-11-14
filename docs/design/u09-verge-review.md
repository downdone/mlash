# U09 补充评审：吸收 Clash Verge 的操作与模型优点

状态：**设计评审完成，未实施**。以 U09 四分区/macOS 原则为基础，不更换视觉方向。对照为参考源码声明的 Clash Verge Rev **2.5.9**，不是对网上最新版或所有 Clash 客户端作判断。只静态读取相关客户端实现，未运行 Verge，不声称可用性实验、性能优势或外部插件已验证。

主方案见 [U09 UI](../ui-u09-shadowrocket.md)，下层变化见 [系统设计](u09-system-design.md)。离线预览展示基础结构，尚未覆盖本补充的全部动作。

## 1. 结论

Shadowrocket 提供简明的界面骨架；Verge 更值得吸收的是对象上下文、就近工具和诊断深度。Mlash 采用前者的视觉秩序，同时保留后者适合桌面的操作效率。不是把两个应用的按钮合并，也不是重新列一份功能越多越好的清单。

下面的 P0/P1 是**新设计中的可发现性与一致性优先级**，不表示原产品功能缺失或已获新的代码实施结论。

## 2. 优先吸收的八项

| ID / 优先级 | Verge 可确认的优点 | Mlash 已有基础 | 吸收后的设计与验收 |
|---|---|---|---|
| V01 / P0 | 组工具有“定位当前”入口，必要时展开组再定位 | locateCurrentNode、locateCurrent QAction 已存在，当前藏在更多 | 节点工具带提升为独立定位按钮；搜索/筛选遮住当前成员时明确提示“清除筛选并定位”，不静默改筛选。定位只滚动，不发选择命令 |
| V02 / P0 | 每组测试 URL 与搜索/测试就近，提示自定义目标 | groupTestUrl、节点偏好/URL 和 URL 绑定缓存已有 | 测速按钮旁菜单直接见“本组测试设置”；有自定义 URL 时显示小标记。详情显示目标/时间/新鲜度，改 URL 后旧结果不能当新结果；单测不切节点 |
| V03 / P0 | 子组可显示 now，类型/能力来自解析模型，unresolved 行不当普通节点执行 | 节点/子组解析及自动组边界已有，U09 建议只读 NodeRow | 子组副标题显示“当前 → 下一跳”，详情显示有界链；未解析/循环/已移除显示具体原因并关闭选择能力。当前/浏览/预选/pending 分开，不能把组名当最终出口 |
| V04 / P0 | 订阅项就近显示更新时间、用量/到期和独立更新状态；来源操作按类型组织 | subscriptionInfo、来源/候选/维护/批量更新已有 | 配置行只放名称+更新时间+运行/候选状态，用量/到期在有效时补一短行；详情就近更新/来源与下载路径/下次计划/最近失败。未知总额不画百分比，文件快照不假装可远程同步 |
| V05 / P0 | 连接有列管理、活动/已关闭、详情中的规则/进程/链和可复制字段 | ConnectionSession、列偏好、详情/规则草稿/转组能力已有 | 连接保留桌面表格；选中对象的检查器分“目标 / 规则 / 路径 / 流量”。“查看此组”“以此目标新建规则”保留当前配置身份，规则仅预填草稿并确认，不能一键直接改分流 |
| V06 / P0 | 统一 mutate 封装按操作身份处理完成/刷新/错误，Busy 不当成功 | AppController 串行写、OperationId 和反馈路由已有 | 采用稳定 action+object/request 身份、局部 pending 和一致终态；不复制 JS 队列或建立第二个业务协调器。全局告警不被局部成功覆盖，数据回读和缓存刷新由原 Controller 负责 |
| V07 / P1 | 组导航器加速大组列表，当前代理有可达的组/成员操作 | 收藏、最近组/常用组、视图书签已有 | 组选择弹层顶部“常用 / 最近”，支持按组名检索；大组可选快速索引。默认点击定位，**不采用 hover 自动跳组**，避免鼠标经过打断浏览；不把大量组卡常驻首屏 |
| V08 / P1 | Provider 状态及单项刷新在对象列表中就近完成 | Provider 版本快照、独立下载策略/计划、候选应用已有 | 配置详情展示来源 → Provider → 候选 → 运行四层，显示上次成功/失败/到期；失败可重试，更新只进候选。不能将冻结 file Provider 的内核刷新假称为来源已更新 |

## 3. 更细的交互补充

### 节点与组

首页常驻工具限定为“组选择、搜索、定位、测速”；排序为明确有勾菜单，当前方式可见，不采用单一图标盲循环三态。收藏与类型/地区/可用性筛选放次级展开区，启用后显示数量和清除入口，空结果不误导为无节点。

“定位当前”的目标是**当前浏览组的实际成员**，不是推测所有流量的统一节点。规则模式下多个业务组并存，首页注明组名；真实连接链只有连接观测能证明。可解析链详情区分直接 now、最终叶子和未解析，不以某次诊断代替持续事实。

协议/UDP 等信息只在解析模型提供时展示，短行默认协议，完整能力留详情。不要从协议名字推测 UDP、TLS 或系统支持；同名、Provider 来源或未解析身份需要稳定成员引用，不能靠显示名拼唯一键。

### 配置与维护

配置列表有维护中状态，批量更新有逐项结果与取消范围，不能让一项失败清空其他结果。行的当前勾选与待应用候选同时成立，更新成功只说明候选准备好。文件快照的“重新导入”与 URL 的“更新”用不同文案。

临近到期/超额提示只基于有效来源数据，不能凭颜色制造告警。无总量只显示已用，无到期不显示猜测期限。来源 URL/token 不放 tooltip 或日志；下载失败需要切路径时由用户明确更改，不能自动绕过路由策略。

### 连接与诊断

详情先回答“哪个目标 → 命中什么规则 → 经过哪个组/节点 → 当前状态”，再展开源地址、进程路径、累计和时间等字段。缺失进程或链明确未观测。来自另一配置/核心代次的连接快照不能直接跳到同名组，更不能给当前配置写规则。

“以此目标新建规则”需选择域名/IP/进程类型、目标策略和规则位置，生成草稿并完整校验；没有合适字段时禁用并说明。暂停数据是快照，关闭成功仅在 GET 确认原 ID 消失后成立。复制详情默认脱敏，具体字段的完整复制为明确动作。

## 4. 有价值但本轮不照搬

| Verge 方向 | 不直接照搬的理由 | Mlash 取舍 |
|---|---|---|
| 全量首页仪表盘和可拖拽布局 | 对常用连接/选节点造成视觉竞争 | 首页连接/节点优先，指标在数据 |
| 每组常驻一排工具、类型徽章很多 | 把对象列表变成按钮矩阵 | 常用四动作，其余按需 |
| 组索引 hover 自动跳转 | 指针经过可意外改变阅读位置 | 明确点击/键盘，常用和搜索优先 |
| 超大多列托盘所有节点 | 长列表难扫读、尺寸随数据爆炸 | 当前组/最近/搜索入口，完整列表回主窗；现托盘需单独设计验证 |
| 自定义脚本/CSS、外部控制面与更宽监听 | 扩大架构和权限面，不是 UI 提升的必要条件 | 本轮不扩展产品边界 |
| React/Tauri/WebView 状态与存储机制 | 参考实现架构不同，复制会产生双状态 | C++ Controller 单写入；Qt 渲染按选型评审 |
| 未经确认的 Provider 原地刷新/自动使用 | 与来源/候选/运行分离冲突 | 坚持候选事务、明确应用 |

以上是对 Mlash 的适用性判断，不表示 Verge 对所有用户都存在相同问题。也不认定参考源码的所有设计都符合 Mlash 的隐私或恢复契约。

## 5. 对六层设计的影响

1. **功能定义**：补充定位当前、组上下文说明、维护状态查看和诊断转任务；多数为已有能力的入口提升。
2. **架构**：仍由 Controller 管状态；UI 只读投影增加 capabilities、新鲜度和对象身份，不建新常驻服务。
3. **模块**：NodePresentation、ConfigurationPresentation、ConnectionInspector 负责对应投影，EditorSession 负责预填后的草稿保护。
4. **协议**：命令/结果带 objectKey、requestId、generation/版本；组成员的 unresolved 是数据状态，不用异常字符串凑能力判断。
5. **接口**：建议补充 revealCurrentMember、openGroupTestSettings、inspectResolvedChain、draftRuleFromConnection、showMaintenance；先适配已有方法，不立刻新增第二套 API。
6. **功能设计**：定位无写入、单测不切节点、来源刷新不应用、诊断预填不保存，四条必须有跨层回归。

技术选型结论不变：Verge 的能力并不要求 Mlash 采用其 UI 框架；Widgets/QML 候选都可以消费相同的领域模型和事务边界。

## 6. 源码证据

参考仓库中的相对路径与符号如下；路径是证据标识，不是依赖或本机链接。未把邻接参考仓库改动或打包进 Mlash。

| 证据 | 参考路径 / 符号 |
|---|---|
| V01/V02 | `src/components/proxy/proxy-group-tools.tsx`：onLocation、onCheckDelay、testUrl、sortType |
| V03 | `src/components/proxy/proxy-item.tsx`：resolve member kind/now、unresolved；`src/types/proxy-view.ts`：ResolvedProxyMember、resolveMember、findCurrentGroupMember |
| V04 | `src/components/profile/profile-item.tsx`：updated、extra、expire、urlModeMenu/fileModeMenu、loadingCache |
| V05 | `src/pages/connections.tsx`：active/closed、filter、column manager；`src/components/connection/connection-detail.tsx`：metadata/rulePayload/chains |
| V06 | `src/services/mutate.ts`：inFlight、isBusyOutcome、onFulfilled/revalidate |
| V07 | `src/components/proxy/proxy-group-navigator.tsx`：handleGroupClick/handleGroupHover；`src/components/home/current-proxy-card.tsx`：组/成员上下文 |
| V08 | `src/components/proxy/provider-button.tsx`：updating 与单 Provider 刷新 |
| 测试缓存 | `src/hooks/use-proxy-delay-state.ts`：DelayUpdate、updatedAt、member/group 监听；只借鉴明确身份和时间，不复制负数状态编码 |

快照指纹：`package.json` SHA256 `e5dd5babd7d00c91043e621532d6f3c7ea4a99b982a8d298d541bd4471a22ff4`；`proxy-view.ts` SHA256 `af1d7ee858a6968c907fd828ad523e7c77d0fd3c223dcb52afd3b0ef4eed8f8b`；`mutate.ts` SHA256 `6ec6cb3f7adfcba256ed187dd594fbeddfcdcc5633c3df252a75d50f3cbdce2e`。这是可追溯的静态评审，不是上游版本一致性或性能证明。
