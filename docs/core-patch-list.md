# 随包内核补丁清单

本文是随包 mihomo 的唯一来源与维护说明。发行选择由 [ADR 0038](adr/0038-local-release-core.md) 确定，实际证据记录在 [验证报告](verification-report.md)。

## 1. 事实

| 项 | 值 | 来源 |
|---|---|---|
| 报告版本 | `v1.19.32-halfclose.1` | [dependencies.lock.json](../dependencies.lock.json)、`cores/manifest.json` |
| 源码提交 | `4ec827bcef5df96560a00dfe71d6945527a07ee3` | 同上 |
| 可执行 SHA-256 | `951bfd2f513ac7de486e53f2bbf1bf804e0a903bb19b237a744d11ca49023755` | 同上 |
| 构建工具链 | Go 1.26.8，`with_gvisor` | [ADR 0038](adr/0038-local-release-core.md) |
| 来源 | 项目自建（仓库 `downdone/mihomo`），非上游官方资产 | [dependencies.lock.json](../dependencies.lock.json) 的 `origin: local-source-pinned` |

上游参考资产 `v1.19.32` 仍记录在依赖锁里，仅作对照，不参与打包。

## 2. 补丁做了什么

补丁是**一个提交**：`4ec827b feat: configure idle cleanup for half-closed TCP relays`，共 13 个文件、+896 / −36。它引入的是功能，不是版本标记：

| 文件 | 作用 |
|---|---|
| `common/net/relay_timeout.go`（新增 114 行）及其测试（新增 315 行） | 半关闭后按方向计时的转发回收 |
| `config/config.go` | 新增 `tcp-half-close-timeout` 的 client/server 字段、解析与校验 |
| `config/halfclose_test.go` | 字段解析与边界测试 |
| `hub/executor/executor.go` | 执行器侧接线 |
| `hub/executor/halfclose_test.go` | 执行器侧回归 |
| `tunnel/halfclose.go`、`tunnel/connection.go` | 把计时接到隧道连接 |
| `tunnel/halfclose_test.go` | 隧道侧回归 |
| `docs/config.yaml` | 公开配置文档 |
| `Makefile`、`test/README.md`、`test/halfclose_windows.py` | 仅新增 `test-halfclose-windows` 目标与对照脚本，不改产品行为 |

上游对照（`94b13ae`，即 v1.19.32 基线）**不包含** `tcp-half-close-timeout` 这个键；补丁提交才引入它。因此：

- 客户端在 [ConfigBuilder](../src/profile/ConfigBuilder.cpp) 里按 H03 写入 `client=300` / `server=30`，**只有这个补丁内核能理解**；
- 切回上游会让该键变成未知字段，H03 承诺的行为随之消失，属于改变产品行为。

结论：**保留补丁是当前唯一不改变行为的选项**。§5 的实验因此从"能否用上游替代"改为"上游是否存在等价机制"——按源码检索，答案是否。

## 3. 补丁范围

见 §2 的逐文件表。补丁只有这一个提交；`git log 94b13ae..4ec827b` 只返回它，没有其他改动。

核对命令（在参照源码目录执行；目录位置记在本机环境文件中，不写入本仓库）：

```sh
git log --oneline <upstream-tag>..<patch-commit>
git show --stat <patch-commit>
git grep -n "tcp-half-close-timeout" <upstream-tag>   # 上游应为空
git grep -n "tcp-half-close-timeout" <patch-commit>   # 补丁应命中 config/
```

上述四条已在 2026-11-24 实际执行：上游无命中，补丁命中 `config/config.go` 多处与 `config/halfclose_test.go`。

## 4. 重基步骤

升级到新的上游版本时按顺序执行，每一步都要留下实际命令与输出：

1. 在上游仓库检出目标标签，建立新分支。
2. 按 §3 的差异清单逐个应用补丁；冲突时以"行为等价于 V1.19.32 基线"为准，不扩大范围。
3. 用锁定 Go 版本构建，记录 `-v` 输出、可执行 SHA-256 与 `with_gvisor` 标签。
4. 更新 `dependencies.lock.json` 的 `mihomo-release`（版本、提交、SHA）与 `cores/manifest.json`。
5. 若变更涉及 TUN，同步 `TunCoreSha256`；打包会拒绝不符的内核。
6. 跑定向回归与默认回归，结果写入验证报告。
7. 便携包白名单、`Mlash.exe` 哈希与八路由冒烟通过后，才更新发布记录。

## 5. 去留结论

已按源码核对：上游 v1.19.32 不含 `tcp-half-close-timeout` 的实现，切回上游会让 H03 写入的键失效。除非产品决定放弃半关闭回收默认值，否则**保留补丁**。

若将来上游自行实现同名机制，按下面的判据复核：

```powershell
cores/mihomo.exe -t -d <临时工作目录> -f <合成配置>
```

- 校验通过且回读行为与 H03 一致 → 评估切回上游，并同步依赖锁、内核清单、TUN 固定哈希、版本断言与兼容矩阵；
- 校验拒绝该键或行为不一致 → 继续保留补丁，重基按 §4。

无论结论如何，都以追加式 ADR 记录，不修改 ADR 0038 原文。

## 6. 不做什么

- 不用镜像或第三方重新打包的资产替代。
- 不在未验证行为等价的前提下更换内核来源。
- 不把"构建成功"当作系统级或发行资格证据；`release_qualified` 仍为 `false`。