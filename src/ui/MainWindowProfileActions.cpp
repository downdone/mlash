#include "app/AppController.h"
#include "TaskDialog.h"
#include "TaskWorkspace.h"
#include "MainWindow.h"
#include "CompactComboBox.h"
#include "DnsDialog.h"
#include "DialogDraftGuard.h"
#include "DownloadPolicyEditor.h"
#include "ImportDialog.h"
#include "ProfileChangesDialog.h"
#include "ProfileCardWidget.h"
#include "ProviderPolicyDialog.h"
#include "PrototypeUi.h"
#include "ProxyChainDialog.h"
#include "ProxyEditorDialog.h"
#include "RuleOverlayDialog.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "WidgetStyle.h"
#include "ViewIds.h"
#include "WindowUiFactories.h"
#include "profile/SubscriptionInfo.h"
#include "qrcodegen.hpp"
#include <QtWidgets>
#include "MenuSection.h"
#include "ProfileRowModel.h"
#include <QFileInfo>
#include <QUrl>
#include "profile/NodeLinks.h"
#include "WrapLayout.h"
#include <QSet>
#include <QStyledItemDelegate>
#include <QTimer>
#include <algorithm>

#include "ImportSource.h"
namespace mlash {
using namespace WindowParts;

void MainWindow::showProfileMenu(const QString& id, const QPoint& globalPoint) {
    if (id.isEmpty()) return;
    const auto& tunnels=controller_->wireGuardProfiles();
    const auto configuration=std::find_if(tunnels.begin(),tunnels.end(),[&](const auto& item) { return item.id==id; });
    if (configuration!=tunnels.end()) {
        auto* menu=new QMenu(this);
        auto* detail=menu->addAction("查看隧道");
        auto* replace=menu->addAction("替换配置文件…");
        auto* rename=menu->addAction("重命名…");
        auto* remove=menu->addAction("删除配置…");
        const auto& state=controller_->state();
        const bool writable=!state.busy&&!state.readOnly;
        replace->setEnabled(writable); rename->setEnabled(writable);
        remove->setEnabled(writable&&!controller_->wireGuardObservation().value("active").toBool()&&
            !controller_->wireGuardObservation().value("cleanup_pending").toBool());
        connect(detail,&QAction::triggered,this,[this,id] { showWireGuardProfile(id); });
        connect(rename,&QAction::triggered,this,[this,id,name=configuration->name] {
            bool ok=false;
            const auto next=QInputDialog::getText(this,"重命名 WireGuard","配置名称",QLineEdit::Normal,name,&ok);
            if (ok&&!next.trimmed().isEmpty()) controller_->renameProfile(id,next);
        });
        connect(remove,&QAction::triggered,this,[this,id,name=configuration->name] {
            if (QMessageBox::question(this,"删除 WireGuard 配置","删除配置「"+name+"」以及本地版本？",
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes) controller_->deleteProfile(id);
        });
        connect(replace,&QAction::triggered,this,[this,id] {
            const auto& catalog=controller_->wireGuardProfiles();
            const auto profile=std::find_if(catalog.begin(),catalog.end(),[&](const auto& p) { return p.id==id; });
            if (profile==catalog.end()) return;
            const auto expected=profile->candidate;
            const auto path=QFileDialog::getOpenFileName(this,"选择 WireGuard 配置",{},"WireGuard (*.conf);;所有文件 (*)");
            if (!path.isEmpty()) reviewWireGuardImport(path,{},nullptr,id,expected);
        });
        connect(menu,&QMenu::aboutToHide,menu,&QObject::deleteLater);
        WidgetStyle::popupMenu(menu,globalPoint);
        return;
    }
    bool remote = false;
    bool pending = false;
    for (const auto& profile : controller_->profiles()) if (profile.id == id) {
        remote = profile.sourceType == "url";
        pending = !profile.active.isEmpty() && !profile.candidate.isEmpty() && profile.active!=profile.candidate;
        break;
    }
    auto* menu = profileContextMenu_;
    if (!menu) return;
    menu->clear();
    menu->setProperty("contextProfileId", id);
    menu->setProperty("profileKind", remote ? "remote" : "local");
    auto* content = menu->addAction("编辑配置内容");
    connect(content, &QAction::triggered, this, [this, id] { showProfileDetails(id); });
    auto* edit = menu->addAction(remote ? "编辑来源与更新" : "管理本地文件");
    connect(edit, &QAction::triggered, this, [this, id] { openProfileTask(id, "source"); });
    if (pending || id!=controller_->state().activeProfile) {
        auto* use=menu->addAction(pending ? "查看更新…" : "使用此配置");
        use->setObjectName("profileUseAction");
        use->setEnabled(pending || (!controller_->state().busy && !controller_->state().readOnly && !controller_->state().exiting));
        connect(use,&QAction::triggered,this,[this,id,pending] {
            if (pending) openProfileTask(id,"candidate");
            else controller_->activateProfile(id);
        });
    }
    if (remote) {
        auto* share = menu->addAction("分享配置…");
        connect(share, &QAction::triggered, this, [this, id] {
            selectProfile(id);
            shareProfileScheme();
        });
    }
    menu->addSeparator();
    auto* remove = menu->addAction("删除配置…");
    remove->setObjectName("deleteProfile");
    connect(remove, &QAction::triggered, this, [this, id] {
        if (QMessageBox::question(this, "删除配置", "删除此配置快照？当前配置将先安全停止。",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            controller_->deleteProfile(id);
    });
    WidgetStyle::popupMenu(menu, globalPoint);
}

void MainWindow::openProfileTask(const QString& id, const QString& task) {
    const auto& tunnels=controller_->wireGuardProfiles();
    if (std::any_of(tunnels.begin(),tunnels.end(),[&](const auto& item) { return item.id==id; })) {
        showWireGuardProfile(id,task=="candidate"?2:0);
        return;
    }
    Profile profile;
    bool found = false;
    for (const auto& item : controller_->profiles()) if (item.id == id) { profile = item; found = true; break; }
    if (!found) return;
    auto* workspace = taskWorkspace();
    QList<TaskWorkspace::Task> tasks;
    QString context = "profile:" + id + ":content";
    if (task == "source") {
        context = "profile:" + id + ":source";
        tasks.append({"source", "来源与更新", [this, id] { selectProfile(id); editProfilePolicy(); }});
    } else if (task == "candidate") {
        context = "profile:" + id + ":candidate";
        tasks.append({"candidate", "更新变化", [this, id] { presentTask(new ProfileChangesDialog(controller_, id, this)); }});
    } else {
        tasks = {
            {"nodes", "节点与组", [this, id] { presentTask(new ProxyEditorDialog(controller_, id, this)); }},
            {"rules", "规则", [this, id] { presentTask(new RuleOverlayDialog(controller_, id, this)); }},
            {"dns", "DNS", [this, id] { presentTask(new DnsDialog(controller_, id, this)); }},
            {"chain", "拨号链", [this, id] { presentTask(new ProxyChainDialog(controller_, id, this)); }},
            {"providers", "规则源/节点源", [this, id] { showProviders(id); }},
            {"compatibility", "兼容性", [this, id] { showProfileCompatibility(id); }}
        };
    }
    if (!workspace->configure(context, profile.name, tasks, [this] { returnFromWorkspace(); })) return;
    if (profiles_ && contentStack_->currentIndex() == 0)
        profileListReturnScroll_ = profiles_->verticalScrollBar()->value();
    profileDetailId_ = id;
    selectProfile(id);
    workspace->select(task);
}
bool MainWindow::canOpenProfile(const QString& id) {
    for (const auto& profile : controller_->profiles()) if (profile.id == id) return true;
    return false;
}
void MainWindow::showProfileDetails(const QString& id) {
    if (!canOpenProfile(id)) return;
    openProfileTask(id, "nodes");
}
// 分享订阅二维码（P14）：地址可能含私人令牌，先给警告，再按需显示。
// 从右键菜单和行详情抽屉的「更多操作」共用。
void MainWindow::shareProfileScheme() {
    const auto data = controller_->profileMaintenance(selectedProfile()); if (data.value("source_type") != "url") return;
    auto* dialog = new TaskDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profileShareDialog"); dialog->setWindowTitle("分享订阅"); auto* box = new QVBoxLayout(dialog);
    auto* warning = new QLabel("订阅地址可能含私人令牌。二维码等同于完整地址，请仅向可信设备展示。"); warning->setWordWrap(true); box->addWidget(warning);
    auto* reveal = new QPushButton("显示私人二维码"); reveal->setObjectName("revealProfileQr"); box->addWidget(reveal);
    auto* image = new QLabel; image->setObjectName("profileQrImage"); image->setAlignment(Qt::AlignCenter); box->addWidget(image);
    connect(reveal, &QPushButton::clicked, dialog, [dialog, reveal, image, source = data.value("source").toString()] {
        const auto bytes = source.toUtf8(); if (bytes.size() > 2048) { image->setText("地址过长，无法生成适合扫描的二维码"); return; }
        try {
            const auto code = qrcodegen::QrCode::encodeText(bytes.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
            const int size = code.getSize(), scale = qMax(2, 320 / (size + 8));
            QImage pixels((size + 8) * scale, (size + 8) * scale, QImage::Format_RGB32); pixels.fill(Qt::white);
            QPainter painter(&pixels); painter.setPen(Qt::NoPen); painter.setBrush(Qt::black);
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) if (code.getModule(x, y)) painter.drawRect((x + 4) * scale, (y + 4) * scale, scale, scale);
            painter.end(); image->setPixmap(QPixmap::fromImage(pixels)); image->setMinimumSize(pixels.size()); reveal->hide(); dialog->adjustSize();
        } catch (const std::exception&) { image->setText("地址无法编码为二维码"); }
    });
    auto* close = new QPushButton("关闭"); box->addWidget(close); connect(close, &QPushButton::clicked, dialog, &QDialog::close); dialog->resize(380, 180); presentTask(dialog);
}


void MainWindow::showProfileCompatibility(const QString& id) {
    auto* dialog=new TaskDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("profileCompatibilityTask"); dialog->setWindowTitle("配置兼容性");
    auto* content=new QVBoxLayout(dialog);
    auto* report=new QPlainTextEdit(dialog); report->setObjectName("compatibilityReport");
    report->setReadOnly(true); report->setPlainText("正在检查…"); content->addWidget(report,1);
    auto* close=new QPushButton("关闭",dialog); content->addWidget(close,0,Qt::AlignRight);
    connect(close,&QPushButton::clicked,dialog,&QDialog::close);
    presentTask(dialog);
    controller_->inspectProfileCompatibility(id,[guard=QPointer<QPlainTextEdit>(report)](Result result) {
        if (!guard) return;
        if (!result.ok) {
            guard->setPlainText(result.error+"\n错误码："+result.data.value("code").toString());
            return;
        }
        QStringList managed;
        for (const auto& value:result.data.value("managed_fields").toArray()) managed.append(value.toString());
        guard->setPlainText("静态兼容检查通过\n\n"+
            (managed.isEmpty()?QString("源配置没有显式设置受控字段。"):
             QString("以下源字段由 Mlash 覆盖或移除：\n")+managed.join('\n'))+
            "\n\n本报告不联网、不改配置；实际可用性仍需内核校验。");
    });
}

void MainWindow::showProviders(const QString& id, const QString& kind) {
    if (id.isEmpty()) return;
    auto* dialog = new TaskDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("providersDialog"); dialog->setWindowTitle(kind=="rule-providers"?"规则源（Provider）":"节点源（Provider）"); dialog->resize(680, 410);
    auto* content = new QVBoxLayout(dialog); auto* versions=new CompactComboBox; versions->setObjectName("providerVersion"); versions->addItem("待使用版本",false); versions->addItem("上次使用版本",true); content->addWidget(versions);
    auto* hint = new QLabel("正在读取…"); hint->setTextFormat(Qt::PlainText); hint->setWordWrap(true); content->addWidget(hint);
    auto* table = new QTableWidget(0, 5); table->setObjectName("providersTable"); table->setHorizontalHeaderLabels({"名称", "类别", "条目", "快照时间", "状态"}); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->verticalHeader()->hide(); styleTable(table); table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch); table->setColumnWidth(2, 58); table->setColumnWidth(3, 125); content->addWidget(table, 1);
    auto* row = new QHBoxLayout; auto* reload = button("刷新列表", row); auto* refresh = button("更新选中", row); refresh->setObjectName("refreshProvider"); auto* policy=button("下载与更新…",row); policy->setObjectName("providerPolicy"); auto* all = button("更新全部", row); all->setObjectName("refreshAllProviders"); row->addStretch(); auto* compare=button("查看变化 / 应用",row); compare->setObjectName("providerApplyCandidate"); auto* close = button("关闭", row); content->addLayout(row); refresh->setEnabled(false); all->setEnabled(false);
    auto waiting = std::make_shared<bool>(false); const QPointer<QTableWidget> guarded(table); const QPointer<QLabel> guardedHint(hint);
    auto load = std::make_shared<std::function<void()>>();
    *load = [this, id, kind, guarded, guardedHint, versions] {
        if (!guarded) return;
        const bool running=versions->currentData().toBool(); versions->setEnabled(false);
        controller_->inspectProviders(id, [guarded, guardedHint, kind, versions](Result result) {
            if (!guarded || !guardedHint) return;
            versions->setEnabled(true); guarded->setRowCount(0); if (!result.ok) { guardedHint->setText(result.error); return; }
            QStringList errors;
            for (const auto& value : result.data.value("providers").toArray()) {
                const auto item = value.toObject(); if (!kind.isEmpty() && item.value("kind").toString() != kind && item.value("type")!="orphan") continue;
                const int index = guarded->rowCount(); guarded->insertRow(index); const auto stamp = QDateTime::fromString(item.value("updated_utc").toString(), Qt::ISODate).toLocalTime();
                const QStringList cells{item.value("name").toString(), item.value("type")=="orphan"?"未引用":item.value("kind") == "proxy-providers" ? "节点" : "规则", item.contains("count") ? QString::number(item.value("count").toInt()) : "—", stamp.isValid() ? stamp.toString("MM-dd HH:mm:ss") : "—", item.value("status").toString()};
                for (int column = 0; column < cells.size(); ++column) { auto* cell = new QTableWidgetItem(cells[column]); cell->setToolTip(item.value("source_host").toString() + "\nSHA256 " + item.value("sha256").toString() + "\n" + item.value("error").toString()); guarded->setItem(index, column, cell); }
                guarded->item(index,0)->setData(Qt::UserRole+1,item); guarded->item(index, 0)->setData(Qt::UserRole, (item.value("type") == "http"||item.value("type")=="orphan") ? item.value("id").toString() : QString());
                if (!item.value("error").toString().isEmpty()) errors.append(item.value("name").toString() + "：" + item.value("error").toString());
            }
            guardedHint->setText(errors.isEmpty() ? (guarded->rowCount() ? "" : "此版本没有对应来源") : errors.join('\n'));
        },running);
    };
    auto update = [this, id, table, refresh, policy, all, reload, waiting, versions, compare] {
        const bool available = !controller_->state().busy && !controller_->state().exiting && !controller_->state().readOnly && !versions->currentData().toBool();
        const auto* selected = table->item(table->currentRow(), 0); refresh->setEnabled(available && selected && selected->data(Qt::UserRole+1).toJsonObject().value("type")=="http"); policy->setEnabled(available&&selected&&!selected->data(Qt::UserRole).toString().isEmpty());
        bool remote = false; for (int i = 0; i < table->rowCount(); ++i) remote |= table->item(i,0)->data(Qt::UserRole+1).toJsonObject().value("type")=="http"; all->setEnabled(available && remote); reload->setEnabled(!*waiting); bool pending=false; for (const auto& profile:controller_->profiles()) if (profile.id==id) pending=!profile.candidate.isEmpty()&&profile.candidate!=profile.active; compare->setEnabled(pending&&!controller_->state().busy&&!controller_->state().exiting);
    };
    connect(table, &QTableWidget::itemSelectionChanged, dialog, update); connect(table->model(), &QAbstractItemModel::rowsInserted, dialog, [dialog, update] { QTimer::singleShot(0, dialog, update); });
    connect(controller_, &AppController::stateChanged, dialog, update);
    connect(reload, &QPushButton::clicked, dialog, [load] { (*load)(); });
    connect(policy,&QPushButton::clicked,dialog,[this,id,table,dialog,load] { const auto* item=table->item(table->currentRow(),0); if (!item||item->data(Qt::UserRole).toString().isEmpty()) return; auto* editor=new ProviderPolicyDialog(controller_,id,item->data(Qt::UserRole+1).toJsonObject(),dialog); connect(editor,&QDialog::accepted,dialog,[load] { (*load)(); }); presentTask(editor); });
    connect(refresh, &QPushButton::clicked, dialog, [this, id, table, waiting] { const auto* selected = table->item(table->currentRow(), 0); if (!selected) return; *waiting = true; controller_->refreshProviders(id, selected->data(Qt::UserRole).toString()); });
    connect(all, &QPushButton::clicked, dialog, [this, id, kind, waiting] { *waiting = true; controller_->refreshProviders(id, kind.isEmpty() ? "*" : "kind:" + kind); });
    connect(controller_, &AppController::operationFinished, dialog, [waiting, guardedHint, load, update](bool ok, const QString& message) { if (!*waiting) return; *waiting = false; if (guardedHint) guardedHint->setText(ok ? "更新已就绪，可查看变化并应用。" : message); (*load)(); update(); });
    connect(versions,&QComboBox::currentIndexChanged,dialog,[load,update] { update(); (*load)(); }); connect(compare,&QPushButton::clicked,dialog,[this,id,dialog] { presentTask(new ProfileChangesDialog(controller_,id,dialog),"changes:"+id); }); connect(controller_,&AppController::profilesChanged,dialog,update);
    connect(close, &QPushButton::clicked, dialog, &QDialog::close); presentTask(dialog);
    // The embedded table becomes visible after TaskWorkspace switches panels.
    QTimer::singleShot(0, dialog, [load] { (*load)(); }); update();
}

void MainWindow::editProfilePolicy() {
    const auto id = selectedProfile(); Profile profile; bool found = false;
    for (const auto& p : controller_->profiles()) if (p.id == id) { profile = p; found = true; break; }
    if (!found || controller_->state().busy) return;
    auto* dialog = new TaskDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profilePolicyDialog"); dialog->setWindowTitle("订阅来源与维护"); dialog->resize(560, 540);
    auto* outer=new QVBoxLayout(dialog); auto* scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); auto* body=new QWidget; auto* layout=new QVBoxLayout(body); scroll->setWidget(body); outer->addWidget(scroll,1); const auto expected=controller_->profileMaintenance(id);
    auto* form=new QFormLayout; auto* name=new QLineEdit(profile.name); name->setObjectName("profileEditName"); name->setMaxLength(120); form->addRow("名称",name);
    auto* source=new QLineEdit(profile.source); source->setObjectName("profileEditSource"); source->setMaxLength(4096); if (profile.sourceType=="url") source->setEchoMode(QLineEdit::Password); form->addRow(profile.sourceType=="url"?"订阅地址":"源文件",source); layout->addLayout(form);
    if (profile.sourceType=="url") { auto* show=new QCheckBox("显示私人地址"); layout->addWidget(show); connect(show,&QCheckBox::toggled,source,[source](bool visible) { source->setEchoMode(visible?QLineEdit::Normal:QLineEdit::Password); }); }
    else { auto* browse=new QPushButton("更换源文件…"); layout->addWidget(browse,0,Qt::AlignLeft); connect(browse,&QPushButton::clicked,dialog,[dialog,source] { const auto file=QFileDialog::getOpenFileName(dialog,"选择订阅来源",{},"YAML (*.yaml *.yml)"); if (!file.isEmpty()) source->setText(file); }); }
    auto* automatic = new QCheckBox(profile.sourceType == "url" ? "自动定时更新订阅" : "本地文件变化时更新候选"); automatic->setObjectName("profileAutoUpdate"); automatic->setChecked(profile.autoUpdate); layout->addWidget(automatic);
    const auto allowAutomatic=[automatic,source] { const bool enabled=!source->text().trimmed().isEmpty(); automatic->setEnabled(enabled); if (!enabled) automatic->setChecked(false); automatic->setToolTip(enabled?QString():QString("本地快照没有外部来源；选择源文件后可启用监测")); }; connect(source,&QLineEdit::textChanged,dialog,allowAutomatic); allowAutomatic();
    QSpinBox* hours = nullptr;
    if (profile.sourceType == "url") { auto* form = new QFormLayout; hours = new QSpinBox; hours->setButtonSymbols(QAbstractSpinBox::NoButtons); hours->setObjectName("profileUpdateHours"); hours->setRange(1, 168); hours->setSuffix(" 小时"); hours->setValue(profile.updateIntervalHours); hours->setEnabled(profile.autoUpdate); form->addRow("更新周期", hours); layout->addLayout(form); }
    if (hours) connect(automatic, &QCheckBox::toggled, hours, &QSpinBox::setEnabled);
    auto* download = new DownloadPolicyEditor(profile.download); layout->addWidget(download);
    const auto usage = SubscriptionInfo::summary(profile.subscriptionInfo); auto* info = new QLabel(usage.isEmpty() ? "" : usage + "\n来源快照：" + profile.subscriptionInfo.value("observed_at").toString()); info->setTextFormat(Qt::PlainText); info->setWordWrap(true); info->setVisible(!usage.isEmpty()); layout->addWidget(info);
    auto* maintenance=new QLabel("最近成功："+(profile.updated.isEmpty()?QString("尚无"):profile.updated)+"\n最近尝试："+(profile.lastUpdateAttempt.isEmpty()?QString("尚无"):profile.lastUpdateAttempt)+"\n"+(expected.value("next").toString().isEmpty()?QString("无定时任务"):"下次检查："+expected.value("next").toString())+(profile.updateError.isEmpty()?QString():"\n上次失败："+profile.updateError)); maintenance->setTextFormat(Qt::PlainText); maintenance->setWordWrap(true); layout->addWidget(maintenance);
    QPushButton* rollback = nullptr;
    if (!profile.lastGood.isEmpty() && profile.lastGood != profile.active) {
        rollback = new QPushButton("回滚上次良好版本…", body);
        rollback->setObjectName("rollbackProfile");
        rollback->setToolTip("切换此配置到上次验证成功的版本；当前运行版会经过安全校验与状态回读");
        layout->addWidget(rollback, 0, Qt::AlignLeft);
    }
    auto* note = new QLabel("更新后需手动应用。"); note->setWordWrap(true); note->setObjectName("secondaryText"); layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText("保存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); outer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto saving=std::make_shared<bool>(false);
    auto rolling=std::make_shared<bool>(false);
    const auto dirty=[automatic,hours,download,profile,name,source] {
        return name->text()!=profile.name || source->text()!=profile.source ||
            automatic->isChecked()!=profile.autoUpdate ||
            (hours && hours->value()!=profile.updateIntervalHours) || download->policy().toJson()!=profile.download.toJson();
    };
    DialogDraftGuard::protect(dialog,dirty,[saving,rolling] { return *saving || *rolling; });
    if (rollback) {
        connect(rollback,&QPushButton::clicked,dialog,[this,dialog,rollback,rolling,id,profile,dirty,note,buttons] {
            const auto& state=controller_->state();
            if (state.busy || state.readOnly || state.exiting) return;
            confirmProfileVersion(dialog,"回滚配置",profile,profile.lastGood,dirty(),
                [this,rollback,rolling,id,profile,note,buttons] {
                    const auto& state=controller_->state();
                    if (state.busy || state.readOnly || state.exiting) {
                        note->setText("当前不能提交回滚，请稍后重试。");
                        return;
                    }
                    *rolling=true;
                    rollback->setEnabled(false);
                    buttons->setEnabled(false);
                    note->setText("正在校验并回滚目标版本…");
                    controller_->activateProfile(id,true,profile.lastGood);
                });
        });
        connect(controller_,&AppController::operationFinished,dialog,[this,dialog,rollback,rolling,note,buttons](bool ok,const QString& error,OperationId operation) {
            if (!*rolling || operation!=OperationId::ProfileActivate) return;
            *rolling=false;
            if (ok) dialog->accept();
            else {
                rollback->setEnabled(true);
                buttons->setEnabled(true);
                note->setText("回滚失败 · "+error+"\n"+runningProfileVersion(*controller_));
            }
        });
    }
    connect(controller_,&AppController::operationFinished,dialog,[dialog,buttons,note,saving](bool ok,const QString& error) { if (!*saving) return; *saving=false; buttons->setEnabled(true); if (ok) dialog->accept(); else note->setText("保存失败，修改已保留 · "+error); });
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, id, automatic, hours, download, buttons, saving, name, source, expected, note, interval = profile.updateIntervalHours] { if (!download->policy().valid()) { note->setText("User-Agent 须为最多 256 个单行 ASCII 字符"); return; } *saving=true; buttons->setEnabled(false); controller_->editProfileSource(id,name->text(),source->text(),automatic->isChecked(),hours?hours->value():interval,download->policy(),expected); }); presentTask(dialog);
}

} // namespace mlash