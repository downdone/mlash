# U16：架构分层后的测试组织

## 根因与原则

现有 CTest 多按历史功能文件和单个 QtTest 方法命名，`EngineTests.cpp` 还直接编译一个 UI 模型，绝大多数目标依赖完整 `mlash_ui`。这会让纯契约变化触发大型链接与慢测试，也让“单元通过”和“真实操作链通过”难以区分。重构不以迁移文件数量为目标；每个测试要对应一条可观察边界和失败方式。

## 测试层与夹具

| 层 | 被测边界 | 夹具与禁止事项 |
|---|---|---|
| unit | domain 纯校验、状态投影和 UI 无 I/O 组件 | 直接链接最小库；不启动 GUI、网络或进程；输入含有效、边界和拒绝场景 |
| integration | 配置版本、存储、内核适配、异步取消、平台只读适配 | 临时数据目录、FakeCore/MockHttp 和显式注入时钟；不得读取用户配置或修改系统代理 |
| scenario | Controller 命令、候选到运行、失败恢复、页面任务链 | 真实模块与隔离外部夹具组合；检查状态和副作用，不只检查信号次数 |
| ui | Qt Widgets 布局、事件、焦点、主题、隐藏/重建 | offscreen 可自动验证语义与几何；原生显示/读屏/DPI 人工结果单独记录 |
| system-gated | 真实系统代理、TUN、部署、登录、长稳 | 仅受控环境显式启用；普通 `scripts/test.ps1` 不执行写入 |

一条测试可以带多个标签，例如 `scenario;ui`。`engine`、`ui_components`、`website_tests` 和 `appearance_tests` 仍是混合套件，因此保留双标签，不假称它们已变成纯测试。命名和文件位置表明主要被测模块；CTest 标签供按层运行，不改变既有名称或 CI 全量入口。先给现有目标标注层级，再把真正纯粹的 domain 和 UI 模型用例抽成独立目标。大型混合文件按以后发生的修改逐步拆，不做一次性搬运，以免丢失历史失败链。

## 这次垂直切片验收

1. `domain_contract_tests` 只链接 `mlash_domain`、Qt Core/Test，覆盖 TUN 路由的有效/拒绝与测速目标 schema 拒绝；`largeNodeModelSorting` 从 `EngineTests` 移到只链接 Qt 和模型文件的 `ui_model_tests`，内核目标不再编译 UI 模型。
2. CTest 标签可查询 unit/integration/scenario/ui/system-gated；`scripts/test.ps1 -Layer unit` 只选 unit，和 `-TestRegex` 取交集。脚本将进程 TEMP/TMP 指向忽略的 `build/test-temp`，不依赖用户或沙盒的临时目录，并在退出时恢复环境变量。
3. 原来的测试名与默认全量行为不变；`system_test_guard` 继续跳过，真实代理写入没有默认入口。
4. 验证分层目标和一组 Controller/UI 回归；记录未重构的大型混合文件及时间预算。

## 实施后对照

新增 `domain_contract_tests` 与 `ui_model_tests` 两个快速目标，后者接走 `EngineTests` 中唯一直接依赖 UI 模型的场景。收尾又增加 `page_router_tests` 和 `core_dto_tests`，CTest 共 50 项并全部有层标签；部分历史混合套件有双标签，仍按原名称和测试方法运行。`scripts/test.ps1` 在 `build/test-temp` 创建隔离临时目录并恢复调用者环境。尚未把所有历史混合文件拆成一类测试一个目标，也未建立系统写入之外的真实 Windows 端到端新夹具。实际通过、沙盒限制与未验范围见验证报告。

收尾切片继续把纯路由生命周期场景从 `UiTests` 移到只链接 `PageLifecycle` 与 Qt Core/Test 的 `page_router_tests`，窗口进入/离开与 Controller 采样仍留在 UI 场景；`core_dto_tests` 只测内核响应解析的有效、缺字段、类型错误和传输失败。历史 `EngineTests` 的其余案例属于集成/Controller 场景，文件本身没有 UI 依赖；按多个 CTest 标签保留原场景名，避免为文件数量搬迁成熟夹具。新增案例的首选位置是各自最小目标，集成与场景继续使用隔离临时目录和可注入 FakeCore。
