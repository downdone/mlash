# ADR 0012：放大字号与可辨识的小象图标

2026-10-13，接受。设计目标全局字号放大一级，修正 EXE 图标显示，并让小尺寸小象更清楚。覆盖 ADR 0006/0008 中的正文 8pt，细节以 ui-interactions.md 为唯一所有者；不改默认窗口大小。

正文/应用默认字体改为 9pt，标题保持原有层级并同步放大；保留紧凑行高和七页布局，以默认及最小窗口原生截图核对可见控件。Windows 原生标题栏字体由系统管理。

小象使用透明轮廓，更大的脸、眼睛和鼻子、更清楚的外耳与内耳对比；13 个托盘状态沿用既有观测映射、懒缓存和静态姿态。SVG 部位以 id 标识，调整几何形状不再依赖半径或颜色字面值定位。

EXE 图标和 Qt 窗口图标是不同资源路径。继续由 RC 内嵌多尺寸 ICO、CMake 跟踪其修改；verify-exe-icon.py 直接读取 PE 的 RT_GROUP_ICON/RT_ICON 并比较各尺寸图像负载，避免 ExtractAssociatedIcon 的 Shell 路径缓存影响验证。ICO 中 planes=0 按单平面归一化后与 windres 生成的 planes=1 元数据对应，图像字节仍须完全一致。

打包完成后，Windows 平台脚本发送指定 EXE 的 SHCNE_UPDATEITEM + SHCNF_PATHW + SHCNF_FLUSHNOWAIT，通知 Shell 文件发生变化。依据 [Microsoft SHChangeNotify 文档](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify)。该通知无返回结果，不把调用成功表述为用户 Explorer/所有快捷方式的显示已验收。

本次不引入运行时 SVG/动画/额外 GUI 驻留资源；Python 验证工具仅供开发，便携包运行与常规打包不依赖 Python。
