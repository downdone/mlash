# 文档导航

面向当前 **1.17.0** 基线。首次使用从使用指南开始，开发修改先读工程方法与架构。历史分析和方案集中归档，不能用其中的阶段判断替代当前状态。

## 使用与发行

| 文档 | 职责 |
|---|---|
| [使用指南](getting-started.md) | 安装、升级、订阅、节点、代理/TUN、观察、备份和故障处理 |
| [版本变更](../CHANGELOG.md) | 各版本用户可见变化 |
| [兼容矩阵](compatibility-matrix.md) | 目标平台、工具链与实际验证边界 |
| [发布验收](release-readiness.md) | 包清单、源码附件、流水线和正式发行门禁 |
| [第三方清单](../licenses/README.md) | 许可、署名和对应源码要求 |

## 开发与当前契约

| 文档 | 唯一职责 |
|---|---|
| [开发构建](development/build.md) | 可重建依赖、环境变量、构建/测试/打包命令 |
| [工程方法](engineering.md) | 任务切片、UI 先设计、模块目录、协作和文档管理 |
| [团队约束](../AGENTS.md) | 工程执行规则 |
| [总体架构](architecture.md) | 进程、模块、线程、状态、存储事务和 OS 恢复 |
| [功能模块契约](development/feature-contracts.md) | 有界字段、计划、并发、快照与取消细则 |
| [内核 API 契约](api-contracts.md) | 固定内核行为、错误/超时和实际回读 |
| [当前交互契约](ui-interactions.md) | 八页、托盘、反馈、视觉与完整操作链 |
| [原始产品规格](Mlash_Codex_Design_Spec_v2.0.md) | 需求和验收基线；接受的偏差由 ADR 说明 |
| [架构决策索引](adr/README.md) | 编号 ADR、后续替代关系和设计理由 |

## 进度与证据

| 文档 | 职责 |
|---|---|
| [项目状态](project-status.md) | 当前实施队列、实现范围及后续验收 |
| [需求映射](requirements-traceability.md) | 需求/场景与实现、证据边界的对应关系 |
| [验证报告](verification-report.md) | 实际执行结果，区分历史汇总与当前验证 |
| [历史归档](archive/README.md) | 旧功能对照、阶段实施及 UI 方案，仅用于追溯 |

## 使用规则

需求、当前契约、实施状态和验证结果分别维护，不能相互冒充。原规格遇到已接受偏差时查看对应 ADR；早期 UI 决策的后续关系见 ADR 索引，当前交互以当前契约为准。

一项事实由一份文档拥有，其他页面链接引用；新增或移动文档必须收录并修复相对链接。提交前执行 `python scripts/check_docs.py` 和 `python scripts/check_document_privacy.py`，不提交个人环境、真实订阅或原始运行记录。
