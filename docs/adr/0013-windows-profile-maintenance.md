# ADR 0013：Windows 订阅维护增强

2026-10-13，接受。用户在保存 V1 基线后要求依设计补齐功能。本轮覆盖原规格 P1 中有明确 Windows 使用价值的订阅增强；保持 P2 禁止项与其他平台 Unsupported。

每订阅可明确开启 URL 定时更新（1–168 小时，初值 12）或本地文件监测，默认关闭。ProfileMaintenance 仅负责到期/变化信号，AppController 仍是下载、校验、状态保存的唯一写入者。URL 共用 60s 粗粒度调度，本地文件与父目录监测支持原子替换，变化合并 1s；内容比较 SHA256 放在 worker，相同字节不生成新版本。应用不运行时不更新；重新启动核对到期 URL 和受监测文件。

自动维护受 busy/退出/只读保护，调用既有不可变来源快照、Provider 冻结、真实 -t 与候选索引事务；始终不自动激活。失败保留 candidate/active/LKG，记录脱敏错误，5 分钟起指数退避、最高 1 小时；成功清除错误。后台维护不反复发托盘气泡，失败可在订阅页面查看。GUI 释放不停止明确开启的订阅维护。

最终 HTTP 成功响应只读取 subscription-userinfo 中 upload/download/total/expire，按所锁 mihomo 的字段约定；只接受有界非负十进制整数，重复/异常字段拒绝整个用量快照，来源未提供就不推测。Header 限 1024 字节，字节计数最多 2^53-1、到期最多有效 Unix 年 9999；数值以字符串持久化避免 JSON 精度损失。用量仅为最近一次成功验证来源的快照，不计量本机实时流量。

schema=1 增加可选 auto_update/update_interval_hours/source_hash/last_update_attempt/update_error/subscription_info；旧索引缺失仍默认关闭。错误类型/越界更新策略/用量拒绝加载，订阅上限 256。重建运行配置不改变来源更新时间；GUI 以更多菜单的单个更新策略弹窗提供设置及用量详情，保持列表紧凑。

标准字段确认使用只读参考仓库 adapter/provider/subscription_info.go；未改动参考仓库。本地自动监测不追踪改名后的其他文件，仍监测导入时明确保存的来源路径；源缺失会报错，已有运行快照不受影响。
