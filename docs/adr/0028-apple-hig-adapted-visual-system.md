# 0028 对照 Apple HIG 的跨页面视觉系统

2026-10-15 · 接受 · 设计目标全面对照 Apple HIG，并精修测速卡片。原规格不改写；Windows/Qt Widgets 与轻量生命周期边界保持。

## 依据与取舍

阅读 Apple 官方 [Color](https://developer.apple.com/design/human-interface-guidelines/color)、[Typography](https://developer.apple.com/design/human-interface-guidelines/typography)、[Layout](https://developer.apple.com/design/human-interface-guidelines/layout)、[Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility)，取其清晰层级、语义颜色、少量强调、对齐/相关分组、适应窗口、键盘可达、状态不只依赖颜色的原则。网页由 JavaScript 渲染，设计正文通过同一官方站点的 DocC JSON 读取。

不把 macOS 的平台点值、SF 字体/SF Symbols、Liquid Glass 或原生窗口外观照搬到 Windows。使用 Windows 的 Segoe UI / Microsoft YaHei UI 字体组合、Qt 控件和自行绘制的统一线条图形；保留 Windows 标题栏，避免模糊、持续动画或新增 SVG 运行依赖。不宣称 Apple 认证、macOS 原生应用或完整辅助技术合格。

## 共用原则落地

- UiTheme 是单一语义色/字体/控件尺寸来源：中性底、白色内容层、侧栏独立层级；蓝色留给主要动作/选中/可交互标记，成功/警告/错误有文字补充。显式Han脚本回退到微软雅黑UI；正文10 Windows pt、页标题15、分组11、数值15，普通名称不全体加粗，稠密表格30 DIP、控件32 DIP。避免照抄示意图的巨大字号和留白。
- 主题在 QApplication 安装，覆盖八页、独立弹窗与无主窗口托盘；通过 Qt 的 colorScheme/accessibility contrastPreference 信号跟随系统。浅/深色角色一起更新，高对比加深边界；主按钮背景与链接文字使用独立语义色。应用不写系统主题。对比度自动验算针对正文、辅助文案、状态与主按钮文字；不把此验算称为完整 WCAG/屏幕阅读器验收。
- MainWindow 的页标题/页内分组/主要动作、侧栏、搜索图形、表格、组/节点卡片、模式分段、日志和设置采用同一层级。首页补充清楚的页标题，继续展示实际运行主摘要；代理页保留节点优先和折叠低频组选择；订阅维护保留行上下文；规则/连接页签统一选中外观；日志和设置保留历史阅读/草稿。低频动作继续渐进呈现，不把现成直达动作无谓藏起。
- 鼠标与键盘焦点区分、Tab/Enter/Esc/快捷键、未保存保护、异步取消和真实回读保持。菜单/下拉不启用展开或淡入，透明圆角从首帧绘制；所有外观都测试该契约。

## 测速卡片

- 12服务内置PNG标记，源/字节哈希/授权归属由 resources/services/manifest.json 与 NOTICE 管理。10个 Simple Icons 13.21.0 SVG轮廓离线配色/栅格化，Google与Disney+使用各自站点原始标记；后两者不标为CC0。应用没有favicon网络请求；未知资源有空图标结果，不误标其他站点。
- 标题和一句任务说明 → 节点/开始/取消/清空 → 完成统计与真实最近时间 → 最多三列、窄窗两列的自适应卡片。每卡名称/主机、较醒目的响应延迟、点与文字状态、直接测试/重测；错误和该项时间在提示中。未测、进行、已响应、超时、取消均独立。色彩不等于解锁，数字不表示带宽；无假曲线。
- 默认节点和请求语义继续遵循 ADR0027；未就绪或无法解析单一叶子时就近提示，正常就绪不重复技术说明。AppController 的构造依赖增加可选目标定义，用于本地HTTP隧道夹具；生产入口仍使用同一组12个HTTPS默认目标，无目标编辑/持久化变更。

## 验证责任

appearance_tests 独立覆盖浅/深/高对比语义与对比度、12资源、八页/导入弹窗、小窗/缩放、草稿保持、菜单首帧，以及真实内核本地目标的卡片成功/403/超时、全部/单卡/清空按钮。原有节点选择/排序、订阅事务、隐藏取消、轻量释放和焦点/菜单测试必须继续通过。实际截图、命令、失败及包校验由 verification-report 唯一记录。
