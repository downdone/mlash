# 构建与发布验收

本文拥有发布流程和资格门禁。开发命令见 [构建指南](development/build.md)，实际结果见 [验证报告](verification-report.md)。发布产物是预发布或正式合格包，不能由“构建成功”直接推导。

## 运行包契约

`scripts/package-files.json` 是运行包的唯一白名单，当前共 34 文件：两份 EXE、固定 mihomo、Qt/MinGW 与 Windows/TLS 插件、qt.conf、简短使用说明、许可/署名和完整性来源清单。缺少或混入额外文件均拒绝打包。

工程文档、开发脚本、依赖源码和测试证据不进入运行 ZIP。`package.ps1` 负责 Qt 部署、PE 版本、固定内核和资格门禁；`package_payload.py` 负责资产暂存和清单前后核对。应用、服务及 mihomo 版本/哈希必须与对应清单一致。

## 源码附件

源码只从已提交的 Mlash 与锁定内核提交执行 git archive，分别生成 ZIP 和 SHA256；source.json 绑定两个提交、固定内核及归档哈希。不得附带未提交文件、工作站配置、订阅、恢复日志或原始诊断。

Mlash 源码归档执行文档隐私检查。第三方完整对应源码与声明仍须按实际组合核对；两份工程源码 ZIP 不表示全部依赖分发义务已完成。许可入口见 [第三方清单](../licenses/README.md)。

## 自动化与触发

[Windows 工作流](../.github/workflows/windows.yml) 使用 Windows Server 2022、公开 Qt 6.10.2 和 MinGW 13.1 固定输入；Action 固定提交，下载核验 SHA256。步骤依次检查版本/文档、准备依赖、构建、默认隔离回归、便携部署、原生冒烟/Schannel、图标和源码附件。

main 推送和 PR 执行构建验证；v* 标签或 workflow_dispatch 才触发发布，版本必须匹配 CMake。写权限仅授予发布 job，PR 不执行发布。失败/取消不上传版本，不覆盖既有稳定版。

手动发布时在 Actions 选择 windows.yml，以目标提交或标签为 ref，release_tag 填入与 CMake 一致的版本。只运行一条完整验证链，成功后标签绑定该构建提交；之后的文档更新可以继续进入 main，不能把新代码覆盖到旧标签冒充原构建。

## 预发布核验

每个预发布应包含：

- Windows x64 便携 ZIP 与 SHA256。
- Mlash、固定内核源码 ZIP 及各自 SHA256。
- source.json 和 package-verification.json。

共八个附件；发布后重新下载，核对 ZIP 字节、侧车 SHA、包白名单、来源提交和标签。记录对应实际运行链接与结果。预发布保持 prerelease、latest=false、release_qualified=false，不把它标为正式合格包。

## 正式发行门禁

`package.ps1 -Release -EvidenceManifest <evidence.json>` 要求完整且通过的证据与 EXE/Core/依赖指纹绑定，缺失或失败即拒绝。资格包括 Windows 10/11 干净部署、真实系统代理/PAC/冲突与异常恢复、登录/Explorer/休眠、TUN 服务/驱动/路由/DNS、72 小时稳定性、资源预算和第三方分发材料。

默认测试不修改宿主代理。真实 OS 写入只在受控 VM 显式启用 --system-proxy-test；写前持久化加密恢复意图，只恢复仍属于测试写入者的状态。短期采样、offscreen、模拟与单次 CI 均不能替代正式系统/长期资格。

## 文档与隐私

公开说明只保留通用产品路径、技术契约、真实匿名结果和必要项目链接。私人身份、绝对工作站路径、真实来源/配置、私人使用记录及会话附件不分发。文档链接/导航及隐私检查在提交和源码打包前执行；自动规则不代替人工复核。

新建公开仓库以已审阅的干净初始快照建立，后续变更正常提交。第三方许可署名必须保留；不得为消除个人信息破坏许可材料或夸大验证结果。
