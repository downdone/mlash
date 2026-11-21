# 项目状态

## 当前版本

- [源码版本](../CMakeLists.txt)为 **1.23.0**，汇总 U30–U40 界面改动。最近的 [U40 Windows 本机验收](verification-report.md#u40-导入栏紧凑精修)覆盖导入栏高度、按钮对齐、窄窗字号和便携预览。

## 当前任务

**1.23.0 Windows x64 预发布进行中。** 核对发布提交，完成 Windows 构建、回归和便携包冒烟，再重新下载并核验八个远端附件。 [发布验收](release-readiness.md)

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
