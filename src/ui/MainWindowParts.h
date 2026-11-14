#pragma once
#include "MainWindow.h"
#include "ViewIds.h"
#include "ProviderPolicyDialog.h"
#include "ProfileChangesDialog.h"
#include "DialogDraftGuard.h"
#include "ImportDialog.h"
#include "DownloadPolicyEditor.h"
#include "BackupDialog.h"
#include "LocalBackupDialog.h"
#include "RuleOverlayDialog.h"
#include "ConnectionDetailsDialog.h"
#include "DnsDialog.h"
#include "ProxyEditorDialog.h"
#include "ProxyChainDialog.h"
#include "ProtocolSettingsDialog.h"
#include "ProxyBypassDialog.h"
#include "CoreUpdateDialog.h"
#include "RemoteBackupDialog.h"
#include "TunDialog.h"
#include "GeoDataDialog.h"
#include "HomeMetricsView.h"
#include "OutboundDialog.h"
#include "HotkeysDialog.h"
#include "SettingsSearch.h"
#include "ConnectionOptions.h"
#include "TrayController.h"
#include "NodePresentation.h"
#include "WidgetStyle.h"
#include "UiTheme.h"
#include "PrototypeUi.h"
#include "WebsiteTestsPage.h"
#include "CompactComboBox.h"
#include "CompactSpinBox.h"
#include "UiIcons.h"
#include "profile/SubscriptionInfo.h"
#include "profile/RuleDraft.h"
#include "qrcodegen.hpp"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QStyle>
#include <QShortcut>
#include <QMenu>
#include <QScrollArea>
#include <QAction>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QGridLayout>
#include <QTabWidget>
#include <QToolButton>
#include <QTabBar>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QKeyEvent>
#include <functional>
#include <algorithm>
#include "ElidedLabel.h"



namespace mlash::WindowParts {
class DashboardCardButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
    QSize sizeHint() const override { auto size=QPushButton::sizeHint(); if (layout()) size.setHeight(qMax(size.height(),layout()->minimumSize().height())); return size; }
    QSize minimumSizeHint() const override { auto size=QPushButton::minimumSizeHint(); if (layout()) size.setHeight(qMax(size.height(),layout()->minimumSize().height())); return size; }
protected:
    bool event(QEvent* event) override {
        const auto handled=QPushButton::event(event);
        if ((event->type()==QEvent::LayoutRequest||event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange)&&layout()) { const int height=layout()->minimumSize().height(); if (height>0&&minimumHeight()!=height) setMinimumHeight(height); }
        return handled;
    }
};
class ChevronToolButton final : public QToolButton {
public:
    using QToolButton::QToolButton;
protected:
    void paintEvent(QPaintEvent* event) override {
        QToolButton::paintEvent(event);
        if ((objectName()!="testGroupButton" || !menu()) && objectName()!="browseGroup") return;
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(isEnabled()?QColor(UiTheme::Secondary):QColor(UiTheme::Disabled),1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const qreal x=width()-11,y=height()/2.0; p.drawLine(QPointF(x-3,y-1),QPointF(x,y+2)); p.drawLine(QPointF(x,y+2),QPointF(x+3,y-1));
    }
};
class MenuPushButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
    QSize sizeHint() const override { auto size=QPushButton::sizeHint(); size.setWidth(qMax(size.width(),fontMetrics().horizontalAdvance(text())+38)); return size; }
protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QStyleOptionButton option; initStyleOption(&option); option.text.clear(); QPainter background(this); style()->drawControl(QStyle::CE_PushButton,&option,&background,this); background.end();
        if (!menu()) return;
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(isEnabled()?QColor(UiTheme::Secondary):QColor(UiTheme::Muted),1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const qreal x=width()-12,y=height()/2.0; p.drawPolyline(QPolygonF{{x-3,y-1},{x,y+2},{x+3,y-1}});
        p.setFont(font()); p.setPen(QColor(isEnabled()?UiTheme::Ink:UiTheme::Muted)); const auto textRect=rect().adjusted(10,0,-28,0); p.drawText(textRect,Qt::AlignCenter,fontMetrics().elidedText(text(),Qt::ElideRight,textRect.width()));
    }
};
inline QPushButton* button(const QString& text, QLayout* layout) { auto* b = new QPushButton(text); layout->addWidget(b); return b; }
inline QToolButton* actionButton(QAction* action, QLayout* layout, const QString& name) {
    auto* control = new ChevronToolButton; control->setObjectName(name); control->setToolButtonStyle(Qt::ToolButtonTextOnly); control->setDefaultAction(action); layout->addWidget(control); return control;
}
// 空态（R7）：保持原有「视口内一个说明标签」的结构不变，只把说明拆成
// 「图标 + 原因」并在下方附一个可聚焦动作按钮。动作留空时隐藏，行为与旧版一致。
inline QLabel* emptyState(QAbstractItemView* table, const QString& text) {
    auto* label = new QLabel(text, table->viewport()); label->setObjectName("emptyState"); label->setWordWrap(true); label->setAlignment(Qt::AlignCenter); label->setAttribute(Qt::WA_TransparentForMouseEvents); label->setTextFormat(Qt::PlainText);
    auto* action = new QPushButton(table->viewport()); action->setObjectName("emptyStateAction"); action->setProperty("quiet", true); action->setFocusPolicy(Qt::StrongFocus); action->hide();
    auto* layout = new QVBoxLayout(table->viewport()); layout->setContentsMargins(0,0,0,0); layout->setSpacing(8); layout->addWidget(label); layout->addWidget(action, 0, Qt::AlignHCenter); label->hide();
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
inline void settingRow(QVBoxLayout* section, const QString& name, const QString& description, QWidget* control,bool stacked=false, const QString& currentValue = QString(), UiIcons::Glyph glyph = UiIcons::Glyph::Settings) {
    auto* row = new QWidget; row->setObjectName("settingRow"); row->setProperty("settingSearchName",name); row->setProperty("settingSearchTerms",name+" "+description);
    // K8：行首 21px 图标位（原型 .setting-icon）。下拉/数字框本身已经显示值，不再重复写在行里。
    auto* labels = new QVBoxLayout; labels->setSpacing(2); auto* title = new QLabel(name); title->setObjectName("settingName"); labels->addWidget(title);
    // 行内「当前值」（原型 05a/05b / 切片 V3）：值不在控件里的那些行（启动页面、TUN 状态、
    // 内核路径…）用一行次要文字说明现状，控件本身仍只负责「改」。
    // 文案为空表示这行的控件已经显示了值（下拉、数字框），不重复。
    QLabel* current = nullptr;
    if (!currentValue.isEmpty()) {
        current = new QLabel(currentValue); current->setObjectName("settingValue");
        current->setProperty("secondary", true); current->setTextFormat(Qt::PlainText);
        current->setWordWrap(false); current->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        current->setToolTip(currentValue);
        current->setAccessibleName(name + " 当前值");
        labels->addWidget(current);
    }
    const bool explain=QStringList{"启动时恢复代理开关","关闭窗口时释放界面"}.contains(name);
    if (!description.isEmpty()) { control->setToolTip(control->toolTip().isEmpty()?description:control->toolTip()+"\n"+description); }
    if (explain&&!description.isEmpty()) { auto* detail = new QLabel(description); detail->setObjectName("secondaryText"); detail->setWordWrap(true); detail->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); labels->addWidget(detail); }
    if (auto* button=qobject_cast<QPushButton*>(control)) button->setProperty("quiet",true);
    if (control->layout()) control->layout()->setSizeConstraint(QLayout::SetMinimumSize);
    control->setAccessibleName(name); control->setAccessibleDescription(description);
    row->setProperty("settingKind",stacked?"stacked":explain?"explained":"single");
    auto* icon = new QLabel; icon->setObjectName("settingIcon"); icon->setFixedSize(21,21); icon->setPixmap(UiIcons::icon(glyph,UiTheme::Brand).pixmap(16,16)); icon->setAlignment(Qt::AlignCenter);
    if (stacked) {
        auto* layout=new QVBoxLayout(row); layout->setContentsMargins(0,10,0,10); layout->setSpacing(8);
        auto* head=new QHBoxLayout; head->setSpacing(13); head->addWidget(icon,0,Qt::AlignVCenter); head->addLayout(labels,1);
        layout->addLayout(head); layout->addWidget(control);
    }
    else {
        auto* layout=new QHBoxLayout(row); layout->setContentsMargins(0,9,0,9); layout->setSpacing(13);
        layout->addWidget(icon,0,Qt::AlignVCenter); layout->addLayout(labels,1);
        // 右侧「行尾」：值（若有）+ 控件 + ›。
        auto* trail=new QHBoxLayout; trail->setSpacing(8); if (current) { labels->removeWidget(current); trail->addWidget(current,0,Qt::AlignVCenter); }
        trail->addWidget(control,0,Qt::AlignVCenter);
        // Form editors use their controls directly; directory chevrons belong to ActionRow only.
        layout->addLayout(trail);
        row->setMinimumHeight(qMax(UiTheme::SettingRowHeight,2*title->fontMetrics().height()+18));
    }
    section->addWidget(row);
}
// K7：配置页的一行（原型 .profile-row，76px）。
// 图标 37 + 标题（当前使用时跟一个 tag）+ 来源/时间 + 右侧用量条 + 行尾 ›。
// 行的动作不在这里：点击行或 › 都打开详情抽屉，行本身没有第二套按钮。
class ProfileRowDelegate final : public QStyledItemDelegate {
public:
    enum Role { IsCurrent = ProfileRowModel::IsCurrent, Kind = ProfileRowModel::Kind,
        Detail = ProfileRowModel::Detail, UsageRatio = ProfileRowModel::UsageRatio, UsageText = ProfileRowModel::UsageText };
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override { return {320,UiTheme::twoLineRowHeight(option.font,UiTheme::ProfileRowHeight)}; }
    static QRect chevronRect(const QRect& row) { return {row.right()-38,row.center().y()-14,28,28}; }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing); const auto rect=option.rect;
        if (option.state.testFlag(QStyle::State_MouseOver)) p->fillRect(rect,QColor(UiTheme::Hover));
        p->setPen(QColor(UiTheme::Separator)); p->drawLine(rect.left()+46,rect.bottom(),rect.right(),rect.bottom());
        const bool current=index.data(IsCurrent).toBool();
        UiIcons::icon(index.data(Kind).toString()=="remote"?UiIcons::Glyph::Globe:UiIcons::Glyph::Folder,UiTheme::Brand).paint(p,QRect(rect.left()+12,rect.center().y()-10,20,20));
        const auto caption=UiTheme::rowCaptionFont(option.font); const int left=rect.left()+46,width=qMax(0,rect.width()-(current?188:100)),line=QFontMetrics(option.font).height(),top=rect.center().y()-(line+QFontMetrics(caption).height()+1)/2;
        auto title=option.font; title.setBold(current); p->setFont(title); p->setPen(QColor(UiTheme::Ink));
        p->drawText(QRect(left,top,width,line),Qt::AlignVCenter,QFontMetrics(title).elidedText(index.data().toString(),Qt::ElideRight,width));
        auto small=UiTheme::rowCaptionFont(option.font); p->setFont(small); p->setPen(QColor(UiTheme::Secondary));
        const auto detail=index.data(Detail).toString();
        p->drawText(QRect(left,top+line+1,width,QFontMetrics(small).height()),Qt::AlignVCenter,QFontMetrics(small).elidedText(detail,Qt::ElideRight,width));
        if (current) UiIcons::icon(UiIcons::Glyph::Check,UiTheme::Brand).paint(p,QRect(rect.right()-133,rect.center().y()-8,16,16));
        UiIcons::icon(UiIcons::Glyph::ChevronRight,UiTheme::Secondary).paint(p,chevronRect(rect).adjusted(4,4,-4,-4));
        p->restore();
    }
};
class GroupCardDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override { return {154,54}; }
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        painter->save(); painter->setRenderHint(QPainter::Antialiasing); const auto rect=option.rect.adjusted(2,2,-4,-4); const bool selected=option.state.testFlag(QStyle::State_Selected);
        painter->setPen(QColor(selected?UiTheme::Brand:UiTheme::Border)); painter->setBrush(QColor(selected?UiTheme::Selection:UiTheme::Surface)); painter->drawRoundedRect(rect,6,6);
        auto font=option.font; font.setBold(selected); painter->setFont(font); painter->setPen(QColor(UiTheme::Ink)); const QFontMetrics metrics(font);
        painter->drawText(rect.adjusted(10,5,-8,-23),Qt::AlignLeft | Qt::AlignVCenter,metrics.elidedText(index.data().toString(),Qt::ElideRight,rect.width()-18));
        font.setBold(false); painter->setFont(font); painter->setPen(QColor(UiTheme::Secondary)); painter->drawText(rect.adjusted(10,25,-8,-3),Qt::AlignLeft | Qt::AlignVCenter,QString("%1 个成员").arg(index.data(Qt::UserRole+1).toInt())); painter->restore();
    }
};
inline QRect nodeDelayRect(const QRect& card,const QFont& font,const QString& value) {
    const int width=qMax(48,QFontMetrics(font).horizontalAdvance(value)+14);
    return {card.right()-width-5,card.center().y()-13,width,26};
}
class NodeNameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool helpEvent(QHelpEvent* event,QAbstractItemView* view,const QStyleOptionViewItem& option,const QModelIndex& index) override {
        if (!index.isValid()) return false;
        if (view->objectName()=="nodeCards" && nodeDelayRect(option.rect.adjusted(3,3,-5,-5),view->font(),index.siblingAtColumn(2).data().toString()).contains(event->pos())) {
            const auto action=view->property("nodeTestEnabled").toBool()?QString("点击重新测试此节点延迟；不会使用或切换节点"):QString("当前无法测速；等待任务结束或内核就绪"); QToolTip::showText(event->globalPos(),action+"<br>"+index.data(Qt::ToolTipRole).toString().toHtmlEscaped().replace('\n',"<br>"),view); return true;
        }
        const auto name=index.siblingAtColumn(0).data(Qt::UserRole).toString(); const bool group=index.siblingAtColumn(1).data().toString()=="策略组 ›";
        const auto region=NodePresentation::location(name);
        const auto hint=(group?QString("策略组 · "):QString())+(!region.code.isEmpty()?region.label+"（按名称识别）":QString("位置未识别"));
        QToolTip::showText(event->globalPos(),index.data(Qt::ToolTipRole).toString().toHtmlEscaped().replace('\n',"<br>")+"<br>"+hint,view); return true;
    }
protected:
    QIcon nodeIcon(const QModelIndex& index) const {
        const auto name=index.siblingAtColumn(0).data(Qt::UserRole).toString(); const bool group=index.siblingAtColumn(1).data().toString()=="策略组 ›";
        const auto key=NodePresentation::iconKey(name,group);
        if (!icons_.contains(key)) icons_.insert(key,NodePresentation::icon(name,group)); return icons_.value(key);
    }
    void initStyleOption(QStyleOptionViewItem* option,const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option,index); option->icon=nodeIcon(index); option->decorationSize={24,18}; option->features|=QStyleOptionViewItem::HasDecoration;
    }
private:
    mutable QHash<QString,QIcon> icons_;
};
class NodeCardDelegate final : public NodeNameDelegate {
public:
    using NodeNameDelegate::NodeNameDelegate;
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override { const auto* view=qobject_cast<QListView*>(parent()); return view?view->gridSize():QSize(220,54); }
    void paint(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing); const auto rect=option.rect.adjusted(3,3,-5,-5); const bool selected=option.state.testFlag(QStyle::State_Selected); const bool current=index.data(Qt::FontRole).value<QFont>().bold();
        const bool hover=option.state.testFlag(QStyle::State_MouseOver);
        p->setPen(QColor(selected?UiTheme::Brand:UiTheme::Border)); p->setBrush(QColor(selected?UiTheme::Selection:hover?UiTheme::Hover:UiTheme::Surface)); p->drawRoundedRect(rect,8,8);
        if (current) { p->setPen(Qt::NoPen); p->setBrush(QColor(UiTheme::Brand)); p->drawEllipse(QPointF(rect.left()+6,rect.center().y()),2,2); }
        nodeIcon(index).paint(p,QRect(rect.left()+14,rect.center().y()-9,24,18)); auto font=option.font; font.setBold(false); const auto delay=index.siblingAtColumn(2); const auto value=delay.data().toString(); const auto hit=nodeDelayRect(rect,font,value);
        const auto name=index.data(Qt::UserRole).toString(); const auto* view=qobject_cast<QListView*>(parent());
        if (view && view->property("nodeTestEnabled").toBool() && view->property("nodeDelayHover").toString()==name) { p->setPen(Qt::NoPen); p->setBrush(QColor(UiTheme::Selection)); p->drawRoundedRect(hit,4,4); }
        const bool group=index.siblingAtColumn(1).data().toString()=="策略组 ›";
        // 协议标签（V5 / 原型 01 的 node-proto）：节点行的类型此前只在表格视图的「类型」列可见，
        // 卡片视图里看不到。这里在名称右侧补一个小标签，宽度先扣掉它再算名称可用宽度。
        const auto protocol=group?QString():index.siblingAtColumn(1).data().toString();
        int protoWidth=0;
        if (!protocol.isEmpty() && protocol!="未知") { auto protoFont=option.font; protoFont.setBold(false); protoWidth=QFontMetrics(protoFont).horizontalAdvance(protocol)+10; }
        const int textWidth=qMax(0,hit.left()-rect.left()-52-(group?12:0)-protoWidth);
        font.setBold(current); p->setFont(font); p->setPen(QColor(UiTheme::Ink)); p->drawText(QRect(rect.left()+48,rect.top(),textWidth,rect.height()),Qt::AlignLeft | Qt::AlignVCenter,QFontMetrics(font).elidedText((index.data(Qt::UserRole+8).toBool()?QString("★ "):QString())+name,Qt::ElideRight,textWidth));
        if (group) { p->setPen(QPen(QColor(UiTheme::Muted),1.1,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); const qreal x=hit.left()-9,y=rect.center().y(); p->drawPolyline(QPolygonF{{x-2,y-3},{x+1,y},{x-2,y+3}}); }
        else if (protoWidth>0) { p->setPen(QColor(UiTheme::Muted)); p->drawText(QRect(hit.left()-protoWidth-4,rect.top(),protoWidth,rect.height()),Qt::AlignRight | Qt::AlignVCenter,protocol); }
        font.setBold(false); p->setFont(font); const auto brush=delay.data(Qt::ForegroundRole).value<QBrush>(); p->setPen(brush.style()==Qt::NoBrush?QColor(UiTheme::Secondary):brush.color()); p->drawText(hit.adjusted(4,0,-5,0),Qt::AlignRight | Qt::AlignVCenter,value);
        if (option.state.testFlag(QStyle::State_HasFocus) && view && view->property("keyboardFocus").toBool()) { p->setPen(QPen(QColor(UiTheme::Brand),1,Qt::DotLine)); p->setBrush(Qt::NoBrush); p->drawRoundedRect(rect.adjusted(2,2,-2,-2),4,4); } p->restore();
    }
};
class NodeCardView final : public QListView {
public:
    explicit NodeCardView(QWidget* parent=nullptr):QListView(parent) { setMouseTracking(true); }
    std::function<void(const QModelIndex&)> delayClicked;
    void setTestEnabled(bool enabled) { if (property("nodeTestEnabled").toBool()!=enabled) { setProperty("nodeTestEnabled",enabled); viewport()->setCursor(enabled && !property("nodeDelayHover").toString().isEmpty()?Qt::PointingHandCursor:Qt::ArrowCursor); viewport()->update(); } }
protected:
    void mouseMoveEvent(QMouseEvent* event) override { const auto index=delayAt(event->position().toPoint()); const auto name=index.isValid()?index.data(Qt::UserRole).toString():QString(); if (property("nodeDelayHover").toString()!=name) { setProperty("nodeDelayHover",name); viewport()->update(); } viewport()->setCursor(index.isValid() && property("nodeTestEnabled").toBool()?Qt::PointingHandCursor:Qt::ArrowCursor); QListView::mouseMoveEvent(event); }
    void mousePressEvent(QMouseEvent* event) override { delayPress_=event->button()==Qt::LeftButton && delayAt(event->position().toPoint()).isValid(); if (delayPress_) { pressedDelay_=delayAt(event->position().toPoint()); event->accept(); return; } pressedDelay_=QPersistentModelIndex{}; QListView::mousePressEvent(event); }
    void mouseReleaseEvent(QMouseEvent* event) override { if (event->button()==Qt::LeftButton && delayPress_) { delayPress_=false; const auto pressed=pressedDelay_; pressedDelay_=QPersistentModelIndex{}; if (pressed.isValid() && pressed==delayAt(event->position().toPoint()) && property("nodeTestEnabled").toBool() && delayClicked) delayClicked(pressed); event->accept(); return; } if (event->button()==Qt::LeftButton && delayAt(event->position().toPoint()).isValid()) { event->accept(); return; } QListView::mouseReleaseEvent(event); }
    void mouseDoubleClickEvent(QMouseEvent* event) override { if (delayAt(event->position().toPoint()).isValid()) { pressedDelay_=QPersistentModelIndex{}; event->accept(); return; } QListView::mouseDoubleClickEvent(event); }
    void resizeEvent(QResizeEvent* event) override { QListView::resizeEvent(event); adjustGrid(); }
    bool viewportEvent(QEvent* event) override { const bool result=QListView::viewportEvent(event); if (event->type()==QEvent::Resize) adjustGrid(); if (event->type()==QEvent::Leave) { setProperty("nodeDelayHover",QString()); viewport()->unsetCursor(); viewport()->update(); } return result; }
private:
    QPersistentModelIndex pressedDelay_;
    bool delayPress_=false;
    QModelIndex delayAt(const QPoint& point) const { const auto index=indexAt(point); if (!index.isValid()) return {}; return nodeDelayRect(visualRect(index).adjusted(3,3,-5,-5),font(),index.siblingAtColumn(2).data().toString()).contains(point)?index:QModelIndex{}; }
    void adjustGrid() {
        // QListView reserves scrollbar space even before its handle becomes visible.
        const int extent=style()->pixelMetric(QStyle::PM_ScrollBarExtent,nullptr,verticalScrollBar());
        const int width=qMax(1,maximumViewportSize().width()-extent-3),columns=qMax(1,width/220);
        const QSize size(qMax(1,width/columns-1),54);
        if (gridSize()!=size) { setGridSize(size); if (model() && model()->rowCount()>0) itemDelegate()->sizeHintChanged(model()->index(0,0)); }
    }
};
class ProxySwitch final : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
    // K 版（U08 §7.3）：轨道 46×26、旋钮 20px，与原型 .switch 一致。
    static constexpr int TrackWidth = 46, TrackHeight = 26, Knob = 20, Inset = 3;
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text()) + 62, qMax(28, TrackHeight + 2)}; }
protected:
    void focusInEvent(QFocusEvent* event) override { keyboardFocus_=event->reason()==Qt::TabFocusReason || event->reason()==Qt::BacktabFocusReason || event->reason()==Qt::ShortcutFocusReason; QCheckBox::focusInEvent(event); update(); }
    void focusOutEvent(QFocusEvent* event) override { keyboardFocus_=false; QCheckBox::focusOutEvent(event); update(); }
    void mousePressEvent(QMouseEvent* event) override { keyboardFocus_=false; QCheckBox::mousePressEvent(event); update(); }
    bool hitButton(const QPoint& point) const override { return rect().contains(point); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(isEnabled() ? QColor(UiTheme::Ink) : QColor(UiTheme::Muted)); p.drawText(QRect(0, 0, width() - 56, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        const QRectF track(width() - TrackWidth, (height() - TrackHeight) / 2.0, TrackWidth, TrackHeight);
        p.setPen(Qt::NoPen); p.setBrush(isChecked() ? QColor(isEnabled() ? UiTheme::Primary : UiTheme::Disabled) : QColor(UiTheme::Track)); p.drawRoundedRect(track, TrackHeight / 2.0, TrackHeight / 2.0);
        p.setBrush(QColor(UiTheme::OnPrimary)); p.drawEllipse(QPointF(track.left() + (isChecked() ? TrackWidth - Inset - Knob / 2.0 : Inset + Knob / 2.0), track.center().y()), Knob / 2.0, Knob / 2.0);
        if (hasFocus() && (keyboardFocus_ || property("keyboardFocus").toBool())) { p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(UiTheme::Focus), 1)); p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4); }
    }
private:
    bool keyboardFocus_=false;
};
// K3：右侧详情抽屉（原型 .drawer）。它是主窗口上的定位覆盖层，不是独立弹窗：
// 贴右缘、随窗口缩放、Esc 关闭。节点详情、配置详情共用同一外壳，内容由调用方填。
class DetailsDrawer final : public QFrame {
    Q_OBJECT
public:
    explicit DetailsDrawer(QWidget* host) : QFrame(host), host_(host) {
        setObjectName("nodeDetailsDrawer"); setFrameShape(QFrame::NoFrame); pageHost_=host->findChild<QStackedWidget*>("mainPages"); setFocusPolicy(Qt::StrongFocus);
        auto* outer = new QVBoxLayout(this); outer->setContentsMargins(20,18,20,18); outer->setSpacing(0);
        auto* header = new QHBoxLayout; header->setSpacing(10); auto* titles = new QVBoxLayout; titles->setSpacing(3);
        title_ = new QLabel; title_->setObjectName("drawerTitle"); title_->setWordWrap(true);
        subtitle_ = new QLabel; subtitle_->setObjectName("drawerSub"); subtitle_->setWordWrap(true); subtitle_->hide();
        titles->addWidget(title_); titles->addWidget(subtitle_); header->addLayout(titles, 1);
        auto* close = new QToolButton; close->setObjectName("drawerClose"); close->setIcon(UiIcons::icon(UiIcons::Glyph::Close)); close->setToolTip(QStringLiteral("关闭")); close->setAutoRaise(true);
        connect(close, &QToolButton::clicked, this, &DetailsDrawer::closeDrawer);
        header->addWidget(close, 0, Qt::AlignTop);
        outer->addLayout(header); outer->addSpacing(16);
        auto* scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* body=new QWidget; body_=new QVBoxLayout(body); body_->setContentsMargins(0,0,0,0); body_->setSpacing(0); body_->addStretch(1); scroll->setWidget(body); outer->addWidget(scroll,1);
        actions_ = new QHBoxLayout; actions_->setSpacing(8); actions_->addStretch(1); outer->addSpacing(22); outer->addLayout(actions_);
        hide(); host_->installEventFilter(this);
    }
    void setHeader(const QString& title, const QString& subtitle = QString()) { title_->setText(title); subtitle_->setText(subtitle); subtitle_->setVisible(!subtitle.isEmpty()); }
    QLabel* addKeyValue(const QString& key, const QString& value) {
        auto* row = new QWidget; row->setObjectName("drawerKeyValue");
        auto* layout = new QHBoxLayout(row); layout->setContentsMargins(0, 11, 0, 11); layout->setSpacing(12);
        auto* keyLabel = new QLabel(key); keyLabel->setObjectName("drawerKey"); layout->addWidget(keyLabel);
        auto* valueLabel = new QLabel(value.isEmpty() ? QStringLiteral("—") : value); valueLabel->setObjectName("drawerValue");
        valueLabel->setTextFormat(Qt::PlainText); valueLabel->setWordWrap(true); valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter); layout->addWidget(valueLabel, 1);
        body_->insertWidget(qMax(0,body_->count()-1),row); return valueLabel;
    }
    void addHint(const QString& text) { auto* hint = new QLabel(text); hint->setObjectName("drawerHint"); hint->setProperty("secondary", true); hint->setWordWrap(true); hint->setTextFormat(Qt::PlainText); body_->insertWidget(qMax(0,body_->count()-1),hint); }
    QHBoxLayout* actions() const { return actions_; }
    void clearActions() { while (actions_->count() > 1) { auto* item = actions_->takeAt(0); if (auto* widget = item->widget()) widget->deleteLater(); delete item; } }
    void clearContent() { clearActions(); while (auto* item = body_->takeAt(0)) { if (auto* widget = item->widget()) widget->deleteLater(); delete item; } body_->addStretch(1); }
    void addWidget(QWidget* widget) { body_->insertWidget(qMax(0,body_->count()-1),widget); }
    void open() { previousFocus_=QApplication::focusWidget(); syncGeometry(); show(); raise(); setFocus(); }
    void closeDrawer() { hide(); restorePage(); if (previousFocus_) previousFocus_->setFocus(Qt::OtherFocusReason); }
    void toggle() { isVisible() ? closeDrawer() : open(); }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == host_ && isVisible() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) syncGeometry();
        return QFrame::eventFilter(watched, event);
    }
    void keyPressEvent(QKeyEvent* event) override { if (event->key() == Qt::Key_Escape) { closeDrawer(); return; } QFrame::keyPressEvent(event); }
private:
    void restorePage() { if (pageHost_) { pageHost_->setContentsMargins(UiTheme::ContentInset,UiTheme::ContentTop,UiTheme::ContentInset,UiTheme::ContentTop); pageHost_->show(); } }
    void hideEvent(QHideEvent* event) override { restorePage(); QFrame::hideEvent(event); }
    void syncGeometry() {
        const int top=UiTheme::TopbarHeight+UiTheme::ContentInset;
        const bool wide=host_->width()-2*UiTheme::ContentInset>=860;
        const int width=wide?UiTheme::DrawerWidth:host_->width()-2*UiTheme::ContentInset;
        setProperty("inspectorMode",wide?"side":"page");
        if (pageHost_) { pageHost_->setContentsMargins(UiTheme::ContentInset,UiTheme::ContentTop,wide?UiTheme::DrawerWidth+2*UiTheme::ContentInset:UiTheme::ContentInset,UiTheme::ContentTop); pageHost_->setVisible(wide); }
        setGeometry(host_->width()-UiTheme::ContentInset-width,top,width,qMax(1,host_->height()-top-UiTheme::ContentInset-30));
    }
    QWidget* host_;
    QPointer<QStackedWidget> pageHost_;
    QPointer<QWidget> previousFocus_;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QVBoxLayout* body_ = nullptr;
    QHBoxLayout* actions_ = nullptr;
};
}

namespace mlash {
// 节点行列表（H3 / 原型 01）：一张列表卡片里两段「手动选择 / 自动策略组」。
// 分组不是独立模型：过滤结果（NodeFilter）提供成员，NodeRowModel 把它们拍平成
// 「组头行 + 成员行」交给 QListView。折叠、计数与选路说明因此只在一处计算。
class NodeRowModel final : public QAbstractTableModel {
public:
    // data() 的自定义角色：委托据此区分行类型并取副标题。
    enum Role {
        RowKind = Qt::UserRole + 20,   // 0 组头 / 1 成员
        IsAutoGroup, IsCurrent, RegionCode, LatencyText, Subtitle, GroupNote, GroupCollapsed, SourceName, SourceStamp
    };
    explicit NodeRowModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    // source 为 NodeFilter（任何以 UserRole 0 列给出成员名的模型都可以）。
    void setSource(QAbstractItemModel* source) {
        source_ = source;
        if (source_) {
            connect(source_, &QAbstractItemModel::modelReset, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::layoutChanged, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::rowsInserted, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::rowsRemoved, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::dataChanged, this, [this] { if (!rows_.isEmpty()) emit dataChanged(index(0, 0), index(rows_.size() - 1, 0)); });
        }
        rebuild();
    }
    // 组类型决定整段归入「手动选择」还是「自动策略组」（Selector 手动，URLTest/Fallback 自动）。
    void setGroupAutomatic(bool automatic) { if (groupAutomatic_ == automatic) return; groupAutomatic_ = automatic; rebuild(); }
    void setGroupContext(const QString& name,const QString& current) { if (groupName_==name&&groupCurrent_==current) return; groupName_=name; groupCurrent_=current; if (!rows_.isEmpty()) emit dataChanged(index(0,0),index(0,0)); }
    void setSourceContext(const QString& name,const QString& stamp) { sourceName_=name; sourceStamp_=stamp; if (!rows_.isEmpty()) emit dataChanged(index(0,0),index(0,0)); }
    bool groupAutomatic() const { return groupAutomatic_; }
    void setCollapsed(bool automaticGroup, bool collapsed) {
        auto& value = automaticGroup ? autoCollapsed_ : manualCollapsed_;
        if (value == collapsed) return; value = collapsed; rebuild();
    }
    bool collapsed(bool automaticGroup) const { return automaticGroup ? autoCollapsed_ : manualCollapsed_; }
    void setCurrent(const QString& name) { if (current_ == name) return; current_ = name; if (!rows_.isEmpty()) emit dataChanged(index(0, 0), index(rows_.size() - 1, 0)); }
    QString current() const { return current_; }
    bool rowIsHeader(int row) const { return rows_.value(row).member < 0; }
    bool rowAutomatic(int row) const { return rows_.value(row).group == 1; }
    QString memberAt(int row) const { const auto& value = rows_.value(row); return value.member < 0 ? QString() : members_.value(value.member); }
    int rowOfMember(const QString& name) const { const int member = members_.indexOf(name); if (member < 0) return -1; for (int i = 0; i < rows_.size(); ++i) if (rows_[i].member == member) return i; return -1; }
    QModelIndex indexForRow(int row) const { return index(row, 0); }
    void rebuild() {
        beginResetModel();
        members_.clear();
        if (source_) for (int row = 0; row < source_->rowCount(); ++row) members_.append(source_->data(source_->index(row, 0), Qt::UserRole).toString());
        rows_.clear();
        // 空段不留组头：只有成员非空才写组头。
        const bool automatic = groupAutomatic_;
        if (!members_.isEmpty()) {
            rows_.append({automatic ? 1 : 0, -1});
            if (!collapsed(automatic)) for (int i = 0; i < members_.size(); ++i) rows_.append({automatic ? 1 : 0, i});
        }
        endResetModel();
    }
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : rows_.size(); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 1; }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= rows_.size()) return {};
        const auto& row = rows_[index.row()];
        const bool automatic = row.group == 1;
        if (row.member < 0) {
            switch (role) {
            case RowKind: return 0;
            case Qt::DisplayRole: return groupName_.isEmpty()?(automatic?QString("自动策略组"):QString("手动选择")):groupName_;
            case SourceName: return sourceName_;
            case SourceStamp: return sourceStamp_;
            case GroupNote: return QString("%1 个").arg(members_.size());
            case GroupCollapsed: return collapsed(automatic);
            case Subtitle: return groupCurrent_.isEmpty()?QString("尚未选定成员"):QString("当前 → ")+groupCurrent_;
            case IsAutoGroup: return automatic;
            default: return {};
            }
        }
        const auto name = members_.value(row.member);
        // members_ 与 source_ 的行是同序一一对应（rebuild 按行号填充），所以 row.member
        // 就是源行号：这里必须直接用它，按名字线性查找会让渲染退化成 O(n²)。
        const auto column = [this, &row](int value) { return source_->index(row.member, value); };
        switch (role) {
        case RowKind: return 1;
        case IsAutoGroup: return automatic;
        case IsCurrent: return name == current_;
        case RegionCode: return NodePresentation::location(name).code;
        case LatencyText: return source_->data(column(2), Qt::DisplayRole);
        case Subtitle: {
            const auto type = source_->data(column(1), Qt::DisplayRole).toString();
            return type == QStringLiteral("策略组 ›") ? QString("子策略组") : type;
        }
        case Qt::AccessibleTextRole: return name+" · "+data(index,Subtitle).toString()+" · "+data(index,LatencyText).toString()+(name==current_?QString(" · 当前成员"):QString());
        case Qt::AccessibleDescriptionRole: return automatic?QString("自动选路，Enter 不修改成员；F5 测试，Ctrl+I 查看详情"):QString("Enter 选择成员，F5 测试，Ctrl+I 查看详情");
        case Qt::DisplayRole: return name;
        case Qt::ToolTipRole: return source_->data(column(0), Qt::ToolTipRole);
        default: return {};
        }
    }
private:
    struct Row { int group = 0; int member = -1; };
    QAbstractItemModel* source_ = nullptr;
    QStringList members_;
    QVector<Row> rows_;
    QString current_,groupName_,groupCurrent_,sourceName_,sourceStamp_;
    bool groupAutomatic_ = false, manualCollapsed_ = false, autoCollapsed_ = false;
};

// Compact two-line rows expand from actual title/caption metrics (ADR 0053).
class NodeRowDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        return {10, UiTheme::twoLineRowHeight(option.font)};
    }
    static QRect infoRect(const QRect& row) { return {row.right()-38,row.center().y()-14,28,28}; }
    static QRect delayRect(const QRect& row) { return {row.right()-108,row.top()+3,62,row.height()-6}; }
    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const QRect rect=option.rect;
        if (index.data(NodeRowModel::RowKind).toInt()==0) { paintHeader(p,option,index); p->restore(); return; }
        const bool current=index.data(NodeRowModel::IsCurrent).toBool();
        if (option.state.testFlag(QStyle::State_MouseOver)) p->fillRect(rect,QColor(UiTheme::Hover));
        p->setPen(QColor(UiTheme::Separator)); p->drawLine(rect.left()+46,rect.bottom(),rect.right(),rect.bottom());
        const QRect badge(rect.left()+12,rect.center().y()-11,22,22);
        const auto name=index.data(Qt::DisplayRole).toString();
        NodePresentation::icon(name,index.data(NodeRowModel::Subtitle).toString().contains("策略组")).paint(p,badge);
        auto small=UiTheme::rowCaptionFont(option.font);
        const auto latency=index.data(NodeRowModel::LatencyText).toString();
        auto color=QColor(UiTheme::Secondary);
        bool numeric=false; const int ms=latency.section(' ',0,0).toInt(&numeric);
        if (numeric) color=QColor(ms<=300?UiTheme::Success:UiTheme::Warning);
        if (latency.contains("失败")||latency.contains("超时")) color=QColor(UiTheme::Error);
        if (const auto* view=qobject_cast<const QListView*>(option.widget); view&&view->property("nodeTestEnabled").toBool()&&view->property("nodeDelayHover").toString()==name) {
            p->setPen(Qt::NoPen); p->setBrush(QColor(UiTheme::Control)); p->drawRoundedRect(delayRect(rect),5,5);
        }
        p->setFont(small); p->setPen(color);
        p->drawText(delayRect(rect).adjusted(3,0,-3,0),Qt::AlignRight|Qt::AlignVCenter,latency.isEmpty()?QStringLiteral("—"):latency);
        if (current) UiIcons::icon(UiIcons::Glyph::Check,UiTheme::Brand).paint(p,QRect(rect.right()-133,rect.center().y()-8,16,16));
        UiIcons::icon(UiIcons::Glyph::Info,UiTheme::Brand).paint(p,infoRect(rect).adjusted(4,4,-4,-4));
        const int left=rect.left()+46, width=qMax(0,delayRect(rect).left()-left-(current?30:8));
        auto title=option.font; title.setBold(current); p->setFont(title); p->setPen(QColor(UiTheme::Ink));
        const int line=QFontMetrics(title).height(),caption=QFontMetrics(small).height(); const int top=rect.center().y()-(line+caption+1)/2;
        p->drawText(QRect(left,top,width,line),Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(title).elidedText(name,Qt::ElideRight,width));
        p->setFont(small); p->setPen(QColor(UiTheme::Secondary));
        const auto sub=index.data(NodeRowModel::IsAutoGroup).toBool()?QString("自动选路 · 不可手动切换"):index.data(NodeRowModel::Subtitle).toString();
        p->drawText(QRect(left,top+line+1,width,QFontMetrics(small).height()),Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(small).elidedText(sub,Qt::ElideRight,width));
        if (option.state.testFlag(QStyle::State_HasFocus)&&option.widget&&option.widget->property("keyboardFocus").toBool()) { p->setPen(QPen(QColor(UiTheme::Focus),1)); p->setBrush(Qt::NoBrush); p->drawRoundedRect(rect.adjusted(1,1,-2,-2),5,5); }
        p->restore();
    }
private:
    static void paintHeader(QPainter* p,const QStyleOptionViewItem& option,const QModelIndex& index) {
        const auto rect=option.rect; p->fillRect(rect,QColor(UiTheme::Surface)); p->setPen(QColor(UiTheme::Separator)); p->drawLine(rect.left()+46,rect.bottom(),rect.right(),rect.bottom());
        UiIcons::icon(index.data(NodeRowModel::GroupCollapsed).toBool()?UiIcons::Glyph::ChevronRight:UiIcons::Glyph::ChevronDown,UiTheme::Brand).paint(p,QRect(rect.left()+12,rect.center().y()-10,20,20));
        auto title=option.font; title.setBold(true); auto small=UiTheme::rowCaptionFont(option.font); const int line=QFontMetrics(title).height(),left=rect.left()+46,width=qMax(0,rect.width()-175),top=rect.center().y()-(line+QFontMetrics(small).height()+1)/2;
        p->setFont(title); p->setPen(QColor(UiTheme::Ink)); p->drawText(QRect(left,top,width,line),Qt::AlignVCenter,QFontMetrics(title).elidedText(index.data(NodeRowModel::SourceName).toString().isEmpty()?index.data().toString():index.data(NodeRowModel::SourceName).toString(),Qt::ElideRight,width));
        p->setFont(small); p->setPen(QColor(UiTheme::Secondary));
        p->drawText(QRect(left,top+line+1,width,QFontMetrics(small).height()),Qt::AlignVCenter,QFontMetrics(small).elidedText(index.data(NodeRowModel::SourceStamp).toString().isEmpty()?index.data().toString():index.data(NodeRowModel::SourceStamp).toString(),Qt::ElideRight,width));
        p->drawText(rect.adjusted(0,0,-46,0),Qt::AlignRight|Qt::AlignVCenter,index.data(NodeRowModel::GroupNote).toString());
        UiIcons::icon(UiIcons::Glyph::Settings,UiTheme::Brand).paint(p,infoRect(rect).adjusted(4,4,-4,-4));
    }
};
// Delay is an independent hit target: consume press/release before QListView selection.
class NodeRowView final : public QListView {
public:
    explicit NodeRowView(QWidget* parent=nullptr):QListView(parent) { setMouseTracking(true); }
    std::function<void(const QModelIndex&)> delayClicked,infoClicked;
    void setTestEnabled(bool enabled) { setProperty("nodeTestEnabled",enabled); viewport()->update(); }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QListView::resizeEvent(event);
        QPainterPath clip; clip.addRoundedRect(QRectF(viewport()->rect()),10,10);
        viewport()->setMask(QRegion(clip.toFillPolygon().toPolygon()));
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        const auto index=delayAt(event->position().toPoint());
        setProperty("nodeDelayHover",index.isValid()?index.data().toString():QString());
        viewport()->setCursor((infoAt(event->position().toPoint()).isValid()||(index.isValid()&&property("nodeTestEnabled").toBool()))?Qt::PointingHandCursor:Qt::ArrowCursor);
        viewport()->update(); QListView::mouseMoveEvent(event);
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button()==Qt::LeftButton&&infoAt(event->position().toPoint()).isValid()) { pressedInfo_=infoAt(event->position().toPoint()); event->accept(); return; }
        if (event->button()==Qt::LeftButton && delayAt(event->position().toPoint()).isValid()) { pressed_=delayAt(event->position().toPoint()); event->accept(); return; }
        pressed_=QPersistentModelIndex{}; QListView::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button()==Qt::LeftButton&&(pressedInfo_.isValid()||infoAt(event->position().toPoint()).isValid())) {
            const auto pressed=pressedInfo_; pressedInfo_=QPersistentModelIndex{};
            if (pressed.isValid()&&pressed==infoAt(event->position().toPoint())&&infoClicked) infoClicked(pressed);
            event->accept(); return;
        }
        if (event->button()==Qt::LeftButton && (pressed_.isValid()||delayAt(event->position().toPoint()).isValid())) {
            const auto pressed=pressed_; pressed_=QPersistentModelIndex{};
            if (pressed.isValid()&&pressed==delayAt(event->position().toPoint())&&property("nodeTestEnabled").toBool()&&delayClicked) delayClicked(pressed);
            event->accept(); return;
        }
        QListView::mouseReleaseEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override { if (delayAt(event->position().toPoint()).isValid()) { event->accept(); return; } QListView::mouseDoubleClickEvent(event); }
    bool viewportEvent(QEvent* event) override {
        if (event->type()==QEvent::Leave) { setProperty("nodeDelayHover",QString()); viewport()->unsetCursor(); viewport()->update(); }
        if (event->type()==QEvent::ToolTip) {
            auto* help=static_cast<QHelpEvent*>(event);
            if (delayAt(help->pos()).isValid()) { QToolTip::showText(help->globalPos(),property("nodeTestEnabled").toBool()?QString("重新测试此节点延迟，不切换节点"):QString("当前无法测速，请等待内核就绪或任务结束"),this); return true; }
        }
        return QListView::viewportEvent(event);
    }
private:
    QPersistentModelIndex pressed_,pressedInfo_;
    QModelIndex infoAt(const QPoint& point) const { const auto index=indexAt(point); return index.isValid()&&NodeRowDelegate::infoRect(visualRect(index)).contains(point)?index:QModelIndex{}; }
    QModelIndex delayAt(const QPoint& point) const {
        const auto index=indexAt(point);
        return index.isValid()&&index.data(NodeRowModel::RowKind).toInt()!=0&&NodeRowDelegate::delayRect(visualRect(index)).contains(point)?index:QModelIndex{};
    }
};

}

namespace mlash::WindowParts {
enum class NavKind { Home, Config, Data, Settings };
// 分区归属按视图常量判断（ViewIds.h）。数据页的四个页签共用同一个视图，因此都归「数据」。
inline NavKind navKindOfView(int view) {
    switch (view) {
    case ViewIds::Home: return NavKind::Home;
    case ViewIds::Profiles: case ViewIds::Rules: return NavKind::Config;
    case ViewIds::Data: return NavKind::Data;
    case ViewIds::Settings: return NavKind::Settings;
    default: return NavKind::Home;
    }
}
// 唯一图标入口（6.2c）：导航与页面共用 UiIcons，不再各画一套描边。
// Selected 态用 OnPrimary，其余用 Secondary —— 与原实现一致，只换成统一绘制器。
inline QIcon navigationIcon(NavKind kind) {
    const auto glyph = kind == NavKind::Home ? UiIcons::Glyph::Home
        : kind == NavKind::Config ? UiIcons::Glyph::Folder
        : kind == NavKind::Data ? UiIcons::Glyph::Chart : UiIcons::Glyph::Settings;
    auto icon=UiIcons::icon(glyph,UiTheme::Brand);
    const auto selected=UiIcons::icon(glyph,UiTheme::OnPrimary);
    for (const int size:{24,48}) icon.addPixmap(selected.pixmap(size,size),QIcon::Selected,QIcon::Off);
    return icon;
}
// K1：顶栏文案的旁表。MainWindow.h 不能反向包含本文件（本文件包含它），
// 因此顶栏控件登记在 WindowParts 自己的表里，updateTopbar() 统一写。
// 用 QPointer：MainWindow 析构时标签先被删除，表项自动置空，不会留下悬空指针。
struct TopbarInfo { QPointer<QLabel> title, subtitle; QPointer<QWidget> status; QPointer<QLabel> dot, statusText; };
inline QHash<QWidget*, TopbarInfo>& topbarRegistry() { static QHash<QWidget*, TopbarInfo> registry; return registry; }
inline void registerTopbar(QWidget* window, const TopbarInfo& info) { topbarRegistry().insert(window, info); }
inline void updateTopbar(QWidget* window, const QString& title, const QString& subtitle, const QString& status, const QString& dotState) {
    const auto it = topbarRegistry().find(window); if (it == topbarRegistry().end()) return;
    const auto& info = *it;
    if (info.title) info.title->setText(title);
    if (info.subtitle) { info.subtitle->setText(subtitle); info.subtitle->setVisible(!subtitle.isEmpty()); }
    if (info.statusText) info.statusText->setText(status);
    if (info.dot) { info.dot->setProperty("state", dotState); info.dot->style()->unpolish(info.dot); info.dot->style()->polish(info.dot); }
}
}
