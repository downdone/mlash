# 第三方组件与资源

Mlash 自有代码采用 GPL-3.0-or-later，完整文本见包根目录 LICENSE；第三方组件保留各自许可与署名。

| 组件 | 随包形式 | 许可及来源 |
|---|---|---|
| Qt 6.10.2 Core/Gui/Widgets/Network、Windows/TLS 插件 | 动态库，官方开源 SDK | LGPL/GPL 文本见 LGPL-3.0.txt、GPL-3.0.txt；[Qt 源码](https://code.qt.io/cgit/qt/qtbase.git/tag/?h=v6.10.2) |
| yaml-cpp 0.8.0 | 静态链接 | yaml-cpp-MIT.txt；[源码](https://github.com/jbeder/yaml-cpp) |
| Zstandard 1.5.7 | 静态链接 | zstd-BSD.txt；[源码](https://github.com/facebook/zstd) |
| QR Code generator 1.8.0 | 静态链接 | qrcodegen-MIT.txt；[源码](https://github.com/nayuki/QR-Code-generator) |
| MinGW GCC 13.1 / winpthreads | 动态运行库 | GPL-3.0.txt、GCC-Runtime-Exception.txt、winpthreads.txt；[GCC](https://gcc.gnu.org/)、[MinGW-w64](https://www.mingw-w64.org/) |
| mihomo @CORE_VERSION@ | 独立进程，含半关闭回收补丁 | mihomo-GPL-3.0.txt；[上游](https://github.com/MetaCubeX/mihomo)，实际提交与SHA见 cores/manifest.json |
| Twemoji 14.0.2 国旗 | 内置 PNG | CC BY 4.0，twemoji-graphics.txt、twemoji-NOTICE.md |
| 服务标记 | 内置 PNG | Simple Icons CC0 及各自商标，simple-icons-CC0.txt、services-NOTICE.md |

许可文本与声明不是完整对应源码分发的替代品。公开发布需一并提供对应版本源码、构建材料及第三方完整声明；详见对应源码包的 licenses/README.md 与发布说明。


## 源码与开发依赖

固定版本、仓库、提交和归档 SHA256 见 dependencies.lock.json。Qt 官方公开 SDK 由 scripts/prepare_open_qt.py 准备；运行包文件清单由 scripts/package-files.json 管理。

Mlash 采用 GPL-3.0-or-later，第三方代码与资源保留各自许可。随包提供许可文本不替代完整的对应源码和分发义务核对；正式发布资格见 docs/release-readiness.md。
