<img src="resources/mlash.png" width="64" height="64" alt="Mlash 小象">

# Mlash

轻量、紧凑的 Windows mihomo 客户端，使用 C++20 和 Qt 6 Widgets 构建。支持订阅管理、节点选择、规则覆盖、连接观察，以及关闭窗口后释放主界面的轻量模式。

[下载预发布版本](https://github.com/downdone/mlash/releases) · [使用指南](docs/getting-started.md) · [开发构建](docs/development/build.md) · [文档导航](docs/README.md)

## 主要功能

| 功能 | 能力 |
|---|---|
| 订阅 | YAML URL/文件导入、节点分享链接预览、多订阅、候选变化比较、手动应用、可选定时更新 |
| 代理 | 规则/全局/直连、策略组与节点选择、表格/卡片视图、国旗、搜索/筛选/排序、单节点与整组延迟测试 |
| 测速 | 常见网站响应测试、自定义目标、独立重测与取消；显示响应证据，不冒充带宽或服务解锁 |
| 规则与 DNS | 按订阅保存覆盖，提供规则集、节点/策略组和拨号链编辑；保留原始来源及独立候选 |
| 网络 | 系统代理、代理归属检查与安全恢复；可选 Windows TUN 服务，默认关闭 |
| 观察与诊断 | 可见页面按需采样、连接详情/过滤/关闭、日志筛选及脱敏导出 |
| 数据与桌面 | 密码备份与选择性恢复、可选自动/WebDAV 备份、内核副本预检/更新、托盘和快捷键 |

## 开始使用

1. 下载并完整解压 Windows x64 `portable.zip`，运行 `Mlash.exe`。
2. 在“订阅”页添加配置，检查后使用选中的订阅。
3. 内核就绪后，按需开启“系统代理”，再到“代理”页选择节点。
4. 关闭窗口后可从托盘重新打开；彻底退出请使用托盘“退出 Mlash”。

升级前退出旧版并备份数据。订阅下载更新会生成候选版本，点击“应用更新”后才切换运行版本。详细说明和故障处理见 [使用指南](docs/getting-started.md)。

## 支持范围

当前版本为 **1.17.1 预发布**，目标平台为 Windows 10/11 x64。固定内核为 `v1.19.32-halfclose.1`；默认半关闭空闲回收为 client 300 秒 / server 30 秒，保留来源显式值。

TUN 服务已实现，但实际服务/驱动/路由恢复、干净机器部署和完整长期验收尚未完成。预发布保持 `release_qualified=false`，不代表正式发行资格。实际验证与未验范围见 [验证报告](docs/verification-report.md)。macOS/Linux 系统集成不受支持。

## 开发与文档

- [构建、测试和源码打包](docs/development/build.md)
- [工程方法与 UI 先设计原则](docs/engineering.md)
- [架构与安全边界](docs/architecture.md)
- [当前交互契约](docs/ui-interactions.md)
- [变更记录](CHANGELOG.md) · [完整文档导航](docs/README.md)

源码不包含运行二进制、订阅或用户配置；依赖版本、来源及哈希由 `dependencies.lock.json` 锁定。

## 许可证

Mlash 以 [GPL-3.0-or-later](LICENSE) 授权。第三方组件和图形资源保留各自许可与署名，见 [第三方清单](licenses/README.md)。

Independent third-party client; not affiliated with Clash or MetaCubeX.
