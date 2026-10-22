# 兼容与工具链

| 项目 | 支持与验证边界 |
|---|---|
| Qt | 6.10.2 EXACT，官方公开 qtbase/qttools SDK，归档 SHA256 锁定 |
| 编译器 | MinGW-w64 GCC 13.1.0 x64，Release；必须与 Qt ABI 匹配 |
| CMake / Ninja | CMake 3.24+，Ninja；版本锁见构建配置 |
| yaml-cpp | 0.8.0，固定提交见 dependencies.lock.json |
| mihomo | v1.19.32-halfclose.1，Windows amd64、Go 1.26.8、with_gvisor；固定 SHA256 |
| Windows 自动化 | Windows Server 2022 构建/测试及隔离冒烟已通过 |
| Windows 10/11 | 目标平台；干净机器与系统级验收待完成 |
| MSVC | 提供配置入口，须使用 MSVC Qt kit；未验证 |
| macOS/Linux | 尚未实际构建/系统集成，系统适配显式 Unsupported |

Qt 和编译器路径使用 MLASH_QT、MLASH_MINGW 环境变量，官方依赖缓存位于 build/_deps。开发工作站安装信息不属于项目基线。

系统代理仅实现 WinINet 默认连接，不修改 WinHTTP/RAS/VPN。TUN 服务默认关闭、显式安装，只运行随包锁定内核；实际系统资格见 verification-report.md。offscreen 测试仅用于行为/生命周期，不能代替原生渲染验收。
