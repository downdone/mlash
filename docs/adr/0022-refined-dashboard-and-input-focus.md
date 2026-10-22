# ADR 0022：精简仪表盘、两列设置与输入焦点

2026-10-14，接受。设计目标以 Apple macOS 的精细、极简方向审视整体：鼠标不残留焦点框，测速下拉更自然，设置两列，首页建立仪表盘层次。

参考 Apple [Layout](https://developer.apple.com/design/human-interface-guidelines/layout)、[Focus and selection](https://developer.apple.com/design/human-interface-guidelines/focus-and-selection/) 与 [Designing for macOS](https://developer.apple.com/design/human-interface-guidelines/designing-for-macos/) 的层级、对齐及键盘访问原则。实现仍是 Windows Qt Widgets，不引入仿系统标题栏、模糊、动画或额外 UI 运行库，也不声称获得 macOS 原生规范认证。

首页保留真实状态条与六页路由，将同权重的 2×3 按钮改为上排两个宽主摘要、下排四个紧凑次级状态；使用同一线条图标、短状态、克制蓝色和统一间距。结合用户追加的 Clash 首页参考，主摘要强调订阅名称/来源类型/最近获取和浏览组实际成员/已知手测延迟，规模降为辅助；不展示来源 token，不将多个规则组虚构成一个全局节点。资源数量来自当前内核，连接仍是按可见性观察，首页不新增轮询，不虚构持续速率/延迟曲线。

设置采用应用/内核两列等高对齐、工具与数据横跨下方两列；紧凑行保持名称、短说明、控件的稳定阅读顺序，URL/路径堆叠以避免挤压。最小 840×560 窗口仅纵向滚动，提交栏固定可见。字段、草稿与应用事务不变。

保留控件真实焦点和全部键盘行为，用输入来源控制绘制：鼠标没有残留焦点边框，键盘可辨认当前位置。事件过滤器限于主窗口对象树、组合编辑器同步，销毁时移除；代理 Fusion 样式仅屏蔽鼠标焦点原语。选中态不等同焦点，不能为了外观删除键盘访问。测速拆分按钮保持动作/菜单语义，改为单一圆角边界和一致箭头。

本决策调整 ADR 0019 的同权重首页与单列设置展示，继续 ADR 0018/0021 的操作和节点空间原则。原始规格、持久化 schema、Controller 命令、安全恢复与轻量生命周期不变。证据及未测界限由 verification-report 管理。
