# 测试层级

测试边界与夹具原则见 [U16 测试架构](../docs/design/u16-test-architecture.md)。CTest 的 `unit`、`integration`、`scenario`、`ui`、`system-gated` 标签按被测主边界划分；原有测试名称保留。默认 `scripts/test.ps1` 运行全部常规测试，其中 `system_test_guard` 在没有受控环境时跳过。

常用命令：

```powershell
./scripts/build.ps1 -Preset windows-mingw
./scripts/test.ps1 -Preset windows-mingw -Layer unit
./scripts/test.ps1 -Preset windows-mingw -Layer integration
./scripts/test.ps1 -Preset windows-mingw -Layer scenario -TestRegex '^(tun_tests|backup_tests)$'
./scripts/test.ps1 -Preset windows-mingw -Layer ui
```

`scripts/test.ps1` 在筛选结果为空时失败，防止拼错标签或测试名却得到绿色结果。`DomainContractTests.cpp` 只链接 domain 与 Qt Core/Test，用于快速检查无 I/O 的输入契约。`CoreDtoTests.cpp` 固定 mihomo 响应解析边界；`PageRouterTests.cpp` 在没有完整 UI 的目标中检查页面进入、离开、销毁。`MockHttp`、`FakeCore` 和临时数据目录用于隔离集成和操作链；新增测试优先注入这些夹具，检查失败、取消、回滚及回读。`-Layer` 只选择测试，不开启系统写入；真实代理/TUN 测试仍只在受控环境按专门门禁显式执行。
