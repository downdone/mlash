#include "YamlFormEditor.h"
#include "DialogDraftGuard.h"
#include "ProxyEditorDialog.h"
#include "ContextNote.h"
#include "NodeLinksEditor.h"
#include "ElidedLabel.h"
#include "WrapLayout.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QSet>
namespace mlash {
ProxyEditorDialog::ProxyEditorDialog(AppController* controller,const QString& id,QWidget* parent):QDialog(parent),controller_(controller),id_(id) {
    const bool running=controller->state().ready&&controller->state().activeProfile==id;
    QString name; bool pending=false; for (const auto& profile:controller->profiles()) if (profile.id==id) { saved_=draft_=profile.proxyEdits; sourceExpected_={{"candidate",profile.candidate},{"active",profile.active}}; name=profile.name; editVersion_=running?profile.active:profile.candidate; pending=profile.active!=profile.candidate; }
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("proxyEditorDialog"); setWindowTitle("节点与策略组 · "+name); resize(650,480);
    auto* layout=new QVBoxLayout(this); auto* note=new QLabel(running?QString("编辑运行版本，保存后重载。")+(pending?QString("不影响待应用更新。"):QString()):QString("编辑候选版本，保存后需手动使用。")); note->setObjectName("proxyEditScope"); note->setWordWrap(true); layout->addWidget(note);
    tabs_=new QTabWidget; nodes_=new QListWidget; groups_=new QListWidget; nodes_->setObjectName("editableNodes"); groups_->setObjectName("editableGroups"); tabs_->setObjectName("proxyEditorTabs"); tabs_->addTab(nodes_,"节点"); tabs_->addTab(groups_,"策略组"); layout->addWidget(tabs_,1);
    auto* row=new WrapLayout; add_=new QPushButton("新增…"); import_=new QPushButton("导入链接…"); import_->setObjectName("importNodeLinks"); edit_=new QPushButton("编辑…"); remove_=new QPushButton("移除"); restore_=new QPushButton("还原此项"); reset_=new QPushButton("全部沿用订阅");
    add_->setObjectName("addProxyEntry"); edit_->setObjectName("editProxyEntry"); remove_->setObjectName("removeProxyEntry"); restore_->setObjectName("restoreProxyEntry"); reset_->setObjectName("resetProxyEdits");
    for (auto* button:{add_,import_,edit_,remove_,restore_,reset_}) row->addWidget(button); layout->addLayout(row);
    hint_=new QLabel("正在读取来源节点…"); hint_->setObjectName("proxyEditorHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); layout->addWidget(hint_);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); actions->button(QDialogButtonBox::Cancel)->setText("取消"); save_=actions->button(QDialogButtonBox::Save); save_->setText("保存并校验"); save_->setObjectName("saveProxyEdits"); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions);
    connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(actions,&QDialogButtonBox::accepted,this,[this] { if (!sourceCurrent()) { updateActions(); return; } saving_=true; updateActions(); controller_->updateProxyEdits(id_,draft_,saved_,sourceExpected_); });
    connect(controller,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (!saving_) return; saving_=false; if (ok) accept(); else { hint_->setText(message); updateActions(); } }); connect(controller,&AppController::stateChanged,this,[this] { updateActions(); });
    connect(add_,&QPushButton::clicked,this,[this] { edit(true); }); connect(edit_,&QPushButton::clicked,this,[this] { edit(false); });
    connect(import_,&QPushButton::clicked,this,&ProxyEditorDialog::importLinks);
    connect(remove_,&QPushButton::clicked,this,&ProxyEditorDialog::previewRemoval);
    connect(restore_,&QPushButton::clicked,this,[this] { if (!list()->currentItem()) return; const auto name=list()->currentItem()->data(Qt::UserRole).toString(); auto entries=draft_.value(key()).toObject(); entries.remove(name); change(key(),entries); const auto removeKey=key()=="nodes"?"remove_nodes":"remove_groups"; auto removed=draft_.value(removeKey).toArray(); for (int i=removed.size()-1;i>=0;--i) if (removed[i].toString()==name) removed.removeAt(i); change(removeKey,removed); refresh(); });
    connect(reset_,&QPushButton::clicked,this,[this] { draft_={}; refresh(); }); connect(tabs_,&QTabWidget::currentChanged,this,[this] { updateActions(); });
    for (auto* view:{nodes_,groups_}) { connect(view,&QListWidget::itemSelectionChanged,this,[this] { updateActions(); }); connect(view,&QListWidget::itemDoubleClicked,this,[this] { if (edit_->isEnabled()) edit(false); }); }
    const QPointer<ProxyEditorDialog> guard(this); controller_->inspectProxyEntries(id,[guard](Result result) { if (!guard) return; guard->loaded_=result.ok; if (result.ok) { guard->source_=result.data; guard->hint_->setText("双击编辑定义；错误会保留草稿。重命名请新增并修改引用，再移除旧项。"); guard->refresh(); } else guard->hint_->setText(result.error); guard->updateActions(); }); updateActions();
    DialogDraftGuard::protect(this,[this] { return draft_!=saved_; },[this] { return saving_||analyzing_; });
}
QString ProxyEditorDialog::key() const { return tabs_->currentIndex()==0?"nodes":"groups"; }
QListWidget* ProxyEditorDialog::list() const { return tabs_->currentIndex()==0?nodes_:groups_; }
void ProxyEditorDialog::change(const QString& key,const QJsonValue& value) { if ((value.isObject()&&value.toObject().isEmpty())||(value.isArray()&&value.toArray().isEmpty())) draft_.remove(key); else draft_[key]=value; }
void ProxyEditorDialog::refresh() {
    for (auto* view:{nodes_,groups_}) { const auto k=view==nodes_?"nodes":"groups"; const auto previous=view->currentItem()?view->currentItem()->data(Qt::UserRole).toString():QString(); view->clear(); const auto source=source_.value(k).toObject(),edits=draft_.value(k).toObject(); const auto removed=draft_.value(view==nodes_?"remove_nodes":"remove_groups").toArray(); QSet<QString> names; for (auto it=source.begin();it!=source.end();++it) names.insert(it.key()); for (auto it=edits.begin();it!=edits.end();++it) names.insert(it.key()); auto sorted=names.values(); sorted.sort(); for (const auto& name:sorted) { auto* item=new QListWidgetItem(name+(removed.contains(name)?" · 已移除":edits.contains(name)?" · 已编辑":""),view); item->setData(Qt::UserRole,name); if (name==previous) view->setCurrentItem(item); } }
    updateActions();
}
bool ProxyEditorDialog::sourceCurrent() const {
    for (const auto& profile:controller_->profiles()) if (profile.id==id_) return profile.proxyEdits==saved_&&sourceExpected_==QJsonObject{{"candidate",profile.candidate},{"active",profile.active}}&&editVersion_==(controller_->state().ready&&controller_->state().activeProfile==id_?profile.active:profile.candidate);
    return false;
}
void ProxyEditorDialog::updateActions() { const bool fresh=sourceCurrent(); const bool available=loaded_&&fresh&&!saving_&&!analyzing_&&!controller_->state().busy&&!controller_->state().readOnly&&!controller_->state().exiting; if (loaded_&&!fresh&&!saving_) hint_->setText("运行状态或来源版本已变化，请重新打开；当前草稿仍保留。"); const bool selected=list()->currentItem(); const auto removed=draft_.value(key()=="nodes"?"remove_nodes":"remove_groups").toArray(); add_->setEnabled(available); import_->setVisible(key()=="nodes"); import_->setEnabled(available&&key()=="nodes"); edit_->setEnabled(available&&selected); remove_->setEnabled(available&&selected&&!removed.contains(list()->currentItem()?list()->currentItem()->data(Qt::UserRole).toString():QString())); restore_->setEnabled(available&&selected); reset_->setEnabled(available&&!draft_.isEmpty()); save_->setEnabled(available&&draft_!=saved_); }
void ProxyEditorDialog::importLinks() {
    if (!import_->isEnabled()) return; const auto before=draft_; QStringList names;
    for (const auto* kind:{"nodes","groups"}) { const auto original=source_.value(kind).toObject(),edited=draft_.value(kind).toObject(); names.append(original.keys()); names.append(edited.keys()); }
    auto* dialog=new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("nodeLinksDialog"); dialog->setWindowTitle("导入节点链接"); dialog->resize(620,470); auto* layout=new QVBoxLayout(dialog);
    auto* editor=new NodeLinksEditor(controller_,dialog,names); layout->addWidget(editor,1); auto* feedback=new ElidedLabel("仅新增节点草稿；可在策略组页添加成员，最后保存并校验。"); feedback->setObjectName("stageNodeLinksHint"); feedback->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); layout->addWidget(feedback);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Cancel); actions->button(QDialogButtonBox::Cancel)->setText("取消"); auto* stage=actions->addButton("加入草稿",QDialogButtonBox::AcceptRole); stage->setObjectName("stageNodeLinks"); stage->setProperty("primary",true); stage->setEnabled(false); layout->addWidget(actions);
    const auto current=[this,before] { return draft_==before&&sourceCurrent(); };
    const auto update=[this,editor,stage,feedback,current] { const bool fresh=current(); stage->setEnabled(fresh&&editor->ready()&&!controller_->state().busy&&!controller_->state().readOnly&&!controller_->state().exiting); if (!fresh) feedback->setText("来源或父窗口草稿已变化，请返回重新导入；未写入任何节点。"); };
    connect(editor,&NodeLinksEditor::changed,dialog,update); connect(controller_,&AppController::stateChanged,dialog,update);
    connect(actions,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(actions,&QDialogButtonBox::accepted,dialog,[this,dialog,editor,current,stage,update] {
        update(); if (!stage->isEnabled()||!current()) return; const auto imported=editor->definitions(); auto entries=draft_.value("nodes").toObject(); for (auto it=imported.begin();it!=imported.end();++it) entries[it.key()]=it.value(); change("nodes",entries); refresh(); hint_->setText(QString("已加入 %1 个节点草稿；可在策略组页添加成员，保存并校验后生效。").arg(imported.size())); dialog->accept();
    });
    DialogDraftGuard::protect(dialog,[editor] { return !editor->text().isEmpty(); },[editor] { return editor->checking(); });
    analyzing_=true; updateActions(); connect(dialog,&QDialog::finished,this,[this] { analyzing_=false; updateActions(); }); dialog->open(); update();
}
void ProxyEditorDialog::previewRemoval() {
    if (!list()->currentItem()||analyzing_) return;
    const auto name=list()->currentItem()->data(Qt::UserRole).toString(),kind=key(); const auto before=draft_; const auto ownDialerNote=kind=="nodes"&&before.value("dialers").toObject().contains(name)?QString(" 同时移除此节点自身的拨号覆盖。"):QString();
    auto* dialog=new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("proxyRemovalDialog"); dialog->setWindowTitle("移除影响 · "+name); dialog->resize(560,400);
    auto* layout=new QVBoxLayout(dialog); auto* summary=new QLabel("正在分析当前草稿的静态引用…"); summary->setTextFormat(Qt::PlainText); summary->setWordWrap(true); summary->setObjectName("proxyRemovalSummary"); layout->addWidget(summary);
    auto* references=new QListWidget; references->setObjectName("proxyRemovalReferences"); references->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); references->setTextElideMode(Qt::ElideRight); layout->addWidget(references,1);
    auto* boundary=new QLabel(QString("仅分析静态定义和已保存规则；最终保存仍检查全图并由内核校验。加入草稿不会修改运行配置。")+ownDialerNote); boundary->setTextFormat(Qt::PlainText); boundary->setWordWrap(true); boundary->setObjectName("proxyRemovalBoundary"); layout->addWidget(boundary);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Cancel); actions->button(QDialogButtonBox::Cancel)->setText("返回调整"); auto* stage=actions->addButton("加入移除草稿",QDialogButtonBox::AcceptRole); stage->setObjectName("stageProxyRemoval"); stage->setEnabled(false); layout->addWidget(actions);
    analyzing_=true; updateActions(); connect(actions,&QDialogButtonBox::rejected,dialog,&QDialog::reject); connect(dialog,&QDialog::finished,this,[this] { analyzing_=false; updateActions(); });
    connect(actions,&QDialogButtonBox::accepted,dialog,[this,dialog,summary,stage,name,kind,before] {
        const bool current=sourceCurrent();
        if (!current||before!=draft_||controller_->state().busy||controller_->state().exiting||controller_->state().readOnly) { summary->setText("来源或保存状态已变化，请返回并重新打开；当前草稿保留。"); stage->setEnabled(false); return; }
        auto entries=draft_.value(kind).toObject(); entries.remove(name); change(kind,entries);
        const auto removeKey=kind=="nodes"?"remove_nodes":"remove_groups"; auto removed=draft_.value(removeKey).toArray(); if (source_.value(kind).toObject().contains(name)&&!removed.contains(name)) removed.append(name); change(removeKey,removed);
        if (kind=="nodes") { auto dialers=draft_.value("dialers").toObject(); dialers.remove(name); change("dialers",dialers); }
        dialog->accept(); refresh();
    });
    const QPointer<ProxyEditorDialog> owner(this); const QPointer<QDialog> guard(dialog);
    controller_->inspectProxyImpact(id_,before,name,sourceExpected_,[owner,guard,summary,references,boundary,stage,ownDialerNote](Result result) {
        if (!owner||!guard) return;
        if (!result.ok) { summary->setText(result.error); return; }
        const int total=result.data.value("total").toInt(); const bool incomplete=result.data.value("incomplete").toBool();
        summary->setText(total?QString("发现 %1 项静态引用；请调整引用后再保存。%2").arg(total).arg(total>100?"仅显示前100项。":""):incomplete?"尚未发现可识别的静态引用；仍有未覆盖的配置。":"未发现静态引用。");
        for (const auto& value:result.data.value("references").toArray()) { const auto row=value.toObject(); const auto kind=row.value("kind").toString(); const auto text=(kind=="group"?"策略组 · ":kind=="dialer"?"节点拨号上游 · ":"规则 #")+row.value("name").toString(); auto* item=new QListWidgetItem(text,references); item->setToolTip(text); }
        if (incomplete) boundary->setText(QString("部分草稿无法解析，或含隐式包含/复杂规则/子规则/动态Provider；列表仅覆盖可识别的静态引用。最终保存仍由内核校验，运行配置保持。")+ownDialerNote);
        stage->setEnabled(true);
    }); dialog->open();
}
void ProxyEditorDialog::edit(bool create) {
    if (!create&&!list()->currentItem()) return; const auto k=key(); const auto selected=create?QString():list()->currentItem()->data(Qt::UserRole).toString();
    auto* dialog=new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("proxyEntryDialog"); dialog->setWindowTitle(create?"新增定义":"编辑定义"); dialog->resize(620,510); auto* layout=new QVBoxLayout(dialog); auto* name=new QLineEdit(selected); name->setMaxLength(256); name->setObjectName("proxyEntryName"); name->setReadOnly(!create); auto* form=new QFormLayout; form->addRow("名称",name); layout->addLayout(form);
    auto* text=new QPlainTextEdit; text->setObjectName("proxyEntryYaml"); text->setPlainText(draft_.value(k).toObject().value(selected).toString(source_.value(k).toObject().value(selected).toString())); text->setPlaceholderText(k=="nodes"?"name: MyNode\ntype: socks5\nserver: example.com\nport: 1080":"name: MyGroup\ntype: select\nproxies: [DIRECT]"); auto* fields=new YamlFormEditor(k,text,dialog,name); layout->addWidget(fields,1); if (k=="groups") { QSet<QString> members{"DIRECT","REJECT","REJECT-DROP","PASS","COMPATIBLE"}; for (const auto* field:{"nodes","groups"}) { const auto original=source_.value(field).toObject(),changed=draft_.value(field).toObject(); const auto removed=draft_.value(QString::fromLatin1(field)=="nodes"?"remove_nodes":"remove_groups").toArray(); for (auto it=original.begin();it!=original.end();++it) if (!removed.contains(it.key())) members.insert(it.key()); for (auto it=changed.begin();it!=changed.end();++it) members.insert(it.key()); } members.remove(selected); auto choices=members.values(); choices.sort(); QStringList providers; for (const auto& value:source_.value("provider_names").toArray()) providers.append(value.toString()); fields->setReferenceOptions(choices,providers); }
    const auto initial=text->toPlainText(); DialogDraftGuard::protect(dialog,[text,name,initial,selected,fields] { return text->toPlainText()!=initial || name->text()!=selected || fields->unapplied(); });
    layout->addWidget(contextNote("YAML 名称须与上方一致。","填写单个 mihomo YAML 定义，name 必须与上方一致。节点可能含凭据；只在编辑器内显示，不写日志。")); auto* note=new QLabel; note->setObjectName("proxyEntryFeedback"); note->setTextFormat(Qt::PlainText); note->setWordWrap(true); layout->addWidget(note); auto* actions=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); actions->button(QDialogButtonBox::Cancel)->setText("取消"); actions->button(QDialogButtonBox::Ok)->setText("保留到草稿"); actions->button(QDialogButtonBox::Ok)->setObjectName("acceptProxyEntry"); layout->addWidget(actions); connect(actions,&QDialogButtonBox::rejected,dialog,&QDialog::reject); auto* stage=actions->button(QDialogButtonBox::Ok); const auto updateStage=[fields,stage] { stage->setEnabled(fields->readyToStage()); }; connect(fields,&YamlFormEditor::activityChanged,dialog,updateStage); updateStage();
    connect(actions,&QDialogButtonBox::accepted,dialog,[this,dialog,name,text,note,k,create,fields] { if (!sourceCurrent()) { note->setText("运行状态或来源版本已变化，请返回重新打开；草稿仍保留。"); return; } if (!fields->readyToStage()) return; const auto n=name->text(); if (fields->identity()!=n) { note->setText("YAML中的name须与名称一致，请修正后加入草稿"); return; } if (n.trimmed()!=n||n.isEmpty()||text->toPlainText().toUtf8().size()>64*1024||(create&&(source_.value(k).toObject().contains(n)||draft_.value(k).toObject().contains(n)))) { note->setText("名称为空、已存在或定义超过64KiB，请修改"); return; } auto entries=draft_.value(k).toObject(); entries[n]=text->toPlainText(); change(k,entries); const auto removeKey=k=="nodes"?"remove_nodes":"remove_groups"; auto removed=draft_.value(removeKey).toArray(); for (int i=removed.size()-1;i>=0;--i) if (removed[i].toString()==n) removed.removeAt(i); change(removeKey,removed); refresh(); dialog->accept(); }); dialog->open();
}
}
