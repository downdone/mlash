# 项目状态

## 当前版本

- [源码版本](../CMakeLists.txt)为 **1.22.0**。公开分发记录为 [v1.22.0 Windows x64 预发布](https://github.com/downdone/mlash/releases/tag/v1.22.0)，发行资格标记为 `release_qualified=false`。
- 当前工作区包含公开预发布之后的 U30–U39 界面改动。最近的 [U39 Windows 本机验收](verification-report.md#u39-快速配置导入)覆盖快速配置导入与独立高级新增窗口，其他阶段结果见[验证报告](verification-report.md)。

## 当前任务

**D04 已完成文档整理和静态验证。** 合并重复入口，更新当前版本及界面路径，将阶段进度和旧版交互说明集中归档。验收见[验证报告](verification-report.md#d04-文档整理)。

## 后续验收

- 复杂配置编辑的内容密度、完整键盘路径与原生读屏仍需人工复核。
- 受控 Windows 10/11 的代理/PAC、TUN/驱动/路由/DNS、强杀恢复、登录、Explorer 重启、休眠和干净部署仍待验收。
- 跨账户迁移、公网凭据服务、72 小时稳定性、完整资源预算和第三方对应源码仍待验收。完整门禁见[发布验收](release-readiness.md)，各平台的已验范围见[兼容矩阵](compatibility-matrix.md)。
- macOS arm64 开发预览的阶段记录见[历史状态](archive/project-status-history.md#已完成历史)。平台能力以兼容矩阵为准。

已实现范围见[总体架构](architecture.md)、[交互契约](ui-interactions.md)及[需求映射](requirements-traceability.md)。历史阶段进展在[历次实施记录](archive/project-status-history.md)。
