# 1.16.0 功能复核与 F09 跟进

> 历史归档：仅用于追溯当时的评审、阶段方案和决策过程。当前行为以 [交互契约](../ui-interactions.md)、[架构](../architecture.md) 和 [项目状态](../project-status.md) 为准；文中阶段状态不代表当前版本。

2026-10-21；复核基线 `a391c6e`。按产品设计直接补齐适合本机使用的缺口，不重新开发已有F08/U22能力，不追加长时间资源采样。

| 复核对象 | 基线结论 | 本轮处理 |
|---|---|---|
| 常见节点分享链接 | 本地Verge的 `src/utils/uri-parser/` 与 `proxies-editor-viewer.tsx`有节点URI解析/批量导入；Mlash只有完整YAML和逐项表单 | F09：有界离线解析、类型/名称/端点预览，同名加后缀，显式加入已有节点草稿 |
| 只有节点链接的首次使用 | URL/文件订阅入口无法直接生成可使用配置 | F09：添加窗口新增来源，生成本地快照、选择组和兜底规则；完整内核校验后仍需明确使用 |
| 剪贴板/拖入失败 | `MainWindow::importPayload`在accepted后异步提交，未绑定ImportDialog，失败原窗已关闭 | F09：统一异步提交，失败保留来源/名称/下载策略，成功才关闭；粘贴/拖入不自动请求 |
| 运行版/候选编辑上下文 | 底层事务已有运行版编辑保留候选，但窗口未明确编辑对象，状态变化时草稿可能对错版本 | F09：明确当前编辑版本；核对覆盖、active/candidate和实际编辑版本，变化即阻止陈旧提交 |
| 本地快照维护 | 无文件来源的恢复快照仍可点同步/配置监测，容易误以为配置坏了 | F09：明确无外部来源；禁用假同步/监测，批量标注略过；可保存元数据，附加源文件仍先校验 |
| 候选/版本/Provider、DNS、规则、表单、连接转规则、快捷键和备份 | 对照F08及当前入口已有实现 | 保持并回归相关导入/节点/存储路径，不当作新缺失重复做 |
| 系统资格和资源预算 | 暂无VM；实际系统写入、跨账户、人工前台/读屏、72h及整体55MiB仍未验或未达 | 明确保留，产品要求不追加长测，模拟和短样本不能代替 |

代码入口：[解析](../../src/profile/NodeLinks.cpp)、[导入窗口](../../src/ui/ImportDialog.cpp)、[粘贴/拖入](../../src/ui/MainWindowProfiles.cpp)、[节点编辑](../../src/ui/ProxyEditorDialog.cpp)、[Controller事务](../../src/app/AppControllerProfiles.cpp)。参照源码仅读取，未修改。

常见格式为ss（SIP002及旧式Base64）、VMess v2 JSON、VLESS、Trojan、Hysteria2/hy2、TUIC、AnyTLS及明确节点来源中的HTTP/HTTPS/SOCKS5。常见TCP/WS/gRPC和TLS/Reality按锁定内核转换；不支持的插件、伪装或参数整批拒绝，完整YAML保留兼容入口。不宣称覆盖所有私有URI扩展，也不增加SSR、任意JS/CSS、开放API/PAC/LAN、多语言/跨平台或静默更新。

生效设计/API/资源归属由ui-interactions、api-contracts、architecture维护；实施状态和实际结果由project-status、verification-report维护，本文件不重复宣称验收通过。
