#pragma once
#include <QStyledItemDelegate>

namespace mlash {

// 订阅用量条（原型 04 / 切片 V2）。
//
// 订阅的 HTTP 头里已经带了上传/下载/总量（SubscriptionInfo），但界面只把它拼成
// 一句文字挂在提示里。这里把「已用 / 总量」画成一条细条，未提供额度时整列隐藏。
// 绘制只读 UiTheme token，不写字面颜色（分层设计 6.3 规则 7）。
class UsageDelegate final : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit UsageDelegate(QObject* parent = nullptr);
    // 该列的「已用比例」由 item 的 UserRole 传入（0.0–1.0），UserRole+1 为显示文本。
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace mlash
