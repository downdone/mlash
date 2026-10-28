#include "LocalBackupDialog.h"
#include "ContextNote.h"
#include "BackupDialog.h"
#include "DialogDraftGuard.h"
#include "UiTheme.h"
#include "profile/LocalBackup.h"
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace mlash {
LocalBackupDialog::LocalBackupDialog(AppController* controller,QWidget* parent)
    :QDialog(parent),controller_(controller),saved_(controller->settings().localBackup) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("localBackupDialog");
    setWindowTitle("本地备份与保护"); resize(660,540);
    auto* outer=new QVBoxLayout(this); auto* scroll=new QScrollArea; scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true); auto* surface=new QWidget; auto* layout=new QVBoxLayout(surface); layout->setContentsMargins(0,0,0,0); scroll->setWidget(surface); outer->addWidget(scroll,1);
    layout->addWidget(contextNote("请保管备份密码；修改密码后仍需保留旧密码。","计划在托盘中继续执行。快照使用备份密码加密，恢复时需要此密码；修改密码仅影响之后的快照，请保存旧密码。"));
    auto* form=new QFormLayout;
    enabled_=new QCheckBox("按计划自动备份"); enabled_->setObjectName("localBackupEnabled");
    before_=new QCheckBox("重大变更前创建快照"); before_->setObjectName("localBackupBeforeChanges");
    before_->setToolTip("应用运行配置、内核更新、删除订阅或恢复前先备份；失败则停止变更");
    auto* toggles=new QHBoxLayout; toggles->addWidget(enabled_); toggles->addWidget(before_); form->addRow(toggles);
    hours_=new QSpinBox; hours_->setObjectName("localBackupHours"); hours_->setRange(1,168); hours_->setButtonSymbols(QAbstractSpinBox::NoButtons); hours_->setSuffix(" 小时");
    retain_=new QSpinBox; retain_->setObjectName("localBackupRetain"); retain_->setRange(1,30); retain_->setButtonSymbols(QAbstractSpinBox::NoButtons); retain_->setSuffix(" 份");
    auto* schedule=new QHBoxLayout; schedule->addWidget(hours_); schedule->addWidget(new QLabel("保留最近")); schedule->addWidget(retain_); schedule->addStretch();
    form->addRow("备份周期",schedule);
    password_=new QLineEdit; password_->setObjectName("localBackupPassword"); password_->setEchoMode(QLineEdit::Password); password_->setMaxLength(1024);
    confirm_=new QLineEdit; confirm_->setObjectName("localBackupConfirm"); confirm_->setEchoMode(QLineEdit::Password); confirm_->setMaxLength(1024);
    form->addRow("备份密码",password_); form->addRow("再次输入",confirm_); layout->addLayout(form);
    enabled_->setChecked(saved_.value("enabled").toBool()); before_->setChecked(saved_.value("before_changes").toBool(true));
    hours_->setValue(saved_.value("interval_hours").toInt(24)); retain_->setValue(saved_.value("retain").toInt(7));
    password_->setText(saved_.value("password").toString()); confirm_->setText(password_->text()); displayed_=draft();
    auto* settings=new QHBoxLayout;
    save_=new QPushButton("保存计划"); save_->setObjectName("saveLocalBackupSettings"); save_->setProperty("primary",true);
    clear_=new QPushButton("停用并清除密码"); clear_->setObjectName("clearLocalBackupSettings");
    settings->addWidget(save_); settings->addWidget(clear_); settings->addStretch(); layout->addLayout(settings);
    auto* tools=new QHBoxLayout;
    run_=new QPushButton("立即备份"); run_->setObjectName("runLocalBackup"); refresh_=new QPushButton("刷新列表");
    auto* directory=new QPushButton("打开目录"); restore_=new QPushButton("预览恢复…"); restore_->setObjectName("restoreLocalBackup");
    tools->addWidget(run_); tools->addWidget(refresh_); tools->addStretch(); tools->addWidget(directory); tools->addWidget(restore_); layout->addLayout(tools);
    files_=new QTableWidget(0,4); files_->setObjectName("localBackupFiles"); files_->setHorizontalHeaderLabels({"创建时间","原因","订阅数","完整性"});
    files_->setShowGrid(false); files_->setFrameShape(QFrame::NoFrame); files_->verticalHeader()->setDefaultSectionSize(UiTheme::TableRow); files_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    files_->setEditTriggers(QAbstractItemView::NoEditTriggers); files_->setSelectionBehavior(QAbstractItemView::SelectRows);
    files_->setSelectionMode(QAbstractItemView::SingleSelection); files_->verticalHeader()->hide(); files_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    files_->setMinimumHeight(120); layout->addWidget(files_,1);
    hint_=new QLabel; hint_->setObjectName("localBackupHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); layout->addWidget(hint_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close); buttons->button(QDialogButtonBox::Close)->setText("关闭"); outer->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    const auto saveConfig=[this](const QJsonObject& config) { saving_=true; updateActions(); controller_->updateLocalBackupConfig(config,saved_); };
    connect(save_,&QPushButton::clicked,this,[this,saveConfig] { saveConfig(draft()); });
    connect(clear_,&QPushButton::clicked,this,[this,saveConfig] { if (QMessageBox::question(this,"停用本地备份","停止计划并清除本机保存的备份密码？已有快照保留，恢复时仍需原密码。",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes) saveConfig({}); });
    connect(run_,&QPushButton::clicked,this,[this] { working_=true; updateActions(); controller_->startLocalBackup(); });
    connect(refresh_,&QPushButton::clicked,this,&LocalBackupDialog::refresh);
    connect(directory,&QPushButton::clicked,this,[this] { QDesktopServices::openUrl(QUrl::fromLocalFile(controller_->dataRoot()+"/backups/local")); });
    connect(restore_,&QPushButton::clicked,this,[this] {
        const auto row=files_->currentRow(); if (row<0) return;
        auto* dialog=new BackupDialog(controller_,true,this); dialog->setBackupPath(files_->item(row,0)->data(Qt::UserRole).toString()); dialog->show();
    });
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& message) {
        if (!saving_&&!working_) return;
        const bool saving=saving_; saving_=working_=false; hint_->setText(message);
        if (ok&&saving) {
            saved_=controller_->settings().localBackup;
            if (saved_.isEmpty()) { enabled_->setChecked(false); before_->setChecked(false); password_->clear(); confirm_->clear(); }
            displayed_=draft();
        }
        updateActions(); if (ok) refresh();
    });
    connect(controller_,&AppController::stateChanged,this,&LocalBackupDialog::updateActions);
    for (auto* box:{enabled_,before_}) connect(box,&QCheckBox::toggled,this,&LocalBackupDialog::updateActions);
    for (auto* number:{hours_,retain_}) connect(number,&QSpinBox::valueChanged,this,&LocalBackupDialog::updateActions);
    for (auto* edit:{password_,confirm_}) connect(edit,&QLineEdit::textChanged,this,&LocalBackupDialog::updateActions);
    connect(files_,&QTableWidget::itemSelectionChanged,this,&LocalBackupDialog::updateActions);
    DialogDraftGuard::protect(this,[this] { return draft()!=displayed_||password_->text()!=confirm_->text(); },[this] { return saving_; });
    updateActions(); refresh();
}
QJsonObject LocalBackupDialog::draft() const {
    return {{"enabled",enabled_->isChecked()},{"before_changes",before_->isChecked()},
        {"interval_hours",hours_->value()},{"retain",retain_->value()},{"password",password_->text()}};
}
void LocalBackupDialog::updateActions() {
    const bool available=!saving_&&!working_&&!listing_&&!controller_->state().busy&&!controller_->state().exiting&&!controller_->state().readOnly;
    const bool dirty=draft()!=displayed_;
    save_->setEnabled(available&&dirty&&validateLocalBackup(draft()).ok&&password_->text()==confirm_->text());
    clear_->setEnabled(available&&!saved_.isEmpty()); run_->setEnabled(available&&!dirty&&!saved_.isEmpty()); refresh_->setEnabled(available);
    const auto row=files_->currentRow();
    restore_->setEnabled(available&&row>=0&&files_->item(row,0)->data(Qt::UserRole+1).toBool());
    for (auto* edit:{password_,confirm_}) edit->setEnabled(available);
    enabled_->setEnabled(available); before_->setEnabled(available); hours_->setEnabled(available); retain_->setEnabled(available);
}
void LocalBackupDialog::refresh() {
    if (listing_||controller_->state().busy) return;
    listing_=true; updateActions(); const QPointer<LocalBackupDialog> guard(this);
    controller_->inspectLocalBackups([guard](Result result) {
        if (!guard) return;
        guard->listing_=false;
        if (!result.ok) { guard->hint_->setText(result.error); guard->updateActions(); return; }
        guard->files_->setRowCount(0);
        for (const auto& item:result.data.value("rows").toArray()) {
            const auto value=item.toObject(); const int row=guard->files_->rowCount(); guard->files_->insertRow(row);
            const auto reason=value.value("reason").toString();
            const auto stamp=QDateTime::fromString(value.value("created_utc").toString(),Qt::ISODateWithMs).toLocalTime();
            const QStringList columns{stamp.isValid()?stamp.toString("MM-dd HH:mm:ss"):QString("—"),
                reason=="manual"?"手动":reason=="scheduled"?"计划":reason=="before-change"?"变更前":"—",value.value("profiles").isDouble()?QString::number(value.value("profiles").toInt()):QString("—"),value.value("valid").toBool()?"已校验":"需检查"};
            for (int col=0;col<4;++col) { auto* cell=new QTableWidgetItem(columns[col]); cell->setToolTip(value.value("valid").toBool()?QString("文件SHA-256匹配；恢复时还会验证密码和档案认证"):value.value("error").toString()); guard->files_->setItem(row,col,cell); }
            guard->files_->item(row,0)->setData(Qt::UserRole,value.value("path")); guard->files_->item(row,0)->setData(Qt::UserRole+1,value.value("valid"));
        }
        guard->updateActions();
    });
}
}
