#include "MainWindow.h"
#include "TaskDialog.h"
#include "ActivityTableModel.h"
#include "DialogDraftGuard.h"
#include "TunDialog.h"
#include "ViewIds.h"
#include "WindowUiFactories.h"
#include "app/AppController.h"
#include <QAbstractItemView>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace mlash {
namespace {
QStringList rows(const QJsonObject& value,const char* field) {
    QStringList text;
    for (const auto& item:value.value(field).toArray()) text.append(item.toString());
    return text;
}

QLabel* textLabel(const QString& text = {},bool secondary = false) {
    auto* label=new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setProperty("secondary",secondary);
    label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    return label;
}

QFormLayout* groupedFields(QVBoxLayout* parent) {
    auto* surface=WindowParts::panel();
    surface->setAttribute(Qt::WA_StyledBackground);
    auto* form=new QFormLayout(surface);
    form->setContentsMargins(14,10,14,10);
    form->setVerticalSpacing(10);
    form->setHorizontalSpacing(16);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    parent->addWidget(surface);
    return form;
}

QLabel* valueRow(QFormLayout* form,const QString& name,const QString& object = {}) {
    auto* value=textLabel();
    value->setObjectName(object);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
    form->addRow(name,value);
    return value;
}

QString scope(const QJsonObject& summary,const char* key) {
    const auto values=rows(summary,key);
    return values.isEmpty()?QString("—"):values.join("、");
}

QString handshakeTime(const QJsonObject& peer) {
    const auto stamp=QDateTime::fromString(peer.value("last_handshake").toString(),Qt::ISODate).toLocalTime();
    return stamp.isValid()?stamp.toString("MM-dd HH:mm:ss"):QString("等待握手");
}

const WireGuardProfile* tunnelProfile(const AppController& controller,const QString& id) {
    const auto& catalog=controller.wireGuardProfiles();
    const auto entry=std::find_if(catalog.cbegin(),catalog.cend(),[&](const auto& item) { return item.id==id; });
    return entry==catalog.cend()?nullptr:&*entry;
}
}

void MainWindow::pasteWireGuard(const QString& id) {
    const auto* profile=tunnelProfile(*controller_,id);
    if (!id.isEmpty()&&!profile) return;
    const auto expected=profile?profile->candidate:QString{};
    auto* editor=new TaskDialog(this);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->setObjectName("wireGuardPaste");
    editor->setWindowTitle("粘贴 WireGuard 配置");
    auto* layout=new QVBoxLayout(editor);
    layout->addWidget(textLabel("粘贴 [Interface] 与 [Peer] 配置。保存前可先查看地址和分流范围。",true));
    auto* text=new QPlainTextEdit(editor);
    text->setPlaceholderText("[Interface]\n…");
    layout->addWidget(text);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,editor);
    buttons->button(QDialogButtonBox::Ok)->setText("预览");
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,editor,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,editor,[this,editor,text,id,expected] {
        auto pasted=text->toPlainText().toUtf8();
        if (pasted.isEmpty()) return;
        reviewWireGuardImport({},std::move(pasted),editor,id,expected);
    });
    editor->resize(480,280);
    presentTask(editor);
}

void MainWindow::reviewWireGuardImport(const QString& path,QByteArray pasted,QDialog* sourceEditor,
                                       const QString& id,const QString& expected) {
    if (auto* existing=findChild<QDialog*>("wireGuardImportPreview")) {
        if (existing->property("embeddedTask").toBool()) presentTask(existing);
        pasted.fill('\0');
        return;
    }
    auto* dialog=new TaskDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("wireGuardImportPreview");
    dialog->setWindowTitle("WireGuard 导入预览");
    if (sourceEditor) connect(sourceEditor,&QDialog::rejected,dialog,[dialog] { dialog->reject(); });
    auto* layout=new QVBoxLayout(dialog);
    auto* identity=groupedFields(layout);
    const auto* target=tunnelProfile(*controller_,id);
    if (!id.isEmpty()&&!target) { dialog->deleteLater(); pasted.fill('\0'); return; }
    auto* name=new QLineEdit(target?target->name:path.isEmpty()?QString("WireGuard 隧道"):QFileInfo(path).completeBaseName(),dialog);
    name->setAccessibleName("配置名称");
    name->setMaxLength(120);
    name->setReadOnly(target!=nullptr);
    identity->addRow("配置名称",name);
    auto* preview=textLabel("正在识别配置…",true);
    preview->setObjectName("wireGuardImportStatus");
    layout->addWidget(preview);
    auto* fields=groupedFields(layout);
    auto* addresses=valueRow(fields,"接口地址");
    auto* routes=valueRow(fields,"分流网段");
    auto* peers=valueRow(fields,"Peer 与 DNS");
    fields->parentWidget()->hide();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,dialog);
    auto* save=buttons->button(QDialogButtonBox::Save);
    save->setText(id.isEmpty()?"保存配置":"保存候选");
    save->setProperty("primary",true);
    save->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(dialog,&QDialog::rejected,controller_,&AppController::cancelWireGuardPreview);
    DialogDraftGuard::protect(dialog,[] { return false; },[dialog] { return dialog->property("saving").toBool(); });
    dialog->resize(500,315);
    auto resultHandler=[this,path,id,expected,guard=QPointer<QDialog>(dialog),source=QPointer<QDialog>(sourceEditor),
        name,preview,save,fields,addresses,routes,peers](Result result) {
        if (!guard) return;
        if (id.isEmpty()&&result.data.value("file_read_failed").toBool()) {
            setPageFeedback(ViewIds::Profiles,result.error);
            guard->accept();
            return;
        }
        if (result.ok && result.data.value("kind")=="mihomo") {
            if (!id.isEmpty()) {
                presentTask(guard);
                preview->setText("该文件是 Mihomo 配置。请选择 WireGuard CONF。");
                return;
            }
            guard->accept();
            quickImportPending_=true;
            controller_->importFile({},path);
            return;
        }
        presentTask(guard);
        if (!result.ok) { preview->setText(result.error); return; }
        preview->setText(result.data.value("full_v4").toBool()||result.data.value("full_v6").toBool()
            ? "已识别 WireGuard · 包含默认路由，可保存供查看。" : "已识别 WireGuard · 保存后可单独连接");
        addresses->setText(scope(result.data,"addresses"));
        routes->setText(scope(result.data,"routes"));
        peers->setText(QString("%1 个 Peer · %2 个 DNS").arg(result.data.value("peer_count").toInt()).arg(result.data.value("dns_count").toInt()));
        fields->parentWidget()->show();
        const auto token=result.data.value("preview_token").toString();
        save->setEnabled(!token.isEmpty());
        connect(controller_,&AppController::operationFinished,guard,[guard,source,preview](bool ok,const QString& message,OperationId operation) {
            if (!guard||operation!=OperationId::ProfileImport||!guard->property("saving").toBool()) return;
            guard->setProperty("saving",false);
            if (!ok) { preview->setText(message+"。请返回并重新预览。"); return; }
            guard->accept();
            if (source) source->accept();
        });
        connect(save,&QPushButton::clicked,guard,[this,guard,name,preview,save,token,id,expected] {
            if (!guard || controller_->state().busy || name->text().trimmed().isEmpty()) return;
            guard->setProperty("saving",true);
            preview->setText("正在保存…");
            save->setEnabled(false);
            quickImportPending_=id.isEmpty();
            controller_->saveWireGuardPreview(name->text(),token,id,expected);
        });
    };
    if (path.isEmpty()) controller_->previewWireGuardText(pasted,std::move(resultHandler));
    else controller_->previewWireGuardFile(path,std::move(resultHandler));
    pasted.fill('\0');
}

void MainWindow::showWireGuardProfile(const QString& id,int tab) {
    const auto* profile=tunnelProfile(*controller_,id);
    if (!profile) return;
    const auto name=profile->name;
    const auto origin=route_;
    if (route_.view!=ViewIds::Profiles) navigate(Route{ViewIds::Profiles});
    if (route_.view!=ViewIds::Profiles) return;
    const bool returningToList=contentStack_->currentIndex()==0;
    auto* workspace=taskWorkspaceFor("wireguard");
    if (!workspace->configure("profile:"+id+":wireguard",name,{},[this,origin] {
        returnFromWorkspace();
        if (origin.view!=ViewIds::Profiles) navigate(origin);
    })) return;
    if (returningToList) profileListReturnScroll_=profiles_->verticalScrollBar()->value();
    profileDetailId_=id;
    selectProfile(id);
    if (auto* existing=workspace->editor("wireGuardProfileDetail")) {
        if (auto* tabs=existing->findChild<QTabWidget*>("wireGuardDetailTabs")) tabs->setCurrentIndex(tab);
        workspace->select("wireGuardProfileDetail");
        return;
    }
    auto* dialog=new TaskDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("wireGuardProfileDetail");
    dialog->setProperty("profileId",id);
    dialog->setWindowTitle(name);
    auto* layout=new QVBoxLayout(dialog);
    layout->setContentsMargins(4,6,4,8);
    layout->setSpacing(10);
    auto* header=WindowParts::panel();
    auto* headerLayout=new QVBoxLayout(header);
    headerLayout->setContentsMargins(14,10,14,10);
    headerLayout->setSpacing(8);
    layout->addWidget(header);
    auto* status=textLabel("未连接");
    status->setObjectName("wireGuardProfileStatus");
    auto* toggle=new QPushButton("连接隧道");
    toggle->setObjectName("wireGuardProfileConnect");
    toggle->setProperty("primary",true);
    auto* stateRow=new QHBoxLayout;
    stateRow->addWidget(status,1);
    auto* service=new QPushButton("权限服务…");
    service->setObjectName("wireGuardOpenService");
    service->setProperty("quiet",true);
    connect(service,&QPushButton::clicked,this,&MainWindow::openWireGuardService);
    stateRow->addWidget(service);
    stateRow->addWidget(toggle);
    headerLayout->addLayout(stateRow);
    auto* notice=textLabel({},true);
    notice->setObjectName("wireGuardNotice");
    headerLayout->addWidget(notice);
    auto* tabs=new QTabWidget(dialog);
    tabs->setObjectName("wireGuardDetailTabs");
    tabs->setDocumentMode(true);
    tabs->tabBar()->setDrawBase(false);
    layout->addWidget(tabs,1);

    auto* network=new QWidget;
    auto* networkLayout=new QVBoxLayout(network);
    networkLayout->setContentsMargins(0,8,0,0);
    auto* networkForm=groupedFields(networkLayout);
    auto* addresses=valueRow(networkForm,"接口地址","wireGuardAddresses");
    auto* routes=valueRow(networkForm,"分流网段","wireGuardRoutes");
    auto* dns=valueRow(networkForm,"接口 DNS","wireGuardDns");
    auto* observedRoutes=valueRow(networkForm,"系统路由回读","wireGuardObservedRoutes");
    auto* targetForm=groupedFields(networkLayout);
    auto* target=new QLineEdit;
    target->setObjectName("wireGuardTarget");
    target->setAccessibleName("目标 IP");
    target->setPlaceholderText("IPv4 或 IPv6 地址");
    target->setMaxLength(64);
    targetForm->addRow("检查网段归属",target);
    auto* targetResult=textLabel("输入目标 IP，检查它是否落在分流网段内。",true);
    targetResult->setObjectName("wireGuardTargetResult");
    targetForm->addRow(targetResult);
    networkLayout->addStretch();
    tabs->addTab(network,"网络");

    auto* peersPage=new QWidget;
    auto* peerLayout=new QVBoxLayout(peersPage);
    peerLayout->setContentsMargins(0,8,0,0);
    auto* peerNote=textLabel({},true);
    peerNote->setObjectName("wireGuardPeerNote");
    peerLayout->addWidget(peerNote);
    auto* peerTable=new QTableWidget(0,4);
    peerTable->setObjectName("wireGuardPeerTable");
    peerTable->setHorizontalHeaderLabels({"Peer","最近握手","接收","发送"});
    peerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    peerTable->verticalHeader()->hide();
    peerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    peerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    peerTable->setShowGrid(false);
    peerTable->setFrameShape(QFrame::NoFrame);
    peerTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    peerLayout->addWidget(peerTable);
    peerLayout->addStretch();
    tabs->addTab(peersPage,"Peer");

    auto* versions=new QWidget;
    auto* versionsLayout=new QVBoxLayout(versions);
    versionsLayout->setContentsMargins(0,8,0,0);
    auto* versionForm=groupedFields(versionsLayout);
    auto* versionStatus=valueRow(versionForm,"版本状态","wireGuardVersionStatus");
    auto* differences=valueRow(versionForm,"变化","wireGuardDifferences");
    auto* file=new QPushButton("选择 CONF…");
    file->setObjectName("wireGuardReplaceFile");
    auto* paste=new QPushButton("粘贴配置…");
    paste->setObjectName("wireGuardReplacePaste");
    auto* previous=new QPushButton("连接上次版本");
    previous->setObjectName("wireGuardPreviousVersion");
    auto* versionActions=new QHBoxLayout;
    versionActions->addWidget(file);
    versionActions->addWidget(paste);
    versionActions->addStretch();
    versionActions->addWidget(previous);
    versionsLayout->addLayout(versionActions);
    versionsLayout->addStretch();
    tabs->addTab(versions,"版本");

    const auto checkTarget=[this,id,target,targetResult] {
        const auto* entry=tunnelProfile(*controller_,id);
        if (!entry || target->text().trimmed().isEmpty()) {
            targetResult->setText("输入目标 IP，检查它是否落在分流网段内。");
            return;
        }
        const QHostAddress ip(target->text().trimmed());
        if (ip.isNull()||!ip.scopeId().isEmpty()) { targetResult->setText("请输入有效的 IPv4 或 IPv6 地址。"); return; }
        const auto seen=controller_->wireGuardObservation();
        const bool current=seen.value("profile_id").toString()==id && seen.value("active").toBool();
        const auto list=rows(current?seen:entry->summary,"routes");
        const auto found=std::find_if(list.cbegin(),list.cend(),[&](const auto& prefix) {
            const auto subnet=QHostAddress::parseSubnet(prefix);
            return !subnet.first.isNull()&&ip.isInSubnet(subnet);
        });
        const QString source=current?"当前回读网段":"配置网段";
        targetResult->setText(found==list.cend()?source+"未覆盖此地址":source+"包含 "+*found);
    };
    connect(target,&QLineEdit::textChanged,dialog,checkTarget);
    const auto redraw=[this,id,dialog,status,notice,tabs,toggle,previous,file,paste,addresses,routes,dns,observedRoutes,
                       peerNote,peerTable,versionStatus,differences,checkTarget] {
        const auto* current=tunnelProfile(*controller_,id);
        if (!current) { dialog->close(); return; }
        const auto observed=controller_->wireGuardObservation();
        const bool own=observed.value("active").toBool()&&observed.value("profile_id").toString()==id;
        const bool pending=observed.value("cleanup_pending").toBool();
        const auto phase=observed.value("phase").toString();
        const bool pendingVersion=!current->active.isEmpty()&&current->candidate!=current->active;
        const bool full=current->summary.value("full_v4").toBool()||current->summary.value("full_v6").toBool();
        const bool elsewhere=observed.value("active").toBool()&&!own;
        const auto block=observed.value("connect_block").toString();
        const auto peers=observed.value("observation").toObject().value("peers").toArray();
        const bool handshaken=std::any_of(peers.begin(),peers.end(),[](const auto& peer) {
            return !peer.toObject().value("last_handshake").toString().isEmpty();
        });
        const auto& state=controller_->state();
        status->setText(phase=="connecting"?"正在连接隧道…":phase=="disconnecting"?"正在清理路由…":
            own?(handshaken?"路由已生效 · Peer 已握手":"路由已生效 · 等待握手"):pending?"隧道待清理":"未连接");
        const auto error=observed.value("error").toString();
        const QString reason=!error.isEmpty()?error:elsewhere?"请先断开正在使用的 WireGuard 配置":
            !own&&!block.isEmpty()?block:full?"此候选包含默认路由。当前仅支持 WireGuard 分流。":
            own&&pendingVersion?"当前运行旧版本，接口地址与分流网段显示候选值。":QString();
        notice->setText(reason);
        notice->setVisible(!reason.isEmpty());
        toggle->setText(own?"断开隧道":pending?"重试清理":pendingVersion?"应用候选并连接":"连接隧道");
        const bool writable=!state.busy&&!state.exiting&&!state.readOnly;
        const bool canConnect=writable&&!elsewhere&&block.isEmpty()&&phase.isEmpty();
        toggle->setEnabled(!state.busy&&!state.exiting&&phase.isEmpty()&&(own||pending||(canConnect&&!full)));
        file->setEnabled(writable);
        paste->setEnabled(writable);
        toggle->setProperty("version",current->candidate);
        const auto baseline=pendingVersion?current->active:current->lastGood;
        previous->setProperty("version",baseline);
        previous->setVisible(!baseline.isEmpty());
        previous->setEnabled(canConnect&&!own&&!pending);

        addresses->setText(scope(current->summary,"addresses"));
        routes->setText(scope(current->summary,"routes"));
        const int dnsCount=current->summary.value("dns_count").toInt();
        dns->setText(dnsCount?QString("配置了 %1 个 DNS%2").arg(dnsCount).arg(
            own&&observed.value("dns_configured").toBool()?" · 接口已回读":""):QString("未配置"));
        observedRoutes->setText(own?scope(observed,"routes")+QString(" · MTU %1").arg(observed.value("mtu").toInt()):QString("未观察到活动路由"));
        checkTarget();

        const auto sample=QDateTime::fromString(observed.value("sampled_at").toString(),Qt::ISODate).toLocalTime();
        peerNote->setText(own&&sample.isValid()?QString("最近采样 %1 · 会话累计量").arg(sample.toString("HH:mm:ss")):
            QString("连接后显示握手时间和会话累计量"));
        const int count=own?int(peers.size()):current->summary.value("peer_count").toInt();
        peerTable->setRowCount(count);
        peerTable->setVisible(count>0);
        peerTable->setMaximumHeight(peerTable->horizontalHeader()->sizeHint().height()+
            qMin(count,6)*peerTable->verticalHeader()->defaultSectionSize()+2);
        for (int index=0;index<count;++index) {
            const auto peer=own?peers[index].toObject():QJsonObject{};
            const QStringList cells{QString("Peer %1").arg(peer.value("peer_id").toInt(index+1)),
                own?handshakeTime(peer):QString("未连接"),
                own?ActivityTableModel::formatBytes(peer.value("rx_bytes").toString().toDouble()):QString("—"),
                own?ActivityTableModel::formatBytes(peer.value("tx_bytes").toString().toDouble()):QString("—")};
            for (int col=0;col<cells.size();++col) {
                auto* cell=peerTable->item(index,col);
                if (!cell) { cell=new QTableWidgetItem; peerTable->setItem(index,col,cell); }
                if (cell->text()!=cells[col]) { cell->setText(cells[col]); cell->setToolTip(cells[col]); }
            }
        }
        versionStatus->setText(pendingVersion?"候选待启用":current->active.isEmpty()?"已保存 · 从未连接":"当前为上次生效版本");
        if (tabs->currentIndex()!=2) return;
        const QString comparisonKey=current->candidate+":"+baseline;
        if (differences->property("comparison").toString()==comparisonKey) return;
        differences->setProperty("comparison",comparisonKey);
        differences->setText(baseline.isEmpty()?"首次保存暂无对照版本":"正在读取版本…");
        if (baseline.isEmpty()) return;
        controller_->compareWireGuardVersions(id,[guard=QPointer<QLabel>(differences),comparisonKey](Result result) {
            if (!guard||guard->property("comparison").toString()!=comparisonKey) return;
            if (!result.ok) { guard->setText(result.error); return; }
            QStringList changes;
            const auto before=result.data.value("before").toObject(),after=result.data.value("after").toObject();
            for (const auto& field:QList<QPair<const char*,QString>>{{"addresses","接口地址"},{"routes","分流网段"}}) {
                if (before.value(field.first)!=after.value(field.first))
                    changes<<QString("%1：%2 → %3").arg(field.second,scope(before,field.first),scope(after,field.first));
            }
            if (result.data.value("dns_changed").toBool()) changes<<"接口 DNS 已变更";
            if (result.data.value("keys_changed").toBool()) changes<<"密钥已变更";
            if (result.data.value("endpoints_changed").toBool()) changes<<"Peer 或 Endpoint 已变更";
            if (result.data.value("interface_options_changed").toBool()) changes<<"MTU 或监听端口已变更";
            guard->setText(changes.isEmpty()?"脱敏字段无变化":changes.join('\n'));
        });
    };
    connect(controller_,&AppController::wireGuardChanged,dialog,redraw);
    connect(controller_,&AppController::profilesChanged,dialog,redraw);
    connect(controller_,&AppController::stateChanged,dialog,redraw);
    connect(tabs,&QTabWidget::currentChanged,dialog,redraw);
    connect(file,&QPushButton::clicked,dialog,[this,id] {
        const auto* current=tunnelProfile(*controller_,id);
        if (!current) return;
        const auto expected=current->candidate;
        const auto path=QFileDialog::getOpenFileName(this,"替换 WireGuard 配置",{},"配置文件 (*.conf *.yaml *.yml);;所有文件 (*)");
        if (!path.isEmpty()) reviewWireGuardImport(path,{},nullptr,id,expected);
    });
    connect(paste,&QPushButton::clicked,dialog,[this,id] { pasteWireGuard(id); });
    connect(toggle,&QPushButton::clicked,dialog,[this,id,toggle] {
        if (controller_->runningWireGuardProfile()==id || controller_->wireGuardObservation().value("cleanup_pending").toBool())
            controller_->disconnectWireGuard();
        else controller_->connectWireGuard(id,toggle->property("version").toString());
    });
    connect(previous,&QPushButton::clicked,dialog,[this,id,previous] {
        controller_->connectWireGuard(id,previous->property("version").toString());
    });
    tabs->setCurrentIndex(tab);
    redraw();
    workspace->mount(dialog);
}

void MainWindow::openWireGuardService() {
    navigate(Route{ViewIds::Settings});
    if (route_.view==ViewIds::Settings)
        showTool("tun",{},[this] { return new TunDialog(controller_,this); });
}
}
