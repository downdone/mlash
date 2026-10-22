# 固定 mihomo 内核

运行二进制位于 cores/mihomo.exe，不进入 Git。版本、公开来源、提交和 SHA256 由 dependencies.lock.json 的 mihomo-release 及 manifest.json 记录；下载和导入须验证实际字节与 -v 版本。

当前采用 v1.19.32-halfclose.1。Mlash 配置合成默认补充缺失方向的半关闭空闲回收 client=300、server=30 秒，保留来源显式 0 或有效自定义值。原始 YAML 不改写，运行版下次启动或重载时使用合成结果。

TUN 服务仅运行固定版本/哈希内核；bootstrap 和打包拒绝不符产物。明确的副本更新/回退及来源策略见 [内核决策](../docs/adr/0038-local-release-core.md)，构建与源码材料见 [开发指南](../docs/development/build.md)。
