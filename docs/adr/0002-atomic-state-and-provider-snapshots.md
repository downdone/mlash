# ADR 0002：原子状态与冻结 Provider

- 日期：2026-10-13；状态：接受。
- 决策一：设置与 Profile 索引合并为 DPAPI 加密 `state.dat`，一次 QSaveFile 提交。
  保留 `state.last-good.dat`。原规格中的两个 JSON 文件是示例；采用单一提交避免活动索引不一致。
- 决策二：源配置中的 HTTP Provider 由客户端执行 HTTPS/DNS/IP/大小校验后下载、验证，再落到版本内。
  生成的运行配置使用 file Provider。激活已有版本复用同一字节快照；手动更新 Profile 才更新 Provider。
- 理由：直接交给内核下载无法在客户端限制解析后的协议/路径能力，也无法固定 DNS 检查结果。
  不可信输入不能引入额外监听、任意本地文件或不受控更新。
- 代价：V1不自动更新 Provider，不支持 MRS 和指定代理下载；通过导入说明和明确拒绝保持可发现。
- 保留：源 YAML、DNS/IPv6/规则语义和 Provider 过滤/健康检查等已支持字段。
- 版本数据按 UUID 隔离，保留 current/LKG/candidate 及上一索引引用版本，不把原始订阅改写。
