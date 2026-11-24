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

void MainWindow::addUrl() {
    navigate(Route{ViewIds::Profiles});
    if (quickImport_->isVisible()) quickImport_->setFocus(Qt::ShortcutFocusReason);
}

void MainWindow::submitQuickImport() {
    const auto& state=controller_->state();
    if (quickImportPending_ || state.busy || state.readOnly || state.exiting || quickImport_->text().trimmed().isEmpty()) return;
    const auto source=classifyImport(quickImport_->text());
    if (source.value.isEmpty()) {
        setPageFeedback(ViewIds::Profiles,isNodeImport(quickImport_->text().trimmed())?
            "节点链接请从「高级新增」检查后导入":"请输入订阅地址或本地配置文件的完整路径");
        return;
    }
    if (!source.remote) {
        reviewWireGuardImport(source.value);
        return;
    }
    quickImportPending_=true;
    setPageFeedback(ViewIds::Profiles,{});
    controller_->importUrl({},QUrl(source.value));
}

void MainWindow::importPayload(const QString& input) {
    const auto& state=controller_->state();
    if (state.busy || state.readOnly || state.exiting || findChild<ImportDialog*>()) return;
    const auto text=input.trimmed();
    if (isNodeImport(text)) {
        auto* dialog=new ImportDialog(this);
        dialog->setNodeLinks(text);
        presentImport(dialog);
        return;
    }
    addUrl();
    if (!quickImport_->isVisible()) return;
    const auto source=classifyImport(text);
    if (source.value.isEmpty()) {
        setPageFeedback(ViewIds::Profiles,"仅支持一个 HTTP/HTTPS 订阅或本地 YAML／CONF 文件");
        return;
    }
    quickImport_->setText(source.remote?source.value:QDir::toNativeSeparators(source.value));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (controller_->state().busy || controller_->state().readOnly) return;
    const auto* mime=event->mimeData(); const auto urls=mime->urls();
    if ((mime->hasUrls()&&urls.size()==1) || (!mime->hasUrls()&&mime->hasText()&&mime->text().size()<=4096)) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    const auto* mime=event->mimeData(); const auto urls=mime->urls(); if (mime->hasUrls()&&urls.size()!=1) return;
    importPayload(mime->hasUrls()?urls.first().toString():mime->text()); event->acceptProposedAction();
}

MainWindow::ProfileBatchScope MainWindow::profileBatchScope() const {
    const auto selection = selectedProfileIds();
    const QSet<QString> selected(selection.cbegin(), selection.cend());
    ProfileBatchScope scope;
    scope.selected = selected.size() > 1;
    for (const auto& profile : controller_->profiles()) {
        if (scope.selected && !selected.contains(profile.id)) continue;
        if (profile.source.isEmpty()) ++scope.skipped;
        else scope.ids.append(profile.id);
    }
    return scope;
}

void MainWindow::updateProfileBatchAction() {
    auto* button = findChild<QPushButton*>("batchProfiles");
    if (!button) return;
    const auto scope = profileBatchScope();
    const auto& state = controller_->state();
    const bool active = controller_->profileBatch().value("active").toBool();
    button->setVisible(active || scope.selected || scope.ids.size() > 1);
    button->setEnabled(!state.exiting && (active || (!state.busy && !state.readOnly && !scope.ids.isEmpty())));
    button->setText(active ? QString("查看更新进度…") : scope.selected
        ? scope.ids.isEmpty() ? QString("更新所选…") : QString("更新所选 %1 项…").arg(scope.ids.size())
        : QString("更新全部…"));
    const auto hint = active ? QString("查看进行中的批量更新，可停止后续项。")
        : scope.ids.isEmpty() ? QString("所选配置没有可更新的来源。")
        : QString("可更新 %1 项。").arg(scope.ids.size())
            + (scope.skipped ? QString("跳过 %1 项无来源快照。").arg(scope.skipped) : QString());
    button->setToolTip(hint);
    button->setAccessibleDescription(hint);
}

void MainWindow::showProfileBatch() {
    if (auto* existing=findChild<QDialog*>("profileBatchDialog")) { existing->show(); existing->raise(); return; }
    const auto scope = profileBatchScope();
    const auto openedStatus = controller_->profileBatch();
    const bool monitoring = openedStatus.value("active").toBool();
    if (scope.ids.isEmpty() && !monitoring) return;
    const auto ids = scope.ids;
    const auto scopeIds = QJsonArray::fromStringList(ids);
    const QSet<QString> scopedIds(ids.cbegin(), ids.cend());
    QJsonArray plan;
    for (const auto& profile : controller_->profiles()) {
        if (scopedIds.contains(profile.id)) plan.append(QJsonObject{{"name",profile.name}});
    }
    auto* dialog=new TaskDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profileBatchDialog"); dialog->setWindowTitle("批量更新订阅"); dialog->resize(600,400); auto* box=new QVBoxLayout(dialog);
    auto* hint=new QLabel; hint->setObjectName("profileBatchStatus"); hint->setWordWrap(true); hint->setTextFormat(Qt::PlainText); box->addWidget(hint);
    auto* table=new QTableWidget(0,3); table->setObjectName("profileBatchResults"); table->setHorizontalHeaderLabels({"配置","状态","说明"}); table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch); table->verticalHeader()->hide(); table->setEditTriggers(QAbstractItemView::NoEditTriggers); box->addWidget(table);
    table->setShowGrid(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    auto* note=new QLabel("更新结果保存为候选。停止后续更新会等待当前项完成；关闭工作区后队列继续。");
    note->setObjectName("profileBatchNote"); note->setWordWrap(true); note->setProperty("secondary",true); box->addWidget(note);
    auto* actions=new QHBoxLayout; auto* run=button(QString("更新 %1 项").arg(ids.size()),actions); run->setObjectName("startProfileBatch"); run->setProperty("primary",true); run->setVisible(!monitoring); auto* cancel=button("停止后续更新",actions); cancel->setObjectName("cancelProfileBatch"); actions->addStretch(); auto* close=button("关闭",actions); box->addLayout(actions);
    box->addStretch(1);
    const auto refresh=[this,table,hint,run,cancel,scope,scopeIds,plan,monitoring] {
        const auto status=controller_->profileBatch();
        const auto& state=controller_->state();
        const auto previousIds=status.value("ids").toArray();
        const bool sameScope=std::is_permutation(previousIds.cbegin(),previousIds.cend(),scopeIds.cbegin(),scopeIds.cend());
        const bool active=status.value("active").toBool();
        const bool showResults=monitoring || sameScope;
        const auto rows=showResults?status.value("results").toArray():plan;
        table->setRowCount(rows.size());
        const int rowHeight=qMax(UiTheme::TableRow,table->fontMetrics().height()+8);
        const int planned=showResults?status.value("total").toInt():plan.size();
        table->verticalHeader()->setDefaultSectionSize(rowHeight);
        table->setFixedHeight(table->horizontalHeader()->sizeHint().height()+rowHeight*qBound(1,planned,6)+2*table->frameWidth());
        for (int row=0;row<rows.size();++row) {
            const auto entry=rows[row].toObject();
            const auto stateText=showResults ? (entry.value("ok").toBool()?QString("已检查"):QString("失败")) : QString("待更新");
            const QStringList cells{entry.value("name").toString(),stateText,entry.value("message").toString()};
            for (int column=0;column<cells.size();++column) {
                auto* cell=new QTableWidgetItem(cells[column]);
                if (column==2 && !cells[column].isEmpty())
                    cell->setToolTip("<qt>"+cells[column].toHtmlEscaped()+"</qt>");
                table->setItem(row,column,cell);
            }
        }
        const auto selectionText=scope.selected?QString("所选配置"):QString("全部配置");
        const auto target=QString("%1 · %2 项可更新").arg(selectionText).arg(scope.ids.size())
            +(scope.skipped?QString(" · 跳过 %1 项无来源快照").arg(scope.skipped):QString());
        QString progress;
        if (active && showResults)
            progress=QString("正在更新 · %1 / %2 项完成").arg(rows.size()).arg(status.value("total").toInt())
                +(status.value("cancel_requested").toBool()?" · 等待当前项结束":"");
        else if (showResults)
            progress=QString("已结束 · %1 项处理 · %2 项未开始").arg(rows.size()).arg(status.value("skipped").toInt());
        hint->setText(monitoring?progress:target+(progress.isEmpty()?QString():"\n"+progress));
        run->setEnabled(!monitoring&&!active&&!state.busy&&!state.readOnly&&!state.exiting);
        cancel->setEnabled(active&&showResults&&!status.value("cancel_requested").toBool());
    };
    connect(controller_,&AppController::profileBatchChanged,dialog,refresh);
    connect(controller_,&AppController::stateChanged,dialog,refresh);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,dialog,refresh);
    connect(run,&QPushButton::clicked,dialog,[this,ids] { controller_->updateProfilesBatch(ids); });
    connect(cancel,&QPushButton::clicked,controller_,&AppController::cancelProfileBatch);
    connect(close,&QPushButton::clicked,dialog,&QDialog::close);
    presentTask(dialog);
    refresh();
}

void MainWindow::presentImport(ImportDialog* dialog) {
    QSet<QString> before; for (const auto& profile:controller_->profiles()) before.insert(profile.id);
    for (const auto& profile:controller_->wireGuardProfiles()) before.insert(profile.id);
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->bindController(controller_);
    // 导入成功后在统一卡片网格中定位新对象，不以来源类型改变落点。
    connect(dialog,&QDialog::accepted,this,[this,before] {
        navigate(Route{ViewIds::Profiles});
        if (route_.view!=ViewIds::Profiles) return;
        for (const auto& profile:controller_->profiles()) if (!before.contains(profile.id)) { selectProfile(profile.id); return; }
        for (const auto& profile:controller_->wireGuardProfiles()) if (!before.contains(profile.id)) { selectProfile(profile.id); return; }
    });
    navigate(Route{ViewIds::Profiles});
    if (route_.view!=ViewIds::Profiles) { dialog->deleteLater(); return; }
    profileImport_=dialog;
    dialog->setProperty("guardReject",true);
    connect(dialog,&QDialog::finished,this,[this] {
        profileImport_=nullptr;
        if (quickImport_->isVisible()) quickImport_->setFocus(Qt::OtherFocusReason);
    });
    dialog->open();
}
void MainWindow::openAdvancedImport() {
    if (profileImport_) { profileImport_->raise(); profileImport_->activateWindow(); return; }
    auto* dialog=new ImportDialog(this);
    dialog->setWindowTitle("高级新增配置 · Mlash");
    auto* wireGuard=new QPushButton("粘贴 WireGuard 配置…",dialog);
    wireGuard->setObjectName("pasteWireGuardConfig");
    dialog->layout()->addWidget(wireGuard);
    connect(wireGuard,&QPushButton::clicked,this,[this,dialog] { dialog->close(); pasteWireGuard(); });
    const auto text=quickImport_->text().trimmed();
    const auto source=classifyImport(text);
    if (!source.value.isEmpty()) dialog->setPrefill(source.value,source.remote);
    else if (isNodeImport(text)) dialog->setNodeLinks(text);
    presentImport(dialog);
}

} // namespace mlash