# ADR 0025：统一 UI/UX 与即时圆角弹出

日期：2026-10-14；状态：接受；细化 ADR 0018/0022/0024，不修改原始规格。

用户采用《Mlash UI/UX 优化实施任务书 v1.0》，并指出下拉展开动画先方后圆。此次采用任务书的层级、语义、对齐与反馈原则，保留既有七页、紧凑窗口、Widgets 技术栈及 Controller 安全事务。

1. `UiTheme` 集中浅色语义色、字体层级、控件/表格度量和主窗口 QSS；`WidgetStyle` 使用同一语义色处理独立托盘和下拉表面。摘要卡片独立于通用按钮高度，防止内容被压缩。
2. 通过公开 `QApplication::setEffectEnabled` 关闭 Combo 展开及 Menu 展开/淡入，启动即执行，重复安装仍执行。Qt 6.10.2 的 `qcombobox.cpp` 在开启 UI_AnimateCombo 时使用约 150ms 的 `qScrollEffect`；`qmenu.cpp` 也按 UI_AnimateMenu/FadeMenu 选择效果窗口。即时呈现原有透明圆角表面，避免效果表面与最终窗口交接；不引用私有 Qt API，不模拟动画。
3. 成功反馈保留约 3 秒；错误持续可见且可复制。规则和设置分别显示保存/校验/失败及草稿状态，后端仍负责完整校验/事务/回读。无改变的当前成员不提交，Esc 只清除临时预选。
4. 日志沿用 2000 行有界记录和可见订阅；插入文档尾部保留选区，只有原本位于底部且未选择文字时跟随最新。离页后新增的同源记录增量补入；筛选变化/有界记录淘汰可以重建显示。暂停保留快照，清空只清视图。
5. 设置保留两列、固定提交栏和统一行对齐，内容宽度不足 640 DIP 时按应用→内核→工具纵向排列，工具列同步切换；不添加新的后台轮询。

任务书的 1180×810 默认和 1000×680 最小是建议值。按照产品的紧凑布局原则，默认 940×620、最小 840×560 DIP 保留，额外验证任务书三个尺寸；不把 Windows 150% 的物理截图尺寸作为逻辑窗口基线。已离开的连接只表示快照消失，沿用此准确用语，不能宣称全部系统连接或已确认 TCP 断开。

参考源码：[Qt Combo](https://github.com/qt/qtbase/blob/v6.10.2/src/widgets/widgets/qcombobox.cpp)、[Qt Menu](https://github.com/qt/qtbase/blob/v6.10.2/src/widgets/widgets/qmenu.cpp)。真实验证结果由 verification-report 维护，逐页效果与边界由 ui-ux-changelog 维护。
