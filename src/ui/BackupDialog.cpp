#include "BackupDialog.h"
#include "profile/BackupArchive.h"
#include "ElidedLabel.h"
#include <QListWidget>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace mlash {
namespace {
class BackupViewport final : public QScrollArea {
public:
    QSize sizeHint() const override { return {480,280}; }
    QSize minimumSizeHint() const override { return {160,80}; }
    bool hasHeightForWidth() const override { return false; }
    int heightForWidth(int) const override { return -1; }
};
}
BackupDialog::BackupDialog(AppController* controller, bool restore, QWidget* parent) : QDialog(parent), controller_(controller), restore_(restore) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("backupDialog"); setWindowTitle(restore ? "恢复本地备份" : "创建本地备份"); resize(520, 390);
    auto* outer=new QVBoxLayout(this); auto* scroll=new BackupViewport; scroll->setObjectName("backupScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); auto* body=new QWidget; auto* layout=new QVBoxLayout(body); layout->setContentsMargins(0,0,0,0); scroll->setWidget(body); outer->addWidget(scroll,1); auto* description = new QLabel(restore ? "恢复为新增订阅，保留现有数据；不会启动内核或接管系统代理。" : "使用密码加密保存；不包含日志、代理恢复记录、内核程序或 Geo 缓存。"); description->setWordWrap(true); layout->addWidget(description);
    auto* form = new QFormLayout; auto* source = new QWidget; auto* sourceRow = new QHBoxLayout(source); sourceRow->setContentsMargins(0,0,0,0); path_ = new QLineEdit; path_->setObjectName("backupPath"); sourceRow->addWidget(path_,1); auto* browse = new QPushButton("浏览…"); sourceRow->addWidget(browse); form->addRow("备份文件", source);
    password_ = new QLineEdit; password_->setObjectName("backupPassword"); password_->setEchoMode(QLineEdit::Password); password_->setMaxLength(1024); password_->setPlaceholderText("至少 8 个字符"); form->addRow("备份密码", password_);
    confirm_ = new QLineEdit(this); confirm_->setObjectName("backupPasswordConfirm"); confirm_->setEchoMode(QLineEdit::Password); confirm_->setMaxLength(1024); if (!restore) form->addRow("确认密码", confirm_); else confirm_->hide(); layout->addLayout(form);
    profiles_ = new QCheckBox("包含订阅、节点机密与自定义规则"); profiles_->setObjectName("backupIncludeProfiles"); profiles_->setChecked(true); profiles_->setVisible(!restore); layout->addWidget(profiles_);
    settings_ = new QCheckBox("同时恢复界面与本地偏好"); settings_->setObjectName("backupRestoreSettings"); settings_->setChecked(true); settings_->setVisible(restore); layout->addWidget(settings_);
    auto* prerequisite=new QWidget; prerequisite->setObjectName("restorePrerequisite"); auto* prerequisiteRow=new QHBoxLayout(prerequisite); prerequisiteRow->setContentsMargins(0,0,0,0); auto* prerequisiteText=new ElidedLabel("恢复前需停止内核"); prerequisiteText->setObjectName("restorePrerequisiteText"); prerequisiteText->setProperty("secondary",true); prerequisiteText->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); prerequisiteRow->addWidget(prerequisiteText,1);
    stopCore_=new QPushButton("停止内核"); stopCore_->setObjectName("backupStopCore"); stopCore_->setAutoDefault(false); prerequisiteRow->addWidget(stopCore_); auto reserved=stopCore_->sizePolicy(); reserved.setRetainSizeWhenHidden(true); stopCore_->setSizePolicy(reserved); prerequisite->setVisible(restore); outer->addWidget(prerequisite);
    hint_ = new ElidedLabel(restore ? "先预览校验，再确认恢复。" : "请妥善保存密码；无法找回备份密码。"); hint_->setObjectName("backupHint"); hint_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); hint_->setMinimumHeight(hint_->fontMetrics().height()+6); outer->addWidget(hint_);
    contents_ = new QLabel; contents_->setObjectName("backupContents"); contents_->setTextFormat(Qt::PlainText); contents_->setWordWrap(true); contents_->setProperty("secondary",true); contents_->setVisible(restore); layout->addWidget(contents_);
    auto* selectionTools=new QWidget; auto* toolRow=new QHBoxLayout(selectionTools); toolRow->setContentsMargins(0,0,0,0); toolRow->addWidget(new QLabel("选择新增订阅")); toolRow->addStretch(); selectAll_=new QPushButton("全选"); selectAll_->setAutoDefault(false); selectNone_=new QPushButton("清空"); selectNone_->setAutoDefault(false); toolRow->addWidget(selectAll_); toolRow->addWidget(selectNone_); selectionTools->setVisible(restore); layout->addWidget(selectionTools);
    selection_=new QListWidget; selection_->setObjectName("backupSelection"); selection_->setAccessibleName("选择恢复的订阅及新增名称"); selection_->setMinimumHeight(70); selection_->setWordWrap(true); selection_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); selection_->setVisible(restore); layout->addWidget(selection_,1);
    connect(selection_,&QListWidget::itemChanged,this,[this] { updateActions(); }); connect(settings_,&QCheckBox::toggled,this,[this] { updateActions(); });
    const auto checkAll=[this](Qt::CheckState state) { const QSignalBlocker blocker(selection_); for (int i=0;i<selection_->count();++i) selection_->item(i)->setCheckState(state); updateActions(); }; connect(selectAll_,&QPushButton::clicked,this,[checkAll] { checkAll(Qt::Checked); }); connect(selectNone_,&QPushButton::clicked,this,[checkAll] { checkAll(Qt::Unchecked); });
    auto* actions = new QDialogButtonBox(QDialogButtonBox::Cancel); actions->button(QDialogButtonBox::Cancel)->setText("取消"); preview_ = actions->addButton("预览校验", QDialogButtonBox::ActionRole); preview_->setObjectName("previewBackup"); preview_->setVisible(restore); submit_ = actions->addButton(restore ? "恢复备份" : "创建备份", QDialogButtonBox::ActionRole); submit_->setObjectName("submitBackup"); submit_->setProperty("primary",true); outer->addWidget(actions);
    connect(actions, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(stopCore_,&QPushButton::clicked,this,[this] {
        stopping_=true; updateActions();
        controller_->stopCore();
    });
    connect(browse, &QPushButton::clicked, this, [this] { const auto path = restore_ ? QFileDialog::getOpenFileName(this,"选择备份",{},"Mlash 备份 (*.mlashbackup)") : QFileDialog::getSaveFileName(this,"保存备份","Mlash-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")+".mlashbackup","Mlash 备份 (*.mlashbackup)"); if (!path.isEmpty()) path_->setText(path); });
    for (auto* field : {path_,password_,confirm_}) connect(field,&QLineEdit::textChanged,this,[this] { digest_.clear(); contents_->clear(); previewNames_.clear(); selection_->clear(); updateActions(); });
    connect(controller_,&AppController::stateChanged,this,[this] { updateActions(); });
    connect(preview_,&QPushButton::clicked,this,[this] {
        previewing_ = true; updateActions(); const QPointer<BackupDialog> guard(this);
        controller_->previewBackup(path_->text(),password_->text(),[guard](Result result) {
            if (!guard) return; guard->previewing_ = false;
            if (!result.ok) guard->hint_->setText(result.error);
            else { guard->digest_ = result.data.value("sha256").toString(); QStringList names; for (const auto& name : result.data.value("profiles").toArray()) names.append(name.toString()); const auto stamp=result.data.value("created_utc").toString(); const auto date=QDateTime::fromString(stamp,Qt::ISODate); guard->contents_->setText(QString("创建时间：%1\n备份订阅：%2").arg(date.isValid()?date.toLocalTime().toString("yyyy-MM-dd HH:mm"):QString("未知")).arg(names.size())); guard->contents_->setToolTip(stamp); guard->previewNames_=names; { const QSignalBlocker blocker(guard->selection_); guard->selection_->clear(); for (const auto& name:names) { auto* item=new QListWidgetItem(name,guard->selection_); item->setFlags(item->flags()|Qt::ItemIsUserCheckable); item->setCheckState(Qt::Checked); } } guard->hint_->setText("勾选需要的订阅；同名自动编号，现有订阅保留。恢复时继续校验所选版本和提供者快照。"); }
            guard->updateActions();
        });
    });
    connect(submit_,&QPushButton::clicked,this,[this] {
        if (!restore_ && QFileInfo::exists(path_->text()) && QMessageBox::question(this,"覆盖备份","替换现有备份文件？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
        submitted_ = true; updateActions();
        if (restore_) { QJsonArray selected; for (int i=0;i<selection_->count();++i) if (selection_->item(i)->checkState()==Qt::Checked) selected.append(i); controller_->restoreBackup(path_->text(),password_->text(),digest_,settings_->isChecked(),selected); } else controller_->exportBackup(path_->text(),password_->text(),profiles_->isChecked());
    });
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& message) {
        if (controller_->state().busy) return;
        if (stopping_) {
            stopping_=false;
            hint_->setText(ok?"内核已停止；确认所选内容后点击恢复备份。":message);
            updateActions(); return;
        }
        if (!submitted_) return;
        submitted_=false;
        if (ok) { password_->clear(); confirm_->clear(); accept(); }
        else { hint_->setText(message); updateActions(); }
    });
    updateActions();
}
void BackupDialog::setBackupPath(const QString& path) { path_->setText(path); }
void BackupDialog::updateActions() {
    const auto& state=controller_->state();
    const bool available = !state.busy && !state.exiting && !previewing_ && !submitted_ && !stopping_;
    const bool stopped=state.core==CoreState::Stopped&&state.proxy!=ProxyState::Owned;
    findChild<QLabel*>("restorePrerequisiteText")->setText(stopped?"内核已停止；确认所选内容后恢复":"恢复前需停止内核并释放本程序代理");
    stopCore_->setVisible(restore_&&!stopped);
    stopCore_->setEnabled(available&&!state.readOnly);
    QStringList chosen; for (int i=0;i<selection_->count();++i) if (selection_->item(i)->checkState()==Qt::Checked) chosen.append(previewNames_.value(i)); const auto targets=restoreProfileNames(chosen,controller_->profiles()); int target=0; { const QSignalBlocker blocker(selection_); for (int i=0;i<selection_->count();++i) { auto* item=selection_->item(i); const auto name=previewNames_.value(i); const auto text=item->checkState()==Qt::Checked?name+" → "+targets.value(target++):name+"（不恢复）"; item->setText(text); item->setToolTip(text); } }
    selection_->setEnabled(available); settings_->setEnabled(available); profiles_->setEnabled(available); selectAll_->setEnabled(available&&!previewNames_.isEmpty()); selectNone_->setEnabled(available&&!previewNames_.isEmpty());
    const bool valid = !path_->text().isEmpty() && password_->text().size() >= 8;
    preview_->setEnabled(available && valid);
    submit_->setEnabled(available && valid && !state.readOnly && (restore_ ? stopped&&!digest_.isEmpty()&&(!chosen.isEmpty()||settings_->isChecked()) : password_->text() == confirm_->text()));
    submit_->setText(restore_?QString("恢复 %1 项").arg(chosen.size()+(settings_->isChecked()?1:0)):QString("创建备份"));
    submit_->setToolTip(restore_&&!stopped?"先停止内核并释放 Mlash 的系统代理，再确认恢复；预览和选择会保留。":QString());
    for (auto* field : {path_,password_,confirm_}) field->setEnabled(available);
}
}
