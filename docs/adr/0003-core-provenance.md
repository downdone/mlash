# ADR 0003：内核来源验证

- 日期：2026-10-13；状态：接受。
- 官方直链网络超时，官方 GitHub release assets API 分段下载最终成功。
- 资产：mihomo-windows-amd64-v1-v1.19.32.zip；SHA256：
  `00549b347d8e124600e46cac67d59293358454f465e9d86d2511567da56b7029`。
- 解压前必须对照 dependencies.lock 核验；不使用未知镜像、不自动执行不匹配文件。
- 初期集成测试暂用本机 Clash Verge 已安装的 v1.19.32；最终与官方资产解压文件再次比较 SHA256。
- 实际内核版本和最终二进制校验信息记录在 cores/manifest.json，随包附带。
