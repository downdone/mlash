#include "MainWindowParts.h"
#include "profile/NodeLinks.h"

namespace mlash {
using namespace WindowParts;
QWidget* MainWindow::profilesPage() {
    auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
    auto* header = new QHBoxLayout; auto* title = new QLabel("订阅"); title->setObjectName("pageTitle"); header->addWidget(title); header->addStretch();
    auto* add = button("添加订阅", header); add->setObjectName("addSubscription"); add->setProperty("primary", true); layout->addLayout(header); mutations_ += {add};
    connect(add, &QPushButton::clicked, this, &MainWindow::addUrl);
    auto* paste=button("从剪贴板导入",header); paste->setObjectName("pasteSubscription"); mutations_.append(paste); connect(paste,&QPushButton::clicked,this,[this] { importPayload(QApplication::clipboard()->text()); });
    auto* batch=button("批量更新…",header); batch->setObjectName("batchProfiles"); connect(batch,&QPushButton::clicked,this,[this] { showProfileBatch(false); });
    header->removeWidget(add); header->addWidget(add);
    auto* listPanel = panel(); auto* listLayout = new QVBoxLayout(listPanel); listLayout->setContentsMargins(12, 10, 12, 10); listLayout->setSpacing(8); layout->addWidget(listPanel, 1);
    auto* objectActions=new QWidget; objectActions->setObjectName("profileObjectActions"); auto* actions = new QHBoxLayout(objectActions); actions->setContentsMargins(0,0,0,0); profileSelection_ = new ElidedLabel("选择订阅后可更新或使用"); profileSelection_->setObjectName("secondaryText"); profileSelection_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); actions->addWidget(profileSelection_, 1);
    auto* update = button("更新订阅", actions); auto* replace = new QAction("更换文件…",this); replace->setObjectName("replaceProfileSource"); listLayout->addWidget(objectActions); mutations_ += {update};
    profileUpdate_ = update; profileReplace_ = replace;
    profileUpdate_->setObjectName("updateProfile");
    connect(update, &QPushButton::clicked, this, [this] { const auto rows=profileTable_->selectionModel()->selectedRows(); if (rows.size()>1) showProfileBatch(true); else controller_->updateProfile(selectedProfile()); }); connect(replace, &QAction::triggered, this, [this] { addFile(true); });
    profileTable_ = new QTableWidget(0, 4); profileTable_->setHorizontalHeaderLabels({"名称", "来源", "最近更新", "状态"});
    profileTable_->setSelectionBehavior(QAbstractItemView::SelectRows); profileTable_->setSelectionMode(QAbstractItemView::ExtendedSelection); profileTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    profileTable_->verticalHeader()->hide(); profileTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch); listLayout->addWidget(profileTable_, 1);
    profileTable_->setObjectName("profilesTable"); styleTable(profileTable_); profileTable_->setColumnWidth(1, 90); profileTable_->setColumnWidth(2, 110); profileTable_->setColumnWidth(3, 130);
    emptyProfiles_ = emptyState(profileTable_, "还没有订阅\n点击“添加订阅”，从 URL 或 YAML 文件导入");
    connect(profileTable_, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateActions);
    auto* bottom = new QHBoxLayout; auto* menu = new QMenu(this); menu->setObjectName("profileContextMenu"); profileTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    menu->addAction(replace);
    connect(profileTable_,&QWidget::customContextMenuRequested,this,[this,menu](const QPoint& point) { const auto index=profileTable_->indexAt(point); if (!index.isValid()) return; profileTable_->setCurrentCell(index.row(),0); profileTable_->selectRow(index.row()); updateActions(); menu->popup(profileTable_->viewport()->mapToGlobal(point)); });
    for (const auto& key:{QKeySequence(Qt::Key_Menu),QKeySequence(Qt::SHIFT | Qt::Key_F10)}) { auto* shortcut=new QShortcut(key,profileTable_); shortcut->setContext(Qt::WidgetWithChildrenShortcut); connect(shortcut,&QShortcut::activated,this,[this,menu] { if (profileTable_->currentRow()<0) return; updateActions(); menu->popup(profileTable_->viewport()->mapToGlobal(profileTable_->visualRect(profileTable_->model()->index(profileTable_->currentRow(),0)).center())); }); }
    auto* changes=button("查看变化",actions); changes->setObjectName("profileChanges"); connect(changes,&QPushButton::clicked,this,[this] { const auto id=selectedProfile(); if (!id.isEmpty()) (new ProfileChangesDialog(controller_,id,this))->show(); });
    auto* up=menu->addAction("上移"); auto* down=menu->addAction("下移"); connect(up,&QAction::triggered,this,[this] { controller_->moveProfile(selectedProfile(),-1); }); connect(down,&QAction::triggered,this,[this] { controller_->moveProfile(selectedProfile(),1); });
    auto* share=menu->addAction("分享订阅二维码…"); share->setObjectName("shareProfileQr");
    connect(menu,&QMenu::aboutToShow,this,[this,up,down,share] { const bool idle=!controller_->state().busy&&!controller_->state().readOnly; up->setEnabled(idle&&profileTable_->currentRow()>0); down->setEnabled(idle&&profileTable_->currentRow()+1<profileTable_->rowCount()); const auto data=controller_->profileMaintenance(selectedProfile()); share->setEnabled(data.value("source_type")=="url"); });
    connect(share,&QAction::triggered,this,[this] {
        const auto data=controller_->profileMaintenance(selectedProfile()); if (data.value("source_type")!="url") return;
        auto* dialog=new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profileShareDialog"); dialog->setWindowTitle("分享订阅"); auto* box=new QVBoxLayout(dialog);
        auto* warning=new QLabel("订阅地址可能含私人令牌。二维码等同于完整地址，请仅向可信设备展示。"); warning->setWordWrap(true); box->addWidget(warning);
        auto* reveal=new QPushButton("显示私人二维码"); reveal->setObjectName("revealProfileQr"); box->addWidget(reveal);
        auto* image=new QLabel; image->setObjectName("profileQrImage"); image->setAlignment(Qt::AlignCenter); box->addWidget(image);
        connect(reveal,&QPushButton::clicked,dialog,[dialog,reveal,image,source=data.value("source").toString()] {
            const auto bytes=source.toUtf8(); if (bytes.size()>2048) { image->setText("地址过长，无法生成适合扫描的二维码"); return; }
            try { const auto code=qrcodegen::QrCode::encodeText(bytes.constData(),qrcodegen::QrCode::Ecc::MEDIUM); const int size=code.getSize(),scale=qMax(2,320/(size+8)); QImage pixels((size+8)*scale,(size+8)*scale,QImage::Format_RGB32); pixels.fill(Qt::white); QPainter painter(&pixels); painter.setPen(Qt::NoPen); painter.setBrush(Qt::black); for (int y=0;y<size;++y) for (int x=0;x<size;++x) if (code.getModule(x,y)) painter.drawRect((x+4)*scale,(y+4)*scale,scale,scale); painter.end(); image->setPixmap(QPixmap::fromImage(pixels)); image->setMinimumSize(pixels.size()); reveal->hide(); dialog->adjustSize(); } catch (const std::exception&) { image->setText("地址无法编码为二维码"); }
        });
        auto* close=new QPushButton("关闭"); box->addWidget(close); connect(close,&QPushButton::clicked,dialog,&QDialog::close); dialog->resize(380,180); dialog->show();
    });
    auto* rename = menu->addAction("重命名…"); profilePolicy_ = new QAction("来源与更新…", this); profilePolicy_->setObjectName("profileUpdatePolicy"); actionButton(profilePolicy_, actions, "profilePolicyButton");
    auto* manage=new MenuPushButton("管理"); manage->setObjectName("profileManage"); manage->setMenu(menu); actions->addWidget(manage);
    connect(profilePolicy_, &QAction::triggered, this, &MainWindow::editProfilePolicy);
    auto* providers = menu->addAction("提供者…"); providers->setObjectName("profileProviders"); connect(providers, &QAction::triggered, this, [this] { showProviders(selectedProfile()); });
    auto* dns = menu->addAction("DNS 设置…"); dns->setObjectName("profileDns"); connect(dns,&QAction::triggered,this,[this] { const auto id=selectedProfile(); if (!id.isEmpty()&&!controller_->state().busy) (new DnsDialog(controller_,id,this))->show(); });
    auto* proxyEditor=menu->addAction("编辑节点与策略组…"); proxyEditor->setObjectName("profileProxyEditor"); connect(proxyEditor,&QAction::triggered,this,[this] { const auto id=selectedProfile(); if (!id.isEmpty()&&!controller_->state().busy) (new ProxyEditorDialog(controller_,id,this))->show(); });
    auto* proxyChain=menu->addAction("代理拨号链…"); proxyChain->setObjectName("profileProxyChain"); connect(proxyChain,&QAction::triggered,this,[this] { const auto id=selectedProfile(); if (!id.isEmpty()&&!controller_->state().busy) (new ProxyChainDialog(controller_,id,this))->show(); });
    auto* compatibility = menu->addAction("配置兼容性…"); compatibility->setObjectName("profileCompatibility");
    connect(compatibility, &QAction::triggered, this, [this] {
        const auto id = selectedProfile(); if (id.isEmpty()) return;
        auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle("配置兼容性"); dialog->resize(540, 380);
        auto* content = new QVBoxLayout(dialog); auto* report = new QPlainTextEdit; report->setObjectName("compatibilityReport"); report->setReadOnly(true); report->setPlainText("正在检查…"); content->addWidget(report);
        auto* close = new QPushButton("关闭"); content->addWidget(close, 0, Qt::AlignRight); connect(close, &QPushButton::clicked, dialog, &QDialog::close); dialog->show();
        controller_->inspectProfileCompatibility(id, [guard = QPointer<QPlainTextEdit>(report)](Result result) {
            if (!guard) return;
            if (!result.ok) { guard->setPlainText(result.error + "\n错误码：" + result.data.value("code").toString()); return; }
            QStringList managed; for (const auto& value : result.data.value("managed_fields").toArray()) managed.append(value.toString());
            guard->setPlainText("静态兼容检查通过\n\n" + (managed.isEmpty() ? QString("源配置没有显式设置受控字段。") : "以下源字段由 Mlash 覆盖或移除：\n" + managed.join("\n")) + "\n\n本机 Mixed 端口、私有控制接口、Secret、日志等级与TUN/LAN 由 Mlash 管理。远程 Provider 保存为版本快照。\n\n本报告不联网、不改配置；实际可用性仍需内核校验。");
        });
    });
    profileReload_ = menu->addAction("重新加载当前版本"); connect(profileReload_, &QAction::triggered, this, [this] { controller_->activateProfile(selectedProfile()); });
    auto* rollback = menu->addAction("回滚上次运行版本"); menu->addSeparator(); auto* remove = menu->addAction("删除订阅…"); menu->addSeparator(); auto* directory = menu->addAction("打开数据目录");
    auto* activate = button("使用选中订阅", actions); activate->setProperty("primary", true); auto slot=activate->sizePolicy(); slot.setRetainSizeWhenHidden(true); activate->setSizePolicy(slot);
    profileDetails_ = new ElidedLabel; profileDetails_->setObjectName("secondaryText"); profileDetails_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); listLayout->addWidget(profileDetails_);
    mutations_ += {activate}; listLayout->addLayout(bottom);
    profileActivate_ = activate; profileRollback_ = rollback; profileRename_ = rename; profileDelete_ = remove;
    rollback->setObjectName("rollbackProfile"); rename->setObjectName("renameProfile"); remove->setObjectName("deleteProfile");
    profileActivate_->setObjectName("activateProfile");
    connect(activate, &QPushButton::clicked, this, [this] { controller_->activateProfile(selectedProfile()); }); connect(rollback, &QAction::triggered, this, [this] { controller_->activateProfile(selectedProfile(), true); });
    connect(rename, &QAction::triggered, this, [this] { QString name; for (const auto& profile : controller_->profiles()) if (profile.id == selectedProfile()) name = profile.name; bool ok; const auto next = QInputDialog::getText(this, "重命名订阅", "订阅名称", QLineEdit::Normal, name, &ok); if (ok && next.trimmed() != name) controller_->renameProfile(selectedProfile(), next); });
    connect(remove, &QAction::triggered, this, [this] { const auto id = selectedProfile(); if (!id.isEmpty() && QMessageBox::question(this, "删除订阅", "删除选中的订阅快照？若是当前订阅，将先安全恢复代理并停止内核。") == QMessageBox::Yes) controller_->deleteProfile(id); });
    connect(directory, &QAction::triggered, this, [this] { openDirectory(controller_->dataRoot()); }); return page;
}

void MainWindow::updateProfiles() {
    const auto& s = controller_->settings();
    const auto toggle = [this](QCheckBox* box, bool before, bool actual) { if (!hasDisplayedSettings_ || box->isChecked() == before) box->setChecked(actual); };
    toggle(autoStart_,displayedSettings_.autoStart,s.autoStart); toggle(silentStart_,displayedSettings_.silentStart,s.silentStart); toggle(startCore_,displayedSettings_.startCore,s.startCore); toggle(restoreProxy_,displayedSettings_.restoreProxy,s.restoreProxy); toggle(lightweight_,displayedSettings_.lightweight,s.lightweight);
    if (!hasDisplayedSettings_ || mixedPort_->value() == displayedSettings_.mixedPort) mixedPort_->setValue(s.mixedPort);
    if (!hasDisplayedSettings_ || logLevel_->currentText() == displayedSettings_.logLevel) logLevel_->setCurrentText(s.logLevel);
    if (!hasDisplayedSettings_ || corePath_->text() == displayedSettings_.corePath) corePath_->setText(s.corePath);
    if (!hasDisplayedSettings_ || testUrl_->text() == displayedSettings_.testUrl) testUrl_->setText(s.testUrl);
    if (!hasDisplayedSettings_ || startupPage_->currentData().toInt()==displayedSettings_.startupPage) startupPage_->setCurrentIndex(startupPage_->findData(s.startupPage));
    for (const auto& tuple:QList<QPair<QComboBox*,QPair<QString,QString>>>{{trayClick_,{displayedSettings_.trayClick,s.trayClick}},{ipv6Mode_,{displayedSettings_.ipv6Mode,s.ipv6Mode}},{unifiedDelayMode_,{displayedSettings_.unifiedDelayMode,s.unifiedDelayMode}}}) if (!hasDisplayedSettings_ || tuple.first->currentData().toString()==tuple.second.first) tuple.first->setCurrentIndex(tuple.first->findData(tuple.second.second));
    if (!hasDisplayedSettings_||appearance_->currentData().toString()==displayedSettings_.appearance) appearance_->setCurrentIndex(appearance_->findData(s.appearance));
    if (!hasDisplayedSettings_||textScale_->currentData().toInt()==displayedSettings_.textScale) textScale_->setCurrentIndex(textScale_->findData(s.textScale));
    displayedSettings_ = s; hasDisplayedSettings_ = true;
    const auto selected = selectedProfile(); const auto selectedCombo = profilesCombo_->currentData().toString(); const QSignalBlocker comboBlock(profilesCombo_); profilesCombo_->clear(); profileTable_->setRowCount(controller_->profiles().size());
    int row = 0;
    for (const auto& p : controller_->profiles()) {
        const bool active = p.id == controller_->settings().activeProfile;
        profilesCombo_->addItem(p.name + (active ? " · 当前" : ""), p.id);
        auto* name = new QTableWidgetItem((active ? "● " : "") + p.name); name->setData(Qt::UserRole, p.id); profileTable_->setItem(row, 0, name);
        profileTable_->setItem(row, 1, new QTableWidgetItem(p.sourceType == "url" ? "URL" : p.source.isEmpty()?"本地快照":"文件快照"));
        profileTable_->setItem(row, 2, new QTableWidgetItem(QDateTime::fromString(p.updated, Qt::ISODate).toLocalTime().toString("MM-dd HH:mm")));
        auto* status = new QTableWidgetItem(!p.updateError.isEmpty() ? "更新失败 · 保留旧版" : active ? p.candidate != p.active ? "当前 · 有待激活版本" : "当前" : "可激活"); status->setToolTip(p.updateError); profileTable_->setItem(row, 3, status);
        if (p.id == selected) profileTable_->selectRow(row); ++row;
    }
    if (profileTable_->currentRow() < 0 && row > 0) profileTable_->selectRow(0);
    const auto wanted = selectedCombo.isEmpty() ? s.activeProfile : selectedCombo; const auto comboIndex = profilesCombo_->findData(wanted); if (comboIndex >= 0) profilesCombo_->setCurrentIndex(comboIndex);
    emptyProfiles_->setVisible(row == 0); updateProxies();
}

QString MainWindow::selectedProfile() const { auto* item = profileTable_->item(profileTable_->currentRow(), 0); return item ? item->data(Qt::UserRole).toString() : QString(); }

void MainWindow::addUrl() {
    addSubscription(true);
}

void MainWindow::importPayload(const QString& input) {
    if (controller_->state().busy || controller_->state().readOnly || findChild<ImportDialog*>()) return;
    const auto text=input.trimmed(); const auto scheme=text.section(':',0,0).toLower();
    if (text.toUtf8().size()<=64*1024&&NodeLinks::acceptsScheme(scheme)&&scheme!="http"&&scheme!="https") { auto* dialog=new ImportDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->bindController(controller_); dialog->setNodeLinks(text); dialog->open(); return; }
    if (text.isEmpty() || text.size()>4096 || text.contains('\n') || text.contains('\r')) { QMessageBox::information(this,"导入订阅","请提供一个订阅地址、本地 YAML 文件或每行一个节点链接"); return; }
    const QUrl url(text); const bool remote=QStringList{"http","https"}.contains(url.scheme())&&!url.host().isEmpty()&&url.userInfo().isEmpty()&&!url.hasFragment();
    const auto path=url.isLocalFile()?url.toLocalFile():text;
    if (!remote && (!QFileInfo(path).isAbsolute() || !QStringList{"yaml","yml"}.contains(QFileInfo(path).suffix().toLower()))) { QMessageBox::information(this,"导入订阅","仅支持一个 HTTP/HTTPS 订阅或本地 YAML 文件"); return; }
    auto* dialog=new ImportDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setPrefill(remote?text:path,remote);
    dialog->bindController(controller_); dialog->open();
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

void MainWindow::showProfileBatch(bool start) {
    if (auto* existing=findChild<QDialog*>("profileBatchDialog")) { existing->show(); existing->raise(); return; }
    QStringList ids; const auto rows=profileTable_->selectionModel()->selectedRows();
    if (rows.size()>1) for (const auto& row:rows) ids.append(profileTable_->item(row.row(),0)->data(Qt::UserRole).toString());
    else for (const auto& p:controller_->profiles()) ids.append(p.id);
    const auto before=ids.size(); ids.removeIf([this](const QString& id) { for (const auto& profile:controller_->profiles()) if (profile.id==id) return profile.source.isEmpty(); return true; }); const auto local=before-ids.size();
    auto* dialog=new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profileBatchDialog"); dialog->setWindowTitle("批量更新订阅"); dialog->resize(600,400); auto* box=new QVBoxLayout(dialog);
    auto* hint=new QLabel; hint->setWordWrap(true); hint->setTextFormat(Qt::PlainText); box->addWidget(hint);
    auto* table=new QTableWidget(0,3); table->setObjectName("profileBatchResults"); table->setHorizontalHeaderLabels({"订阅","结果","说明"}); table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch); table->verticalHeader()->hide(); table->setEditTriggers(QAbstractItemView::NoEditTriggers); box->addWidget(table,1);
    auto* note=new QLabel("只更新候选，不切换运行版本。取消会停止后续队列，当前已开始的保存安全完成。关闭窗口不终止任务。"+(local?QString(" 本地快照 %1 项无外部来源，已略过。").arg(local):QString())); note->setWordWrap(true); box->addWidget(note);
    auto* actions=new QHBoxLayout; auto* run=button(QString("更新 %1 项").arg(ids.size()),actions); run->setObjectName("startProfileBatch"); auto* cancel=button("停止后续更新",actions); cancel->setObjectName("cancelProfileBatch"); actions->addStretch(); auto* close=button("关闭",actions); box->addLayout(actions);
    const auto refresh=[this,table,hint,run,cancel,ids] {
        const auto status=controller_->profileBatch(); const bool active=status.value("active").toBool(); const auto rows=status.value("results").toArray(); table->setRowCount(rows.size());
        for (int row=0;row<rows.size();++row) { const auto value=rows[row].toObject(); const QStringList fields{value.value("name").toString(),value.value("ok").toBool()?QString("已检查"):QString("失败"),value.value("message").toString()}; for (int col=0;col<3;++col) table->setItem(row,col,new QTableWidgetItem(fields[col])); }
        hint->setText(active?QString("正在逐项更新 · %1 / %2 完成%3").arg(rows.size()).arg(status.value("total").toInt()).arg(status.value("cancel_requested").toBool()?" · 等待当前项结束":""):status.isEmpty()?QString("准备更新 %1 项").arg(ids.size()):QString("已结束 · %1 项完成 · %2 项跳过").arg(rows.size()).arg(status.value("skipped").toInt()));
        run->setEnabled(!controller_->state().busy&&!controller_->state().readOnly&&!ids.isEmpty()); cancel->setEnabled(active&&!status.value("cancel_requested").toBool());
    };
    connect(controller_,&AppController::profileBatchChanged,dialog,refresh); connect(controller_,&AppController::stateChanged,dialog,refresh); connect(run,&QPushButton::clicked,dialog,[this,ids] { controller_->updateProfilesBatch(ids); }); connect(cancel,&QPushButton::clicked,controller_,&AppController::cancelProfileBatch); connect(close,&QPushButton::clicked,dialog,&QDialog::close); dialog->show(); refresh(); if (start&&run->isEnabled()) run->click();
}

void MainWindow::addSubscription(bool url) {
    auto* dialog = new ImportDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setSourceTab(url);
    dialog->bindController(controller_);
    dialog->open();
}

void MainWindow::addFile(bool replace) {
    if (!replace) { addSubscription(false); return; }
    const auto id = replace ? selectedProfile() : QString(); if (replace && id.isEmpty()) return;
    const auto path = QFileDialog::getOpenFileName(this, "导入完整 YAML", {}, "YAML (*.yaml *.yml)"); if (!path.isEmpty()) controller_->importFile({}, path, id);
}

void MainWindow::showProviders(const QString& id, const QString& kind) {
    if (id.isEmpty()) return;
    auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("providersDialog"); dialog->setWindowTitle("提供者快照"); dialog->resize(680, 410);
    auto* content = new QVBoxLayout(dialog); auto* versions=new CompactComboBox; versions->setObjectName("providerVersion"); versions->addItem("候选版本",false); versions->addItem("已使用版本",true); content->addWidget(versions);
    auto* hint = new QLabel("正在读取…"); hint->setTextFormat(Qt::PlainText); hint->setWordWrap(true); content->addWidget(hint);
    auto* table = new QTableWidget(0, 5); table->setObjectName("providersTable"); table->setHorizontalHeaderLabels({"名称", "类别", "条目", "快照时间", "状态"}); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->verticalHeader()->hide(); styleTable(table); table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch); table->setColumnWidth(2, 58); table->setColumnWidth(3, 125); content->addWidget(table, 1);
    auto* row = new QHBoxLayout; auto* reload = button("刷新列表", row); auto* refresh = button("更新选中", row); refresh->setObjectName("refreshProvider"); auto* policy=button("下载与更新…",row); policy->setObjectName("providerPolicy"); auto* all = button("更新全部", row); all->setObjectName("refreshAllProviders"); row->addStretch(); auto* compare=button("查看变化 / 应用",row); compare->setObjectName("providerApplyCandidate"); auto* close = button("关闭", row); content->addLayout(row); refresh->setEnabled(false); all->setEnabled(false);
    auto waiting = std::make_shared<bool>(false); const QPointer<QTableWidget> guarded(table); const QPointer<QLabel> guardedHint(hint);
    auto load = std::make_shared<std::function<void()>>();
    *load = [this, id, kind, guarded, guardedHint, versions] {
        if (!guarded || !guarded->isVisible()) return;
        const bool running=versions->currentData().toBool(); versions->setEnabled(false);
        controller_->inspectProviders(id, [guarded, guardedHint, kind, versions](Result result) {
            if (!guarded || !guardedHint || !guarded->isVisible()) return;
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
            guardedHint->setText(errors.isEmpty() ? (guarded->rowCount() ? "更新仅生成候选；点击“查看变化 / 应用”确认切换。" : "此版本没有对应提供者") : errors.join('\n'));
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
    connect(policy,&QPushButton::clicked,dialog,[this,id,table,dialog,load] { const auto* item=table->item(table->currentRow(),0); if (!item||item->data(Qt::UserRole).toString().isEmpty()) return; auto* editor=new ProviderPolicyDialog(controller_,id,item->data(Qt::UserRole+1).toJsonObject(),dialog); connect(editor,&QDialog::accepted,dialog,[load] { (*load)(); }); editor->open(); });
    connect(refresh, &QPushButton::clicked, dialog, [this, id, table, waiting] { const auto* selected = table->item(table->currentRow(), 0); if (!selected) return; *waiting = true; controller_->refreshProviders(id, selected->data(Qt::UserRole).toString()); });
    connect(all, &QPushButton::clicked, dialog, [this, id, kind, waiting] { *waiting = true; controller_->refreshProviders(id, kind.isEmpty() ? "*" : "kind:" + kind); });
    connect(controller_, &AppController::operationFinished, dialog, [waiting, guardedHint, load, update](bool ok, const QString& message) { if (!*waiting) return; *waiting = false; if (guardedHint) guardedHint->setText(ok ? "提供者新候选已就绪；当前运行配置不变" : message); (*load)(); update(); });
    connect(versions,&QComboBox::currentIndexChanged,dialog,[load,update] { update(); (*load)(); }); connect(compare,&QPushButton::clicked,dialog,[this,id,dialog] { (new ProfileChangesDialog(controller_,id,dialog))->show(); }); connect(controller_,&AppController::profilesChanged,dialog,update);
    connect(close, &QPushButton::clicked, dialog, &QDialog::close); dialog->show(); (*load)(); update();
}

void MainWindow::editProfilePolicy() {
    const auto id = selectedProfile(); Profile profile; bool found = false;
    for (const auto& p : controller_->profiles()) if (p.id == id) { profile = p; found = true; break; }
    if (!found || controller_->state().busy) return;
    auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profilePolicyDialog"); dialog->setWindowTitle("订阅来源与维护"); dialog->resize(560, 540);
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
    const auto usage = SubscriptionInfo::summary(profile.subscriptionInfo); auto* info = new QLabel(usage.isEmpty() ? "来源未提供标准流量/到期信息" : usage + "\n来源快照：" + profile.subscriptionInfo.value("observed_at").toString()); info->setTextFormat(Qt::PlainText); info->setWordWrap(true); layout->addWidget(info);
    auto* maintenance=new QLabel("最近成功："+(profile.updated.isEmpty()?QString("尚无"):profile.updated)+"\n最近尝试："+(profile.lastUpdateAttempt.isEmpty()?QString("尚无"):profile.lastUpdateAttempt)+"\n"+(expected.value("next").toString().isEmpty()?QString("无定时任务"):"下次检查："+expected.value("next").toString())+(profile.updateError.isEmpty()?QString():"\n上次失败："+profile.updateError)); maintenance->setTextFormat(Qt::PlainText); maintenance->setWordWrap(true); layout->addWidget(maintenance);
    auto* note = new QLabel("只下载、校验并保存候选；当前运行配置不变。失败保留原版本，稍后重试。"); note->setWordWrap(true); note->setObjectName("secondaryText"); layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText("保存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); outer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto saving=std::make_shared<bool>(false);
    DialogDraftGuard::protect(dialog,[automatic,hours,download,profile,name,source] { return name->text()!=profile.name || source->text()!=profile.source || automatic->isChecked()!=profile.autoUpdate || (hours && hours->value()!=profile.updateIntervalHours) || download->policy().toJson()!=profile.download.toJson(); },[saving] { return *saving; });
    connect(controller_,&AppController::operationFinished,dialog,[dialog,buttons,note,saving](bool ok,const QString& error) { if (!*saving) return; *saving=false; buttons->setEnabled(true); if (ok) dialog->accept(); else note->setText("保存失败，修改已保留 · "+error); });
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, id, automatic, hours, download, buttons, saving, name, source, expected, interval = profile.updateIntervalHours] { if (!download->policy().valid()) { QMessageBox::warning(dialog, "下载设置", "User-Agent 须为最多 256 个单行 ASCII 字符"); return; } *saving=true; buttons->setEnabled(false); controller_->editProfileSource(id,name->text(),source->text(),automatic->isChecked(),hours?hours->value():interval,download->policy(),expected); }); dialog->open();
}

}
