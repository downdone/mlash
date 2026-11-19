# U21：Windows 字体栅格精修

## 原则与根因审视

保持四分区任务、字号层级、Noto Sans SC 字族和系统 DPI 原样；先解决文字实际栅格化差异，不用加粗、放大或整窗缩放掩盖彩边。原生标题栏由系统绘制，正文由 Qt Widgets 绘制，两者的抗锯齿方式可不同。

本机原生探针读到 Windows DPR 1.5、Qt 逻辑 DPI 96、正文 9.5pt 对应 13 逻辑像素。应用没有设置 `QT_SCALE_FACTOR`、`QT_FONT_DPI` 或 Windows DPI 兼容性覆盖，Windows 字体平滑已开启。用户截图和相同字号的最小 Widgets 探针均显示默认 Windows 字体引擎产生强烈的 RGB 子像素边缘；Noto、微软雅黑及 Segoe UI 都如此。`QFont::NoSubpixelAntialias` 在本机 Qt 6.10.2/Windows 输出完全相同，字形 hinting 的变化不能消除彩边。Qt 官方支持的 Windows `fontengine=freetype` 在相同 DPR 和字体下输出灰度边缘，屏幕截图与颜色计数均确认。

## 采用方案

- Windows 应用启动、`QApplication` 创建之前，若调用环境没有指定 `QT_QPA_PLATFORM`，选择 Qt Windows 平台插件的 FreeType 字体引擎。明确的 Qt 平台命令行参数仍可覆盖，隔离测试设置的 `offscreen` 不受影响。
- 包内冒烟脚本不强设 `QT_QPA_PLATFORM=windows`，以实际产品默认入口验证字体引擎；仍清除外部插件路径，保持部署隔离。
- 不改变 QFont 字族、点数、文字缩放、布局、QSS、系统 ClearType 或系统 DPI。macOS 等平台沿用本来的平台字体引擎。所有页面、菜单和弹窗均由同一 Qt 启动入口决定，不逐控件修补。
- 不引入图片化文字、重采样或应用外字体安装。FreeType 是当前 Qt Windows 插件公开支持的选项，仍使用已打包的 Qt 运行时。

## 可观察验收

1. 最小原生 Widgets 样片在 150% DPR 下默认字体引擎与 FreeType 形成可重复的像素差异；后者文字区域的强 RGB 彩边比例接近零，字族和字号不变。
2. Mlash 隔离原生冒烟的首页、配置与设置仍能加载，页面尺寸和操作位置稳定；配置页面文本采用同一灰度边缘。
3. Windows 构建和受影响的外观/页面回归通过；`QT_QPA_PLATFORM=offscreen` 的自动测试不被 Windows 默认值替换。
4. 用户实际屏幕、其他显卡/Windows 版本和人工读屏仍需独立验收；本机样片不构成正式发布资格。

## 实施后复核

Windows 默认入口已在 `QApplication` 前选择 FreeType，隔离便携冒烟不再强制 Windows 默认字体引擎。布局、字阶、原生标题栏和系统设置未改。相同 150% DPR 下，修复版配置页与显式 `-platform windows:fontengine=freetype` 的配置页逐像素相同；旧版默认引擎与修复版有 7,735 个不同像素，主要在字形边缘。原生字体回读仍为 Noto Sans SC；便携包截图与开发构建修复版逐像素相同。具体构建、通过项、既有失败与未验边界见[验证报告](../verification-report.md)。
