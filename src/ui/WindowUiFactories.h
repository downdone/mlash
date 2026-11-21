#pragma once
#include "UiTheme.h"
#include "ChevronToolButton.h"

namespace mlash::WindowParts {
struct ProfileSetupCopy {
    QString title;
    QString action;
};
inline ProfileSetupCopy profileSetupCopy(bool hasProfiles) {
    return hasProfiles ? ProfileSetupCopy{"还未使用配置","选择配置"}
                       : ProfileSetupCopy{"还没有配置","导入配置"};
}
inline QPushButton* button(const QString& text, QLayout* layout) { auto* b = new QPushButton(text); layout->addWidget(b); return b; }
inline QToolButton* actionButton(QAction* action, QLayout* layout, const QString& name) {
    auto* control = new ChevronToolButton; control->setObjectName(name); control->setToolButtonStyle(Qt::ToolButtonTextOnly); control->setDefaultAction(action); layout->addWidget(control); return control;
}
// 空态（R7/U19）：说明与操作是同一个视觉单元，随视口居中，避免按钮贴底。
inline QLabel* emptyState(QAbstractItemView* table, const QString& text, bool nearTop=false) {
    auto* label = new QLabel(text, table->viewport()); label->setObjectName("emptyState"); label->setWordWrap(true); label->setAlignment(Qt::AlignCenter); label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum); label->setAttribute(Qt::WA_TransparentForMouseEvents); label->setTextFormat(Qt::PlainText);
    auto* action = new QPushButton(table->viewport()); action->setObjectName("emptyStateAction"); action->setProperty("quiet", true); action->setFocusPolicy(Qt::StrongFocus); action->hide();
    auto* layout = new QVBoxLayout(table->viewport()); layout->setContentsMargins(0,nearTop?28:0,0,0); layout->setSpacing(8);
    if (!nearTop) layout->addStretch(1);
    layout->addWidget(label, 0, Qt::AlignHCenter); layout->addWidget(action, 0, Qt::AlignHCenter); layout->addStretch(1); label->hide();
    return label;
}
// 空态动作按钮；调用方按当前原因设置文案与处理函数。文案为空即整块隐藏。
inline QPushButton* emptyStateAction(QAbstractItemView* table) {
    return table->viewport()->findChild<QPushButton*>("emptyStateAction");
}
inline void setEmptyStateAction(QAbstractItemView* table, const QString& text, const std::function<void()>& run) {
    auto* action = emptyStateAction(table); if (!action) return;
    action->setText(text); action->setVisible(!text.isEmpty());
    QObject::disconnect(action, &QPushButton::clicked, nullptr, nullptr);
    if (!text.isEmpty() && run) QObject::connect(action, &QPushButton::clicked, action, run);
}
inline QString ruleTargetValue(const QComboBox* box) {
    const int index = box->currentIndex();
    return index >= 0 && box->currentText() == box->itemText(index) ? box->itemData(index).toString() : box->currentText().trimmed();
}
inline void openDirectory(const QString& directory) { QDesktopServices::openUrl(QUrl::fromLocalFile(directory)); }
inline void styleTable(QTableView* table) {
    table->setAlternatingRowColors(false); table->setShowGrid(false); table->verticalHeader()->setDefaultSectionSize(UiTheme::TableRow);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setHighlightSections(false); const auto names=QHash<QString,QString>{{"nodesTable","代理成员列表"},{"profilesTable","订阅列表"},{"connectionsTable","实时连接列表"},{"activeRulesTable","内核规则列表"}}; table->setAccessibleName(names.value(table->objectName(),"数据列表"));
    auto palette = table->palette(); palette.setColor(QPalette::Inactive, QPalette::HighlightedText, palette.color(QPalette::Active, QPalette::HighlightedText)); table->setPalette(palette);
}
inline QFrame* panel() { auto* frame = new QFrame; frame->setObjectName("panel"); return frame; }
// K7：配置页的分组标题（原型 .section-heading / .section-title），右侧可挂工具按钮。
inline QWidget* sectionHeading(const QString& title, QLayout* tools = nullptr, QWidget* parent = nullptr) {
    auto* heading = new QWidget(parent); auto* layout = new QHBoxLayout(heading);
    layout->setContentsMargins(2, 0, 2, 0); layout->setSpacing(10);
    auto* label = new QLabel(title); label->setObjectName("sectionTitle"); layout->addWidget(label);
    layout->addStretch(1);
    if (tools) { auto* holder = new QWidget; holder->setLayout(tools); tools->setContentsMargins(0, 0, 0, 0); layout->addWidget(holder); }
    return heading;
}
}
