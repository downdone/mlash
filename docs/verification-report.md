# 验证报告

本文只记录实际结果，区分本轮执行、历史匿名汇总与尚未验证的资格。原始配置、私人环境、真实订阅及原始诊断不进入公开文档。

## P03：验证与发布流程收敛（2026-11-17）

审视 1.20.0 发布链发现：纯文档记录曾触发完整 Windows 构建/回归和打包；普通代码 CI 也准备发布专用内核源码、打包并上传未发布附件；测试筛选为空时 CTest 可返回成功；重复预发布使用 `--clobber` 会覆盖同名附件。对应调整为常规 main/PR 独立文档轻检、代码 Windows 构建与完整隔离回归、仅标签/显式发布准备源码及打包冒烟，并禁止不同摘要的同名附件覆盖。发布文档中的包文件数按实际清单修正为 35。

本机对 Windows 侧车直接运行 GNU `sha256sum -c` 失败，原因为 CRLF 被读入文件名；发布脚本改为读取时规范化换行，三个 1.20.0 本地 ZIP/侧车均通过。`scripts/test.ps1` 对不存在的测试名现在按预期失败；六项 unit 层和 `release_tools` 一项均通过。`scripts/build.ps1 -Preset windows-mingw-open` 完整构建通过；文档导航、隐私检查、四项文档工具测试、工作流 YAML 解析及发布 Bash 语法检查通过。

提交 `05bab6b6a92e63a07d207e5ca505f339c39a56ea` 已推送 main。[文档工作流](https://github.com/downdone/mlash/actions/runs/43996640520)成功；[Windows 工作流](https://github.com/downdone/mlash/actions/runs/43996640734)完成构建及完整隔离回归：51 项、50 通过、0 失败、1 项 `system_test_guard` 跳过，456.23 秒。准备步骤成功且仅拉取二进制固定内核，打包/原生冒烟、发行附件上传与发布 job 均按普通代码提交规则跳过。随后仅修改文档的提交 `44f3b57239f34625aa823a27f8d6a29d5fdb713f` 只触发[文档轻检](https://github.com/downdone/mlash/actions/runs/43999206584)并成功，没有启动 Windows 工作流。本轮未运行新的标签发布流程，也未生成新发行版本；发布重跑的“不覆盖已有不同摘要附件”逻辑只完成 Bash 语法与本地侧车校验，待下次真实发布验证。

## U18：1.20.0 Windows 候选与发布前验收（2026-11-17）

按 [U18 事前设计](design/u18-fast-tests-window-chrome-update.md)完成整组最多 12 个有界异步延迟请求（两条本地连接池）、应用内/系统窗口标题栏设置，以及固定 GitHub 发布源的应用更新检查、校验与独立候选包。`AppUpdateSource` 的版本/地址/摘要校验、损坏包保留原候选和取消旧回包由隔离测试覆盖；实际 GitHub API 当时触及公共限流，未将在线实网检查记作通过。应用更新目前是已验证候选的人工便携升级，不作后台原地替换。

`scripts/build.ps1 -Preset windows-mingw-open` 在最终 1.20.0 代码上通过。首次全量运行 49 通过、1 系统门禁跳过、1 热键用例失败；失败是旧模拟 `/configs` 仅含 mode，违反现行类型化响应的 mixed-port 契约。修正夹具并定向复测后，最终 `scripts/test.ps1 -Preset windows-mingw-open` **50 通过、0 失败、1 系统写入门禁跳过**（51 项，457.79 秒）。整组测速定向夹具验证首批 12 个回环请求、进度与取消；应用更新定向单元测试通过。`python scripts/check_docs.py`（118 份 Markdown、0 问题）、`python scripts/check_document_privacy.py`（125 项、0 问题）与架构边界检查均通过。

Windows 11 原生 Qt 后端的应用内/系统标题栏可逆切换测试通过，150% DPI 样片生成并查看；早先受限沙盒中偏好保存失败不计产品失败。尚未人工逐项验证真实鼠标拖动、边缘缩放、读屏、Windows 10 或系统级代理/TUN 写入。`scripts/package.ps1 -Preset windows-mingw-open` 的 35 文件白名单与逐文件完整性通过，便携 ZIP SHA-256 为 `89a81334c227a7e5a8a3eba50320a3dde13b25c386b85d40e4b24704ee63d3e3`；包内 `scripts/ci_smoke.ps1` 通过：CLI 为 1.20.0、Qt/Schannel 可用、八页样片齐全。当前为预发布，`release_qualified=false`。

提交 `1376b0301369680a88d1f024606e090c9ccfb316` 与 `v1.20.0` 标签已推送 main。[标签发布流水线](https://github.com/downdone/mlash/actions/runs/43989694240)的 Windows 编译、默认完整回归、打包、隔离原生冒烟、图标与来源核对及发布均成功；[主分支流水线](https://github.com/downdone/mlash/actions/runs/43989693858)构建成功、发布步骤按规则跳过。[GitHub 预发布](https://github.com/downdone/mlash/releases/tag/v1.20.0)含八个附件。GitHub 的便携 ZIP 摘要为 `d77d82c2acaa7445c40604aaa2ca41a58a9e6dda02dc62ba0362f715d0b07819`；重新下载的三份 SHA256 侧车、源码来源清单和 35 文件包校验记录，与 GitHub 附件摘要、标签提交及固定内核版本一致。大 ZIP 的独立重下载因本机连接 GitHub 附件服务器超时未完成，因此不记作重下载逐字节复验；CI 已在上传前验证包完整性。发布环境仅执行 Windows 检查，macOS/Linux 平台检查跳过；真实在线更新检查受本机公共 API 限流，未记作已验。

## U17：全界面视觉系统重建（2026-11-16）

按[事前设计与实施后复核](design/u17-visual-system-rebuild.md)合并字体计算链：Noto Sans SC 应用字体、语义字号和文字缩放由 `UiTheme` 一次确定；移除 QSS `font-size` 正则重写及局部数值字体覆盖。Shadowrocket 特化集中给出明暗/高对比颜色、字阶与几何；常态以面色而非叠加外框分层，非交互图标改用次级色。节点、配置、设置行的自绘线对齐单个设备像素；焦点不再修改内边距。节点卡片按 300 DIP 最小宽度布局并分行显示名称/协议，修复中等窗口四列导致名称几乎不可读的问题。未修改 Controller 命令、配置格式或系统写入。

`scripts/build.ps1 -Preset windows-mingw-open` 完整构建及最终增量构建通过。`scripts/test.ps1 -Preset windows-mingw-open -Layer unit` **5 通过、0 失败**，包括新增 1.5 DPR 单物理像素线条场景；`flat_workspace_tests`、`settings_value_tests` **2 通过**。受限沙盒中的 `ui_components` 与 `appearance_tests` 首次失败，失败为测试内核监听端口身份无法只读核对，连带一项候选写入次数断言；同机隔离目录允许该只读观测后，两组复测 **2 通过、0 失败**（45.65 秒）。未启用 `--system-proxy-test`。

Qt Windows 原生后端的全页明暗/高对比场景 **3 个 QtTest 事件通过、0 失败**，生成 52 张样片；有配置内容的首页/配置/数据/设置与卡片视图场景亦 **3 个 QtTest 事件通过、0 失败**。原生环境报告有效 DPR 1.5，字体请求、解析及汉字字形均为 Noto Sans SC。复核 840/940/1180 DIP、100/120% 文字及明暗样片，主操作未见遮挡，节点卡片在宽窗三列、窄窗两列，设置行和节点分隔线保持单线。`check_ui_tokens.py` 为 0 命中，文档导航/隐私检查为 0 发现，`git diff --check` 无空白错误。

这是本机自动交互与截图人工对照，未覆盖人工键盘全路径、读屏、Windows 10、不同显卡/显示器或正式发行完整回归；`release_qualified=false`。

`scripts/package.ps1 -Preset windows-mingw-open -OutputName Mlash-1.19.0-u17-preview` 生成独立本地预览目录与 ZIP；35 文件白名单及逐文件完整性检查通过。ZIP SHA-256 为 `f8cca85e6aa4755e43b5d3b99f2157ce965227d12a137bc5aa149286746a8bd2`。`scripts/ci_smoke.ps1` 在隔离数据目录运行包内 `Mlash.exe` 通过：版本 1.19.0、Qt 6.10.2、Schannel TLS 可用，八张页面截图齐全；人工查看空配置首页样片未见启动或布局异常。该文件仅为本地预览，未上传或替换现有发行包。

## P02：U16 Windows 便携预览包（2026-11-16）

`scripts/package.ps1 -Preset windows-mingw-open -OutputName Mlash-1.19.0-u16-preview` 成功生成独立本地目录、ZIP 与 SHA256 侧车。包内 `Mlash.exe`/服务 PE 及 CLI 版本均为 1.19.0，固定内核与清单哈希吻合；白名单及逐文件完整性核对为 35 文件。ZIP 52,736,765 字节，SHA256 为 `2c403e76dcfa3b09538e596fa7b7cdf7419f4cdccc7d5414ab1e2f0c2a4e2e86`，侧车一致。

`scripts/ci_smoke.ps1` 对包内程序在隔离数据目录运行通过：Qt 6.10.2、Schannel TLS 可用、八张页面截图齐全；人工查看首页空配置样片，未见启动失败或布局溢出。未使用真实订阅或改写宿主系统代理。此为预览包，`release_qualified=false`；没有执行完整回归、干净机安装、真实 TUN/系统代理写入或人工全页验收。

## U16：模块边界与单文件组件拆分（2026-11-16）

**收尾切片：类型契约、弱生命周期、兼容头清理。** 旧 `app/Types.h`、`app/ConnectionColumns.h` 和 UI 三个聚合/兼容头已移除，消费者改用 domain 类型与明确所属组件；UI 公开头只前置声明 Controller。版本、运行配置和代理组响应在 mihomo 适配端解析为类型化 DTO，拒绝字段缺失/类型错误，保留 Result 错误与现有状态快照。`PageRouter` 对页面使用 Qt 弱引用；纯路由场景从 `UiTests` 移到独立目标，协议解析另有独立目标。CTest 现有 50 项，均带层标签。没有更改配置格式、系统写入路径或页面视觉值。

`scripts/build.ps1 -Preset windows-mingw-open` 使用仓库内固定的开源 Qt 与本地缓存依赖完整构建通过；默认 `windows-mingw` Qt 套件在当前环境被本机 Qt 授权服务阻断，不能把它记为产品编译失败。`scripts/test.ps1 -Preset windows-mingw-open -Layer unit` **5 通过、0 失败**（0.49 秒）；同机允许本地端口观测后，`page_router_tests`、`tun_tests`、`core_readiness_tests`、`engine`、`ui_components`、`page_lifecycle_tests`、`website_tests`、`architecture_boundaries` **8 通过、0 失败**（166.01 秒）；`flat_workspace_tests`、`settings_value_tests`、`appearance_tests` **3 通过、0 失败**（35.18 秒）。这些重叠选择组的通过数不相加冒充全量 CTest。受限沙盒内 TUN 端口 PID 观测曾被系统拒绝，随后在同机允许该只读观测的隔离测试环境通过；没有执行 `--system-proxy-test`。文档导航、隐私、UI token 和 `git diff --check` 均通过。完整 50 组、人工原生视觉/读屏、真实系统代理与 TUN 写入、干净机部署和长期稳定性未验，`release_qualified=false`。

**后续切片：内核特化、UI 设计系统和测试分层。** `CoreApi` 为当前实际能力接口，`MihomoClient` 为唯一实现；进程管理与只读探测/测速消费者依赖接口，Controller 仍持有具体实例。`ui/design` 新增只读语义规格、Shadowrocket 风格取值及独立组件 QSS 模板，`UiTheme` 只负责 Qt 外观应用、token 替换与兼容桥；没有更改页面任务层级、既有视觉取值或系统代理写入路径。测试新增只链接 domain 的 `domain_contract_tests` 和只链接 Qt/节点模型的 `ui_model_tests`；旧 `EngineTests` 不再编译 UI 模型。48 个 CTest 项目均有层标签，历史混合套件保留双标签。`scripts/test.ps1` 支持 `-Layer` 并将测试 TEMP/TMP 放在忽略的 `build/test-temp`，执行后恢复原环境。

`scripts/build.ps1 -Preset windows-mingw` 完整构建及最终增量配置通过。`scripts/test.ps1 -Layer unit` **3 通过、0 失败**（0.37 秒）；`navigation_contract_tests` **1 通过**；`scripts/test.ps1 -Layer system-gated` **1 跳过**，没有启动系统写入。在受限沙盒中，核心端口 PID 观测被拒，`core_readiness_tests`、`engine`、`home_metrics_tests`、`website_tests` 和 `ui_components` 失败/超时；改用同机隔离测试目录且允许 Windows 本机端口观测后，`core_readiness_tests` **1 通过**（22.84 秒），`engine`、`ui_components`、`home_metrics_tests`、`website_tests` **4 通过**（120.28 秒），`page_lifecycle_tests`、`appearance_tests` **2 通过**（33.22 秒）。这区分沙盒能力限制与产品回归，不把失败记录抹去。文档链接、隐私和架构静态检查均 0 发现；未执行本轮全量测试、原生人工视觉/DPI、真实系统代理/TUN 写入或正式发行验收。

QSS 模板从运行时适配中再拆出后，最终完整构建通过；`domain_contract_tests`、`ui_model_tests`、`appearance_tests`、`architecture_boundaries` **4 通过、0 失败**（31.25 秒），确认颜色替换、主题切换、对比度和最小模型目标未受拆分影响。此项自动结果不替代原生前台视觉评审。

框架几何规格补入侧栏内边距、顶栏两侧动作槽及间距后，再次完整构建通过；`domain_contract_tests`、`ui_model_tests`、`navigation_contract_tests`、`architecture_boundaries` **4 通过、0 失败**（1.89 秒）。尺寸取值与变更前一致；此次仅验证代码与导航契约，没有追加 DPI 截图比较。

事前设计见 [U16 设计](design/u16-modular-architecture.md)，决策见 [ADR 0055](adr/0055-domain-contracts-and-ui-components.md)。`mlash_domain` 独立编译；存储/配置不再反向包含连接列、测速会话、TUN 运行策略和平台快捷键类。`MainWindowParts.h` 拆为控件/模型/委托/顶栏组件，`MainWindow.cpp` 和 `AppController.cpp` 按职责分编译单元；Controller 的内核 REST 调用改为具名端点。未变更 UI 布局、数据格式及 OS 恢复流程。

`scripts/build.ps1 -Preset windows-mingw` 最终完整构建通过。拆分与纯契约迁移后，`tun_tests`、`flat_workspace_tests`、`workflow_depth_tests`、`ui_interactions`、`ui_components`、`page_lifecycle_tests`、`connection_view_tests`、`hotkey_tests`、`website_tests` **9 通过、0 失败**（109.46 秒）。独立 domain 库与 REST 端点迁移后，`engine`、`ui_interactions`、`page_lifecycle_tests`、`website_tests` **4 通过、0 失败**（110.41 秒）。Controller 文件拆分后，`tun_tests`、`engine`、`ui_interactions` **3 通过、0 失败**（126.80 秒）。新窗口编译单元改用所属组件头后，最终增量构建及 `ui_interactions`、`page_lifecycle_tests` **2 通过、0 失败**（18.18 秒）。`check_docs.py` 与 `check_document_privacy.py` 分别 0 发现。默认测试未启用系统代理写入门禁。

另新增 `architecture_boundaries` 静态依赖门禁：domain 不得引入上层模块，配置/存储不得重新引入已解除的反向依赖，app 不得直接发送原始内核 REST 请求；CTest **1 通过、0 失败**。本轮验证只说明模块拆分在这些自动化场景中保持行为；尚未完成 Controller 公共接口收敛、全端点 DTO、逐页对象所有权或原生人工视觉复核，不宣称正式发行资格。

## U15：界面细节根因修复（2026-11-16）

先定位四项可复现根因，再按 [U15 事前设计与实施后对照](design/u15-precision-root-causes.md) 修改。`ActionRow` 默认奇偶填充使悬停圆角与方角重叠区域消失；样式表又绘制同一悬停及分隔线。`NodeRowView` 把 viewport 圆角转整数区域蒙版，使 150% DPI 圆弧缺少半透明边缘。配置详情任务页签与目录行重复，工作区返回概览时重建页面；顶栏左右两个动作都进入配置添加。外部代理需确认接管被错误当作持续全局故障。

`scripts/build.ps1 -Preset windows-mingw` 完整构建通过。最终代码新增两个隔离 UI 场景：首/中/末设置行悬停底边像素；配置任务返回复用详情、保持详情与列表滚动，以及外部确认/真实错误告警区分，均通过。`scripts/test.ps1 -Preset windows-mingw -TestRegex '^(prototype_ui_tests|appearance_tests|flat_workspace_tests)$'` 为 **3 通过、0 失败**（34.55 秒）；随后仅删除已不再使用的详情页签样式并完成最终增量构建、两个场景复测。原生 QtTest 的 1040×720 DIP 首页、设置、配置详情样片在本机有效 DPR 1.5 下为 1560×1080 像素；复核节点列表圆角边缘可见过渡像素，顶栏只剩一个添加入口，详情无重复页签。文档链接/导航检查 118 份 Markdown 零问题，隐私检查 125 项零命中；`git diff --check` 通过。

`scripts/package.ps1 -Preset windows-mingw -OutputName Mlash-1.19.0-u15-review` 生成独立本地预览目录和 ZIP；35 文件白名单及逐文件完整性通过，ZIP SHA-256 为 `f5f7cf62c1b93502189e1c4003968288b1f0e1ea496e954d2aa36f15832aabf7`。没有覆盖现用便携目录或上传远端。

本轮不进行真实系统代理/TUN 写入，不宣称截图预览缩放或所有字体/显卡/DPI 环境均无锯齿；人工前台读屏和正式发行资格仍待独立验收。

## U14：原生全页精修与统一字体（2026-11-16）

按 [U14 设计](design/u14-shadowrocket-all-pages.md) 和 [ADR 0054](adr/0054-shadowrocket-all-pages-refinement.md) 收紧正文、次要文字、行高、间距与网站测试卡；移除全局 QSS 对应用字体族的二次覆盖。图标按设备像素比生成缓存变体。托盘快捷节点限制为 16 个，自动悬停测速优先当前节点并只测快捷范围；显式测试仍保留原上限与取消能力。未改变配置/系统代理写入路径。

追加内置 Google Fonts Noto Sans SC 可变字体，随 Qt 资源编入可执行文件；原始 SIL OFL 1.1 声明随便携包放置于 `licenses/NotoSansSC-OFL.txt`。导入字体 SHA-256 为 `a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`。Windows 原生 QtTest 的字形回读为 `font_requested=Noto Sans SC`、`font_resolved=Noto Sans SC`、`glyph_families=[Noto Sans SC]`；主页面样片有效 DPR 1.5，设置弹窗样片有效 DPR 1.25。对首页窄/宽窗、配置、设置和测速深色页面的原尺寸截图做了视觉复核，未见中文缺字或布局溢出。此为本机样片检查，不等于所有显示器 DPI、人工读屏或 Windows 10 验收。

`scripts/build.ps1 -Preset windows-mingw` 完整构建通过；原生 `appearance_tests` 两个指定场景和 `appearance_prefs_tests` 通过。`scripts/test.ps1 -Preset windows-mingw` 全量运行 45 组，**44 通过、0 失败、1 个系统写入门禁跳过**，耗时 485.96 秒；未在真实系统代理或 TUN 上执行写入。`scripts/package.ps1 -Preset windows-mingw -OutputName Mlash-1.19.0-u14-font-review` 生成本地候选，35 文件白名单及逐文件完整性通过，ZIP SHA-256 为 `97581abfe3f1d964f176dfbab8f3c46ec7abb0947d509554816632a77447a2ef`。包内文档隐私扫描 0 命中，源码文档链接检查 0 问题。本地候选不是正式发行资格，未发布。

## U14：Shadowrocket 原则驱动的全页设计（2026-11-16）

先对照 1.19.0 原生隔离冒烟截图、UiTheme/UiIcons/节点委托/网站卡片几何以及 U09–U13 当前设计，记录[全页精修规范](design/u14-shadowrocket-all-pages.md)，并制作[可切换原型](design/u14-all-pages.html)。设计覆盖首页、配置列表/详情与七类对象任务、数据四页签、设置目录与十三类任务，以及托盘和运行/空/异常状态。原型只使用虚构配置与节点，不访问网络、文件、内核或系统代理。

实际检查：HTML 解析成功；脚本经 Node `--check` 通过；在最小模拟 DOM 中渲染 29 个页面/任务路由均产生内容；`git diff --check`、文档链接/导航检查（115 份 Markdown、0 问题）、隐私检查（122 项、0 问题）通过。未执行浏览器视觉检查、Windows 原生 DPI/字体链路测试或产品构建/回归；这些结果不能证明锯齿、密度及布局已在 Mlash.exe 中修复。产品源代码与发布包未修改。

## R19：GitHub 1.19.0 发布与附件复核（2026-11-15）

按用户明确授权的平台验收规则，直接将提交推送 main，不等待 PR/跨平台审批。[PR #5](https://github.com/downdone/mlash/pull/5) 因提交进入主分支标为 merged。发布标签 v1.19.0 固定到 `8b838c0c509dfeb9f8b99ee68fd217f1d88d5a6f`，后续文档提交不移动标签。

此前[第三轮 PR 检查](https://github.com/downdone/mlash/actions/runs/43616168211) 实际通过：45 组为 44 通过、0 失败、1 系统门禁跳过（474.47 秒），Windows 编译、34 文件便携部署、八页原生冒烟/Schannel、图标与源码附件检查通过。取得原始 XML 与八页截图，未将自动化当成人工/系统写入验收。

[实际发布链](https://github.com/downdone/mlash/actions/runs/43670447421) 使用 main 的上述提交，以 run_windows_regression=false 手动触发；Windows 全量步骤明确 skipped，Windows 编译、package.ps1 部署、包内冒烟、图标与来源/完整性检查全部成功，build/publish 两 job 通过。此发布链不声明 Windows 全量回归通过；该证据属于上一条 PR 检查。

[GitHub v1.19.0](https://github.com/downdone/mlash/releases/tag/v1.19.0) 已公开（draft=false、prerelease=true、latest=false），八个附件均 uploaded。重新下载全部附件至忽略的 `dist/Mlash-1.19.0-release-assets/`；逐项检查字节大小、GitHub digest、三份 ZIP 的 CRC/SHA256 及侧车。源码清单、Mlash 归档内版本/实际实现、固定内核提交与哈希、34 文件白名单与包内逐文件完整性、运行指纹及 package-verification.json 一致。标签 API 回读为同一来源提交。便携 ZIP SHA256 为 `5c010b8d28b9eba38854dce098f72f830bd02a895a4232f4925c190010ceb881`；下载源码/便携包共 125 份文档隐私检查零命中。

原始发布日志与复核报告在 `build/r19-release.log`、`build/r19-asset-verification.json`，不随源码分发。发布来自公共 Windows 工具链，本机旧 macOS 预览包未上传。保留 prerelease、release_qualified=false 和未签名边界；真实 Windows 系统代理/TUN/登录恢复、干净部署、人工读屏及长期资格仍未完成。

## R19：1.19.0 发布准备（2026-11-14）

用户授权发布到 GitHub Releases。汇总 U12/U13，更新 CMake、入口版本、变更记录和 1.19.0 发布说明；依赖、固定内核及发行工作流保持不变。Inspector 关闭后边距的旧测试同步使用当前 ContentTop，保留几何恢复断言。版本/发布标签检查、文档导航/隐私/UI token、四项文档工具和十七项发布工具检查通过。

Windows 首轮 [PR 流水线](https://github.com/downdone/mlash/actions/runs/43610049516) 构建成功；45 组回归为 38 通过、6 失败、1 系统门禁跳过，未产生发布。四组超时停在旧测试未明确处理工作区脏草稿确认；另有主视图/详情父级断言依赖旧 QObject 树，以及出口诊断在主窗口销毁期间回调已释放页面导致崩溃。本轮更新这些交互路径，保留未写入、失败留稿、父级归属与快照内容断言；显式主视图定位改用 mainPages。主窗口销毁前先释放出口诊断，新增带嵌套依据详情的整体销毁回归。

修复后本机完整重新构建成功，工作区/出口两组通过（6.38 秒），三项相关 Depth 个案通过（1.285 秒）。Windows 第二轮 [PR 流水线](https://github.com/downdone/mlash/actions/runs/43613898627) 为 43 通过、1 失败、1 系统门禁跳过（478.21 秒），仅 UI 组件组的两条旧可见性断言失败：搜索已默认收起、连接详情入口已迁入上下文/键盘命令。测试改为先明确展开搜索，再检查工具排及两张卡片完整可见；连接详情验证实际 QAction 的空选择禁用状态。Windows 全量复跑、发布及附件复核仍待实际结果；不放宽超时、不跳过失败组。预发布保持 release_qualified=false。

用户于 2026-11-15 明确修订发布规则：开发平台只需本平台检查通过，可直接推送，不等待跨平台回归或 PR。此次采用已通过的 macOS 构建/专项证据，直接推送 main；手动发布关闭 Windows 完整回归，仍运行 Windows 编译、包内冒烟、来源/图标及完整性检查。工作流新增显式 run_windows_regression 输入，默认仍执行；本次跳过情况须在发布结果中记录，不能声称该发布链 Windows 全量通过。

## U13 紧凑密度（2026-11-14）

先记录 [U13 方案](design/u13-compact-density.md) 和 [ADR 0053](adr/0053-compact-density.md)，再实施。UiTheme 集中收紧顶栏/正文/共享行/字号，双行高度与绘制改为实际正文加副文指标，配置列表容器随行高收缩；设置目录和工作区页签同步减少留白。短列表的多余空间留在末尾，长名称给当前标记预留区域。没有更改 Controller 命令、持久化格式、平台能力或恢复保护。

使用同一 30 个虚构节点、1040×720、100% 字号设置的 Cocoa 夹具比较：

| 观测 | U12 | U13 最终原生整组 |
|---|---:|---:|
| 节点行高（DIP） | 52 | 38 |
| 控制卡高（DIP） | 146 | 108 |
| 组头顶部 y（DIP） | 270 | 206 |
| 节点视口高（DIP） | 381 | 455 |
| 完整可见成员 | 6 | 10 |

初次 U13 单用例观测控制卡 112、组头 y=210；最终原生整组记录为上表值。仅为这些隔离渲染场景的实际几何，未把图像估算的 Shadowrocket 尺寸或不同平台字号换算冒充实测。六节点短列表另有检查，组头保持顶部预算，剩余空白不再均摊进工具排。

构建入口 `scripts/build.ps1 -Preset portable` 完整 arm64 Release 编译/链接成功，固定 Qt 6.10.2，版本仍为 1.18.0。经标准测试入口执行 flat_workspace_tests、status_bar_tests、connection_view_tests、ui_refinement_tests 四组全部通过（33.68 秒）；追加实际鼠标命中回归后，专项再次通过（4.03 秒）。检查选择、测速、详情互不误触，覆盖 100/120% 文字，三尺寸首页/Inspector，失败留稿/取消和隐藏任务等既有保护。

Cocoa 原生工作区专项七项全部通过（含初始化/清理为 9 passed，5.604 秒）；外观程序 allPagesAndDialogsAcrossAppearances、groupedSettingsAndPopulatedPagesRemainUsable、toolWindowsKeepActionsAndDraftsAccessible 三项全部通过（含初始化/清理为 5 passed，20.395 秒）。取得 26 张专项、240 张页面/工具组合截图；已图像复核首页、短列表、配置、设置及前后密度对比。原始截图与日志位于 `build/u13-before/`、`build/u13-after/`、`build/u13-pages/` 和 `build/u13-*.log/txt`，不纳入源码分发。默认测试未改宿主系统代理，模拟就绪/代理状态不作实际核心验证。

文档链接/导航检查 114 份 Markdown、隐私检查 121 份文档、UI token 检查均零问题，git diff --check 通过。未验 Windows 原生与完整回归、真实系统/公网任务、人工读屏、150% 系统 DPI、长期性能及正式发行资格；本轮未提交、推送或打包发布。

## U12 页面内工作区与首页收敛（2026-11-14）

按 [事前设计与事后复核](design/u12-flat-workspaces.md)、[ADR 0052](adr/0052-flat-task-workspaces.md) 实施。首页采用三行控制、右侧图标工具排、默认收起搜索、来源/真实浏览组单份标题和按内容结束的节点列表。配置副标题角色不再误读 UUID，非空列表移除覆盖内容的空态导入动作；稳定正文宿主修正窄窗 Inspector 顶栏/状态栏错位。

高级表单与嵌套编辑转入正文工作区；内核直达完整管理，备份四任务同级，配置内容按对象切换，来源与维护合并。重复入口复用、同级/返回留稿、隐藏 pending/草稿参与关闭保护、父子任务关闭、普通帮助与网站结果就地展开均已实施。组参数绑定配置/组身份，切走隐藏、返回恢复；外部预填未编辑可取消。日志在阅读历史时直接显示“回到最新”，常态保持安静。

构建：使用固定 Qt 6.10.2，本机 arm64 Release 经 `scripts/build.ps1 -Preset portable` 编译及链接成功，未更改发行版本或平台适配。构建日志与测试原始结果保留在 `build/u12-*.log`、`build/u12-*.txt` 及 CTest XML，不进入源码分发。

自动回归：通过标准测试脚本执行 flat_workspace_tests、status_bar_tests、tool_window_tests、proxy_bypass_tests、protocol_tests、connection_view_tests、home_metrics_tests、ui_refinement_tests、release_tools 九组，全部通过（42.00 秒）。专项使用隔离临时存储及明确禁止 OS 写入的平台夹具，覆盖失败留稿/取消、隐藏草稿、提交期间拒绝关闭/换对象、嵌套生命周期、对象/组身份、搜索定位与恢复、配置实际显示角色，以及 Inspector 三尺寸外壳几何。不能把模拟 Ready/Owned 状态作为实际核心与系统代理验证。

Cocoa 原生自动检查：工作区专项六项与外观程序的 allPagesAndDialogsAcrossAppearances、groupedSettingsAndPopulatedPagesRemainUsable、toolWindowsKeepActionsAndDraftsAccessible 实际通过。截图目录 `build/u12-review/` 与 `build/u12-pages/` 包含首页/配置/备份/内核任务、三尺寸 Inspector，以及八个旧入口适配、浅深色和 100/120% 文字组合；已复核首页、页面合览及任务布局。图像与自动按键不替代人工前台/读屏验收。

文档导航检查 112 份 Markdown、隐私检查 119 份文档、UI token 检查及四项文档工具回归全部通过，git diff --check 无问题。

失败与修复：首次协议组因 macOS 默认临时路径过长无法创建本地 IPC，使用短隔离临时目录后完整协议组通过；新布局测试误取隐藏工作区的返回按钮，改为检验当前可见宿主。状态详情原测试仍要求模态消息框，更新为验证正文只读全文后通过。切组回归曾访问重建后已销毁的菜单项，改为触发一次即停止；产品的 selectGroup 同时按值持有组名，避免菜单销毁使后续组名引用失效。以上修复未绕过业务边界。

扩展检查的限制：page_lifecycle_tests 与 settings_value_tests 在此平台等待 exitReady 失败，home_control_tests 在非 Windows 存储夹具准备阶段失败；剪贴板 NodeLinks 个案亦在默认平台存储初始化中止，未进入 UI。保留这些失败记录，不将本轮称为完整全绿；对应页面/关闭草稿/空态和失败保护通过新隔离夹具专项验证。没有放宽非 Windows Unsupported 或代理恢复保护。

未验：Windows 原生与该分支完整 Windows 回归、真实核心/公网任务、实际系统代理/TUN/登录恢复、人工读屏、150% 系统 DPI、长期性能及正式发行资格。尚未提交、推送或发布本轮变更；此前 1.18.0 公开附件不包含 U12。

## R18 GitHub 发布与远端核验（2026-11-14）

- 当前 UI 与 1.18.0 经 [PR #4](https://github.com/downdone/mlash/pull/4) 合入 main，合入提交 `0fa2eb79e5f47acb1e59b24fef10d3d6c92ca7cd`。发布标签 `v1.18.0` 绑定 `d6cfc9c48a85c3fac20346178e614ead97f38812`，是 main 的祖先，代码树与合入结果一致；不移动已发布标签。
- [PR 验证](https://github.com/downdone/mlash/actions/runs/43559478390)、[正式预发布链](https://github.com/downdone/mlash/actions/runs/43560411319) 和 [main 推送验证](https://github.com/downdone/mlash/actions/runs/43560939108) 均成功。公共 Qt/MinGW 构建、默认隔离回归、便携包、原生启动/TLS、图标、源码归档及上传均通过。PR 回归 469.18 秒，发布回归 418.46 秒；两者均为 44 组、43 通过、0 失败、1 系统门禁跳过。
- [GitHub v1.18.0](https://github.com/downdone/mlash/releases/tag/v1.18.0) 已确认 `draft=false`、`prerelease=true`；八个附件均为 uploaded。通过 GitHub API 再核对标签与构建提交。
- 发布后重新下载便携 ZIP、两份源码 ZIP、三个 SHA256 侧车、source.json、package-verification.json。网络连接中断经断点续传完成；最终各 ZIP 字节摘要与侧车、GitHub 存储摘要、来源清单及 CI 验证 JSON 一致。便携 ZIP SHA256：`efbe19469f4818f224b5cad1695a2009cdf88dd7f26553217ef63ca5a57dd2c8`，34 文件清单/完整性通过；源码文档隐私检查零命中，来源提交与固定内核一致。
- 初次登录的授权不足以修改工作流文件，因此将字体目录设置放入现有测试脚本，在没有显式覆盖时选择系统字体，退出恢复环境；工作流文件未改，未扩大账户权限。测试入口已本地验证，三个线上运行均实际使用同一入口。
- 本机商业 SDK 的旧包没有上传；公开附件来自公共工具链的发布流水线。`release_qualified=false`、未签名，真实 Windows 10/11 系统代理/TUN/登录恢复、干净部署、长期稳定性与完整第三方材料验收仍按发布门禁保留。本次成功仅声明上述预发布范围。

## R18：1.18.0 提交与预发布准备（2026-11-14）

- 更新 CMake、产品入口说明、CHANGELOG 和版本发布说明为 1.18.0；保留历史验证事实。现有 Windows 工作流调用的测试脚本补上系统字体目录，再执行原默认隔离测试，不增加系统写入或放宽超时。
- 本机构建通过；完整 44 组首轮 42 通过、1 失败、1 系统门禁跳过，451.60 秒。失败为网站 GUI 测试仍要求规则页保留数据导航，违反 ADR 0051 的配置归属；修正旧断言后网站组复测通过，12.05 秒。没有将多次结果冒充一次完整全绿。
- 1.18.0 便携 ZIP 白名单/完整性 34 文件通过，CLI/PE 版本一致，包与构建 EXE 字节一致。ZIP SHA256：`a06b6b1c6fff14e1e4464148e578df616a2da30e821261a798d8ec8d2274807f`。两份 EXE 的 16/24/32/48/64/128/256 图标与源图一致。
- `ci_smoke.ps1` 清理 Qt/PATH 外部依赖后运行包内程序通过，八页截图齐全，Schannel TLS 可用。未覆盖日常使用包，release_qualified=false。
- 已提交源码与发布准备；实际推送因没有 GitHub 登录凭据被拒绝。本轮尚无远端新标签、线上运行或 Release 可核验。不能把本地发布准备称为 GitHub 发布成功。
- 文档链接 110 份、隐私 117 份、文档工具四项单测和 UI token 检查通过。真实系统、正式发行与第三方材料边界沿用发布验收文档。

## U11 最终原型原生实施（2026-11-14）

按 [系统规范](design/u11-ui-system.md) 先记录设计，再完成共享行/分组、AppShell、NodeRow、Inspector 及四分区装配，决策见 [ADR 0051](adr/0051-prototype-ui-task-hierarchy.md)。保留进入本轮前的 U08/U10 未提交工作，不将整个工作区差异都计作本轮新增。

### 实施与原型对照

- 首页三行控制区、真实组名/当前成员/数量、国旗节点连续列表、独立延迟和信息命中、搜索/定位/测试/更多；当前勾选与预选分开，自动组只读。品牌置底，居中标题，状态栏固定在内容区。
- 配置列表进入对象详情，再进入规则、DNS、节点与组、拨号链、维护、Provider 和来源任务。候选先预览再显式应用；浏览、下载、详情不激活配置。
- 设置默认分组目录，启动/网络/内核/备份/工具使用单份原字段和草稿。网络 URL 失焦不保存；保存失败保留输入。修复早期校验拒绝未标记 SettingsApply 导致保存状态不结束的问题，取消/返回语义已回归。
- 数据采用流量/连接/测试/日志四页签。连接默认目标/规则/链/状态四列，原列偏好保留；连接详情复用快照并增加版本/代次检查，新规则仍仅预填。
- Inspector 宽窗实际收窄正文并列，窄窗替代正文，关闭恢复布局与焦点。共享主题、分隔线、行高、字体后备与图标统一；深色开关圆钮和品牌原色图标底托经实机复核修正。

### 实际执行结果

- `scripts/build.ps1` 构建通过。全量 43 组首轮：35 通过、7 失败、1 系统门禁跳过，482.38 秒。失败涉及旧入口/断言及实际布局、导航、设置失败反馈缺陷，已修复后复测，没有将首轮记录改写为全通过。
- 修复后的 9 组回归全部通过（workflow_depth、prototype_ui、ui_interactions、ui_components、feedback_routing、page_lifecycle、home_control、connection_view、appearance），90.51 秒；TUN 状态与首页指标组在前一轮针对性复测通过。新增导航契约组单独通过，2.01 秒。开关/搜索修正后，原型、导航、外观三组再次通过，35.32 秒；末次品牌图标底托修正后，外观组再通过，30.57 秒，原生网站卡片用例通过。各组最终成功证据来自多次执行，未声称一次最终全量通过。
- 原生 Windows QTest：原型任务流/窄宽 Inspector 通过；填充页面深浅主题、100%/120% 文字与三尺寸检查、网站响应卡片测试通过；连接嵌入 Inspector 与规则预填/取消用例通过。数据为隔离合成配置、本地 Mock HTTP，未访问真实订阅或写入系统代理。
- 原生截图实际复核：首页、配置列表/详情、设置目录/网络编辑、节点和连接窄宽 Inspector、数据页签及网站响应卡片。截图只证明对应状态，未完成真人可用性实验、完整读屏或独立切换系统 DPI 的矩阵；原型 HTML 浏览器渲染因工具策略未执行，不宣称同屏像素差分。
- `scripts/package.ps1` 生成独立 U11 UI 预览包；34 文件白名单与完整性校验通过。包内程序隔离原生冒烟输出八页，TLS/Schannel 可用；未替换日常包、增加真实用户数据、变更发布版本或上传。
- 文档包含未跟踪文件的链接/导航检查 109 份 Markdown 零问题，文档与 HTML 隐私检查零命中，UI token 检查零命中。

这是本轮 UI 实施与预览验证，不是重新声明正式发行资格。没有执行系统代理写入、TUN 权限实机、干净机器部署、Windows 10 或长稳验收。Qt 商业 SDK 许可服务仍有告警；没有接受条款或设置绕过选项，工具链发行授权仍未由本轮验证。

## U11 系统设计复核（2026-11-14，仅设计）

按用户优先 UI 系统方法的要求，重新读取最终原型 CSS、页面渲染/详情函数，与 U10 的外壳、首页、节点、配置、设置、连接详情和 UiTheme 对照。形成 [U11 规范](design/u11-ui-system.md)，逐项列出结构偏差、共享组件、明确尺寸、原型颜色、字体后备、状态语义及 G1–G6 门禁。没有修改产品源码、依赖、已确认原型或运行包；没有将既有自动测试通过作为原型视觉达标证据。

包含未跟踪文档的实际链接/导航检查为 108 份 Markdown、零问题；文档及 HTML 隐私检查零命中，相关文档空白检查通过。纯设计变更未重跑产品构建和测试。

## U10 紧凑版原生首切片（2026-11-14）

范围和设计后复核见 [U10 施工记录](design/u10-implementation.md)，行为决策见 [ADR 0050](adr/0050-shadowrocket-compact-widgets.md)。保留进入本轮之前的未提交 U08 工作；没有将其全部计作本轮新实现。

- `scripts/build.ps1` 构建通过。Qt 商业 SDK 的许可服务仍输出条款接受告警；未代为接受、未设置绕过选项，不能据此宣称开源工具链或发行授权验证完成。
- `scripts/test.ps1` 筛选 8 组：workflow_depth_tests、ui_interactions、ui_components、status_bar_tests、feedback_routing_tests、page_lifecycle_tests、settings_value_tests、appearance_tests 全通过，合计 90.61 秒。离屏测试使用系统字体目录；限时不变。
- 新回归固定订阅浏览不激活、显式应用、延迟点击不发选择、禁用延迟不测试；已有真实内核隔离流程继续覆盖选择/配置重载/轻量重建。没有执行系统代理写入、TUN 权限、干净部署或长稳测试。
- 窄窗、主题、100%/120% 字号布局自动检查通过。原生 Windows 八页截图复核修正文字裁切和旧几何抓图；不等同于完整人工键盘/读屏验收。
- `scripts/package.ps1` 生成独立 UI 预览，34 文件白名单及完整性检查通过；暂存程序原生冒烟输出八图，Qt 6.10.2、Schannel TLS 可用。没有替换日常使用包或公开发布。
- 包含新文档的链接/导航检查 107 份 Markdown 零问题；文档和 HTML 隐私检查零命中；UI token 检查零命中。原始构建、测试及截图证据留在忽略的构建目录。

初次构建缺少测试头文件、旧断言仍期望下拉即激活及隐藏按钮的旧几何，均已修正后重跑。缺省离屏插件无字体时的超时不计成功；最终使用可用系统字体的完整筛选结果。产品全部最终稿功能与正式发行资格仍未完成。

## U09-V Verge 设计补充（2026-11-13，仅静态评审）

只读复核声明版本 2.5.9 的参考源码：组工具/成员/组导航、延迟状态、订阅项、连接页/详情、当前代理及 mutate 协调；并交叉检查 Mlash 节点偏好/定位/测试 URL、配置维护和连接详情的已有基础。输出 [U09-V](design/u09-verge-review.md) 八项采纳建议、不同优先级及不照搬清单，同步 U09 UI 和系统设计。参考路径/符号和选取文件哈希在该文档维护，不记录工作站路径或真实订阅。

包含未跟踪设计文件的实际检查：104 份 Markdown 文件链接/导航零问题，新评审文档隐私规则零命中。没有编译/运行 Verge、执行用户可用性实验或性能比较；只证明所读取客户端代码存在相应入口/逻辑，不证明插件内实现或实机行为。Mlash 产品源码、依赖、原型和现有未提交实现未修改，未重跑产品构建/回归，不将设计补充计作功能交付。

## U09 设计评审稿（2026-11-13，仅设计）

交付 [UI 方案](ui-u09-shadowrocket.md)、[系统设计](design/u09-system-design.md)、[Widgets/QML 评审](design/u09-technology-review.md) 和 [离线预览](design/u09-preview.html)。技术评审结合现有 CMake/模块/状态/路由/GUI 生命周期以及 Qt 6.10 官方资料；未运行框架样片，不宣称性能或资源优势。

实际检查：将未跟踪的新设计文件也纳入文档导航检查，103 份 Markdown 的文件链接和导航零问题；新设计四文件隐私规则零命中；HTML 唯一静态 ID、相对说明链接、六地区内置旗帜路径检查通过，未包含远程脚本或网络/存储 API；提取脚本经 node --check 通过。八种候选文本/背景组合对比度均 >=4.5:1，实际为 16.83、15.63、5.33、6.64、5.57、6.48、5.57、6.20；未测的其他组合不能据此标通过。

浏览器工具拒绝 file:// 本地页面，原因是协议安全策略；未通过改地址、服务或其他浏览器绕过。故预览的浏览器渲染、实际交互、响应式几何和焦点/读屏尚未验证，静态检查不等同视觉验收。原型只演示合成会话，按钮无真实代理、文件或网络副作用。

本轮只修改设计/索引/状态/验证文档，未修改 src、CMake、运行依赖和已有 U08 未提交代码。原始规格不改，未把新方案强写成已实施 ADR。产品构建/完整回归未重跑，因为没有产品实现变化；后续先做核心组件样片和选型门禁，再按系统设计顺序固化接口及实施。

## U08 第二版：按 GPT 原型逐项照做（2026-11-02）

计划、验收点与逐片结果见 [U08 工作计划 §7](ui-u08-plan.md)。触发原因是用户对第一版的评价「现在非常难用」与指令「原型怎么做你怎么做」：GPT 原型自本日起是直接规格，第一版 §3 的取舍作废。K0–K9 九片均先在该文件补验收点再改代码，每片后重建并跑定向用例。

### 实施内容

| 片 | 落地内容 |
|---|---|
| K0 token | `UiTheme` 按 §7.3 补齐分级常量与原型暗色取值；浅色 `Brand=#0C5FBF`、`Primary/Focus=#0F63C8`（`OnPrimary` 对 `Primary` 5.77:1，`Brand` 对侧栏底 ≥4.5）；`check_ui_tokens.py` → `0 hits` |
| K1 顶栏与侧栏 | `QWidget#topbar` 固定 72px：`topbarTitle`/`topbarSubtitle` 随路由、`topbarStatus` 胶囊、`topbarTheme`、`topbarMore`；侧栏 4 项 43px。视图编号收敛为 `ViewIds.cpp` 一处（`Home=0`…`Settings=4`），旧编号只在 `fromLegacy()` 解释 |
| K2 路由方式 | 行点击弹 `QMenu`（规则模式/全局模式/直连模式，当前项带勾），`modeControl` 行内分段删除 |
| K3 节点详情 | `WindowParts::DetailsDrawer`：无边框 `Qt::Tool`，宽 410，贴主窗右侧等高；自动组无「切换到此节点」 |
| K4 节点标题行 | 删除 `#nodeSubscriptionRow`；订阅下拉选中即 `activateProfile`；测速、排序拆成两个图标钮各带浮层菜单 |
| K5 首页构图 | 删除 `homeDetailsDisclosure`/`homeMetrics`/`homeOnboarding`/曲线/出口摘要/探测间隔/代理总开关按钮；顺序固定为控制卡 → 节点标题 → 节点列表 → `homeInlineStat` → 提示 |
| K6 数据页 | `QTabBar#dataTabs`（流量/连接/测试/日志）+ `QStackedWidget#dataPanels`；可见性按当前页签 OR 合成；旧编号 5/7 经 `fromLegacyPage()` 落到对应页签 |
| K7 配置页 | `QListView#remoteProfileList`/`#localProfileList`，行高 76，`ProfileRowModel` 按 `sourceType` 分流；行尾 › / 双击打开 410px `profileDrawer` |
| K8 设置页 | 删除分类页签，改为「通用 / 网络 / 内核与维护」三张 `settingsGroup` 卡片；`ensureSettingsGroupVisible()` 按真实几何定位 |
| K9 测试改写 | 见下「测试改写」与「实施中修掉的缺陷」 |

### 测试改写（不删断言，改写为断言新结构）

| 原断言 | 改写为 |
|---|---|
| `homeDetailsDisclosure` 可见、首页曲线可读 | 三个已删控件 `findChild(...)` 为空；指标块在 `setPage(4)` 下 `!isHidden()`，回 `setPage(0)` 后 `currentRoute().view == Home` |
| `homeProbeInterval` 下拉存在 | 该控件为空；`setHomeVisible(true)` 后 `homeProbe()->result()` 含 `delay` |
| 首页 `homeOutboundSummary` | 同名控件在流量页签内；经 `setPage(4)` 打开诊断，断言摘要高度 > 0 |
| 侧栏 `setCurrentRow(3)` 生效 | 逐行对照 `currentRoute().view == item(n)->data(Qt::UserRole)`；第 2 行 → `ViewIds::Data`，第 3 行 → `ViewIds::Settings` 且 `page()==6` |
| 旧编号 2 = 首页节点 | `navigate(Route{2})` → `ViewIds::Rules` / `page()==3`；只有 `setPage(2)` 回到首页 + `HomeAnchor::Nodes` |
| 启动页/托盘路由期望 2 | 改为 0（首页 Control）；节点深链单独断言 `HomeAnchor::Nodes` |
| 无订阅时 `homeControlCard` 隐藏、整卡空态 | 卡片恒可见，三行分别显示「尚未连接 / 规则模式 / 尚未测试」；`overviewTitle` 文本为「尚未连接」 |
| 首页空态「前往首页启动」（`homeNextStep`） | 该控件为空；导入入口改用配置页 `addSubscription` 打开 `ImportDialog` |
| 路由方式行内分段按钮（`property("mode")`） | 该控件为空；`homeRouteRow` 整行点击，鼠标焦点帧 ≠ Tab 焦点帧 |
| 指标块在首页、直接断言 `metrics->isVisible()` | 指标块在「数据 › 流量」：隐藏窗口下先 `navigate(Route{Data,…,(int)DataTab::Traffic})` 断言 `!isHidden()`，`show()` 后 `isVisible()`，回首页不可见。注意旧编号 `setPage(4)` 落在**连接**页签 |
| `profilesTable` 行操作「策略」「分享二维码」 | 鼠标点击 `remoteProfileList` 行开抽屉 → `profileDetailsMore` → 菜单项 `profilePolicyButton` / `shareProfileQr`（`selectionModel()->select()` 不触发 `clicked`） |
| 首页探测组由组卡片设置 | K5 删掉组卡片后 `homeProbeGroup_` 无来源；测试显式 `setHomeProbeGroup("Test")`，选路语义不变 |

### 实施中修掉的缺陷

| 缺陷 | 根因 | 证据 |
|---|---|---|
| 侧栏「数据」打开规则页、「设置」打开数据页 | 侧栏条目存视图 ID（0/1/3/4），点击处理交给 `setPage()` 当旧编号解释（旧 3=规则、旧 4=数据·连接） | `WebsiteTests` 逐行导航断言；修复前 `navigation->setCurrentRow(3)` 无反应、行停在 2 |
| 规则视图被吞成首页 | `navigate()` 与 `PageRouter::go()` 各留一次 `view==2 → 首页` 的历史归一化 | `coherentActionsAndBilingualRuleTokens` 修复前落在首页 |
| 设置深链不滚动 / 标题丢分类 | `ensureWidgetVisible()` 向下多滚半个视口且离屏几何未更新；`Route{Settings}` 的 `kNoCategory` 覆盖当前分类 | 修复前 `max=1538 val=0`；分类 `Actual -1 Expected 2` |
| 本地订阅列表右键无反应 / 菜单挡住对话框 | 两张列表共用对象名，`QMenu::popup()` 以模态接管本轮 | 修复前同一 `findChild` 恒返回远端菜单 |
| 浅色对比度不足 | `Brand on Sidebar` 4.47、`OnPrimary on Primary` < 4.5 | 对比度用例，改色后全部达标 |
| 深色主按钮白字只有 3.55:1 | K0 把原型 `--blue #2488f5` 同时用作 `Primary`，原型本身的白字底就不足 4.5:1 | `semanticContrastAndArtwork` 深色分支失败；`Primary` 深色改 `#1968c2`（5.53:1），`Brand` 保留原型的链接蓝 |
| 控制区第 2 行没有键盘焦点环 | 行样式按 `objectName=homeControlRow` 选中，K2 把该行改成按钮后换了名字，QSS 落空且失去行背景 | `websiteResponseCards…` 的鼠标/键盘焦点对比断言；改为按 `controlRow` 属性选中，并补一条行级焦点环 |
| 样式表整段静默失效 | 在 `UiTheme` 的 QSS 里写了 `//` 注释；Qt 只认 `/* */`，之后的规则被丢弃且不报编译/运行错误（键盘焦点环因此一直不出现） | 改回 `/* */`；已复查 `UiTheme.cpp` 的样式串内无 `//` 注释残留 |
| 非数据页路由带上数据页签 | `fromLegacyPage()` 把 `DataTab::Traffic` 塞进首页/规则/设置的 `Route.tab` | 同一条路由各字段不等；`toLegacyPage()` 往返变数据页编号。改为仅数据页写入 |

### 自动测试与仓库检查

完整 `ctest --preset windows-mingw`：调整限时前 **39 通过、2 失败、1 跳过（42 项，586.57s）**；把 `appearance_tests` 限时放宽到 100s 后重跑完整整轮，**40 通过、1 失败、1 跳过（588.53s）**：

| 用例 | 调整前 | 调整后 | 处理 |
|---|---|---|---|
| `core_update_tests` | Timeout 60.09s（60s 限） | Timeout 60.09s | 既有；第一版基线即超时（73.5s），本机下载/校验慢，限时未改 |
| `appearance_tests` | Timeout 45.09s（45s 限） | **Passed 64.55s** | 45s 从第一版起就低于实测耗时（基线 57.3s）。经用户同意放宽到 100s；单跑 64.39s、整轮 64.55s 均通过 |
| `system_test_guard` | Skipped | Skipped | 设计如此（默认不改宿主系统代理） |
| 其余 39 项 | Passed | Passed | 含本轮改写的 `ui_tests`（43）、`workflow_depth_tests`（16）、`website_tests`（10）、`home_metrics_tests`（5）；`appearance_prefs_tests` 整轮 40.6s 通过 |

`ui_refinement_tests`（非 Windows 分组，跑同一份 `AppearanceTests.cpp` 的子集）限时仍为 45s，Windows 上不注册；若将来在 Linux/macOS 上运行需一并放宽，此处未改。

定向复跑（`QT_QPA_PLATFORM=offscreen`，单进程）：`appearance_tests` 11 passed / 64.4s（放宽限时后经 ctest 复跑亦为 Passed / 64.39s）、`ui_tests` 43 passed / 86.0s、`workflow_depth_tests` 16 passed / 18.2s、`website_tests` 10 passed / 14.4s、`home_probe_tests` 5 passed、`outbound_tests` 6 passed、`home_metrics_tests` 5 passed（连跑 3 次一致）、`appearance_prefs_tests` 6 passed / 40.7s。K9 过程中曾被判为 SEGFAULT 的三处，根因都是测试引用已删控件（空指针解引用），不是产品崩溃。

仓库检查：`check_ui_tokens.py` → `0 hits`；`check_docs.py` → 0 findings；`check_document_privacy.py` → 0 findings。

### 未验

真实 Windows 实机观感（鼠标悬停、键盘焦点顺序、Alt+Tab 与任务栏）、读屏播报、高对比与 125%/150% 文字缩放下的人工复核、原型在浏览器中的逐屏人工比对。`release_qualified` 仍为 `false`。

路由模型的实质变更（视图收敛为 5 个、数据页四页签、设置三组平铺）另记于 [ADR 0049](adr/0049-view-ids-and-data-tabs.md)。

## U08 第一版：首页对齐 GPT 原型（2026-10-31）

计划、取舍与分片设计见 [U08 工作计划](ui-u08-plan.md)（H1–H4 均先在该文件记录设计再改代码）。参照物是 `prototypes/Mlash_Shadowrocket_Style_Interactive_Prototype_from_chatgpt.html`；原型不是验收依据，`ui-layered-design.md` 与 ADR 0048 的裁决优先。

### 实施内容

| 片 | 落地内容 |
|---|---|
| H1 三行控制区 | `QFrame#homeControlCard` 内三行：① 当前代理（状态点 + 成员名 + 「订阅 · 选择方式」 + 状态句，行尾 `ProxySwitch#systemProxy`）；② 路由方式（标题 + 说明 + 行尾当前模式与 ›，点开才显示原 `modeControl` 分段）；③ 连通性测试（标题 + 上次结果 / 测试目标 + 「开始测试」 + 进入网站测试的 ›）。首页 TUN 开关与「TUN 设置…」按 `ui-layered-design.md:169` 删除 |
| H2 删除六卡 | 六张 `dashboardCard`、`HomeSummaryDialog.{h,cpp}`、`Settings::homeSummaryMask`、`AppController::updateHomeSummary`、`home_summary_mask` 持久化字段全部移除；`ProfileStore`/`Storage` 读入旧状态时忽略该键 |
| H3 节点行列表 | `NodeRowModel`（组头/成员两类行，`RowKind`/`IsAutoGroup`/`RegionCode`/`LatencyText`/`Subtitle` 等角色）与 `NodeRowDelegate`（组头 34px、成员行 46px，区域徽标 / 名称 / 协议副标题 / ✓ / 延迟 / ⓘ）；`QListView#nodeList` 与 `nodeCards_` 组成 `QStackedWidget#nodeViews`，四列表格页删除；标题行收纳订阅下拉、搜索、测速与「更多」溢出菜单 |
| H4 指标收敛 | 首屏只有 `homeInlineStat` 一行（内核 · 模式 · ↓/↑速率 · 连接数 + 「数据详情 ›」）；「更多运行信息」抽屉补齐运行版本、内核状态与版本、接管明细、会话速率与峰值、最近启动、内核单独启停与代理总开关；无订阅时抽屉仍可见 |

### 实施中修掉的 6 个缺陷

| 缺陷 | 根因 | 证据 |
|---|---|---|
| 首页段错误 | `coreLabel_` 构造在 H1 被删，`updateState()` 仍写 `coreLabel_->setText()` | gdb 栈落在 `QLabel::setText` |
| 首页段错误 | `cancelTestAction_` 漏建且无 `= nullptr`，实际值 `0xbaadf00d` | gdb 显示 `QAction::setEnabled` 的 `rcx = 0xbaadf00dbaadf00d` |
| 构造期段错误 | `updateActions()` 由 `updateState()` 在页面构造途中触发，页面指针尚为空 | `saveRules_`/`rulesEditor_`/`nodeList_`/`connectionsTable_` 前置守卫 + 逐条空指针保护 |
| 自动组成员可被回车固化 | `chooseSelectedNode()` 不校验组类型 | 与单击共用 `commitMemberSelection()`，仅 `Selector` 提交 |
| 行列表渲染 O(n²) | `NodeRowModel::data()` 每次按名字线性查源行 | `members_` 与源行同序，直接用 `row.member` 作源行号 |
| 标题行控件越界（x=-142） | `WrapLayout` 没有 spacer/拉伸语义，只增长第一个 `Expanding` 项 | 删 `addStretch`，改由搜索框承担拉伸 |

其中「行列表渲染 O(n²)」与「`WrapLayout` 越界」都是本轮**自己引入又修掉**的，未进入最终代码。

### 自动测试

完整 `ctest --test-dir build/windows-mingw -j4`：**42 项中 38 通过、0 失败、1 跳过（`system_test_guard`）、3 超时**，总耗时 164.48s。三个超时用例逐条复核如下。

串行复跑（`ctest -R "appearance_prefs_tests|core_update_tests|appearance_tests"`，无并发）：

| 用例 | 限时 | 串行结果 | 判定 |
|---|---|---|---|
| `appearance_prefs_tests` | 45s | **32.79s 通过** | `-j4` 并行下的资源争用，非回归；单独运行时 35s 也通过 |
| `core_update_tests` | 60s | 60.10s 超时 | 既有问题 |
| `appearance_tests` | 45s | 45.14s 超时 | 既有问题 |

后两者经基线复跑确认为本机环境问题，与本轮无关：

| 用例 | 限时 | 基线（改动全部暂存后重建） | 本轮改动后 |
|---|---|---|---|
| `appearance_tests` | 45s | 57.3s 超时 | 49.4s 超时 |
| `core_update_tests` | 60s | 73.5s 超时 | 73.5s 超时 |

两者在本轮之前就已超限；本轮改动使 `appearance_tests` 略快。本轮不调限时——改限时属于降低证据强度，且本机性能不能代表干净机器。串行复跑下除这两个用例外全部通过，因此记为「2 项本机超时 + 1 项并行争用」。

仓库检查：`check_ui_tokens.py` → `0 hits`；`check_docs.py` → 98 份 Markdown 0 findings；`check_document_privacy.py` → 105 份 0 findings。

### 规范化入口与打包

| 入口 | 结果 |
|---|---|
| `scripts/build.ps1` | `ninja: no work to do`（树已是最新） |
| `scripts/test.ps1`（串行） | **38 通过、2 超时、1 跳过**（`system_test_guard`），总耗时 558.57s；`appearance_prefs_tests` 在串行下通过，进一步印证 `-j4` 那次超时是并行争用 |
| `scripts/package.ps1` | 产出 `dist/Mlash-1.17.1-windows-x64-portable/` 与同名 ZIP；脚本自校验 34 个文件通过，ZIP SHA256 `7C440D82…95568FEB` |
| 打包产物冒烟 | `dist/Mlash-1.17.1-windows-x64-portable/Mlash.exe --version` → `Mlash 1.17.1`；`--smoke-test` 退出码 0，产出 8 张 1410×930 截图（`build/u08pkg/`），空态首页与开发构建一致 |

打包时发现一个**环境问题**（不是本轮代码问题）：`scripts/package.ps1` 调用了 `Get-FileHash`，Windows PowerShell 5.1 在本机报 `CommandNotFoundException` 直接中止。改用本机已安装的 PowerShell 7（`pwsh`）后同一脚本完整通过。若后续要求在 5.1 上跑打包，需要脚本侧加兼容实现或在 `build.md` 里写明最低版本；本轮未改脚本。

### 截图复核

`Mlash.exe --smoke-test --data-dir build/u08shots/data --screenshots-dir build/u08shots`（退出码 0）产出 8 张 1410×930 截图；另以 `MLASH_UI_SCREENSHOTS` 从 `mlash_ui_tests currentGroupSelectionAndNativePreview` 取得填充态首页 940×620，并按**原生比例**切成独立条带逐块查看（不缩放，避免重演 2026-10-30 那次「把缩小图误读成塌缩」的错误）。

已核对：控制区三行与卡片圆角连续、无非常驻控件溢出；节点区标题行四项工具同排且互不重叠；组头与成员行高度可分；inline-stat 只在有活跃订阅且内核就绪时出现。

### 未验

真实 Windows 实机的观感（含鼠标悬停、键盘焦点顺序、Alt+Tab 与任务栏）、读屏播报、高对比与 125%/150% 文字缩放下的人工复核、原型在浏览器中的逐屏人工比对。`release_qualified` 仍为 `false`。

## R2 标题槽（2026-10-29）

实施前设计与实施后结果见 [R2 施工记录](ui-r2-title-slot.md)。要点：页面自绘标题全部删除，`replaceWidget` 与 `sectionTitle_` 移除；页签宿主固定为内容区第一行；窗口标题由 `routeTitle()` 随路由更新；`navigationIcon` 收敛为 4 个 `NavKind`。新增 `titleSlotIsSingleAndWindowTitleFollowsRoute` 覆盖「无 pageTitle / 首页无页签 / 标题随 8 视图与 3 分类变化 / 五视图页签几何一致 / 数据分区 3 页签」。完整 `ctest`：33 通过、0 失败、1 跳过。截图复核见施工记录第 5 节。未验：真实 Windows 实机的 Alt+Tab 与任务栏标题、人工读屏。

## R2.5 侧栏状态条（2026-10-29）

实施前设计与实施后结果见 [R2.5 施工记录](ui-r2-5-sidebar-status.md)。要点：新增 `statusSentence()` 纯函数产出四段文案（内核 / 接管 / TUN / 当前订阅）与状态点角色；侧栏底部常驻状态条单击回首页控制区；托盘 tooltip 前两行与侧栏逐字相同，第三行保留内核模式。`statusTooltip` 扩为 `(state, settings, profileName)`，两处旧测试补实参。新增 `sidebarStatusIsPermanentAndSharesTrayWording` 覆盖六状态文案、八视图可见性、侧栏/托盘一致性、点击回首页与 840×560 截图。完整 `ctest`：33 通过、0 失败、1 跳过。未验：真实托盘 tooltip 显示、人工读屏、120% 文字换行。

## R5 / R6 / R9 实施（2026-10-30）

- **R5 十态**（[记录](ui-r5-states.md)）：内核实际规则区分未就绪/读取中/失败（带重试）/无匹配/已就绪但为零，失败不再清空表格；日志页新增空态说明，区分未就绪/无匹配/已暂停/没有日志；连接刷新失败保留上一份快照并标注「刷新失败 · 显示上次快照」，`connectionsCurrent()` 收紧为「最近一次刷新成功」。新增 `ConnectionViewTests::logAndRuleRegionsDistinguishStates`。
- **R6 工具窗口**（[记录](ui-r6-tool-windows.md)）：`DialogDraftGuard` 只拦 Esc 与取消按钮，窗口关闭静默丢弃草稿；长流程关窗不中止任务由 `BackupTests::closingTheWindowDoesNotAbortTheTask` 固定；修正 `协议关联` 标题。
- **R9 Token**（[记录](ui-r9-tokens.md)）：新增 `scripts/check_ui_tokens.py`，改动前跑出 6 处命中，迁移后 `0 hits`；行为不变。

完整 `ctest`：34 项中 33 通过、0 失败、`system_test_guard` 跳过。未验：真实内核下的连接/规则刷新、读屏播报、高对比与缩放观感、21 个对话框的人工视觉复核。

## R9 收尾：高对比第四套取值与图标绘制器合并（2026-10-30）

设计与复算表见 [R9 施工记录](ui-r9-tokens.md#13-高对比第四套取值)。要点：

- 高对比从「只改边框」改为**独立的第四套并行取值**（浅/深 × 普通/高对比），同时改写表面分级、选中底、开关轨道与焦点环。9 对按 WCAG 2.x 相对亮度脚本实算全部达标：HC 浅主文字/表面 21.00、焦点环/底色 5.69、开关轨道/表面 8.86；HC 深主文字/表面 19.03、焦点环/底色 14.88。
- `semanticContrastAndArtwork` 由「4 套外观 × 7 前景 × 3 背景」扩展为同时断言焦点环、悬停底，并在 HC 下额外断言边框、轨道与选中底。
- `navigationIcon(NavKind)` 改调 `UiIcons::icon()`，`UiIcons` 新增 `Home`/`Chart`/`List` 字形，两套绘制器收敛为一套。
- 完整 `ctest`：34 项中 33 通过、0 失败、`system_test_guard` 跳过；`python scripts/check_ui_tokens.py` → `0 hits`。

未验：HC 与 125%/150% 文字缩放下的人工视觉复核、读屏播报、真实 Windows 实机观感。

## R10 反馈路由（2026-10-30）

设计与实施中修正的六处细节见 [R10 施工记录](ui-r10-feedback-routing.md)。要点：

- `OperationId` 以默认参数注入 `operationStarted`/`operationFinished`，99 处既有 emit 保持可编译；`OperationScope` 把标识带进事务与拒绝路径，`finish()` 随结果发出。
- `feedbackTarget()` 是纯函数归属表：订阅→1、首页→0、连接→4、设置→6、规则→3，其余（内核、备份、恢复、批量之外的跨页任务）走状态栏「最近操作」段。
- 状态栏拆「最近操作 / 当前告警」两段：`state.lastError` 不再覆盖刚发生的操作结果（N10）。
- 托盘失败气泡只承载没有页面可用的失败；`refreshRules` 的重复 emit 已删除（同一事实只留页面空态）。

新增 `UiTests::operationResultsGoToTheirOwnPage` 与 `feedbackTargetsStayExplicit`（ctest 名 `feedback_routing_tests`）。实施中被测试抓到三处真实缺陷：重复关闭代理被当成失败、`settingsFeedback_` 成功文案压住「有 n 项需要重启的修改」、首页控制区提示随 `overview` 面板一起隐藏。

完整 `ctest`：**35 项中 34 通过、0 失败、`system_test_guard` 跳过**，448 秒；`check_ui_tokens.py` `0 hits`；文档链接与隐私检查 0 findings。未验：真实 Windows 托盘气泡观感、读屏两段播报顺序、21 个对话框内部 hint 的去重。

## U07 原型补全与视觉结构对齐（2026-10-30）

计划与分片见 [U07 工作计划](ui-u07-plan.md)。九片全部实施，每片一次完整 ctest：

| 片 | 要点 | 新增测试 |
|---|---|---|
| P1/P2 原型 | 新增 08–11 四屏；补齐原型引用却从未定义的基础类；用量条与菜单标题 | —（原型不是验收依据） |
| S1 生命周期 | `PageRouter`/`PageLifecycle`；`navigate()` 只剩顺序 | `page_lifecycle_tests` |
| S2 窗口注册表 | `ToolWindowRegistry`；同键 raise；订阅删除关闭对应窗口 | `tool_window_tests` |
| V1 形状编码 | `StatusShape` 五态五形状 + `SkeletonBlock` | `status_shape_tests` |
| V2 用量条 | `UsageDelegate`；无额度整列隐藏；状态列后移 | `profile_usage_tests` |
| V3 行内当前值 | `settingRow` 可选 `currentValue` | `settings_value_tests` |
| V4 菜单标题 | `menuSection()`；五组标题不可触发 | `menu_section_tests` |
| V5 控制区 | 卡片协议标签、组选路说明、控制区失败原因 | `home_control_tests` |

**实施中修掉的两个真实缺陷**：

1. **越界写（R10 引入）**：`routeOperationFeedback` 的 `ProxyToggle` 分支直接写 `pageFeedback_[0]`，
   而列表此时可能为空。`QList::operator[]` 越界是未定义行为，实际表现为段错误。
   旧测试恰好先走订阅页路径把列表撑开，因此一直没暴露；V5 的测试是第一条先进首页路径的用例。
2. **设置行提示压住重启提示**：V3 之前 `settingFeedback_` 在成功时写成「设置已应用」，
   会一直盖住「有 n 项需要重启内核的修改」。改为成功后清空。

**另有一处真实缺陷在 S2 之前就存在**：原型 01/03/06b 三屏引用的 `control-*`、`node-*`、
`home-drawer` 等类在 CSS 里从未定义，浏览器中首页整块退化为纯文本堆叠。P1 已补齐。

### 事后核实（2026-10-30）

按「声称已实施」逐条回查代码，发现两处**声明与实现不符**，已在本轮补上：

| 项 | 核实前的真实状态 | 处理 |
|---|---|---|
| `SkeletonBlock` | 类已建、有单测，但**没有任何界面实例化它**——文档写「已完成」属过度声明 | 接到「内核实际规则」的读取中状态，新增 `rulesTableHasSkeletonWired` 断言它挂在表格视口内且未就绪时收起 |
| V3 行内当前值 | 更新循环里列了「启动页面 / 托盘单击」，但这两行从未创建 `settingValue` 标签，是死代码 | 从循环移除；下拉本身已显示值，再挂标签就是同一事实两处 |

以下经核实与声明一致：`navigate()` 已无页面副作用分支（仅剩视图 2 归一化与设置分类两处路由逻辑）；13 个工具窗口全部走 `ToolWindowRegistry`，无残留 `findChild` 复用；R4/R6/R8/R9/R10 的验收点在代码中可查。

最终完整 `ctest`：**42 项中 41 通过、0 失败、`system_test_guard` 跳过**，442 秒；
`check_ui_tokens.py` `0 hits`；文档 98 个文件 0 findings。
未验：真实 Windows 实机观感、读屏播报、HC 与 125%/150% 缩放下的人工复核、原型在浏览器中的逐屏人工比对。

### 冒烟截图复查与一次误判的更正（2026-10-30）

按用户要求在默认窗口尺寸下逐屏看冒烟截图（`Mlash.exe --smoke-test --screenshots-dir`，940×620）。
**本轮曾误判「规则页与连接页的工具栏被压成竖排、导航缺失」，并据此改动 `src/main.cpp` 的抓图逻辑
（`grab()` → `render()`、手动泵 `LayoutRequest`）与新增调试断言。复查后确认该判断错误**：
所谓「塌缩」是把 1410×930 的原生截图缩进窄视口查看时的视觉错觉，逐行像素扫描显示各内容带连续填充、
几何正常，`pageToolbarsStayHorizontal` 也从未失败。那批改动的理由不成立，已整体回退到原 `grab()` 实现，
只保留一条说明切页间隔的注释。

复查结论（实测）：

- `src/main.cpp` 的 `render()` 改写会让冒烟截图只剩侧栏（同机实测 `build/smoke10/` 为空白内容），
  回退后 `build/smoke11/` 八页均正常，尺寸 940×620。
- 逐页看图：首页（控制区 + 引导卡）、订阅、规则、连接、日志、设置、网站测试均无结构性缺陷；
  首页在当前空态下没有此前提到的「大段空白带」。
- `tests/UiTests.cpp` 的 `pageToolbarsStayHorizontal` 去掉 `qInfo` 打印与临时 `resize(1128,744)` 脚手架，
  改为对连接页与日志页工具条的宽度、高度（未换行）与搜索框宽度的实际断言，单独运行通过。

因此本轮**没有发现需要修改的 UI 缺陷**，不新增 UI 切片；`check_ui_tokens.py` 仍为 `0 hits`。

本轮最终完整 `ctest`（`scripts/test.ps1`）：**42 项中 41 通过、0 失败、`system_test_guard` 跳过**，437.28 秒；
`check_ui_tokens.py` `0 hits`；`check_docs.py` 98 个 Markdown 链接/导航 0 findings；文档隐私检查 105 份 0 findings。
未验：真实 Windows 实机观感、读屏播报、HC 与 125%/150% 缩放下的人工复核。

## U06 产物打包（2026-10-30，非发行资格）

在 `feature/r3-proxy-switch` 分支上按仓库入口实际执行：

1. `scripts/build.ps1` → `ninja: no work to do`（工作树与已构建产物一致）。
2. `scripts/test.ps1` → **34 项中 33 通过、0 失败、`system_test_guard` 跳过**，总耗时 403 秒。
3. `scripts/package.ps1 -OutputName Mlash-1.17.1-windows-x64-u06` → portable 目录 + ZIP + 清单 + SHA256。

实际产物：

| 项 | 值 |
|---|---|
| 包名 | `Mlash-1.17.1-windows-x64-u06` |
| ZIP | `dist/Mlash-1.17.1-windows-x64-u06.zip`，41,240,954 字节 |
| ZIP SHA256 | `5f8d51435f6d415a9e3d6c11e84710ec935a42c1d46de2240e6b2949af375957` |
| `Mlash.exe --version` | `Mlash 1.17.1`，退出码 0 |
| PE 版本 | `Mlash.exe` 与 `MlashTunService.exe` 均与 CLI 版本一致（脚本内断言） |
| 载荷 | 33 个文件入清单，加 `package-manifest.json` 共 34；白名单与完整性复核通过 |
| `release_qualified` | `false` — 未走 `-Release` 证据门禁 |

打包过程中发现并绕开的两点，均与环境而非代码有关：

- PowerShell 5.1 在该 PATH 下无法自动加载 `Microsoft.PowerShell.Utility`，`Get-FileHash` 报「无法识别」，改用 `pwsh` 7 执行同一脚本即可。
- 默认包名 `Mlash-1.17.1-windows-x64-portable` 下有一个 11:47 启动的实例仍在运行（用户自用），脚本按设计拒绝覆盖，故改用新 `OutputName`。

**这是未取得发行资格的候选包**：未做 Windows 10 实机、干净机器部署、72 小时稳定性或性能预算验证，也未运行 `-Release` 的证据门禁；`dist/` 不入库。

### R10 之后的重新打包（2026-10-30）

R10 提交后重新执行 `scripts/package.ps1`。原包名目录下有一个 13:38 启动的实例在运行（用户自用），脚本按设计拒绝覆盖，故用新包名：

| 项 | 值 |
|---|---|
| 包名 | `Mlash-1.17.1-windows-x64-u06-r10` |
| ZIP | 41,248,406 字节 |
| SHA256 | `19bc5e4a9e5495cbeb3407c67ea387363303acde77605f33b0116574efe4a480` |
| `Mlash.exe --version` | `Mlash 1.17.1` |
| 载荷 | 34 个文件，白名单与完整性复核通过 |
| `release_qualified` | `false` |

同为未取得发行资格的候选包，边界同上。

## R8 托盘六组重排（2026-10-29）

设计与结果见 [R8 施工记录](ui-r8-tray.md)。托盘顶层按跨对象六组重排，删除「更多页面」与独立「节点」入口；单击切换与菜单总开关统一走 `setProxyEnabled()` 并复用首页的外部代理确认。

修正：重写时丢失「重测本组延迟」的强制重测参数（`testTrayGroup(name,true)`），导致该用例由绿转红，已恢复；并给使用系统剪贴板的三个套件加 `RESOURCE_LOCK system_clipboard`，解决 `ctest -j4` 并行互相覆盖。完整 `ctest`：33 通过、0 失败、1 跳过。未验：真实托盘系统集成与缩放下的观感。

## R7 空态组件（2026-10-29）

设计与一次失败尝试的记录见 [R7 施工记录](ui-r7-empty-state.md)。`emptyState()` 保留原签名，在说明标签下追加可聚焦动作按钮；订阅、节点、连接、规则四处按当前原因给出下一步动作（添加订阅 / 清除搜索或筛选 / 查看日志 / 启动内核 / 重试）。

新增 `emptyStatesOfferFocusableNextStep` 覆盖订阅页与连接页的文案、`StrongFocus` 与点击结果。完整 `ctest`：33 通过、0 失败、1 跳过。未验：读屏焦点顺序与空态图标视觉。

## R3b 首页连通性行（2026-10-29）

设计与结果见 [R3b 施工记录](ui-r3b-connectivity.md)。新增 `ConnectivityProbe`：不受测速页可见性约束的单次只读延迟请求，结果带状态、延迟、节点、地址与时间戳，超 10 分钟标记过期。首页控制区第三行显示「连通性 · 结果 · 测试 · 详情」。

测试 `mlash_home_probe_tests connectivityProbeRunsWithoutWebsitePageAndReportsFreshness` 覆盖未就绪拒绝且零请求、首页未显示仍可测、成功/失败/取消、节点选择不被改写。完整 `ctest`：33 通过、0 失败、1 跳过。未验：真实内核下的实测与人工读屏。

## R4 订阅菜单与首页单击即生效（2026-10-29）

实施前设计与实际结果见 [R4 施工记录](ui-r4-subscription-nodes.md)。订阅管理菜单按 5.6b 重排为只读头 + 常用动作 / 编辑配置 ▸ / 对象维护 / 文件与目录 / 危险动作；首页删除「切换节点」提交条，`selectNodeInHome()` 仅在手动策略组写入。

实现中发现并修复：用 `QMenu::clear()` 重排会 delete 成员持有的 action（只读头、回滚、删除等），造成三个套件同时段错误；改为逐个 `removeAction` 后按序 `addAction`。`ui_interactions` 的节点交互测试按新语义改写。完整 `ctest`：33 通过、0 失败、1 跳过，112 秒。未验：真实鼠标/键盘下的菜单观感与读屏顺序。

## R3 首页合并与代理总开关（2026-10-29）

实施前设计、实现与偏差见 [R3 施工记录](ui-r3-home-merge.md)。要点：新增 `AppController::setProxyEnabled(bool)`（ADR 0048 的唯一 Controller 例外）；`proxyPage()` 改为节点区并入首页滚动视图，页面栈由 8 页降为 7 页并引入 `MainWindow::stackIndex(int)` 做视图 ID → 栈索引映射；首页主控件为代理总开关。

自动测试：`proxyMasterSwitchRollsBackAndKeepsCoreRunning`（接管/释放/幂等/注入失败回滚，全程注入平台替身，`autoWrites==0`）、`homeOwnsNodeSectionInOneScrollView`（7 页栈、首页含节点区、ID 映射、入口 2 回首页）；五处几何前提按合并后的结构改写。完整 `ctest`：33 通过、0 失败、1 跳过，107 秒。未验：真实 TUN/系统代理切换、人工读屏、性能与稳定性预算。

## 本轮：R1 路由切片（2026-10-29）

按 [分层设计实施切片](ui-layered-design.md#实施切片) 的 R1 落地路由模型：新增 `Route{view, category, anchor}`、`fromLegacyPage` / `toLegacyPage` / `isRoutableView`；`MainWindow::navigate(Route)` 成为唯一切换入口，`setPage(int)` 保留为历史重载并统一经 `fromLegacyPage`；`GuiHost` 保存 `Route` 而非整数页号，轻量重建后首页锚点一并恢复；托盘新增 `showRoute`，设置搜索新增 `routeRequested`，两者都不再以整数 2 表达「首页 › 节点」。设计与 ADR 0048 同步修正了 R1/R3 的边界：**索引 2 的节点控件删除并入 R3**，R1 只做路由归一化，避免两片之间「换节点」不可用。

构建：Windows 本机 Qt 6.10.2 + MinGW 13.1.0，`windows-mingw` 预设全目标链接成功，无错误。

自动测试（实际执行）：
- 新增 `mlash_ui_tests routesCarryHomeAnchorAndSettingsCategory`：入口 ID ↔ 路由双向转换（0↔0、2↔2、6→常规分类）；`isRoutableView` 拒绝 `view=2/8/-1` 等非法组合；侧栏「首页」回显 0、显式节点入口回显 2 且 `currentRoute()` 为权威；设置分类可深链 `(6,2)`，未指定分类时沿用当前分类；`GuiHost.show(Route{0,-1,Nodes})` 重建后锚点仍在。
- 更新陈旧断言两处：`mlash_ui_tests desktopPreferencesAndEnvironmentCopy` 的托盘单击改为断言 `showRoute`（Node 锚点）而非整数 2；`mlash_appearance_tests taskEntryPointsAndSettingsStayCoherent` 的首页分区页签改为断言回显 0 且页面栈落在索引 0。
- 完整 `ctest --test-dir build/windows-mingw -j4`：34 项中 33 通过、0 失败、`system_test_guard` 按设计跳过，用时 114 秒。

顺带修正的实现缺陷：`showEvent` 原先用 `page()==0` 判断首页可见性，在节点入口下漏置；`applyHomeAnchor` 原先查找的 `nodeContent` 在 `homePage()` 执行时尚未由 `proxyPage()` 创建，恒为空指针——现已改为在节点视图内定位当前成员行，并注释说明 R3 合并后改为滚动首页。

未验：真实 Windows 实机交互、人工读屏、性能与发行资格。R2 起的分区页签收敛、R3 首页合并与代理总开关均未实施。

## 本轮：设置即时生效、内核历史与标题统一（2026-10-29）

按 [ADR 0044](adr/0044-immediate-settings-core-history.md) 与本轮工程审视先记录方案，再修改。设置页分类页签与搜索并入统一标题行，不再保留独立“设置”标题；偏好类设置在控件变化时立即提交，需要内核安全重启的项只保留显式动作；设置分组内 `CompactComboBox` 与新增 `CompactSpinBox` 在未聚焦且未展开时忽略滚轮；内核历史以 `core_history` 持久化（上限 8、按 sha256 去重、旧版在前），清理时保留在册副本并移除已删除记录。

构建：Windows 本机 Qt 6.10.2 + MinGW 13.1.0，`scripts/build.ps1`（windows-mingw 预设）配置与全部目标链接成功，复跑为 `ninja: no work to do`。首次构建发现新结构体的 const 捕获与 `QJsonValue::size` 误用，修正后零错误；保留既有 `AppearanceTests.cpp` 弃用警告。

自动测试（实际执行）：
- `mlash_core_update_tests` 全部通过，含新增 `coreHistoryKeepsBoundedSwitchableVersions`：应用后记录 sha256/版本/路径/来源；按记录路径再预检并切换回原版本；`appendedCoreHistory` 去重并保持 8 条上限与新旧顺序；状态文件往返持久化；非法 sha256 与重复摘要被存储校验拒绝；管理窗列出历史、标记当前版本、选择当前版本时拒绝重复预检。
- `mlash_ui_tests quietMouseFocusAndGroupedSettings` 通过，含新增滚轮检查：未聚焦的下拉与数字框接收滚轮后值不变，聚焦后滚轮按预期改变选项。
- `mlash_appearance_prefs_tests`、`mlash_appearance_tests`、`mlash_ui_tests` 专项、`core_update`、`official_core`、`backup`、`remote_backup`、`status_bar`、`release_tools` 等通过；`system_test_guard` 按设计跳过。
- 更新后的 `draftApplyFailureAndPersistence` 验证两次连续偏好提交各产生一次事务、重启类修改保持待应用且不被后续偏好提交吞掉。

首轮完整 `scripts/test.ps1`：31/34 通过。为区分来源，将本轮全部改动暂存后以同一工具链重建并复跑 `workflow_depth_tests`、`ui_components`、`node_links_tests`、`outbound_tests`、`website_tests`，失败用例与断言逐条一致，确认均为合入的 `c4d2c8c`（PR #2）遗留：测试仍指向已移除的设置行与旧八页导航索引、代理编辑作用域旧文案、首页摘要旧空态文案、首页指标按钮旧去向，以及 `proxyManagement` 等已移入菜单的控件。

随后修复：更新上述陈旧断言以匹配当前契约（出口诊断统一从网站测试页进入、首页预选需点“切换”、设置不再拥有 DNS 行）；修复两个真实缺陷——`ConnectionOptions::fitDefault` 在 840 DIP 下把名称列压到 134（改为先按紧凑下限压缩其余列、再按比例兜底），`SettingsSearch::locate` 点击结果后菜单不关闭（补 `menu_->hide()`）。修复后五组全部通过，再跑完整 `scripts/test.ps1`：33 通过、0 失败、1 系统门禁跳过（34 项中），用时 442 秒。

本地分发验证：`scripts/package.ps1` 的默认输出名改为跟随 CMake 版本后，生成 `Mlash-1.17.1-windows-x64-portable`，34 文件白名单与完整性校验通过，`Mlash.exe --version` 实际输出 1.17.1，ZIP SHA256 `1332bc2a62604976f14278f1895ca8ba5727cb0b5e7f826dc7dbfee414d914e2`，`release_qualified=false`。

发布：改动经 `feature/immediate-settings-core-history` 分支提交 `cb26fad` 并以 PR #3 合入 `main`（`50027ab`）；该 PR 的 CI 工作流通过。随后推送标签 `v1.17.1` 触发同一工作流，build/test/package/smoke/publish 全部成功，[GitHub 预发布](https://github.com/downdone/mlash/releases/tag/v1.17.1) 已生成，含 portable ZIP 与 SHA256、两份源码归档与 source.json、`package-verification.json` 共 8 个附件；经 API 复核标签版本匹配、`prerelease=true`、`draft=false`。CI 产物与本机包的 SHA256 不同属预期：两者各自独立构建，不以本机哈希替代 CI 证据。

未验：Windows 实机原生交互、人工读屏、真实内核与系统代理/TUN 写入、性能与发行资格。UI 几何只在离屏自动检查中覆盖，人工理解度与视觉复核未执行。

## 本轮：U03 功能组织与完整任务重审（2026-10-28）

按 [事前方案及逐页复核](ui-apple-review.md#u03-任务流程与信息架构重审) 先梳理首次使用、日常切换、配置维护和诊断恢复，再实现。用户追加 Shadowrocket 截图后，将八个并列入口收敛为首页/配置/数据/设置四分区，域内页签占原标题位置，保留原视图 ID 与启动页值。订阅配置和节点操作、规则维护及出口诊断各归所属页面；不复制参考截图中的私有节点/地址或未实现能力。当前交互、架构、使用指南和 ADR 0043 同步维护。

构建：使用固定 Qt 6.10.2 本机工具链，经 `scripts/build.ps1 -Preset portable` Release 编译及全部目标链接成功。最终测试源码同步后再次增量构建通过。编译仍有既有单元素初始化写法和测试激活窗口的弃用警告，无编译/链接错误。

第一轮 `scripts/test.ps1 -Preset portable` 筛选 ui_refinement_tests、home_metrics_tests、proxy_bypass_tests、connection_view_tests、service_tests，五组全部通过（33.35 秒）。四分区、自动组命名、导入高度及操作可用性收尾后，再以同一脚本复测 UI 专项八项，全部通过（15.94 秒）。该专项涵盖主题/下拉/草稿、八视图空态与填充、三尺寸、100/120% 文字、设置分类、工具窗口、状态错误、四分区路由/记忆、导入完成导航/选中、单/多选及规则/节点任务链。

新增行为证据：设置切分类/搜索/跨页保持草稿；订阅多选禁用单对象编辑；首页预选不写入，点击切换后才有一次 PUT，随后按单组 GET 回读并持久化。规则列表/文本同步，删除/排序/还原不误提交，选项可见；257 条原文完整保留，表格有界渲染且保存禁用。自动组显示固定与恢复自动选择的真实语义。未就绪/无对象时有下一步或明确禁用，不保留空点击入口。隔离平台确认系统写入为零；核心 API 为本地 HTTP 夹具。

AppearancePrefsTests 的草稿应用失败/持久化、首页摘要与重建、全部文字比例、新增工具小窗四项在最终组织下重新执行，全部通过（9.19 秒）。未运行该程序中依赖 Windows 核心的真实网站测试。

Cocoa 原生后端执行空态、填充及分组设置、工具窗口、错误/帮助和两项任务流程，共六项全部通过（25.21 秒）；取得 244 张隔离夹具截图，覆盖四分区八视图、三尺寸、浅深色、100/120% 文字及工具窗口。已复核八视图合览、分类与导入窗口。截图/模拟状态及自动按键不替代人工用户理解度、读屏或真实系统验收。

复核修正：导入各页原先共用最高页的最小高度，链接页名称和下载入口被推到首屏之外；改为当前来源决定高度，补充默认窗口下必要字段可见及跨来源切换检查，离屏/原生均通过。首轮切换夹具误把单组回读返回成全量代理对象，按实际 API 结构区分后显式切换与回读检查通过。测试脚本首次调用缺少 ctest 路径，使用既有工具链路径重跑上述五组通过。

原始构建、测试和截图位于 `build/ui-task-review/`，不纳入分发。文档工具四项通过；标准入口核验 77 份 Markdown 链接/导航与 84 份文档隐私，均零问题，git diff --check 通过。

未验：该分支 Windows 原生及完整真实核心回归、真实系统代理/TUN、人工前台/用户理解度/读屏、性能与发行资格。Controller 命令、持久化 schema 及 macOS Unsupported/退出恢复边界未扩展。当前 UI 修改未提交或推送。

## 此前：U02 界面信息减噪与任务重审（2026-10-28）

按 [事前设计与实施后复核](ui-apple-review.md#u02-信息减噪与任务重审)，清理八页及工具窗口的重复名称、教学段落、空闲反馈及实现细节。规则提示并入保存栏，日志跟随入口并入工具带，统一说明入口按需异步打开。失败、草稿、候选/运行版本、来源覆盖和密码/权限后果保留，Controller 命令及平台能力未改变。

构建：`scripts/build.ps1 -Preset portable` Release 编译/链接成功。`scripts/test.ps1 -Preset portable` 筛选 ui_refinement_tests、home_metrics_tests、proxy_bypass_tests、connection_view_tests、service_tests，五组全部通过（33.32 秒）；UI 专项六项通过，新增空闲不重复、错误跨页保留全文、成功超时不移动主体、设置待应用和键盘说明不触发写入检查。内存平台验证系统写入为零。

额外执行 AppearancePrefsTests 四项，覆盖草稿应用失败/持久化、首页摘要与重建、90–120% 文字和新增窗口小尺寸布局；全部通过。YamlFormsTests 三项通过，覆盖未触及字段保留/损坏输入拒绝、共享草稿与未解析高级文本、复杂来源和长机密值。未运行依赖 Windows 核心的真实网站测试。

Cocoa 原生后端执行八页空态、填充与分组设置、工具窗口、减噪行为四项，全部通过（21.91 秒）。取得 220 张隔离夹具截图，包含八页、三尺寸、浅深色、100/120% 文字及工具窗口；已复核八页合览和工具窗口。图像检查与自动按键不能替代人工前台或读屏验收。原始构建、测试和截图位于 `build/ui-copy-review/`，不纳入源码分发。

首次编译发现说明控件替换后节点编辑器错误反馈仍引用原标签，以及测试缺少 QMessageBox 头；已分别保留独立就近错误标签并补齐头文件，重新构建及以上检查全部通过。

文档工具四项通过；标准检查入口核验 76 份 Markdown 链接/导航和 83 份文档隐私，均零问题，git diff --check 通过。

未验：Windows 原生及完整回归、真实内核/公网任务、系统配置写入、人工/读屏、性能与发行资格。macOS 平台适配仍保持既有 Unsupported 与退出保护边界。本轮 UI 变更尚未提交或推送。

## 此前：U01 Apple 风格界面精修（2026-10-28）

按 [逐项审视](ui-apple-review.md) 先记录八页、共用控件、按钮布局、密度及工具窗口设计，再实施。设置采纳系统设置参考的单列分组/细分隔/名称与控件对齐；另一桌面工具仅借鉴侧栏、工具带和内容层级。平台字体、语义主题、按下/焦点反馈统一；规则、连接、日志支持工具带重排，首页摘要工具正文可滚动。当前契约、架构和 ADR 0042 同步维护。

构建：使用既有固定 Qt 6.10.2 本机工具链，`scripts/build.ps1 -Preset portable` 完整 Release 编译/链接成功，产物仍为 Mlash 1.17.0 arm64。原始构建和测试记录位于 `build/ui-review/`，不纳入源码分发。

自动回归：`scripts/test.ps1 -Preset portable` 筛选 ui_refinement_tests、home_metrics_tests、proxy_bypass_tests、connection_view_tests、service_tests，五组全部通过（30.74 秒）。UI 专项包含语义对比/资源、下拉首帧/草稿/阅读位置、八页空态、填充状态与分组设置、工具窗口五项。后续补充轻量重建、状态栏稳定及弱按钮/模式按下反馈后，完整重新构建并复测 UI 专项通过（13.01 秒）。系统代理和自启写入在 UI 夹具中被明确禁止并验证零调用，真实核心未启动。

额外执行 AppearancePrefsTests 的 draftApplyFailureAndPersistence、finiteHomeSummaryDraftsPersistenceAndFailure、allPagesAtLimitedTextScales、addedDialogsFitSmallWindows，四项全部通过。覆盖保存失败留稿、索引持久化、旧草稿冲突、取消、首页摘要与重建、90/100/110/120% 文字、16 种工具窗口在浅/深/高对比及 100/120% 下的按钮完整宽度、520 DIP 内高度和无横向滚动。未运行该程序中依赖 Windows 核心的真实网站测试用例。

Cocoa 自动验证：原生 macOS 后端运行 allPagesAndDialogsAcrossAppearances、groupedSettingsAndPopulatedPagesRemainUsable、toolWindowsKeepActionsAndDraftsAccessible 三项全部通过；取得八页空态/填充、三尺寸、浅深色、100/120% 文字、节点卡片、设置工具区域和 13 个工具入口的原生截图，共 220 张。截图采用公开虚构数据及本地 HTTP 夹具，已复核八页合览和工具窗口合览，不能当作人工前台/读屏验收。最后按下态调整后的离屏专项已复测通过。

发现并修复：浅色品牌文字在侧栏背景初为 4.43:1，不足既有 4.5:1 检查，调深后语义对比复测通过；QLineEdit 内部图标按钮继承普通按钮最小高度而越界，增加内部按钮专用约束；设置环境变量复合行的子按钮比父容器高，令复合布局维护最小尺寸；首页摘要小窗在放大文字时裁切，改为滚动正文/固定反馈提交并通过工具窗口检查。首次构建发现 ProxyTableModel 单独测试目标不链接 UiTheme，实现改为继承 QGuiApplication 的实际字体后全部链接通过。

文档工具四项通过；标准文档入口检查 76 份 Markdown 链接/导航、83 份文档隐私均零问题，git diff --check 通过。当前 UI 修改保留在 feature/apple-ui-refinement 分支，未执行提交、推送或更新此前构建适配 PR。

未验：Windows 原生绘制与该分支完整 Windows 回归、真实内核/公网任务链、人工鼠标/读屏、系统设置写入、性能和发行资格。macOS 系统代理、自启、机密存储、TUN 等仍为原有 Unsupported；此次不放宽恢复/退出保护，也不以自动截图宣称平台功能完成。

## 此前：B01 本机 portable 构建（2026-10-23）

构建完成：使用 Qt 6.10.2 官方 macOS SDK、Apple Clang 15.0.0、CMake 3.31.10、Ninja 1.13.2 和 PowerShell 7.6.7，经 `scripts/build.ps1 -Preset portable` 成功生成 Mlash 1.17.0 arm64 Release 及全部测试目标。产物为 `build/portable/bin/Mlash`；`--version` 实际输出 Mlash 1.17.0。依赖位于 `build/_deps/`，构建记录为 `build/portable-build.log`，不纳入源码分发。

首次配置发现 macOS Qt 不提供 ZlibPrivate，非 Windows 构建已改用系统 ZLIB::ZLIB；Windows 继续使用 Qt6::ZlibPrivate。CMake 4 默认移除旧策略兼容，使锁定 yaml-cpp 无法配置，本次选择 CMake 3.31.10 后完整构建通过。编译有四处已有初始化写法警告，无编译/链接错误。

已执行：仓库测试脚本筛选运行五组 CTest，其中 proxy_bypass_tests、connection_view_tests、service_tests、home_metrics_tests 四组通过；gui_smoke 启动后在 15 秒预算内未退出，记为失败。日志确认初始化和退出时的代理观测均返回“此平台的系统代理尚未支持”，退出保护流程未发出 exitReady。该限制未通过放宽代理恢复检查规避。完整 CTest 未执行，不能宣称 macOS 完整回归通过。

额外运行 OfficialCoreTests 的 metadataRejectsUntrustedOrIncompleteAssets、archiveChecksHashPathsBoundsAndCrc、cancellationIgnoresLateMetadata 三项，全部通过，包含系统 zlib 的压缩/非压缩 ZIP、哈希、路径、CRC、截断及取消验证。test_documentation_tools.py 四项通过。test_release_tools.py 首轮因缺少固定 Windows 内核而有四项错误；按锁下载并验证 SHA256 后复测十七项全通过，未在 macOS 执行该 Windows 内核。

首次构建输入是无 Git 元数据的源码快照，文档检查脚本直接入口的 git ls-files 失败；改用源码文件清单调用相同检查函数，74 份 Markdown 链接/导航和 81 份文档隐私检查均零问题。系统代理、自启、机密存储、受控进程、端口身份和 TUN 的非 Windows 适配保持 Unsupported；未执行系统写入、Windows 构建、原生人工验收或发行打包。

后续经同一构建脚本增量构建成功，以 Cocoa 后端和隔离数据目录启动。首次原生启动未能创建本地导入通道；本次进程使用较短临时目录后，日志确认 Mlash 1.17.0 初始化完成且进程保持运行。该启动记录不替代人工前台操作或退出验收，代理观测仍明确报告 Unsupported。提交前接回原仓库 Git 历史后，再以文档检查脚本的标准入口复核，74 份 Markdown 链接/导航和 81 份文档隐私均零问题，git diff --check 通过。

## 此前：D03 文档与新仓库

状态：文档重整、针对性检查、新仓库构建和预发布全部完成。

范围：用户/开发/当前契约/历史归档分层；整理导航、变更记录、模块细则和现行交互；消除过时的页面数量、TUN 范围、轻量生命周期和构建环境说明；新增链接/导航及隐私失败路径检查。产品源码、数据格式和运行实例未改变。

已执行：check_docs.py 检查 74 份 Markdown，文件链接和导航收录零问题；check_document_privacy.py 检查 81 份文档，零规则命中。test_documentation_tools.py 四项和 test_release_tools.py 十七项均通过；格式检查通过。失败路径包括文档移动后断链/未收录、越出仓库、示例代码误识别，以及隐私报告不回显命中值。

检查发现的旧外部参考目录链接已改为明确的快照源码路径，归档中的编辑器行号移除；当前契约连接到当前文件。产品 src 目录和 CMake 构建图未改变。

新仓库以项目身份的干净初始快照建立，初始提交为 `185fba91f0bbd4a87401ed24e4fc5889067e1a55`，发布标签 v1.17.0 绑定该提交；旧提交未上传。后续文档核验记录正常进入 main，不移动已构建标签。

[新仓库流水线](https://github.com/downdone/mlash/actions/runs/39720165873) 实际通过：Windows Server 2022 公开固定 Qt/MinGW 准备、完整编译、34 组隔离 CTest（33 通过、0 失败、1 系统门禁跳过，416.93 秒）、八页原生自动冒烟、隔离 PATH/Schannel、双 EXE 七尺寸图标、34 文件包与源码检查、发布。另有四项文档工具检查直接在流水线执行；原生自动冒烟不计作人工前台/读屏验收。

[v1.17.0 预发布](https://github.com/downdone/mlash/releases/tag/v1.17.0) 已公开，八个附件重新下载后核验 ZIP/SHA256、来源提交、标签、固定内核和运行指纹一致。下载的源码及便携文档合计检查 84 份，零隐私规则命中；首次直接下载速度异常，切换网络路径取得完整文件后所有校验通过，不将中断的部分文件计为有效产物。版本保持 prerelease、latest=false、release_qualified=false。

发布后的最终文档复核同时修正轻量模式的旧简写：GuiHost 除几何、页面和视图索引外，还保留不持有 QObject 的有界节点视图书签。该说明与源码一致，没有产品行为变更。

## 历史匿名汇总

以下为此前已执行的结果，原始运行证据不随新仓库发布，不作为当前提交的新验证。

| 基线 | 已观察结果 | 限制 |
|---|---|---|
| 1.17.0 | Windows 隔离回归 33 通过、0 失败、1 系统门禁跳过；34 文件包、八页/Schannel 与图标检查通过 | 不代表系统写入或干净部署 |
| 1.16.0 | 完整回归 32 通过、0 失败、1 跳过，原生精修场景检查通过 | 人工读屏和资源预算未完成 |
| 1.15.1 | 三轮回归各 32 通过、0 失败、1 跳过；一小时隔离观察完成 | 不代表 72 小时稳定性或内存预算达标 |
| TUN | 模拟/受限会话、固定内核、取消及失败恢复自动测试通过 | 实际 UAC/SCM、驱动及网络恢复未验 |

## 重现入口

依赖与命令统一见 [开发构建](development/build.md)。运行 scripts/build.ps1、scripts/test.ps1、文档/隐私检查和发布工具测试；打包后独立检查白名单、TLS、图标和源码来源。默认测试使用隔离数据，不修改系统代理。

## 未完成的正式验收

Windows 10/11 干净部署、真实代理/PAC 与冲突恢复、强杀恢复、登录/Explorer/休眠、TUN 服务/驱动/路由/DNS、跨账户实际迁移、72 小时稳定性、完整资源预算和第三方完整源码/分发确认未完成。MSVC、Linux 未实际构建验证；macOS 仅有上述构建及部分组件证据，其他平台系统适配返回 Unsupported。

预发布保持 release_qualified=false。没有真实运行证据的场景不能标为通过，不能从语言、架构或短期样本推导正式资格。
