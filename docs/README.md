# 文档导航

面向当前 **1.21.0 预发布候选**。首次使用从使用指南开始，开发修改先读工程方法与架构。历史分析和方案集中归档，不能用其中的阶段判断替代当前状态。

## 使用与发行

| 文档 | 职责 |
|---|---|
| [使用指南](getting-started.md) | 安装、升级、订阅、节点、代理/TUN、观察、备份和故障处理 |
| [版本变更](../CHANGELOG.md) | 各版本用户可见变化 |
| [兼容矩阵](compatibility-matrix.md) | 目标平台、工具链与实际验证边界 |
| [发布验收](release-readiness.md) | 包清单、源码附件、流水线和正式发行门禁 |
| [第三方清单](../licenses/README.md) | 许可、署名和对应源码要求 |

## 开发与当前契约

[U22 逐页操作链与返回上下文](design/u22-page-operation-flows.md)：对照 Daymark 的导航、行主动作和菜单源码，逐页规定跳转、就近操作、草稿保护及回到原位置的验收边界；实施结果以项目状态与验证报告为准。

[U23 导航重设](design/u23-daymark-navigation.md)：以 Daymark 的品牌、工作区和底部工具分区为参考，记录 Mlash 侧栏的设计、实现后复核与路由边界。
[U22 Daymark 方法逐页重审](design/u22-daymark-page-reaudit.md)：把“简约而不简单”落实到 Mlash 的配置、节点、观察和恢复路径；逐页检查布局、信息、交互与切换状态。设计不等于完成。
[U22 订阅优先的配置页重设计](design/u22-config-rethink.md)：先脱离旧的列表→详情目录→检查器结构，形成第一版[内容工作台原型](design/u22-config-rethink.html)；按用户明确的高频任务与 Clash 源码操作链，第二版改为[订阅列表/更新优先原型](design/u22-subscription-first.html)。当前运行 UI 已迁入统一卡片网格和内容工作区；实际完成度以项目状态与验证报告为准。

[U21 Windows 字体栅格精修](design/u21-windows-font-rasterization.md)：以原生同屏探针对照默认 Windows 字体引擎与 FreeType，先定位彩边，再限定应用启动时的字体引擎选择和验证边界。

[U20 Daymark 体验逻辑与就近操作审视](design/u20-contextual-menus-and-surfaces.md)：从对象、主次动作、信息层级、上下文连续、草稿保护和反馈闭环出发，审计 Mlash 全应用菜单及五条用户任务链；菜单、筛选、配置导入/右键、连接列及托盘快选已实施，实际验收边界见项目状态与验证报告。

[U19 Daymark 界面精修](design/u19-daymark-ui-alignment.md)：逐项对照导航、首页、节点列表、菜单与浮层的事前原则、组件规则和验收点。

[U18 并发测速、窗口顶栏与在线更新](design/u18-fast-tests-window-chrome-update.md)：事前功能/模块/状态与 UI 原则评审，实施和发布验收以项目状态与验证报告为准。

[U17 全界面视觉系统重建](design/u17-visual-system-rebuild.md)：先审视任务和根因，再统一字体、线条、颜色、容器、控件与逐页验收。

[U16 模块化架构与界面边界](design/u16-modular-architecture.md)：功能/层次/协议/接口及单文件 UI 组件拆分的事前设计与迁移验收。

[U16 独立 UI 设计系统](design/u16-shadowrocket-design-system.md)：先固定页面任务和分组规则，再以 Shadowrocket 特化集中管理颜色、字阶和几何规格；运行时 Qt 适配独立。

[U16 测试架构](design/u16-test-architecture.md)：按纯契约、隔离集成、操作链、Widgets 与受控系统门禁组织测试与运行入口。

[U15 细节根因与施工设计](design/u15-precision-root-causes.md)：定位悬停缺口、重复顶栏动作、配置返回跳位与外部代理告警语义，事前验收与实施范围。

[U14 全页精修设计](design/u14-shadowrocket-all-pages.md)：以 Shadowrocket 的内容优先、直接操作和渐进展开为原则，重审首页、配置、数据、设置、托盘与任务状态；[可切换原型](design/u14-all-pages.html)用于设计对照，尚非原生实现。

[U13 紧凑密度](design/u13-compact-density.md)：修订共享尺寸、双行高度和首页留白，实际几何及截图验收。

[U12 页面内工作区](design/u12-flat-workspaces.md)：减少独立弹窗与中间层级，统一编辑任务与稳定正文宿主；本轮实施以此补充 U11。

[U11 UI 系统规范](design/u11-ui-system.md)：以已确认最终原型为唯一基准，规定信息架构、全局布局、组件、视觉 token、输入状态及逐页对照门禁；修正 U10 的结构偏差；本轮已原生实施，实际验证范围见验证报告。

[U10 紧凑版施工设计](design/u10-implementation.md)：最终原型进入原生开发，尺寸与浏览/应用边界见 [ADR 0050](adr/0050-shadowrocket-compact-widgets.md)。

[U09 整体设计评审稿](ui-u09-shadowrocket.md)：macOS 原则、Shadowrocket 元素提炼及 UI；关联 [系统六层设计](design/u09-system-design.md)、[技术选型](design/u09-technology-review.md) 与 [最终交互原型](design/u09-final.html) 与 [最终稿说明](design/u09-final-spec.md)。设计稿已进入 U10 实施，完成范围以施工记录为准。 [结合 Verge 的补充评审](design/u09-verge-review.md) 说明可吸收的八项操作/模型优点及取舍。

| 文档 | 唯一职责 |
|---|---|
| [开发构建](development/build.md) | 可重建依赖、环境变量、构建/测试/打包命令 |
| [工程方法](engineering.md) | 任务切片、UI 先设计、模块目录、协作和文档管理 |
| [团队约束](../AGENTS.md) | 工程执行规则 |
| [总体架构](architecture.md) | 进程、模块、线程、状态、存储事务和 OS 恢复 |
| [功能模块契约](development/feature-contracts.md) | 有界字段、计划、并发、快照与取消细则 |
| [内核 API 契约](api-contracts.md) | 固定内核行为、错误/超时和实际回读 |
| [当前交互契约](ui-interactions.md) | 四分区任务层级、托盘、反馈、视觉与完整操作链 |
| [界面精修审视](ui-apple-review.md) | Apple 风格精修的逐页、控件、按钮与密度设计及复核 |
| [界面系统重构方案](ui-system-review.md) | U05 全量评审：信息架构、视觉 token、实施阶段与验收边界 |
| [界面分层设计](ui-layered-design.md) | U06 六层设计：架构、原则、分类、容器、Widget、视觉 |
| [R2 施工记录：标题槽](ui-r2-title-slot.md) | R2 片的实施前设计与实施后结果：单一页签宿主、窗口标题、图标收敛 |
| [R2.5 施工记录：侧栏状态条](ui-r2-5-sidebar-status.md) | R2.5 片的实施前设计与实施后结果：共用状态句、侧栏常驻、托盘同源 |
| [R3 施工记录：首页合并](ui-r3-home-merge.md) | R3 片的实施前设计与实施后结果：代理总开关命令、首页单滚动、栈索引映射 |
| [R4 施工记录：订阅与节点](ui-r4-subscription-nodes.md) | R4 片：订阅菜单五组重排、首页单击即生效 |
| [R3b 施工记录：连通性行](ui-r3b-connectivity.md) | R3b 片：首页控制区第三行的只读探测 |
| [R7 施工记录：空态组件](ui-r7-empty-state.md) | R7 片：空态从透明标签改为「图标 + 原因 + 可聚焦动作」 |
| [R8 施工记录：托盘](ui-r8-tray.md) | R8 片：托盘按跨对象六组重排，总开关与首页同源 |
| [R5 施工记录：十态](ui-r5-states.md) | R5 片：连接/日志/规则的加载、失败、过期与冻结状态 |
| [R6 施工记录：工具窗口](ui-r6-tool-windows.md) | R6 片：关闭直接丢弃草稿、取消才询问；长流程关窗不中止 |
| [R9 施工记录：Token](ui-r9-tokens.md) | R9 片：`check_ui_tokens.py` 与硬编码清零 |
| [R10 施工记录：反馈路由](ui-r10-feedback-routing.md) | P4.1 片：`OperationId`、页面级结果归属与状态栏两段 |
| [U07 工作计划](ui-u07-plan.md) | 原型补全（P1–P2）与视觉结构对齐（S1–S2、V1–V5）的分片计划 |
| [U08 工作计划](ui-u08-plan.md) | 首页对齐 GPT 原型：第一版 H1–H4；第二版按原型逐屏照做（K1–K9：顶栏、浮层菜单、抽屉、数据页四页签、配置行列表、设置三组） |
| [静态原型](prototypes/ui-prototype.html) | 按 U06 渲染的 HTML 静态原型，用于观感复核；不是实现，不作验收依据 |
| [GPT 原型](prototypes/Mlash_Shadowrocket_Style_Interactive_Prototype_from_chatgpt.html) | U08 第二版的直接规格（四分区交互原型）；第一版期间「不是验收依据」的注记已被 2026-11-02 用户指令取代，见 [U08 §7](ui-u08-plan.md) |
| [对标调研](ui-benchmark-research.md) | U07 Clash Verge 2.5.9 源码与 Shadowrocket 的结构调研及证据等级 |
| [原始产品规格](Mlash_Codex_Design_Spec_v2.0.md) | 需求和验收基线；接受的偏差由 ADR 说明 |
| [架构决策索引](adr/README.md) | 编号 ADR、后续替代关系和设计理由 |

## 进度与证据

| 文档 | 职责 |
|---|---|
| [项目状态](project-status.md) | 当前实施队列、实现范围及后续验收 |
| [需求映射](requirements-traceability.md) | 需求/场景与实现、证据边界的对应关系 |
| [验证报告](verification-report.md) | 实际执行结果，区分历史汇总与当前验证 |
| [历史归档](archive/README.md) | 旧功能对照、阶段实施及 UI 方案，仅用于追溯 |

## 使用规则

需求、当前契约、实施状态和验证结果分别维护，不能相互冒充。原规格遇到已接受偏差时查看对应 ADR；早期 UI 决策的后续关系见 ADR 索引，当前交互以当前契约为准。

一项事实由一份文档拥有，其他页面链接引用；新增或移动文档必须收录并修复相对链接。提交前执行 `python scripts/check_docs.py` 和 `python scripts/check_document_privacy.py`，不提交个人环境、真实订阅或原始运行记录。
