#include "WidgetStyle.h"
#include "UiTheme.h"
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProxyStyle>
#include <QStyledItemDelegate>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>
#include <QToolTip>
#include <QHelpEvent>
#include <QScreen>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QFocusEvent>
#include <QPushButton>
#include <QKeyEvent>
#include <QDialog>

namespace mlash {
namespace {
class InputSource final : public QObject {
public:
    explicit InputSource(QObject* parent):QObject(parent) { qApp->installEventFilter(this); }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        auto* widget=qobject_cast<QWidget*>(watched); if (!widget) return false;
        if (event->type()==QEvent::Show) if (auto* dialog=qobject_cast<QDialog*>(widget);dialog&&!dialog->inherits("QMessageBox")&&!dialog->inherits("QFileDialog")&&!dialog->inherits("QInputDialog")) {
            QList<QPushButton*> primary;
            const auto buttons=dialog->findChildren<QPushButton*>();
            for (auto* button:buttons) if (button->window()==dialog&&button->isVisibleTo(dialog)&&button->property("primary").toBool()) primary.append(button);
            for (auto* button:buttons) if (button->window()==dialog&&!button->property("primary").toBool()) { button->setAutoDefault(false); if (!primary.isEmpty()&&button->isDefault()) button->setDefault(false); }
            if (primary.size()==1) primary.first()->setDefault(true);
        }
        const auto mark=[](QWidget* target,bool keyboard) {
            if (!target) return;
            QList<QWidget*> controls{target};
            if (auto* spin=qobject_cast<QAbstractSpinBox*>(target)) if (auto* editor=spin->findChild<QLineEdit*>()) controls.append(editor);
            for (auto* owner=target->parentWidget();owner;owner=owner->parentWidget()) if (owner->focusProxy()==target||qobject_cast<QAbstractSpinBox*>(owner)) controls.append(owner);
            for (auto* control:controls) if (control->property("keyboardFocus").toBool()!=keyboard) {
                control->setProperty("keyboardFocus",keyboard);
                if (control->property("focusStylePending").toBool()) continue;
                control->setProperty("focusStylePending",true);
                QMetaObject::invokeMethod(control,[guard=QPointer<QWidget>(control)] {
                    if (!guard) return; guard->setProperty("focusStylePending",false);
                    guard->style()->unpolish(guard);
                    if (guard) { guard->style()->polish(guard); guard->update(); }
                },Qt::QueuedConnection);
            }
        };
        if (event->type()==QEvent::MouseButtonPress) { keyboard_=false; mark(widget,false); mark(QApplication::focusWidget(),false); }
        else if (event->type()==QEvent::KeyPress) {
            keyboard_=true; mark(QApplication::focusWidget(),true);
            const auto* key=static_cast<QKeyEvent*>(event);
            auto* button=qobject_cast<QPushButton*>(widget);
            if ((key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter)&&button&&button->hasFocus()&&!button->autoDefault()&&!button->isDefault()) {
                if (button->isEnabled()&&!key->isAutoRepeat()) { if (button->menu()) button->menu()->popup(button->mapToGlobal(QPoint(0,button->height()))); else button->click(); }
                return true;
            }
        }
        else if (event->type()==QEvent::FocusIn) { const auto reason=static_cast<QFocusEvent*>(event)->reason(); if (reason==Qt::TabFocusReason||reason==Qt::BacktabFocusReason||reason==Qt::ShortcutFocusReason) keyboard_=true; else if (reason==Qt::MouseFocusReason) keyboard_=false; mark(widget,keyboard_); }
        else if (event->type()==QEvent::FocusOut) mark(widget,false);
        return false;
    }
private:
    bool keyboard_=false;
};
void checkmark(QPainter* painter,const QPointF& center,const QColor& color) {
    painter->setPen(QPen(color,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    painter->drawPolyline(QPolygonF{center+QPointF(-4,0),center+QPointF(-1,3),center+QPointF(5,-4)});
}
class PopupSurface final : public QObject {
public:
    explicit PopupSurface(QWidget* widget,QComboBox* combo=nullptr):QObject(widget),combo_(combo) { widget->installEventFilter(this); }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        auto* widget=qobject_cast<QWidget*>(watched);
        if (widget && event->type()==QEvent::Paint && !qobject_cast<QMenu*>(widget)) {
            QPainter painter(widget); painter.setRenderHint(QPainter::Antialiasing); painter.setPen(QPen(QColor(UiTheme::Border),1)); painter.setBrush(QColor(UiTheme::Surface)); painter.drawRoundedRect(QRectF(widget->rect()).adjusted(.5,.5,-.5,-.5),8,8);
        }
        if (widget && combo_ && event->type()==QEvent::Show) {
            const QFontMetrics metrics(combo_->font()); int content=120;
            for (int row=0;row<combo_->model()->rowCount(combo_->rootModelIndex());++row) { content=qMax(content,metrics.horizontalAdvance(combo_->model()->index(row,combo_->modelColumn(),combo_->rootModelIndex()).data().toString())+44); if (content>=460) { content=460; break; } }
            auto* screen=QApplication::screenAt(combo_->mapToGlobal(QPoint(0,0))); if (!screen) screen=widget->screen(); const auto available=screen->availableGeometry().adjusted(4,4,-4,-4);
            widget->resize(qMin(available.width(),qMax(combo_->width(),content)),widget->height());
            widget->move(qBound(available.left(),widget->x(),qMax(available.left(),available.right()-widget->width()+1)),qBound(available.top(),widget->y(),qMax(available.top(),available.bottom()-widget->height()+1)));
        }
        if (auto* menu=qobject_cast<QMenu*>(widget); menu && event->type()==QEvent::Show) {
            auto* screen=QApplication::screenAt(menu->pos()); if (!screen) screen=menu->screen(); const auto available=screen->availableGeometry().adjusted(4,4,-4,-4);
            menu->setMaximumHeight(qMin(720,available.height())); menu->resize(menu->width(),qMin(menu->height(),menu->maximumHeight()));
            menu->move(qBound(available.left(),menu->x(),qMax(available.left(),available.right()-menu->width()+1)),qBound(available.top(),menu->y(),qMax(available.top(),available.bottom()-menu->height()+1)));
        }
        return QObject::eventFilter(watched,event);
    }
private:
    QPointer<QComboBox> combo_;
};
void preparePopup(QWidget* widget,bool menu,QComboBox* combo=nullptr) {
    if (!widget || widget->windowType()!=Qt::Popup || widget->property("mlashPopup").toBool()) return;
    widget->setProperty("mlashPopup",true); widget->setWindowFlags(widget->windowFlags()|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint); widget->setAttribute(Qt::WA_TranslucentBackground);
    new PopupSurface(widget,combo);
    if (!menu) {
        widget->setObjectName("mlashComboPopup"); widget->setStyleSheet(QString("QAbstractItemView {background:transparent;border:none;outline:none;color:palette(text);} QScrollBar:vertical {width:8px;background:transparent;} QScrollBar::handle:vertical {background:#d0d0d7;border-radius:4px;min-height:24px;} QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}"));
    }
}
class ComboItemDelegate final : public QStyledItemDelegate {
public:
    ComboItemDelegate(QComboBox* combo,QObject* parent):QStyledItemDelegate(parent),combo_(combo) {}
    QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        auto size=QStyledItemDelegate::sizeHint(option,index); size.setHeight(qMax(26,option.fontMetrics.height()+10)); size.rwidth()+=28; return size;
    }
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        QStyleOptionViewItem item=option; initStyleOption(&item,index); const auto rect=item.rect.adjusted(4,1,-4,-1);
        const bool enabled=item.state.testFlag(QStyle::State_Enabled),selected=item.state.testFlag(QStyle::State_Selected) || item.state.testFlag(QStyle::State_HasFocus) || item.state.testFlag(QStyle::State_MouseOver);
        painter->save(); painter->setRenderHint(QPainter::Antialiasing); painter->setPen(Qt::NoPen);
        if (selected && enabled) { painter->setBrush(QColor(UiTheme::Selection)); painter->drawRoundedRect(rect,5,5); }
        if (combo_ && index.row()==combo_->currentIndex()) checkmark(painter,QPointF(rect.left()+11,rect.center().y()),enabled?QColor(UiTheme::Brand):QColor(UiTheme::Disabled));
        painter->setFont(item.font); painter->setPen(enabled?QColor(UiTheme::Ink):QColor(UiTheme::Disabled));
        const auto textRect=rect.adjusted(26,0,-8,0); painter->drawText(textRect,Qt::AlignVCenter|Qt::AlignLeft,item.fontMetrics.elidedText(item.text,Qt::ElideRight,textRect.width())); painter->restore();
    }
    bool helpEvent(QHelpEvent* event,QAbstractItemView* view,const QStyleOptionViewItem& option,const QModelIndex& index) override {
        if (event->type()==QEvent::ToolTip && index.isValid()) { const auto detail=index.data(Qt::ToolTipRole).toString(); QToolTip::showText(event->globalPos(),detail.isEmpty()?index.data().toString():detail,view); return true; }
        return QStyledItemDelegate::helpEvent(event,view,option,index);
    }
private:
    QPointer<QComboBox> combo_;
};
class RefinedStyle final : public QProxyStyle {
public:
    RefinedStyle():QProxyStyle(QStyleFactory::create("Fusion")) { setObjectName("mlash-refined"); }
    int styleHint(StyleHint hint,const QStyleOption* option=nullptr,const QWidget* widget=nullptr,QStyleHintReturn* result=nullptr) const override {
        if (hint==SH_ComboBox_Popup || hint==SH_ComboBox_PopupFrameStyle) return 0;
        if (hint==SH_Menu_Scrollable) return 1;
        return QProxyStyle::styleHint(hint,option,widget,result);
    }
    void polish(QWidget* widget) override {
        QProxyStyle::polish(widget);
        if (auto* combo=qobject_cast<QComboBox*>(widget)) WidgetStyle::prepareCombo(combo);
        if (qobject_cast<QMenu*>(widget)) preparePopup(widget,true);
    }
    int pixelMetric(PixelMetric metric,const QStyleOption* option=nullptr,const QWidget* widget=nullptr) const override {
        if (qobject_cast<const QMenu*>(widget)) { if (metric==PM_MenuPanelWidth) return 5; if (metric==PM_MenuHMargin || metric==PM_MenuVMargin) return 0; if (metric==PM_SmallIconSize) return 18; }
        return QProxyStyle::pixelMetric(metric,option,widget);
    }
    QSize sizeFromContents(ContentsType type,const QStyleOption* option,const QSize& size,const QWidget* widget=nullptr) const override {
        if (type==CT_MenuItem) if (const auto* item=qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
            if (item->menuItemType==QStyleOptionMenuItem::Separator) return QSize(24,9);
            const QFontMetrics metrics(item->font); const auto parts=item->text.split('\t');
            const int width=28+(item->maxIconWidth>0?24:0)+metrics.size(Qt::TextHideMnemonic,parts.first()).width()+(parts.size()>1?22+metrics.horizontalAdvance(parts.last()):0)+22;
            return QSize(qBound(180,width,460),qMax(26,metrics.height()+10));
        }
        return QProxyStyle::sizeFromContents(type,option,size,widget);
    }
    void drawPrimitive(PrimitiveElement element,const QStyleOption* option,QPainter* painter,const QWidget* widget=nullptr) const override {
        if (element==PE_FrameFocusRect && (!widget || !widget->property("keyboardFocus").toBool())) return;
        if (element==PE_IndicatorCheckBox || element==PE_IndicatorItemViewItemCheck) {
            const bool enabled=option->state.testFlag(State_Enabled),checked=option->state.testFlag(State_On),partial=option->state.testFlag(State_NoChange);
            painter->save(); painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(QPen(QColor(enabled?(checked||partial?UiTheme::Primary:UiTheme::Secondary):UiTheme::Disabled),1));
            painter->setBrush(QColor(checked||partial?(enabled?UiTheme::Primary:UiTheme::Disabled):UiTheme::Surface));
            painter->drawRoundedRect(QRectF(option->rect).adjusted(.5,.5,-.5,-.5),3,3);
            if (checked) checkmark(painter,option->rect.center(),QColor(UiTheme::OnPrimary));
            else if (partial) { painter->setPen(QPen(QColor(UiTheme::OnPrimary),1.5)); painter->drawLine(option->rect.center()+QPoint(-4,0),option->rect.center()+QPoint(4,0)); }
            painter->restore(); return;
        }
        if (element==PE_PanelMenu || element==PE_FrameMenu) {
            if (element==PE_PanelMenu) { painter->save(); painter->setRenderHint(QPainter::Antialiasing); painter->setPen(QPen(QColor(UiTheme::Border),1)); painter->setBrush(QColor(UiTheme::Surface)); painter->drawRoundedRect(QRectF(option->rect).adjusted(.5,.5,-.5,-.5),8,8); painter->restore(); } return;
        }
        QProxyStyle::drawPrimitive(element,option,painter,widget);
    }
    void drawControl(ControlElement element,const QStyleOption* option,QPainter* painter,const QWidget* widget=nullptr) const override {
        if (element!=CE_MenuItem) { QProxyStyle::drawControl(element,option,painter,widget); return; }
        const auto* item=qstyleoption_cast<const QStyleOptionMenuItem*>(option); if (!item) return;
        const auto rect=item->rect; painter->save(); painter->setRenderHint(QPainter::Antialiasing);
        if (item->menuItemType==QStyleOptionMenuItem::Separator) { painter->setPen(QColor(UiTheme::Separator)); painter->drawLine(rect.left()+8,rect.center().y(),rect.right()-8,rect.center().y()); painter->restore(); return; }
        const bool enabled=item->state.testFlag(State_Enabled),selected=item->state.testFlag(State_Selected); const auto ink=enabled?QColor(UiTheme::Ink):QColor(UiTheme::Disabled);
        if (selected && enabled) { painter->setPen(Qt::NoPen); painter->setBrush(QColor(UiTheme::Selection)); painter->drawRoundedRect(rect.adjusted(1,1,-1,-1),5,5); }
        if (item->checked) checkmark(painter,QPointF(rect.left()+10,rect.center().y()),enabled?QColor(UiTheme::Brand):ink);
        int left=rect.left()+23;
        if (item->maxIconWidth>0) { if (!item->icon.isNull()) item->icon.paint(painter,QRect(left,rect.center().y()-9,18,18),Qt::AlignCenter,enabled?QIcon::Normal:QIcon::Disabled); left+=24; }
        const auto parts=item->text.split('\t'); const QFontMetrics metrics(item->font); painter->setFont(item->font);
        int right=rect.right()-17;
        if (parts.size()>1) { const int width=metrics.horizontalAdvance(parts.last()); painter->setPen(enabled?QColor(UiTheme::Secondary):ink); painter->drawText(QRect(right-width,rect.top(),width,rect.height()),Qt::AlignVCenter|Qt::AlignRight,parts.last()); right-=width+20; }
        painter->setPen(ink); painter->drawText(QRect(left,rect.top(),qMax(0,right-left),rect.height()),Qt::AlignVCenter|Qt::AlignLeft|Qt::TextHideMnemonic,metrics.elidedText(parts.first(),Qt::ElideRight,qMax(0,right-left)));
        if (item->menuItemType==QStyleOptionMenuItem::SubMenu) { painter->setPen(QPen(enabled?QColor(UiTheme::Secondary):ink,1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); const QPointF center(rect.right()-9,rect.center().y()); painter->drawPolyline(QPolygonF{center+QPointF(-2,-3),center+QPointF(1,0),center+QPointF(-2,3)}); }
        painter->restore();
    }
};
}
void WidgetStyle::install() {
    // Qt's rollout/fade uses a separate rectangular effect window. Draw our
    // translucent rounded popup immediately, including on the first opening.
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo,false);
    QApplication::setEffectEnabled(Qt::UI_AnimateMenu,false);
    QApplication::setEffectEnabled(Qt::UI_FadeMenu,false);
    if (!qApp->property("mlashStyleInstalled").toBool()) { QApplication::setStyle(new RefinedStyle); new InputSource(qApp); qApp->setProperty("mlashStyleInstalled",true); }
    UiTheme::install();
}
void WidgetStyle::prepareCombo(QComboBox* combo) {
    if (combo->property("mlashComboPrepared").toBool()) return;
    combo->setProperty("mlashComboPrepared",true); combo->setMaxVisibleItems(10);
    // App-wide repolish may be traversing QApplication::allWidgets(). Replacing
    // the view here would delete a widget still present in that traversal.
    auto* view=combo->view();
    if (auto* list=qobject_cast<QListView*>(view)) { list->setUniformItemSizes(true); list->setSpacing(0); }
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); view->setMouseTracking(true); view->setFrameShape(QFrame::NoFrame);
    view->setItemDelegate(new ComboItemDelegate(combo,view)); preparePopup(view->window(),false,combo);
}
}
