#include "SettingsSearch.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QStyle>
namespace mlash {
SettingsSearch::SettingsSearch(QWidget* catalog,QScrollArea* scroll,QWidget* parent):QWidget(parent),scroll_(scroll) {
    setObjectName("settingsLocator"); auto* layout=new QHBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(6);
    search_=new QLineEdit; search_->setObjectName("settingsSearch"); search_->setAccessibleName("搜索设置与功能"); search_->setPlaceholderText("搜索设置或功能 · Ctrl+F"); search_->setMaxLength(80); search_->setClearButtonEnabled(true); search_->setMinimumWidth(160); layout->addWidget(search_,1);
    count_=new QPushButton("查找"); count_->setObjectName("settingsSearchResults"); count_->setFixedWidth(64); count_->setToolTip("输入关键词；Enter下一项，Shift+Enter上一项，Esc清空"); layout->addWidget(count_);
    menu_=new QMenu(count_); menu_->setObjectName("settingsSearchMenu");
    for (auto* row:catalog->findChildren<QWidget*>("settingRow")) {
        const auto name=row->property("settingSearchName").toString(); if (name.isEmpty()||entries_.size()>=96) continue;
        QString section; if (auto* label=row->parentWidget()->findChild<QLabel*>("sectionTitle",Qt::FindDirectChildrenOnly)) section=label->text();
        QString terms=row->property("settingSearchTerms").toString(); for (auto* child:row->findChildren<QWidget*>()) if (!child->objectName().isEmpty()) terms+=' '+child->objectName();
        entries_.append({section.isEmpty()?name:section+" › "+name,section+" "+terms,row,-1});
    }
    connect(search_,&QLineEdit::textChanged,this,&SettingsSearch::refresh); connect(count_,&QPushButton::clicked,this,&SettingsSearch::showMatches); search_->installEventFilter(this);
    markTimer_.setSingleShot(true); markTimer_.setInterval(1800); connect(&markTimer_,&QTimer::timeout,this,&SettingsSearch::clearMark);
    shortcut_=new QShortcut(QKeySequence::Find,parent?parent:this); shortcut_->setEnabled(false); connect(shortcut_,&QShortcut::activated,this,[this] { search_->setFocus(Qt::ShortcutFocusReason); search_->selectAll(); }); refresh();
}
void SettingsSearch::addRoute(const QString& title,const QString& terms,int page) { if (entries_.size()>=96) return; entries_.append({title,title+' '+terms,{},page}); refresh(); }
void SettingsSearch::clearMark() { markTimer_.stop(); if (!marked_) return; marked_->setProperty("located",false); marked_->style()->unpolish(marked_); marked_->style()->polish(marked_); marked_->update(); marked_=nullptr; }
void SettingsSearch::refresh() {
    menu_->hide(); clearMark(); matches_.clear(); current_=-1; const auto words=search_->text().trimmed().split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
    if (!words.isEmpty()) for (int i=0;i<entries_.size();++i) { bool match=true; for (const auto& word:words) if (!entries_[i].terms.contains(word,Qt::CaseInsensitive)) { match=false; break; } if (match) matches_.append(i); }
    count_->setText(words.isEmpty()?QString("查找"):QString("%1 项").arg(matches_.size())); count_->setEnabled(!words.isEmpty()); count_->setAccessibleName(words.isEmpty()?QString("输入关键词后查看结果"):QString("找到%1项设置与功能").arg(matches_.size())); search_->setToolTip(words.isEmpty()?QString("只检索设置名称和说明，不读取用户字段；Enter定位"):matches_.isEmpty()?QString("没有匹配项；可尝试DNS、备份、快捷键、规则等关键词"):QString("找到%1项；Enter下一项，Shift+Enter上一项，点击结果数选择").arg(matches_.size()));
}
void SettingsSearch::locate(int match) {
    if (match<0||match>=matches_.size()) return; clearMark(); current_=match; const auto& entry=entries_[matches_[match]];
    count_->setAccessibleName(QString("%1，共%2项：%3").arg(match+1).arg(matches_.size()).arg(entry.title)); count_->setToolTip(entry.title);
    if (entry.page>=0) { emit pageRequested(entry.page); return; }
    if (!entry.anchor) return; scroll_->ensureWidgetVisible(entry.anchor,8,12); marked_=entry.anchor; marked_->setProperty("located",true); marked_->style()->unpolish(marked_); marked_->style()->polish(marked_); marked_->update(); markTimer_.start();
}
void SettingsSearch::showMatches() {
    menu_->clear(); if (matches_.isEmpty()) { menu_->addAction("没有匹配项")->setEnabled(false); }
    for (int i=0;i<qMin(20,int(matches_.size()));++i) { auto* action=menu_->addAction(entries_[matches_[i]].title); action->setObjectName("settingsSearchMatch"+QString::number(i)); connect(action,&QAction::triggered,this,[this,i] { locate(i); }); }
    if (matches_.size()>20) menu_->addAction(QString("还有%1项；Enter继续定位").arg(matches_.size()-20))->setEnabled(false);
    menu_->popup(count_->mapToGlobal(QPoint(0,count_->height())));
}
bool SettingsSearch::eventFilter(QObject* watched,QEvent* event) {
    if (watched==search_&&event->type()==QEvent::KeyPress) { const auto* key=static_cast<QKeyEvent*>(event); if (key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter) { if (!matches_.isEmpty()) locate((current_+(key->modifiers().testFlag(Qt::ShiftModifier)?(current_<0?0:-1):1)+matches_.size())%matches_.size()); return true; } if (key->key()==Qt::Key_Escape) { search_->clear(); return true; } }
    return QWidget::eventFilter(watched,event);
}
void SettingsSearch::showEvent(QShowEvent* event) { QWidget::showEvent(event); shortcut_->setEnabled(true); }
void SettingsSearch::hideEvent(QHideEvent* event) { shortcut_->setEnabled(false); menu_->hide(); clearMark(); QWidget::hideEvent(event); }
}
