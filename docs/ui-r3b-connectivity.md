# R3b 施工记录：首页控制区「连通性」行

> 归属：[界面分层设计](ui-layered-design.md#实施切片) 的 R3b 片。

## 1. 问题

ADR 0048 的控制区第三行要求「连通性：上次结果 + 时间，点击测试」。但现有 `AppController::startWebsiteTests` 有一道 `websiteTestsVisible_` 闸门，含义是「测速页当前可见」——首页要复用它，就必须先把测速页打开，那是反向的依赖。

## 2. 采用方案

新增 `ConnectivityProbe`（`src/app`），与 `HomeNodeProbe` 同构但**没有可见性条件**：

- `run(node)`：调 `MihomoClient::delay`（`GET /proxies/{leaf}/delay`），一次请求、5 秒超时，只读，不写节点选择。
- 结果结构：`status ∈ {running, ok, failed}`、`delay`、`node`、`url`、`measured_utc`、`error`。**0 ms 是有效响应**，只有失败才记 `-1`。
- `stale()`：超过 10 分钟即过期；过期结果仍显示，但界面追加「已过期」。
- `cancel()`：中止请求并清空结果。

Controller 侧 `runConnectivityProbe()` 先用 `websiteNode(homeNode())` 解析叶子（与首页探测同一函数），拒绝条件只有：未就绪 / 忙碌 / 退出中 / 只读 / 无法解析叶子。**不含页面可见性。**

UI：控制区第三行 = 「连通性」标签 + 结果文本 + 「测试/取消」+「详情」（跳转网站测试页）。

## 3. 验收点

1. 首页可见即可测试，不需要先打开测速页。
2. 结果带时间戳；失败显示原因；未测显示「尚未测试」。
3. 测试不改变 `proxies[group].now`。
4. 未就绪时拒绝并说明原因，且不发起任何网络请求。
5. 既有测试全绿。

## 4. 实际结果

**已实施并自动测试（2026-10-29）。**

`ConnectivityProbe` 与 `runConnectivityProbe()` / `cancelConnectivityProbe()` 已落地；控制区第三行接入，`updateHome()` 按 `status` 渲染文本并禁用不适用按钮。

测试 `mlash_home_probe_tests connectivityProbeRunsWithoutWebsitePageAndReportsFreshness` 覆盖：未就绪时拒绝且零请求；首页从未显示时仍可测试（这是与 `startWebsiteTests` 的关键差别）；成功时 `delay==37` 且 `node` 正确、时间戳非空、未过期；节点选择未被改写；失败保留原因；取消清空结果。测试通过 `AppController::configureApiForTests()` 注入被测端点（生产路径由 `CoreManager` 在启动内核时配置同一端点）。

一处实现取舍：`ConnectivityProbe` 首次接在首页时连接的是 `controller_->connectivity()` 这个 `const` 访问器，信号连接需要非 const 指针，改为新增 `cancelConnectivityProbe()` 后再以 `const_cast` 连接信号；没有为「取消」新增可见性相关状态。

完整 `ctest`：34 项中 33 通过、0 失败、1 跳过。

**未验**：真实内核下的一次性延迟请求、人工读屏与 10 分钟过期的实际观感。
