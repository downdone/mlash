# ADR 0016：开发交付、隔离采样与公开发行门禁

状态：接受。开发验证与正式发布资格分开记录；项目采用 GPL-3.0-or-later。

本轮开发交付要求功能回归、原生小窗、包完整性、隔离 PATH/TLS 和真实十分钟采样。公开发行的 Windows 10/11 干净部署、系统写入/登录/Explorer/休眠、72h、同条件对照与第三方分发材料继续独立记录，不作为开发交付前提，也不伪造为已通过。

`--qualification-seconds` 必须同时给出新建隔离数据目录和新报告路径；只使用合成 500 节点配置。平台包装器禁止代理和 Run 写入，保留真实 Core/健康检查/Widgets/托盘行为。客户端和 Core 分开采样 Private Bytes/Working Set/CPU/句柄/线程；全部数值流写 JSONL，内存仅保留 32 行与最多 64 行积压，报告写入串行合并。短测或中断不会升级成十分钟/72h；Core 重启、采样缺口、哈希变化与持续增长会使对应证据失败。菜单预构建不冒充 OS 点击/绘制延迟，未控制缓存的启动不冒充冷启动。

`mlash_system_tests` 只有显式 `--system-proxy-test` 且 BIOS 识别为 VM 才执行。宿主/默认 CTest 返回 77，明确跳过；不以 HypervisorPresent 为虚拟机证明。真实 PAC/冲突/有限重启/强杀恢复/Run 回读在受控 VM 用隔离文件与写前加密恢复快照测试，测试结束只恢复仍属于测试写入者的状态；未知外部写入保留快照。登录/Explorer/休眠仍需独立实际验收。

正式 `package.ps1 -Release` 要求 EXE/Core/依赖指纹绑定的全部发行证据，缺失或失败即拒绝。普通验证包带文件清单及 SHA256，`release_qualified=false` 表示未通过公开发行门禁，不影响开发验证用途。

准备独立的官方公共 Qt 6.10.2 SDK：归档 SHA256 锁定在 prepare_open_qt.py，解压于 build/_deps/qt-open；标准 qt.conf 仅用于重定位，不设置许可绕过变量。windows-mingw-open 预设使用配套 GCC 13.1，实际构建/运行证据见验证报告。Qt 的动态替换/对应源码要求仍按 [官方说明](https://www.qt.io/development/open-source-lgpl-obligations) 处理；准备 SDK 不等于公开发行义务全部完成。
