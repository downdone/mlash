# 验证报告

本文只记录实际结果，区分本轮执行、历史匿名汇总与尚未验证的资格。原始配置、私人环境、真实订阅及原始诊断不进入公开文档。

## 本轮：B01 本机 portable 构建（2026-10-23）

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
