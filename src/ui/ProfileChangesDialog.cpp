#include "app/AppController.h"
#include "ProfileChangesDialog.h"
#include "DialogDraftGuard.h"
#include "UiTheme.h"
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace mlash {
namespace {
QString versionLabel(const QString& version) {
    return version.isEmpty() ? QStringLiteral("无") : version.left(12);
}
}

QString runningProfileVersion(const AppController& controller) {
    const auto& state = controller.state();
    if (state.ready) {
        for (const auto& profile : controller.profiles()) {
            if (profile.id == state.activeProfile && !profile.active.isEmpty())
                return QString("当前运行：%1 · %2").arg(profile.name, versionLabel(profile.active));
        }
    }
    return QStringLiteral("当前没有已确认的运行版本");
}

void confirmProfileVersion(QWidget* parent, const QString& action, const Profile& profile,
                           const QString& version, bool discardDraft, std::function<void()> confirmed) {
    if (auto* existing = parent->findChild<QMessageBox*>("profileVersionConfirmation", Qt::FindDirectChildrenOnly)) {
        existing->raise();
        return;
    }
    auto* prompt = new QMessageBox(QMessageBox::Question, action, {}, QMessageBox::Yes | QMessageBox::No, parent);
    prompt->setObjectName("profileVersionConfirmation");
    prompt->setAttribute(Qt::WA_DeleteOnClose);
    prompt->setTextFormat(Qt::PlainText);
    prompt->setText(QString("配置：%1\n目标版本：%2").arg(profile.name, version));
    prompt->setInformativeText(QString("若内核正在运行，切换版本可能短暂中断连接。")
                               + (discardDraft ? QString("\n成功回滚后将放弃未保存的来源修改。") : QString()));
    prompt->button(QMessageBox::Yes)->setText(action);
    prompt->button(QMessageBox::No)->setText("取消");
    prompt->setDefaultButton(QMessageBox::No);
    prompt->setEscapeButton(QMessageBox::No);
    QObject::connect(prompt, &QMessageBox::finished, parent, [confirmed=std::move(confirmed)](int result) {
        if (result == QMessageBox::Yes) confirmed();
    });
    prompt->open();
}

ProfileChangesDialog::ProfileChangesDialog(AppController* controller, const QString& id, QWidget* parent)
    : TaskDialog(parent), controller_(controller), id_(id) {
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("profileChangesDialog");
    setWindowTitle("候选版本变化");
    resize(720,510);
    auto* layout = new QVBoxLayout(this);

    versions_ = new QLabel;
    versions_->setObjectName("versionChangesSummary");
    versions_->setWordWrap(true);
    versions_->setTextFormat(Qt::PlainText);
    layout->addWidget(versions_);

    hint_ = new QLabel;
    hint_->setObjectName("versionChangesHint");
    hint_->setWordWrap(true);
    hint_->setTextFormat(Qt::PlainText);
    layout->addWidget(hint_);

    table_ = new QTableWidget(0,6);
    table_->setObjectName("versionChangesTable");
    table_->setHorizontalHeaderLabels({"类别","已使用","候选","新增","移除","修改"});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->hide();
    table_->setShowGrid(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    table_->verticalHeader()->setDefaultSectionSize(UiTheme::TableRow);
    table_->setMinimumHeight(UiTheme::TableRow*5+8);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table_,1);

    details_ = new QTextEdit;
    details_->setObjectName("versionChangesDetails");
    details_->setAccessibleName("当前类别的变更及受影响的选择");
    details_->setReadOnly(true);
    details_->setAcceptRichText(false);
    layout->addWidget(details_,1);
    metadata_ = new QLabel;
    metadata_->setObjectName("versionChangesMetadata");
    metadata_->setProperty("secondary",true);
    metadata_->setTextFormat(Qt::PlainText);
    metadata_->setWordWrap(true);
    layout->addWidget(metadata_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    refresh_ = buttons->addButton("重新比较", QDialogButtonBox::ActionRole);
    refresh_->setObjectName("reloadComparedVersion");
    apply_ = buttons->addButton("应用更新…", QDialogButtonBox::ActionRole);
    apply_->setObjectName("applyComparedVersion");
    apply_->setProperty("primary",true);
    apply_->setAutoDefault(false);
    layout->addWidget(buttons);

    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(refresh_,&QPushButton::clicked,this,&ProfileChangesDialog::reload);
    connect(table_,&QTableWidget::currentCellChanged,this,[this](int row) { showSection(row); });
    connect(apply_,&QPushButton::clicked,this,&ProfileChangesDialog::confirmApply);
    connect(controller_,&AppController::stateChanged,this,&ProfileChangesDialog::updateActions);
    connect(controller_,&AppController::profilesChanged,this,&ProfileChangesDialog::updateActions);
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& error,OperationId operation) {
        if (!applying_ || operation!=OperationId::ProfileActivate) return;
        applying_ = false;
        if (ok) accept();
        else {
            feedback_ = "应用失败 · " + error + "\n" + runningProfileVersion(*controller_);
            updateActions();
        }
    });
    DialogDraftGuard::protect(this,[] { return false; },[this] { return applying_; });
    reload();
}

const Profile* ProfileChangesDialog::currentProfile() const {
    for (const auto& profile : controller_->profiles()) {
        if (profile.id == id_) return &profile;
    }
    return nullptr;
}

void ProfileChangesDialog::reload() {
    if (loading_ || applying_) return;
    loading_ = true;
    feedback_.clear();
    report_ = {};
    table_->setRowCount(0);
    details_->clear();
    updateActions();
    const QPointer<ProfileChangesDialog> guard(this);
    controller_->inspectProfileChanges(id_,[guard](Result result) {
        if (!guard) return;
        guard->loading_ = false;
        if (!result.ok) {
            guard->feedback_ = result.error;
            guard->updateActions();
            return;
        }
        guard->report_ = result.data;
        const auto sections = result.data.value("sections").toArray();
        guard->table_->setRowCount(sections.size());
        for (int row=0; row<sections.size(); ++row) {
            const auto item = sections[row].toObject();
            const QStringList cells{item.value("label").toString(),QString::number(item.value("before").toInt()),
                QString::number(item.value("after").toInt()),QString::number(item.value("added_count").toInt()),
                QString::number(item.value("removed_count").toInt()),QString::number(item.value("changed_count").toInt())};
            for (int column=0; column<cells.size(); ++column)
                guard->table_->setItem(row,column,new QTableWidgetItem(cells[column]));
        }
        guard->table_->setCurrentCell(0,0);
        guard->showSection(0);
        guard->updateActions();
    });
}

void ProfileChangesDialog::updateActions() {
    const auto* profile = currentProfile();
    const auto candidate = report_.value("candidate_version").toString();
    const auto active = report_.value("active_version").toString();
    const auto& state = controller_->state();
    const bool stale = !report_.isEmpty() && (!profile || profile->active!=active || profile->candidate!=candidate);
    const bool fresh = profile && !stale && !candidate.isEmpty() && candidate!=active;
    apply_->setEnabled(fresh&&!loading_&&!applying_&&!state.busy&&!state.exiting&&!state.readOnly);
    refresh_->setEnabled(profile&&!loading_&&!applying_&&!state.exiting);

    const auto shownActive = report_.isEmpty() && profile ? profile->active : active;
    const auto shownCandidate = report_.isEmpty() && profile ? profile->candidate : candidate;
    const auto lastGood = profile ? profile->lastGood : QString();
    const auto activeStamp = report_.value("active_time").toString();
    const auto candidateStamp = report_.value("candidate_time").toString();
    const auto snapshot = [](const QString& version, const QString& stamp) {
        const auto local=QDateTime::fromString(stamp,Qt::ISODate).toLocalTime();
        return versionLabel(version) + (local.isValid() ? "  "+local.toString("MM-dd HH:mm") : QString());
    };
    auto heading=profile?profile->name:QString("配置已移除");
    if (!report_.isEmpty()) {
        int added=0,removed=0,changed=0;
        for (const auto& value : report_.value("sections").toArray()) {
            const auto section=value.toObject();
            added+=section.value("added_count").toInt();
            removed+=section.value("removed_count").toInt();
            changed+=section.value("changed_count").toInt();
        }
        heading+=QString("\n新增 %1 项 · 移除 %2 项 · 修改 %3 项").arg(added).arg(removed).arg(changed);
        const int lost=report_.value("lost_selections").toArray().size();
        if (lost) heading+=QString("\n有 %1 项已保存选择在候选中不可用").arg(lost);
    }
    versions_->setText(heading);
    metadata_->setText(QString("已使用 %1  ·  候选 %2  ·  上次良好 %3")
        .arg(snapshot(shownActive,activeStamp),snapshot(shownCandidate,candidateStamp),versionLabel(lastGood)));
    metadata_->setToolTip(QString("已使用  %1\n候选  %2\n上次良好  %3").arg(shownActive,shownCandidate,lastGood));

    QString status = feedback_;
    if (applying_) status = "正在校验并应用目标版本…";
    else if (loading_) status = "正在比较版本…";
    else if (stale) {
        const auto message = QString("比较已失效。请选择「重新比较」查看当前版本。");
        status = status.isEmpty() ? message : status + "\n" + message;
    } else if (status.isEmpty() && !fresh) status = "当前没有待应用的候选版本。";
    hint_->setText(status);
    hint_->setVisible(!status.isEmpty());
}

void ProfileChangesDialog::confirmApply() {
    const auto* profile = currentProfile();
    if (!profile || !apply_->isEnabled()) return;
    const auto candidate = profile->candidate;
    const auto active = profile->active;
    confirmProfileVersion(this,"应用更新",*profile,candidate,false,[this,candidate,active] {
        const auto* current = currentProfile();
        if (!current || current->candidate!=candidate || current->active!=active || !apply_->isEnabled()) {
            feedback_ = "此次确认未提交。请核对当前版本后重试。";
            updateActions();
            return;
        }
        applying_ = true;
        updateActions();
        controller_->activateProfile(id_,false,candidate);
    });
}

void ProfileChangesDialog::showSection(int row) {
    const auto sections = report_.value("sections").toArray();
    if (row<0 || row>=sections.size()) return;
    const auto section = sections[row].toObject();
    QStringList lines;
    for (const auto& key : QStringList{"added","removed","changed"}) {
        const auto entries = section.value(key).toArray();
        if (entries.isEmpty()) continue;
        lines.append(key=="added"?"新增：":key=="removed"?"移除：":"修改：");
        for (const auto& name : entries) lines.append("  "+name.toString());
    }
    if (section.value("order_changed").toBool()) lines.append("规则内容或匹配顺序发生变化。");
    if (section.value("truncated").toBool()) lines.append("各类最多展示100个名称；总数见表格。");
    const auto lost = report_.value("lost_selections").toArray();
    if (!lost.isEmpty()) {
        lines.append("\n以下已保存选择在候选中不再可用：");
        for (const auto& item : lost) lines.append("  "+item.toString());
        lines.append("使用新版本时按内核默认选择恢复。");
    }
    if (!report_.value("unverified_selections").toArray().isEmpty())
        lines.append("部分选择属于动态 Provider，激活后才核对。");
    details_->setPlainText(lines.isEmpty()?"此类别没有名称变化。":lines.join('\n'));
}
}
