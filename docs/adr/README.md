# 架构决策索引

ADR 保留决策发生时的背景和取舍；较新的已接受决策可以替代旧决定的具体部分。当前行为统一见 [架构](../architecture.md)、[功能模块契约](../development/feature-contracts.md) 与 [交互契约](../ui-interactions.md)。

## 重要后续关系

| 主题 | 后续决策 |
|---|---|
| 工具链与 SDK | 0001 固定 Qt/ABI；0016 补充公开 SDK 和资格门禁 |
| 内核来源与回收 | 0003 来源基线由 0038 补丁发布锁及 H03 默认策略补充 |
| 导航、字体与设置布局 | 早期 0005–0012/0019 的具体布局由 0022、0027、0028、0040 收敛为八页；0042 改单列及平台字体，0043 明确功能归属与设置分类 |
| 权限与 TUN | 0036 明确扩展原规格及 0029 的排除范围，其他权限边界保持 |
| 当前任务交互 | 0018/0022/0028 的原则由 0040 细化；当前操作以交互契约为准 |
| 公开文档 | 0041 将当前契约与历史阶段资料分层 |

## 编号记录

| 编号 | 决策 |
|---|---|
| 0001 | [ADR 0001：锁定可重建的 Qt 工具链](0001-build-baseline.md) |
| 0002 | [ADR 0002：原子状态与冻结 Provider](0002-atomic-state-and-provider-snapshots.md) |
| 0003 | [ADR 0003：内核来源验证](0003-core-provenance.md) |
| 0004 | [ADR 0004：Geo 校验与本地数据](0004-geo-validation-and-local-data.md) |
| 0005 | [ADR 0005：首页与代理页分工](0005-home-and-proxy-navigation.md) |
| 0006 | [ADR 0006：日志主导航与小字号](0006-log-navigation-and-small-type.md) |
| 0007 | [ADR 0007：紧凑策略组总览与嵌套浏览](0007-compact-group-overview.md) |
| 0008 | [ADR 0008：五页统一视觉与操作层级](0008-unified-compact-widgets.md) |
| 0009 | [ADR 0009：品牌更新、订阅规则覆盖和可见连接快照](0009-brand-rules-and-connections.md) |
| 0010 | [ADR 0010：小象吉祥物与轻量 GUI 生命周期](0010-elephant-and-lightweight-gui.md) |
| 0011 | [ADR 0011：按观测状态变化的小象托盘](0011-stateful-elephant-tray.md) |
| 0012 | [ADR 0012：放大字号与可辨识的小象图标](0012-readable-type-and-executable-icon.md) |
| 0013 | [ADR 0013：Windows 订阅维护增强](0013-windows-profile-maintenance.md) |
| 0014 | [ADR 0014：按需诊断与最近成员](0014-on-demand-diagnostic-completion.md) |
| 0015 | [ADR 0015：运行版本保护与 Windows 功能闭环](0015-state-recovery-and-v11-completion.md) |
| 0016 | [ADR 0016：开发交付、隔离采样与公开发行门禁](0016-personal-qualification-and-open-sdk.md) |
| 0017 | [ADR 0017：紧凑连接观察与日常设置补齐](0017-compact-connections-and-desktop-preferences.md) |
| 0018 | [ADR 0018：面向任务的整套 UI/UX](0018-coherent-task-oriented-ui.md) |
| 0019 | [ADR 0019：摘要仪表盘与统一设置项](0019-dashboard-and-settings-rows.md) |
| 0020 | [ADR 0020：共用状态的节点表格与卡片](0020-shared-node-table-and-cards.md) |
| 0021 | [ADR 0021：节点优先布局与卡片延迟操作](0021-member-first-layout-and-inline-delay.md) |
| 0022 | [ADR 0022：精简仪表盘、两列设置与输入焦点](0022-refined-dashboard-and-input-focus.md) |
| 0023 | [ADR 0023：托盘具体策略组按需测速](0023-tray-group-demand-tests.md) |
| 0024 | [ADR 0024：统一下拉与托盘弹出层](0024-shared-popup-surfaces.md) |
| 0025 | [ADR 0025：统一 UI/UX 与即时圆角弹出](0025-uiux-refinement-and-immediate-popups.md) |
| 0026 | [ADR 0026：内置国旗与紧凑节点卡片](0026-country-flags-and-node-card-polish.md) |
| 0027 | [0027 按指定节点测试常见目标](0027-on-demand-website-tests.md) |
| 0028 | [0028 对照 Apple HIG 的跨页面视觉系统](0028-apple-hig-adapted-visual-system.md) |
| 0029 | [0029：Verge 差异清单的实施边界](0029-feature-completion-boundaries.md) |
| 0030 | [0030：密码备份与增量恢复事务](0030-portable-backup-transactions.md) |
| 0031 | [ADR 0031：Mlash 自有链接与明确确认](0031-local-link-import.md) |
| 0032 | [ADR 0032：代理目标更新与完整快照恢复](0032-proxy-target-transition.md) |
| 0033 | [ADR 0033：本地内核预检与独立副本更新](0033-manual-core-update.md) |
| 0034 | [ADR 0034：密码加密的 WebDAV 版本备份](0034-encrypted-webdav-snapshots.md) |
| 0035 | [ADR 0035：常驻操作状态栏](0035-persistent-operation-status-bar.md) |
| 0036 | [0036 · Windows TUN 权限服务](0036-windows-tun-service.md) |
| 0037 | [0037 · 功能深度与完整任务流程](0037-feature-depth-and-task-flows.md) |
| 0038 | [0038：本地bugfix构建作为随包内核](0038-local-release-core.md) |
| 0039 | [ADR0039 本地保护与显式内核维护](0039-local-protection-and-explicit-core-maintenance.md) |
| 0040 | [ADR 0040：任务层级与统一输入反馈精修](0040-task-oriented-ui-refinement.md) |
| 0041 | [ADR 0041：公开文档基线与重新发布](0041-public-documentation-baseline.md) |
| 0042 | [ADR 0042：单列分组设置与平台排版](0042-grouped-settings-and-platform-typography.md) |
| 0043 | [ADR 0043：功能归属、明确动作与设置分类](0043-task-ownership-and-clear-actions.md) |
| 0044 | [ADR 0044：设置即时生效、内核历史与设置页标题统一](0044-immediate-settings-core-history.md) |
| 0045 | [ADR 0045：界面系统重构方向](0045-ui-system-rebuild.md) |
| 0046 | [ADR 0046：导航可寻址、反馈路由与设置分类深链](0046-navigation-routability-and-feedback-routing.md) |
| 0047 | [ADR 0047：网站测试归属数据分区与页面标题移除](0047-website-tests-and-title-removal.md) |
