# 项目状态

## 当前版本

- [源码版本](../CMakeLists.txt)为 **1.23.0**，汇总 U30–U40 界面改动。已分发 [v1.23.0 Windows x64 预发布](https://github.com/downdone/mlash/releases/tag/v1.23.0)，发行资格标记为 `release_qualified=false`。

## 当前任务

**双内核 Qt 界面已完成本地实现、回归和便携预览。** 同一配置列表按类型分流到专属详情，首页独立操作，数据页分开采样，设置支持权限状态与清理确认。完成宽窄两档、150%／200% DPI 原生截图和包内冒烟。系统网络仍待隔离 VM 验收。 [界面设计](design/mihomo-wireguard-ui.md#qt-交付设计) · [验证结果](verification-report.md#双内核-qt-界面交付)

**Windows 双链路已实现并产出本地便携预览，系统级验收待隔离 VM。** 统一导入后按类型打开详情，首页和托盘独立启停，流量页分别显示 WireGuard Peer 和 Mihomo 代理读数。Windows 服务使用固定资产、单一系统路由租约及恢复记录。SDK、Qt 定向回归和解压冒烟通过。真实 Wintun、三路流量、DNS 与故障恢复尚未执行。 [ADR 0059](adr/0059-wireguard-windows-service-scope.md) · [设计与 DoD](design/mihomo-wireguard-dual-engine.md) · [验证结果](verification-report.md#windows-双链路接入与便携预览)

**U42 侧栏折叠与位置稳定性已完成本机验收。** 60 DIP 折叠栏可正常切页，展开宽度连续变化。5 组定向回归、Windows 原生侧栏用例和本地便携包解压冒烟通过。 [设计与验收](design/u40-visual-interaction-design.md#侧栏折叠与位置稳定性) · [实测记录](verification-report.md#u42-侧栏折叠与位置稳定性)

**U41 底栏告警精修已完成本机验收。** 只读保护直接打开原因与数据目录，定向回归 5 组及原生用例 3 条通过。 [设计与验收](design/u40-visual-interaction-design.md#底栏告警精修) · [实测记录](verification-report.md#u41-底栏告警精修)

**1.23.0 Windows x64 预发布已完成。** 云端 Windows 默认回归 61 组通过，真实系统写入 1 组按门禁跳过。八个公开附件已重新下载并核验，下载的便携包通过本机隔离冒烟。 [实测记录](verification-report.md#v1230-windows-x64-预发布)

**U40 导入栏紧凑精修已完成。** 常规窗口的导入栏从 78 DIP 降至约 48 DIP。3 组定向回归、3 条 Windows 原生用例和便携包校验通过。 [设计与验收](design/u40-visual-interaction-design.md#导入栏紧凑精修) · [实测记录](verification-report.md#u40-导入栏紧凑精修)

**U40 导入栏与配置卡按参考图重排已完成。** 4 组定向回归、4 条 Windows 原生用例和便携包校验通过。 [设计与验收](design/u40-visual-interaction-design.md#导入栏与配置卡按图重排) · [实测记录](verification-report.md#u40-导入栏与配置卡按图重排)

**U40 配置页参考稿精修已完成。** 3 组定向回归、4 条 Windows 原生用例和便携包校验通过。 [设计与验收](design/u40-visual-interaction-design.md#配置页参考稿精修) · [实测记录](verification-report.md#u40-配置页参考稿精修)

**U40 侧栏竖排已完成。** 3 组定向回归、3 条 Windows 原生用例和便携包校验通过。 [设计与验收](design/u40-visual-interaction-design.md#侧栏竖排) · [实测记录](verification-report.md#u40-侧栏竖排)

**U40 Daymark 配色、导航与配置卡已实现并完成原生验收。** 8 组定向回归通过，3 组达到组合时限，相关场景拆分复测通过。本地便携包通过校验。 [设计与验收](design/u40-visual-interaction-design.md#daymark-配色导航与测速卡) · [实测记录](verification-report.md#u40-daymark-配色导航与测速卡)

**U40 配置卡精简与文字完整性修整已完成。** 保留名称、URL 与时间，刷新和更多固定在右上角。8 组定向回归、2 条 Windows 原生用例、截图检查和本地便携预览校验通过。 [改动设计](design/u40-visual-interaction-design.md#配置卡片文字完整性) · [实测记录](verification-report.md#u40-配置卡精简与文字完整性)

**U40 配置卡视觉精修已完成。** 名称、来源和时间集中，状态与操作对齐。8 组定向回归、4 条原生用例和便携预览校验通过。 [实施设计](design/u40-visual-interaction-design.md#配置卡片精修) · [实测记录](verification-report.md#u40-配置卡片精修)

**U40 独立盲评问题修整已完成。** 定向回归、Windows 原生 Qt 用例、便携包路由和资源校验通过。 [实施设计](design/u40-visual-interaction-design.md#独立盲评后精修) · [实测记录](verification-report.md#u40-独立盲评后精修)

**U40 原生首轮精修已完成。** 首次接管、批量更新、候选应用与回滚已实现，定向回归、Windows Qt 用例和便携预览包冒烟通过。U40 整体 DoD 仍有导航任务发现、大字号、读屏及实际系统权限待验收。 [原生实施设计](design/u40-visual-interaction-design.md#原生精修实施设计) · [实测记录](verification-report.md#u40-原生首轮精修)

**U40 当前实现复审已完成。** 以现有 Widgets 为精修底稿，优先补齐首次接管的状态语义、批量更新的范围和候选应用的后果。五区导航仍待任务发现验证。[复审结论](design/u40-visual-interaction-design.md#当前-widgets-实现复审) · [实测记录](verification-report.md#u40-当前-widgets-实现复审)

**D04 已完成文档整理和静态验证。** 合并重复入口，更新当前版本及界面路径，将阶段进度和旧版交互说明集中归档。验收见[验证报告](verification-report.md#d04-文档整理)。

## 后续验收

- 复杂配置编辑的内容密度、完整键盘路径与原生读屏仍需人工复核。
- 受控 Windows 10/11 的代理/PAC、TUN/驱动/路由/DNS、强杀恢复、登录、Explorer 重启、休眠和干净部署仍待验收。
- 跨账户迁移、公网凭据服务、72 小时稳定性、完整资源预算和第三方对应源码仍待验收。完整门禁见[发布验收](release-readiness.md)，各平台的已验范围见[兼容矩阵](compatibility-matrix.md)。
- macOS arm64 开发预览的阶段记录见[历史状态](archive/project-status-history.md#已完成历史)。平台能力以兼容矩阵为准。

已实现范围见[总体架构](architecture.md)、[交互契约](ui-interactions.md)及[需求映射](requirements-traceability.md)。历史阶段进展在[历次实施记录](archive/project-status-history.md)。
