# 构建、测试与打包

## 前置条件

Windows x64、Git、Python 3、PowerShell 7、CMake 3.24+、Ninja，以及可在 PATH 中找到的 7-Zip。需要访问依赖锁中列出的公开来源。Qt 与编译器 ABI 必须匹配，实际验证范围见 [兼容矩阵](../compatibility-matrix.md)。

以下命令在项目根目录运行。构建输出位于 `build/`，发行暂存位于 `dist/`；不要使用真实用户数据或管理员权限运行默认测试。

## 官方公开工具链

```powershell
python scripts/ci_prepare.py
$compiler = Get-ChildItem build/_deps/mingw -Recurse -Filter g++.exe |
    Where-Object { $_.Directory.Name -eq 'bin' } | Select-Object -First 1
if (!$compiler) { throw 'MinGW compiler not found' }
$env:MLASH_MINGW = $compiler.Directory.Parent.FullName
./scripts/build.ps1 -Preset windows-ci
./scripts/test.ps1 -Preset windows-ci
```

准备脚本校验公开 Qt、MinGW 和固定补丁内核的哈希，并取得对应内核源码。CI 中脚本通过 GITHUB_ENV 设置编译器位置；交互式 PowerShell 需要按上述方式显式设置 MLASH_MINGW。不会写入工作站专属路径或修改现有 SDK。

## 已安装的开发工具链

设置 MLASH_QT 指向 Qt 6.10.2 MinGW kit，MLASH_MINGW 指向配套 GCC 13.1.0 工具链根目录，再执行：

```powershell
./scripts/bootstrap.ps1
./scripts/build.ps1 -Preset windows-mingw
./scripts/test.ps1 -Preset windows-mingw
./scripts/run.ps1 -SmokeTest
```

bootstrap 要求 `cores/mihomo.exe` 是发布锁匹配产物；首次准备可使用官方工具链流程下载。显式 `-CoreFile <可信文件>` 会预检导入，正式包仍须满足固定版本/哈希。已有 SDK 不包含内核源码，生成源码包时还需提供含锁定提交的内核 Git 仓库。

仅使用公开 Qt SDK、沿用已安装编译器时，可执行 `python scripts/prepare_open_qt.py` 后使用 windows-mingw-open 预设。MSVC 入口要求 Developer Shell 和 MLASH_QT_MSVC，尚未实际验证，不能混用 MinGW 库。

## 文档与发布工具检查

```powershell
python scripts/check_docs.py
python scripts/check_document_privacy.py
python -m unittest discover -s tests -p test_release_tools.py
```

文档检查覆盖相对链接与导航收录；隐私检查只输出文件与规则，不回显命中值。不能替代人工检查真实订阅、身份和使用记录。

## 便携包与源码

```powershell
./scripts/package.ps1 -Preset windows-ci
./scripts/package-source.ps1 -CoreRepository build/_deps/mihomo-source
```

便携白名单由 `scripts/package-files.json` 拥有，源码归档只读取已提交版本。文件 SHA256、源码来源及固定内核记录随产物输出；不能把工作目录、构建缓存、个人配置或原始测试报告作为源码附件。输出目录正在运行时请使用新的 OutputName。

`package.ps1 -Release` 是额外的正式资格门禁，普通打包不代表正式资格。流水线、触发方式和门禁见 [构建与发布验收](../release-readiness.md)，验证结果只记录在 [验证报告](../verification-report.md)。
