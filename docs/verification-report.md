# 验证报告

本文只记录实际结果，区分本轮执行、历史匿名汇总与尚未验证的资格。原始配置、私人环境、真实订阅及原始诊断不进入公开文档。

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
