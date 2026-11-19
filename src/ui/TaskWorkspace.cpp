#include "TaskWorkspace.h"
#include "MainWindow.h"
#include "DialogDraftGuard.h"
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QTimer>
#include <QSet>
#include <QStyle>
namespace mlash {
void presentTaskDialog(QDialog* dialog) {
    for (auto* parent=dialog->parentWidget();parent;parent=parent->parentWidget()) {
        if (auto* workspace=dynamic_cast<TaskWorkspace*>(parent)) { workspace->mount(dialog); return; }
        if (auto* main=qobject_cast<MainWindow*>(parent)) { main->presentTask(dialog); return; }
    }
    dialog->show();
}
TaskWorkspace::TaskWorkspace(QWidget* parent):QWidget(parent) {
    setObjectName("taskWorkspace"); auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(12,8,12,12); layout->setSpacing(6);
    header_=new QHBoxLayout; back_=new QPushButton("‹ 返回"); back_->setObjectName("taskBack"); back_->setProperty("backLink",true);
    header_->addWidget(back_); title_=new QLabel; title_->setObjectName("taskTitle"); title_->setTextFormat(Qt::PlainText);
    header_->addWidget(title_,1); layout->addLayout(header_);
    summary_=new QLabel; summary_->setObjectName("taskContextSummary"); summary_->setProperty("secondary",true);
    summary_->setTextFormat(Qt::PlainText); summary_->hide(); layout->addWidget(summary_);
    tabs_=new QTabBar; tabs_->setObjectName("taskTabs"); tabs_->setExpanding(false); tabs_->setUsesScrollButtons(true); tabs_->setAccessibleName("当前对象的任务");
    layout->addWidget(tabs_); hint_=new QLabel; hint_->setObjectName("taskFeedback"); hint_->setWordWrap(true); hint_->setTextFormat(Qt::PlainText); hint_->hide(); layout->addWidget(hint_);
    panels_=new QStackedWidget; panels_->setObjectName("taskPanels"); layout->addWidget(panels_,1);
    connect(tabs_,&QTabBar::currentChanged,this,[this](int i) { activate(i); });
    connect(back_,&QPushButton::clicked,this,[this] {
        const bool profile=context_.startsWith("profile:");
        if (profile && !canReplace()) return;
        const auto back=backAction_;
        if (back) back();
        if (profile) clear();
    });
}
TaskWorkspace::~TaskWorkspace() {
    clearing_=true;
    for (auto& entry:entries_) if (entry.editor) QObject::disconnect(entry.editor,nullptr,this,nullptr);
}
int TaskWorkspace::indexOf(const QString& key) const { for (int i=0;i<entries_.size();++i) if (entries_[i].task.key==key) return i; return -1; }
QWidget* TaskWorkspace::editor(const QString& key) const { const int i=indexOf(key); return i<0?nullptr:entries_[i].editor.data(); }
void TaskWorkspace::note(const QString& text) { hint_->setText(text); hint_->setVisible(!text.isEmpty()); }
void TaskWorkspace::setContextSummary(const QString& text) { summary_->setText(text); summary_->setVisible(!text.isEmpty()); }
bool TaskWorkspace::canReplace() {
    if (DialogDraftGuard::hasPending(this)) { note("当前提交尚未结束，请稍后再切换对象。"); return false; }
    if (DialogDraftGuard::hasDrafts(this)&&QMessageBox::question(this,"未保存的修改","切换对象将放弃此工作区的草稿，是否继续？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return false;
    return true;
}
bool TaskWorkspace::canClose(QWidget* editor) {
    for (const auto& entry:entries_) if (entry.owner==editor&&entry.editor) {
        if (!canClose(entry.editor)||!DialogDraftGuard::allowLeave(entry.editor)) return false;
    }
    return true;
}
bool TaskWorkspace::configure(const QString& context,const QString& title,const QList<Task>& tasks,std::function<void()> back) {
    if (context_!=context) { if (!canReplace()) return false; clear(); context_=context; }
    contextTitle_=title; title_->setText(title); backAction_=std::move(back); note({}); setContextSummary({});
    const bool profile=context_.startsWith("profile:");
    const bool contentProfile=profile && context_.endsWith(":content");
    header_->removeWidget(back_); header_->removeWidget(title_);
    header_->addWidget(back_); header_->addWidget(title_,1);
    back_->setText(profile ? "‹ 返回配置" : "‹ 返回");
    back_->setProperty("backLink",true); back_->style()->unpolish(back_); back_->style()->polish(back_);
    {
        const QSignalBlocker block(tabs_);
        QSet<QString> declared; for (const auto& task:tasks) declared.insert(task.key);
        // A declared task can disappear as the same profile changes (for example,
        // after applying a candidate). Keep mounted editors until the user closes
        // them; remove only empty, obsolete tabs so a draft cannot vanish.
        for (int i=entries_.size()-1;i>=0;--i) if (entries_[i].task.open && !declared.contains(entries_[i].task.key) && !entries_[i].editor) {
            tabs_->removeTab(i); entries_.removeAt(i);
        }
        for (const auto& task:tasks) {
            const int index=indexOf(task.key);
            if (index>=0) { entries_[index].task=task; tabs_->setTabText(index,task.title); }
            else { entries_.append({task,{},{}}); tabs_->addTab(task.title); }
        }
    }
    // The content workspace has one stable view switcher. Source and candidate
    // are separate single-task views, so they do not need a tab strip.
    tabs_->setVisible(entries_.size()>1 && contentProfile); return true;
}
void TaskWorkspace::clear() {
    clearing_=true; const QSignalBlocker block(tabs_); while (tabs_->count()) tabs_->removeTab(tabs_->count()-1);
    // Drop connections before deleting forms: their destructors may emit state changes.
    auto entries=std::move(entries_); entries_.clear();
    for (auto& entry:entries) if (entry.surface) { if (entry.editor) QObject::disconnect(entry.editor,nullptr,this,nullptr); panels_->removeWidget(entry.surface); entry.surface->hide(); if (entry.editor) entry.editor->setProperty("embeddedTask",false); entry.surface->deleteLater(); }
    context_.clear(); contextTitle_.clear(); creatingKey_.clear(); clearing_=false;
}
void TaskWorkspace::activate(int index) {
    if (clearing_||index<0||index>=entries_.size()) return;
    note({}); const auto task=entries_[index].task;
    if (entries_[index].editor) { title_->setText(context_.startsWith("profile:")?contextTitle_+" / "+task.title:task.title); panels_->setCurrentWidget(entries_[index].surface); entries_[index].editor->show(); if (requested) requested(); }
    else if (task.open) { creatingKey_=task.key; task.open(); creatingKey_.clear(); }
}
void TaskWorkspace::select(const QString& key) { const int i=indexOf(key); if (i<0) return; if (tabs_->currentIndex()==i) activate(i); else tabs_->setCurrentIndex(i); }
void TaskWorkspace::mount(QWidget* editor,const QString& requestedKey,const QString& title) {
    if (!editor) return;
    const auto key=creatingKey_.isEmpty()?(requestedKey.isEmpty()?editor->objectName():requestedKey):creatingKey_;
    int index=indexOf(key);
    if (index<0) {
        if (entries_.size()>=24) { editor->deleteLater(); note("请先完成或取消已有任务，再打开其他任务。"); return; }
        const QSignalBlocker block(tabs_); entries_.append(Entry{Task{key,title.isEmpty()?editor->windowTitle():title,{}},{},{}}); tabs_->addTab(entries_.last().task.title); index=entries_.size()-1;
    }
    if (entries_[index].editor&&entries_[index].editor!=editor) { editor->deleteLater(); select(key); return; }
    if (!entries_[index].surface) {
        for (auto* parent=editor->parentWidget();parent;parent=parent->parentWidget()) {
            bool found=false; for (const auto& entry:entries_) if (entry.editor==parent) { entries_[index].owner=parent; found=true; break; }
            if (found||parent==this) break;
        }
        auto* scroll=new QScrollArea; scroll->setFrameShape(QFrame::NoFrame); scroll->setObjectName("taskScroll"); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* canvas=new QWidget; auto* center=new QHBoxLayout(canvas); center->setContentsMargins(0,0,0,0);
        editor->setProperty("embeddedTask",true); editor->setParent(canvas,Qt::Widget);
        if (auto* dialog=qobject_cast<QDialog*>(editor)) {
            dialog->setModal(false);
            connect(dialog,&QDialog::finished,this,[this,key] { removeEditor(key); });
        }
        editor->setMinimumSize(0,0); editor->setMaximumWidth(960); editor->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        if (editor->layout()) editor->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        center->addWidget(editor,1); scroll->setWidget(canvas); entries_[index].editor=editor; entries_[index].surface=scroll; panels_->addWidget(scroll);
        connect(editor,&QObject::destroyed,this,[this,key,identity=QPointer<QWidget>(scroll)] { const int i=indexOf(key); if (!clearing_&&i>=0&&entries_[i].surface==identity) removeEditor(key); });
    }
    { const QSignalBlocker block(tabs_); tabs_->setCurrentIndex(index); }
    title_->setText(context_.startsWith("profile:")?contextTitle_+" / "+entries_[index].task.title:entries_[index].task.title);
    panels_->setCurrentWidget(entries_[index].surface); tabs_->setVisible(entries_.size()>1 && (!context_.startsWith("profile:") || context_.endsWith(":content"))); editor->show();
    if (requested) requested();
}
void TaskWorkspace::removeEditor(const QString& key) {
    const int index=indexOf(key); if (clearing_||index<0) return;
    const auto removed=entries_[index].editor;
    QStringList dependents; for (const auto& entry:entries_) if (removed&&entry.owner==removed) dependents.append(entry.task.key);
    for (const auto& child:dependents) removeEditor(child);
    const int currentIndex=indexOf(key); if (currentIndex<0) return;
    auto surface=entries_[currentIndex].surface; const bool wasCurrent=isVisible()&&panels_->currentWidget()==surface; entries_[currentIndex].editor=nullptr; entries_[currentIndex].surface=nullptr;
    if (surface) { panels_->removeWidget(surface); surface->deleteLater(); }
    // Declared tasks remain reopenable; temporary details are removed altogether.
    const bool declared=bool(entries_[currentIndex].task.open);
    if (!declared) { const QSignalBlocker block(tabs_); tabs_->removeTab(currentIndex); entries_.removeAt(currentIndex); }
    QTimer::singleShot(0,this,[this,wasCurrent] {
        if (clearing_||!wasCurrent||!isVisible()) return;
        for (int i=0;i<entries_.size();++i) if (entries_[i].editor) { select(entries_[i].task.key); return; }
        if (backAction_) backAction_();
    });
}
}
