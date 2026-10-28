#pragma once
#include "ProviderPolicyDialog.h"
#include "ProfileChangesDialog.h"
#include "MainWindow.h"
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
#include "HomeSummaryDialog.h"
#include "ConnectionOptions.h"
#include "TrayController.h"
#include "NodePresentation.h"
#include "WidgetStyle.h"
#include "UiTheme.h"
#include "WebsiteTestsPage.h"
#include "CompactComboBox.h"
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
        if (objectName()!="testGroupButton" || !menu()) return;
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
inline QLabel* emptyState(QAbstractItemView* table, const QString& text) {
    auto* label = new QLabel(text, table->viewport()); label->setObjectName("emptyState"); label->setWordWrap(true); label->setAlignment(Qt::AlignCenter); label->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* layout = new QVBoxLayout(table->viewport()); layout->setContentsMargins(0,0,0,0); layout->addWidget(label); label->hide(); return label;
}
inline QString ruleTargetValue(const QComboBox* box) {
    const int index = box->currentIndex();
    return index >= 0 && box->currentText() == box->itemText(index) ? box->itemData(index).toString() : box->currentText().trimmed();
}
inline void openDirectory(const QString& directory) { QDesktopServices::openUrl(QUrl::fromLocalFile(directory)); }
inline void heading(QVBoxLayout* layout, const QString& title, const QString& subtitle) {
    auto* row = new QHBoxLayout; auto* label = new QLabel(title); label->setObjectName("pageTitle"); row->addWidget(label);
    auto* detail = new QLabel(subtitle); detail->setObjectName("secondaryText"); detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); detail->setToolTip(subtitle); row->addWidget(detail, 1); layout->addLayout(row);
}
inline void styleTable(QTableView* table) {
    table->setAlternatingRowColors(false); table->setShowGrid(false); table->verticalHeader()->setDefaultSectionSize(UiTheme::TableRow);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setHighlightSections(false); const auto names=QHash<QString,QString>{{"nodesTable","代理成员列表"},{"profilesTable","订阅列表"},{"connectionsTable","实时连接列表"},{"activeRulesTable","内核规则列表"}}; table->setAccessibleName(names.value(table->objectName(),"数据列表"));
    auto palette = table->palette(); palette.setColor(QPalette::Inactive, QPalette::HighlightedText, palette.color(QPalette::Active, QPalette::HighlightedText)); table->setPalette(palette);
}
inline QFrame* panel() { auto* frame = new QFrame; frame->setObjectName("panel"); return frame; }
inline void settingRow(QVBoxLayout* section, const QString& name, const QString& description, QWidget* control,bool stacked=false) {
    auto* row = new QWidget; row->setObjectName("settingRow"); row->setProperty("settingSearchName",name); row->setProperty("settingSearchTerms",name+" "+description);
    auto* labels = new QVBoxLayout; labels->setSpacing(2); auto* title = new QLabel(name); title->setObjectName("settingName"); labels->addWidget(title);
    const bool explain=QStringList{"启动时恢复代理开关","关闭窗口时释放界面"}.contains(name);
    if (!description.isEmpty()) { control->setToolTip(control->toolTip().isEmpty()?description:control->toolTip()+"\n"+description); }
    if (explain&&!description.isEmpty()) { auto* detail = new QLabel(description); detail->setObjectName("secondaryText"); detail->setWordWrap(true); detail->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); labels->addWidget(detail); }
    if (auto* button=qobject_cast<QPushButton*>(control)) button->setProperty("quiet",true);
    if (control->layout()) control->layout()->setSizeConstraint(QLayout::SetMinimumSize);
    control->setAccessibleName(name); control->setAccessibleDescription(description);
    row->setProperty("settingKind",stacked?"stacked":explain?"explained":"single");
    if (stacked) { auto* layout=new QVBoxLayout(row); layout->setContentsMargins(0,10,0,10); layout->setSpacing(8); layout->addLayout(labels); layout->addWidget(control); }
    else { auto* layout=new QHBoxLayout(row); layout->setContentsMargins(0,8,0,8); layout->setSpacing(12); layout->addLayout(labels,1); layout->addWidget(control,0,Qt::AlignRight | Qt::AlignVCenter); row->setMinimumHeight(explain?qMax(56,2*title->fontMetrics().height()+16):44); }
    section->addWidget(row);
}
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
        const int textWidth=qMax(0,hit.left()-rect.left()-52-(group?12:0));
        font.setBold(current); p->setFont(font); p->setPen(QColor(UiTheme::Ink)); p->drawText(QRect(rect.left()+48,rect.top(),textWidth,rect.height()),Qt::AlignLeft | Qt::AlignVCenter,QFontMetrics(font).elidedText((index.data(Qt::UserRole+8).toBool()?QString("★ "):QString())+name,Qt::ElideRight,textWidth));
        if (group) { p->setPen(QPen(QColor(UiTheme::Muted),1.1,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); const qreal x=hit.left()-9,y=rect.center().y(); p->drawPolyline(QPolygonF{{x-2,y-3},{x+1,y},{x-2,y+3}}); }
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
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text()) + 54, qMax(28, fontMetrics().height() + 8)}; }
protected:
    void focusInEvent(QFocusEvent* event) override { keyboardFocus_=event->reason()==Qt::TabFocusReason || event->reason()==Qt::BacktabFocusReason || event->reason()==Qt::ShortcutFocusReason; QCheckBox::focusInEvent(event); update(); }
    void focusOutEvent(QFocusEvent* event) override { keyboardFocus_=false; QCheckBox::focusOutEvent(event); update(); }
    void mousePressEvent(QMouseEvent* event) override { keyboardFocus_=false; QCheckBox::mousePressEvent(event); update(); }
    bool hitButton(const QPoint& point) const override { return rect().contains(point); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(isEnabled() ? QColor(UiTheme::Ink) : QColor(UiTheme::Muted)); p.drawText(QRect(0, 0, width() - 48, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        const QRectF track(width() - 40, (height() - 20) / 2.0, 38, 20);
        p.setPen(Qt::NoPen); p.setBrush(isChecked() ? QColor(isEnabled() ? UiTheme::Primary : UiTheme::Disabled) : QColor(UiTheme::Track)); p.drawRoundedRect(track, 10, 10);
        p.setBrush(Qt::white); p.drawEllipse(QPointF(track.left() + (isChecked() ? 28 : 10), track.center().y()), 7, 7);
        if (hasFocus() && (keyboardFocus_ || property("keyboardFocus").toBool())) { p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(UiTheme::Focus), 1)); p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4); }
    }
private:
    bool keyboardFocus_=false;
};
inline QIcon navigationIcon(int kind) {
    QIcon icon;
    for (auto mode : {QIcon::Normal, QIcon::Selected}) {
        QPixmap pixmap(48, 48); pixmap.setDevicePixelRatio(2); pixmap.fill(Qt::transparent);
        QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(mode == QIcon::Selected ? QColor(UiTheme::OnPrimary) : QColor(UiTheme::Secondary), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        if (kind == 0) { path.moveTo(3, 11); path.lineTo(12, 3); path.lineTo(21, 11); path.moveTo(6, 9); path.lineTo(6, 21); path.lineTo(10, 21); path.lineTo(10, 15); path.lineTo(14, 15); path.lineTo(14, 21); path.lineTo(18, 21); path.lineTo(18, 9); }
        else if (kind == 1) { path.moveTo(3, 7); path.lineTo(3, 20); path.lineTo(21, 20); path.lineTo(21, 8); path.lineTo(12, 8); path.lineTo(9, 4); path.lineTo(3, 4); path.closeSubpath(); }
        else if (kind == 2) { path.moveTo(3, 10); path.lineTo(21, 3); path.lineTo(14, 21); path.lineTo(10, 14); path.closeSubpath(); path.moveTo(10, 14); path.lineTo(21, 3); }
        else if (kind == 3) { p.drawLine(4, 6, 20, 6); p.drawLine(7, 12, 17, 12); p.drawLine(10, 18, 14, 18); }
        else if (kind == 4) { p.drawEllipse(QPointF(5, 6), 3, 3); p.drawEllipse(QPointF(19, 18), 3, 3); path.moveTo(8, 6); path.lineTo(16, 6); path.lineTo(16, 18); path.lineTo(8, 18); }
        else if (kind == 7) { p.drawArc(QRectF(3,4,18,18),0,180*16); path.moveTo(12,13); path.lineTo(17,8); p.drawEllipse(QPointF(12,13),1.5,1.5); }
        else if (kind == 5) { p.drawRoundedRect(QRectF(4, 3, 16, 18), 2, 2); p.drawLine(8, 8, 16, 8); p.drawLine(8, 12, 16, 12); p.drawLine(8, 16, 14, 16); }
        else { p.drawEllipse(QPointF(12, 12), 7, 7); p.drawEllipse(QPointF(12, 12), 2.5, 2.5); for (int angle = 0; angle < 360; angle += 45) { p.save(); p.translate(12, 12); p.rotate(angle); p.drawLine(QPointF(0, 8), QPointF(0, 10)); p.restore(); } }
        p.drawPath(path); icon.addPixmap(pixmap, mode);
    }
    return icon;
}
}
