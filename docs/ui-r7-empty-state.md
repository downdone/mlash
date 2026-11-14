# R7 施工记录：空态组件（图标 + 原因 + 可聚焦动作）

> 归属：[界面分层设计](ui-layered-design.md#实施切片) 的 R7 片，规格见 [状态呈现规格](ui-layered-design.md#状态呈现规格)。

## 1. 问题

`emptyState()` 原来只创建一个 `WA_TransparentForMouseEvents` 的 `QLabel`：它说明了原因，却没有下一步动作，键盘用户在空态里无处可去。设计明确要求「空态与失败态必须有可聚焦的动作按钮」。

## 2. 采用方案

保持「视口内一个说明标签」的结构不变（不改视图布局、不引入新的容器控件），在标签下方追加一个**可聚焦的 `QPushButton`**：

```cpp
QLabel*      emptyState(QAbstractItemView*, const QString& text);   // 旧签名保持不变
QPushButton* emptyStateAction(QAbstractItemView*);
void         setEmptyStateAction(QAbstractItemView*, const QString& text, const std::function<void()>& run);
```

- 动作文案为空即整块隐藏；有文案时必须 `StrongFocus`。
- 每个调用点在刷新时按**当前原因**设置动作：
  - 订阅空 → 「添加订阅…」
  - 节点空：有筛选结果为空 → 「清除搜索」；无订阅 → 「添加订阅…」；有订阅未使用 → 「选择订阅」；其余 → 「重试」
  - 连接空：读取失败 → 「查看日志」；有数据但无匹配 → 「清除筛选」；内核未就绪 → 「启动内核」；其余 → 「刷新」
  - 规则空 → 「打开完整规则编辑…」

## 3. 一次失败的尝试（记录在案）

第一版把空态做成带 `Q_OBJECT` 的 `EmptyState : QWidget`，并**塞进视图视口的新布局**。结果 `mlash_appearance_tests themeChangesPreserveDraftAndPopupFirstFrame` 在 `UiTheme::applyAppearance` 内稳定段错误（`QScrollbarStyleAnimation` 虚表损坏），且与改动前基线对比可复现。改成 overlay + eventFilter 后仍崩溃。最终放弃该结构，回到「标签 + 按钮」的最小改动，问题消失。

结论：**不要在 `QAbstractItemView::viewport()` 里再建一层布局来承载空态**；Qt 的样式重算会遍历到它。

## 4. 验收点

1. 订阅、节点、连接、规则的空态都有可聚焦动作，且文案与原因一致。
2. 动作点击产生正确的下一步（添加订阅 / 清除筛选 / 查看日志 / 刷新 / 启动内核）。
3. 旧 `emptyState()` 签名与既有调用点不变。
4. 完整 `ctest` 全绿。

## 5. 实际结果

**已实施并自动测试（2026-10-29）。**

`setEmptyStateAction()` 接在四处刷新点；新增 `mlash_ui_tests emptyStatesOfferFocusableNextStep`：订阅页空态动作文案为「添加订阅…」、`focusPolicy == StrongFocus`、点击后打开导入对话框；连接页空态动作可见且有文案、可聚焦。完整 `ctest` 34 项中 33 通过、0 失败、1 跳过。

**未验**：真实读屏下的焦点顺序；空态图标的视觉复核（当前沿用 `UiIcons::Glyph::Info`，未新增绘制规格）。
