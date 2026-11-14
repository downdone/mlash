#include "MainWindowParts.h"
#include "MenuSection.h"
#include "ProfileRowModel.h"
#include <QFileInfo>
#include "profile/NodeLinks.h"
#include "WrapLayout.h"
#include <QSet>

namespace mlash {
using namespace WindowParts;
// K7 · 配置页（原型 viewProfiles）：标题行三个按钮 + 「远程订阅 / 本地配置」两张 76px 列表。
// 行本身没有动作条：点击行（或行尾 ›）打开右侧抽屉，抽屉里是「设为当前配置 / 更多操作」。
QWidget* MainWindow::profilesPage() {
    auto* page = new QWidget; page->setObjectName("profilesSurface"); auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(14);
    // 标题行：添加订阅（primary）/ 导入文件 / 更新全部。
    using namespace PrototypeUi;
    auto* imports=group(layout);
    auto* add=row(imports,"addSubscription","导入…",UiIcons::Glyph::Folder);
    auto* updateAll=row(imports,"batchProfiles","更新订阅",UiIcons::Glyph::Refresh,{},"只准备候选，运行版本保持");
    mutations_ += {add,updateAll};
    connect(add,&QPushButton::clicked,this,&MainWindow::addUrl); connect(updateAll,&QPushButton::clicked,this,[this] { showProfileBatch(false); });
    // Legacy commands remain accessible through the import dialog and context menu.
    auto* import=new QPushButton(page); import->setObjectName("importSubscription"); import->hide(); connect(import,&QPushButton::clicked,this,[this] { addSubscription(false); });
    auto* paste=new QPushButton(page); paste->setObjectName("pasteSubscription"); paste->hide(); connect(paste,&QPushButton::clicked,this,[this] { importPayload(QApplication::clipboard()->text()); });
    // 两张列表共用一份行模型，各自按 kind 过滤；列表本身只负责选择与点击分流。
    auto* rows = new ProfileRowModel(this);
    rows->setObjectName("profileRows");
    profileRowsModel_ = rows;
    remoteProfileFilter_.setSourceModel(rows); remoteProfileFilter_.setKind(QStringLiteral("remote"));
    localProfileFilter_.setSourceModel(rows); localProfileFilter_.setKind(QStringLiteral("local"));
    setAccessibleName("配置页行数据");
    const auto makeList = [this](const QString& name, const QString& emptyText, const QString& emptyAction) {
        auto* card = panel(); auto* cardLayout = new QVBoxLayout(card); cardLayout->setContentsMargins(0, 0, 0, 0); cardLayout->setSpacing(0);
        auto* list = new QListView; list->setObjectName(name); list->setFrameShape(QFrame::NoFrame);
        list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        list->setSelectionMode(QAbstractItemView::ExtendedSelection); list->setSelectionBehavior(QAbstractItemView::SelectRows);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); list->setUniformItemSizes(true); list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list->setItemDelegate(new ProfileRowDelegate(list));
        list->setAccessibleName(name == "remoteProfileList" ? "远程订阅列表" : "本地配置列表");
        emptyState(list, emptyText);
        cardLayout->addWidget(list, 1);
        return card;
    };
    auto* remoteCard = makeList("remoteProfileList", "还没有远程订阅", "添加订阅…");
    auto* localCard = makeList("localProfileList", "还未导入本地配置", "导入文件…");
    remoteProfiles_ = remoteCard->findChild<QListView*>("remoteProfileList");
    localProfiles_ = localCard->findChild<QListView*>("localProfileList");
    remoteProfiles_->setModel(&remoteProfileFilter_);
    localProfiles_->setModel(&localProfileFilter_);
    auto* remoteHeading=new QLabel("远程订阅"); remoteHeading->setObjectName("remoteProfileHeading"); remoteHeading->setProperty("secondary",true); layout->addWidget(remoteHeading); remoteCard->setObjectName("remoteProfileGroup"); layout->addWidget(remoteCard);
    auto* localHeading=new QLabel("本地配置"); localHeading->setObjectName("localProfileHeading"); localHeading->setProperty("secondary",true); layout->addWidget(localHeading); localCard->setObjectName("localProfileGroup"); layout->addWidget(localCard);
    auto* noProfiles=new QLabel("还没有配置。导入订阅链接、本地文件或节点分享链接。"); noProfiles->setObjectName("profilesEmptyCaption"); noProfiles->setAlignment(Qt::AlignCenter); noProfiles->setWordWrap(true); noProfiles->setProperty("secondary",true); layout->addWidget(noProfiles);
    setEmptyStateAction(remoteProfiles_, "添加订阅…", [this] { addUrl(); });
    setEmptyStateAction(localProfiles_, "导入文件…", [this] { addSubscription(false); });
    // 页面级反馈位（P4.1）：更新/激活/批量更新的结论写在这里，不进全局状态栏。
    auto* submitHint = new ElidedLabel; submitHint->setObjectName("profileSubmitHint"); submitHint->setProperty("secondary", true);
    submitHint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); submitHint->setAccessibleName("订阅操作结果"); submitHint->hide(); layout->addWidget(submitHint);
    auto* hint = new QLabel("更新只写候选版本，不自动替换正在运行的配置；「设为当前配置」才会切换并启动。");
    hint->setObjectName("secondaryText"); hint->setProperty("secondary", true); hint->setWordWrap(true); hint->hide(); hint->setParent(page);
    wireProfileList(remoteProfiles_, "remote");
    wireProfileList(localProfiles_, "local");
    layout->addStretch(1);
    auto* scroll=new QScrollArea; scroll->setObjectName("profilesScroll"); scroll->setWidgetResizable(true);
    page->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setWidget(page);
    profileStack_=new QStackedWidget; profileStack_->setObjectName("profileTaskStack"); profileStack_->addWidget(scroll); return profileStack_;
}

QAbstractItemModel* MainWindow::profileRows(const QString& kind) const {
    return kind == "remote" ? static_cast<QAbstractItemModel*>(const_cast<ProfileKindFilter*>(&remoteProfileFilter_))
                            : static_cast<QAbstractItemModel*>(const_cast<ProfileKindFilter*>(&localProfileFilter_));
}

// 行 → 配置 ID。两张列表共用同一个行模型，所以角色编号在两处一致。
QString MainWindow::profileAt(const QModelIndex& index) {
    return index.isValid() ? index.data(ProfileRowModel::ProfileId).toString() : QString();
}

QStringList MainWindow::selectedProfileIds() const {
    QStringList ids;
    for (auto* list : {remoteProfiles_, localProfiles_}) {
        if (!list) continue;
        for (const auto& index : list->selectionModel()->selectedRows()) ids.append(index.data(ProfileRowModel::ProfileId).toString());
    }
    return ids;
}

// 选中一个对象：先清掉另一张列表的选择，再在它的归属列表里定位。
void MainWindow::selectProfile(const QString& id) {
    if (!remoteProfiles_ || !localProfiles_) return;
    for (auto* list : {remoteProfiles_, localProfiles_}) {
        if (!list) continue;
        list->clearSelection();
        for (int row = 0; row < list->model()->rowCount(); ++row) {
            const auto index = list->model()->index(row, 0);
            if (index.data(ProfileRowModel::ProfileId).toString() != id) continue;
            list->setCurrentIndex(index); list->scrollTo(index); updateActions(); return;
        }
    }
}

// 两张列表的行为相同：单击/双击/Enter/› 都打开抽屉；右键菜单保留完整对象维护。
// 选择状态是「当前要看/要操作的对象」，不改变正在运行的订阅。
void MainWindow::wireProfileList(QListView* list, const QString& kind) {
    connect(list, &QListView::doubleClicked, this, [this](const QModelIndex& index) { showProfileDetails(profileAt(index)); });
    connect(list, &QListView::clicked, this, [this, list](const QModelIndex& index) {
        if (auto* other = list->objectName() == "remoteProfileList" ? localProfiles_ : remoteProfiles_) if (other) other->clearSelection();
        showProfileDetails(profileAt(index));
    });
    connect(list, &QAbstractItemView::activated, this, [this](const QModelIndex& index) { showProfileDetails(profileAt(index)); });
    connect(list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateActions);
    // 每张列表一份菜单：共用一个对象名会让 findChild 拿到先建的远端菜单，
    // 本地列表的右键/Shift+F10 看起来「没反应」。
    auto* menu = new QMenu(this); menu->setObjectName("profileContextMenu");
    menu->setProperty("profileKind", kind);
    // 五组顺序（设计 5.6b）：只读头 / 使用 / 编辑与配置 ▸ / 对象维护 / 文件与目录 / 危险。
    auto* header = menu->addAction(QString()); header->setObjectName("profileMenuHeader"); header->setEnabled(false); menu->addSeparator();
    menuSection(menu, "使用");
    auto* activate = menu->addAction("设为当前配置"); activate->setObjectName("activateProfile");
    auto* reload = menu->addAction("重新加载当前版本"); reload->setObjectName("reloadActiveProfile"); reload->setObjectName("rollbackProfile");
    auto* rollback = menu->addAction("回滚上次运行版本");
    menu->addSeparator(); menuSection(menu, "编辑与配置");
    auto* editMenu = menu->addMenu("编辑配置"); editMenu->setObjectName("profileEditMenu");
    auto* providers = menu->addAction("节点源…"); providers->setObjectName("profileProviders");
    auto* dns = menu->addAction("DNS 设置…"); dns->setObjectName("profileDns");
    auto* proxyEditor = menu->addAction("编辑节点与策略组…"); proxyEditor->setObjectName("profileProxyEditor");
    auto* proxyChain = menu->addAction("上游代理链…"); proxyChain->setObjectName("profileProxyChain");
    editMenu->addActions({providers, dns, proxyEditor, proxyChain});
    menu->addSeparator(); menuSection(menu, "对象维护");
    auto* rename = menu->addAction("重命名…"); rename->setObjectName("renameProfile");
    auto* policy = menu->addAction("来源与更新…"); policy->setObjectName("profileUpdatePolicy");
    auto* compatibility = menu->addAction("配置兼容性…"); compatibility->setObjectName("profileCompatibility");
    menu->addSeparator(); menuSection(menu, "文件与目录");
    auto* share = menu->addAction("分享订阅二维码…"); share->setObjectName("shareProfileQr");
    auto* up = menu->addAction("上移"); up->setObjectName("moveProfileUp");
    auto* down = menu->addAction("下移"); down->setObjectName("moveProfileDown");
    auto* replace = menu->addAction("更换文件…"); replace->setObjectName("replaceProfileSource");
    auto* directory = menu->addAction("打开数据目录"); directory->setObjectName("openDataDirectory");
    menu->addSeparator(); menuSection(menu, "危险");
    auto* remove = menu->addAction("删除订阅…"); remove->setObjectName("deleteProfile");
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QWidget::customContextMenuRequested, this, [this, list, menu](const QPoint& point) {
        // 同一列表的另一个菜单若还开着（例如刚刚用 Shift+F10 打开过），先收起；
        // 否则空白处的右键会被已经弹出的菜单吃掉，看起来像「关不掉」。
        const auto index = list->indexAt(point); if (!index.isValid()) return;
        list->setCurrentIndex(index); if (auto* other = list->objectName() == "remoteProfileList" ? localProfiles_ : remoteProfiles_) if (other) other->clearSelection();
        updateActions(); menu->popup(list->viewport()->mapToGlobal(point));
    });
    for (const auto& key : {QKeySequence(Qt::Key_Menu), QKeySequence(Qt::SHIFT | Qt::Key_F10)}) {
        auto* shortcut = new QShortcut(key, list); shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, list, menu] { if (!list->currentIndex().isValid()) return; updateActions(); menu->popup(list->viewport()->mapToGlobal(list->visualRect(list->currentIndex()).center())); });
    }
    connect(menu, &QMenu::aboutToShow, this, [this, header, up, down, share, reload] {
        const auto id = selectedProfile(); QString name; int index = -1, count = 0;
        for (const auto& profile : controller_->profiles()) { ++count; if (profile.id == id) { name = profile.name; index = count - 1; } }
        const auto& state = controller_->state(); const bool using_ = id == state.activeProfile;
        const auto runtime = using_ ? (state.core == CoreState::Running ? QString("运行中") : QString("已选用 · 未运行")) : QString("尚未使用");
        header->setText(id.isEmpty() ? QString("未选择订阅") : name + " · " + runtime);
        const bool idle = !state.busy && !state.readOnly;
        up->setEnabled(idle && index > 0); down->setEnabled(idle && index >= 0 && index + 1 < count);
        share->setEnabled(idle && controller_->profileMaintenance(id).value("source_type") == "url");
        reload->setEnabled(idle && using_);
    });
    connect(up, &QAction::triggered, this, [this] { controller_->moveProfile(selectedProfile(), -1); });
    connect(down, &QAction::triggered, this, [this] { controller_->moveProfile(selectedProfile(), 1); });
    connect(activate, &QAction::triggered, this, [this] { const auto id = selectedProfile(); if (!id.isEmpty()) controller_->activateProfile(id); });
    connect(reload, &QAction::triggered, this, [this] { const auto id = selectedProfile(); if (!id.isEmpty()) controller_->activateProfile(id, true); });
    connect(rollback, &QAction::triggered, this, [this] { const auto id = selectedProfile(); if (!id.isEmpty()) controller_->activateProfile(id, true); });
    connect(rename, &QAction::triggered, this, [this] {
        QString name; for (const auto& profile : controller_->profiles()) if (profile.id == selectedProfile()) name = profile.name;
        bool ok = false; const auto next = QInputDialog::getText(this, "重命名订阅", "订阅名称", QLineEdit::Normal, name, &ok);
        if (ok && next.trimmed() != name) controller_->renameProfile(selectedProfile(), next);
    });
    connect(policy, &QAction::triggered, this, &MainWindow::editProfilePolicy);
    connect(remove, &QAction::triggered, this, [this] {
        const auto id = selectedProfile(); if (id.isEmpty()) return;
        if (QMessageBox::question(this, "删除订阅", "删除选中的订阅快照？若是当前订阅，将先安全恢复代理并停止内核。") == QMessageBox::Yes) controller_->deleteProfile(id);
    });
    connect(directory, &QAction::triggered, this, [this] { openDirectory(controller_->dataRoot()); });
    connect(replace, &QAction::triggered, this, [this] { addFile(true); });
    connect(providers, &QAction::triggered, this, [this] { showProviders(selectedProfile(), "proxy-providers"); });
    connect(dns, &QAction::triggered, this, [this] { const auto id = selectedProfile(); if (!id.isEmpty() && !controller_->state().busy) showTool("dns", id, [this, id] { return new DnsDialog(controller_, id, this); }); });
    connect(proxyEditor, &QAction::triggered, this, [this] { const auto id = selectedProfile(); if (!id.isEmpty() && !controller_->state().busy) showTool("proxy-editor", id, [this, id] { return new ProxyEditorDialog(controller_, id, this); }); });
    connect(proxyChain, &QAction::triggered, this, [this] { const auto id = selectedProfile(); if (!id.isEmpty() && !controller_->state().busy) showTool("proxy-chain", id, [this, id] { return new ProxyChainDialog(controller_, id, this); }); });
    connect(compatibility, &QAction::triggered, this, [this] {
        const auto id = selectedProfile(); if (id.isEmpty()) return;
        auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle("配置兼容性"); dialog->resize(540, 380);
        auto* content = new QVBoxLayout(dialog); auto* report = new QPlainTextEdit; report->setObjectName("compatibilityReport"); report->setReadOnly(true); report->setPlainText("正在检查…"); content->addWidget(report);
        auto* close = new QPushButton("关闭"); content->addWidget(close, 0, Qt::AlignRight); connect(close, &QPushButton::clicked, dialog, &QDialog::close);
        // QMenu::popup() 会以模态方式接管这一轮（win32 上是 own-message loop），
        // 菜单里 show() 出来的对话框会被压在菜单后面。先收起菜单再打开对话框。
        if (auto* menu = qobject_cast<QMenu*>(sender())) menu->close();
        dialog->show();
        controller_->inspectProfileCompatibility(id, [guard = QPointer<QPlainTextEdit>(report)](Result result) {
            if (!guard) return;
            if (!result.ok) { guard->setPlainText(result.error + "\n错误码：" + result.data.value("code").toString()); return; }
            QStringList managed; for (const auto& value : result.data.value("managed_fields").toArray()) managed.append(value.toString());
            guard->setPlainText("静态兼容检查通过\n\n" + (managed.isEmpty() ? QString("源配置没有显式设置受控字段。") : "以下源字段由 Mlash 覆盖或移除：\n" + managed.join("\n")) + "\n\n本机 Mixed 端口、私有控制接口、Secret、日志等级与TUN/LAN 由 Mlash 管理。远程 Provider 保存为版本快照。\n\n本报告不联网、不改配置；实际可用性仍需内核校验。");
        });
    });
    connect(share, &QAction::triggered, this, &MainWindow::shareProfileScheme);
    Q_UNUSED(kind);
}

// K7：行详情抽屉（原型 profileDetail / .drawer 410px）。
// 只读键值 + 「设为当前配置 / 更多操作」；应用更新与重新加载合并成一个动词，避免两个按钮做同一件事。
void MainWindow::showProfileDetails(const QString& id) {
    Profile profile; bool found=false; for (const auto& item:controller_->profiles()) if (item.id==id) { profile=item; found=true; break; }
    if (!found||!profileStack_) return;
    selectProfile(id); profileDetailId_=id;
    if (profileDetailsPage_) { auto* previous=profileDetailsPage_.data(); profileStack_->removeWidget(previous); previous->deleteLater(); }
    auto* page=new QWidget; page->setObjectName("profileDetailsPage"); auto* outer=new QVBoxLayout(page); outer->setContentsMargins(0,0,0,16); outer->setSpacing(12);
    auto* content=new QWidget; content->setMaximumWidth(760); auto* column=new QVBoxLayout(content); column->setContentsMargins(0,0,0,0); column->setSpacing(8); auto* centered=new QHBoxLayout; centered->setContentsMargins(0,0,0,0); centered->addWidget(content,1); outer->addLayout(centered,1);
    using namespace PrototypeUi; using G=UiIcons::Glyph;
    auto* returnButton=back(column,"profileDetailsBack","返回配置"); connect(returnButton,&QPushButton::clicked,this,[this] { profileStack_->setCurrentIndex(0); profileDetailId_.clear(); });
    auto* title=new QLabel(profile.name); title->setObjectName("dialogTitle"); title->setTextFormat(Qt::PlainText); column->addWidget(title);
    const bool current=id==controller_->state().activeProfile,pending=profile.candidate!=profile.active;
    auto* summary=new QLabel((profile.sourceType=="url"?QString("远程订阅"):QString("本地配置"))+" · "+(current?QString("当前使用"):QString("尚未使用"))+(pending?QString(" · 有候选待应用"):QString())); summary->setProperty("secondary",true); column->addWidget(summary);
    auto* stateRows=group(column);
    auto* use=row(stateRows,"profileDetailsActivate",pending?"查看候选变化":"使用此配置",pending?G::Refresh:G::Check,current&&!pending?"当前":"");
    use->setEnabled(!controller_->state().busy&&!controller_->state().readOnly&&!controller_->state().exiting&&(pending||!current||!controller_->state().ready));
    connect(controller_,&AppController::stateChanged,use,[this,use,id,pending,version=profile.candidate] {
        bool same=false; for (const auto& item:controller_->profiles()) if (item.id==id) same=item.candidate==version;
        const auto& state=controller_->state(); use->setEnabled(same&&!state.busy&&!state.exiting&&!state.readOnly&&(id!=state.activeProfile||!state.ready||pending));
    });
    connect(use,&QPushButton::clicked,this,[this,id,pending] { if (pending) (new ProfileChangesDialog(controller_,id,this))->show(); else controller_->activateProfile(id); });
    auto* config=group(column,"配置内容");
    auto* rules=row(config,"profileDetailsRules","规则与分流",G::Route); connect(rules,&QPushButton::clicked,this,[this,id] {
        if (rulesDirty_&&rulesDraftProfile_!=id&&QMessageBox::question(this,"未保存的规则","放弃当前规则草稿并查看此配置？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
        if (rulesDraftProfile_!=id) rulesDirty_=false;
        navigate(Route{ViewIds::Rules}); const int index=rulesProfiles_->findData(id); if (index>=0) rulesProfiles_->setCurrentIndex(index); loadRuleDraft();
    });
    auto* dns=row(config,"profileDetailsDns","DNS",G::Globe); connect(dns,&QPushButton::clicked,this,[this,id] { showTool("dns",id,[this,id] { return new DnsDialog(controller_,id,this); }); });
    auto* nodes=row(config,"profileDetailsNodes","节点与策略组",G::Layers); connect(nodes,&QPushButton::clicked,this,[this,id] { showTool("proxy-editor",id,[this,id] { return new ProxyEditorDialog(controller_,id,this); }); });
    auto* chain=row(config,"profileDetailsChain","拨号链",G::Route); connect(chain,&QPushButton::clicked,this,[this,id] { showTool("proxy-chain",id,[this,id] { return new ProxyChainDialog(controller_,id,this); }); });
    auto* maintenance=group(column,"来源与维护");
    auto* policy=row(maintenance,"profileDetailsMaintenance","更新与维护",G::Clock,profile.autoUpdate?QString("每 %1 小时").arg(profile.updateIntervalHours):QString("手动")); connect(policy,&QPushButton::clicked,this,[this,id] { selectProfile(id); editProfilePolicy(); });
    auto* providers=row(maintenance,"profileDetailsProviders","Provider",G::Layers); connect(providers,&QPushButton::clicked,this,[this,id] { showProviders(id); });
    auto* source=row(maintenance,"profileDetailsSource","来源信息",G::Folder,profile.sourceType=="url"?"远程订阅":"文件快照"); connect(source,&QPushButton::clicked,this,[this,id] { selectProfile(id); editProfilePolicy(); });
    auto* more=row(maintenance,"profileDetailsMore","更多维护操作",G::More); connect(more,&QPushButton::clicked,this,[this,id,more] {
        auto* menu=new QMenu(more); menu->setAttribute(Qt::WA_DeleteOnClose); menu->setObjectName("profileDetailsMenu");
        auto* update=menu->addAction("更新订阅"); update->setObjectName("updateProfile"); connect(update,&QAction::triggered,this,[this,id] { controller_->updateProfile(id); });
        auto* policy=menu->addAction("来源与更新…"); policy->setObjectName("profilePolicyButton"); connect(policy,&QAction::triggered,this,[this,id,menu] { menu->hide(); selectProfile(id); editProfilePolicy(); });
        auto* share=menu->addAction("分享二维码…"); share->setObjectName("shareProfileQr"); connect(share,&QAction::triggered,this,[this,id,menu] { menu->hide(); selectProfile(id); shareProfileScheme(); });
        menu->addAction("打开数据目录",this,[this] { openDirectory(controller_->dataRoot()); }); menu->addSeparator();
        auto* remove=menu->addAction("删除订阅…"); remove->setObjectName("deleteProfile"); connect(remove,&QAction::triggered,this,[this,id] { if (QMessageBox::question(this,"删除订阅","删除此订阅快照？当前订阅将先安全停止。",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes) controller_->deleteProfile(id); }); menu->popup(more->mapToGlobal(QPoint(0,more->height())));
    });
    if (!profile.updateError.isEmpty()) { auto* error=new QLabel("上次更新失败："+profile.updateError); error->setWordWrap(true); error->setTextFormat(Qt::PlainText); error->setProperty("secondary",true); column->addWidget(error); }
    column->addStretch(1); auto* scroll=new QScrollArea; scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true); scroll->setWidget(page); profileDetailsPage_=scroll; profileStack_->addWidget(scroll); profileStack_->setCurrentWidget(scroll);
}

// 分享订阅二维码（P14）：地址可能含私人令牌，先给警告，再按需显示。
// 从右键菜单和行详情抽屉的「更多操作」共用。
void MainWindow::shareProfileScheme() {
    const auto data = controller_->profileMaintenance(selectedProfile()); if (data.value("source_type") != "url") return;
    auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profileShareDialog"); dialog->setWindowTitle("分享订阅"); auto* box = new QVBoxLayout(dialog);
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
    auto* close = new QPushButton("关闭"); box->addWidget(close); connect(close, &QPushButton::clicked, dialog, &QDialog::close); dialog->resize(380, 180); dialog->show();
}

void MainWindow::updateProfiles() {
    const auto& s = controller_->settings();
    // Programmatic sync must not look like a user edit and resubmit settings.
    settingsSyncing_ = true;
    const auto toggle = [this](QCheckBox* box, bool before, bool actual) { if (!hasDisplayedSettings_ || box->isChecked() == before) box->setChecked(actual); };
    toggle(autoStart_,displayedSettings_.autoStart,s.autoStart); toggle(silentStart_,displayedSettings_.silentStart,s.silentStart); toggle(startCore_,displayedSettings_.startCore,s.startCore); toggle(restoreProxy_,displayedSettings_.restoreProxy,s.restoreProxy); toggle(lightweight_,displayedSettings_.lightweight,s.lightweight);
    if (!hasDisplayedSettings_ || mixedPort_->value() == displayedSettings_.mixedPort) mixedPort_->setValue(s.mixedPort);
    if (!hasDisplayedSettings_ || logLevel_->currentData().toString() == displayedSettings_.logLevel) logLevel_->setCurrentIndex(logLevel_->findData(s.logLevel));
    if (!hasDisplayedSettings_ || corePath_->text() == displayedSettings_.corePath) corePath_->setText(s.corePath);
    if (!hasDisplayedSettings_ || testUrl_->text() == displayedSettings_.testUrl) testUrl_->setText(s.testUrl);
    if (!hasDisplayedSettings_ || startupPage_->currentData().toInt()==displayedSettings_.startupPage) startupPage_->setCurrentIndex(startupPage_->findData(s.startupPage));
    for (const auto& tuple:QList<QPair<QComboBox*,QPair<QString,QString>>>{{trayClick_,{displayedSettings_.trayClick,s.trayClick}},{ipv6Mode_,{displayedSettings_.ipv6Mode,s.ipv6Mode}},{unifiedDelayMode_,{displayedSettings_.unifiedDelayMode,s.unifiedDelayMode}}}) if (!hasDisplayedSettings_ || tuple.first->currentData().toString()==tuple.second.first) tuple.first->setCurrentIndex(tuple.first->findData(tuple.second.second));
    if (!hasDisplayedSettings_||appearance_->currentData().toString()==displayedSettings_.appearance) appearance_->setCurrentIndex(appearance_->findData(s.appearance));
    if (!hasDisplayedSettings_||textScale_->currentData().toInt()==displayedSettings_.textScale) textScale_->setCurrentIndex(textScale_->findData(s.textScale));
    settingsSyncing_ = false;
    displayedSettings_ = s; hasDisplayedSettings_ = true;
    // K7：两张列表的行模型同步（远端下拉 profilesCombo_ 只给节点页浏览用，这里只补当前项）。
    // 重建下拉时保持原有选择：否则每次订阅变化都会回落到第一项，
    // 而节点区只给「正在使用的订阅」填充行，浏览别的订阅会变成空列表。
    const auto selected = selectedProfile();
    const auto selectedCombo = profilesCombo_->currentData().toString(); const QSignalBlocker comboBlock(profilesCombo_); profilesCombo_->clear();
    for (const auto& p : controller_->profiles()) profilesCombo_->addItem(p.name + (p.id == controller_->state().activeProfile ? " · 当前" : ""), p.id);
    {   const int restore = !selectedCombo.isEmpty() ? profilesCombo_->findData(selectedCombo)
            : profilesCombo_->findData(controller_->state().activeProfile);
        profilesCombo_->setCurrentIndex(restore >= 0 ? restore : (profilesCombo_->count() ? 0 : -1)); }
    if (auto* rows = profileRowsModel_) {
        rows->setProfiles(controller_->profiles());
        rows->setState(controller_->state().activeProfile, controller_->state().ready);
        for (auto* list:{remoteProfiles_,localProfiles_}) {
            const int height=qMax(UiTheme::ProfileRowHeight,QFontMetrics(list->font()).height()*2+16);
            list->setFixedHeight(qBound(list->model()->rowCount()==0?144:64,list->model()->rowCount()*height+4,304));
        }
    }
    const auto selectById = [](QListView* list, const QString& id) {
        if (!list) return;
        for (int row = 0; row < list->model()->rowCount(); ++row) {
            const auto index = list->model()->index(row, 0);
            if (index.data(ProfileRowModel::ProfileId).toString() == id) { list->setCurrentIndex(index); return; }
        }
    };
    if (!selected.isEmpty()) selectById(remoteProfiles_, selected);
    if (!selected.isEmpty()) selectById(localProfiles_, selected);
    if (!selected.isEmpty() && !remoteProfiles_->currentIndex().isValid() && !localProfiles_->currentIndex().isValid()) {
        if (auto* list = remoteProfiles_->model()->rowCount() ? remoteProfiles_ : localProfiles_->model()->rowCount() ? localProfiles_ : nullptr) {
            list->setCurrentIndex(list->model()->index(0, 0));
        }
    }
    // 两张列表的空态：没有任何订阅时引导到「添加订阅」/「导入文件」。
    // 两张列表的空态：没有对应来源的行时给出该组的引导动作。
    if (remoteProfiles_ && localProfiles_) {
        if (auto* label = remoteProfiles_->findChild<QLabel*>("emptyState")) label->setVisible(remoteProfiles_->model()->rowCount() == 0);
        if (auto* label = localProfiles_->findChild<QLabel*>("emptyState")) label->setVisible(localProfiles_->model()->rowCount() == 0);
        setEmptyStateAction(remoteProfiles_, "添加订阅…", [this] { addUrl(); });
        setEmptyStateAction(localProfiles_, "导入文件…", [this] { addSubscription(false); });
    }
    if (!profileDetailId_.isEmpty()) {
        bool found=false; for (const auto& profile:controller_->profiles()) if (profile.id==profileDetailId_) found=true;
        if (!found) { profileDetailId_.clear(); if (profileStack_) profileStack_->setCurrentIndex(0); }
    }
    const bool hasRemote=remoteProfiles_->model()->rowCount()>0,hasLocal=localProfiles_->model()->rowCount()>0;
    for (const auto& name:{"remoteProfileHeading","remoteProfileGroup"}) if (auto* widget=findChild<QWidget*>(name)) widget->setVisible(hasRemote);
    for (const auto& name:{"localProfileHeading","localProfileGroup"}) if (auto* widget=findChild<QWidget*>(name)) widget->setVisible(hasLocal);
    if (auto* empty=findChild<QLabel*>("profilesEmptyCaption")) empty->setVisible(!hasRemote&&!hasLocal);
    // 行内「当前值」（V3）：只更新已有 settingValue 的行；控件本身就显示值的行不建这个标签。
    {
        QString tunText = s.tunEnabled ? QString("已启用 · 下次启动生效") : controller_->state().tunObserved ? QString("已启用") : QString("未启用");
        const QList<QPair<QString, QString>> values{
            {"TUN 虚拟网卡", tunText},
            {"内核更新", s.corePath.isEmpty() ? QString("尚未设置内核路径") : QFileInfo(s.corePath).fileName()},
        };
        for (auto* row : findChildren<QWidget*>("settingRow")) {
            const auto name = row->property("settingSearchName").toString();
            for (const auto& pair : values) {
                if (pair.first != name) continue;
                if (auto* current = row->findChild<QLabel*>("settingValue")) {
                    if (current->text() != pair.second) { current->setText(pair.second); current->setToolTip(pair.second); }
                }
            }
        }
    }
    updateProxies();
}

QString MainWindow::selectedProfile() const {
    if (profileStack_&&profileStack_->currentIndex()>0&&!profileDetailId_.isEmpty()) return profileDetailId_;
    for (auto* list : {remoteProfiles_, localProfiles_}) {
        if (!list || !list->currentIndex().isValid()) continue;
        const auto id = list->currentIndex().data(ProfileRowModel::ProfileId).toString();
        if (!id.isEmpty()) return id;
    }
    return {};
}

void MainWindow::addUrl() {
    addSubscription(true);
}

void MainWindow::importPayload(const QString& input) {
    if (controller_->state().busy || controller_->state().readOnly || findChild<ImportDialog*>()) return;
    const auto text=input.trimmed(); const auto scheme=text.section(':',0,0).toLower();
    if (text.toUtf8().size()<=64*1024&&NodeLinks::acceptsScheme(scheme)&&scheme!="http"&&scheme!="https") { auto* dialog=new ImportDialog(this); dialog->setNodeLinks(text); presentImport(dialog); return; }
    if (text.isEmpty() || text.size()>4096 || text.contains('\n') || text.contains('\r')) { QMessageBox::information(this,"导入订阅","请提供一个订阅地址、本地 YAML 文件或每行一个节点链接"); return; }
    const QUrl url(text); const bool remote=QStringList{"http","https"}.contains(url.scheme())&&!url.host().isEmpty()&&url.userInfo().isEmpty()&&!url.hasFragment();
    const auto path=url.isLocalFile()?url.toLocalFile():text;
    if (!remote && (!QFileInfo(path).isAbsolute() || !QStringList{"yaml","yml"}.contains(QFileInfo(path).suffix().toLower()))) { QMessageBox::information(this,"导入订阅","仅支持一个 HTTP/HTTPS 订阅或本地 YAML 文件"); return; }
    auto* dialog=new ImportDialog(this); dialog->setPrefill(remote?text:path,remote); presentImport(dialog);
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
    QStringList ids; const auto selected=selectedProfileIds();
    if (selected.size()>1) ids=selected; else for (const auto& p:controller_->profiles()) ids.append(p.id);
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

void MainWindow::presentImport(ImportDialog* dialog) {
    QSet<QString> before; for (const auto& profile:controller_->profiles()) before.insert(profile.id);
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->bindController(controller_);
    // 导入成功后停在配置页并选中新对象：两张列表都要找，因为来源类型决定它落在哪一张。
    connect(dialog,&QDialog::accepted,this,[this,before] { navigate(Route{ViewIds::Profiles}); for (const auto& profile:controller_->profiles()) if (!before.contains(profile.id)) { selectProfile(profile.id); break; } });
    dialog->open();
}
void MainWindow::addSubscription(bool url) {
    auto* dialog = new ImportDialog(this); dialog->setSourceTab(url); presentImport(dialog);
}

void MainWindow::addFile(bool replace) {
    if (!replace) { addSubscription(false); return; }
    const auto id = replace ? selectedProfile() : QString(); if (replace && id.isEmpty()) return;
    const auto path = QFileDialog::getOpenFileName(this, "导入完整 YAML", {}, "YAML (*.yaml *.yml)"); if (!path.isEmpty()) controller_->importFile({}, path, id);
}

void MainWindow::showProviders(const QString& id, const QString& kind) {
    if (id.isEmpty()) return;
    auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("providersDialog"); dialog->setWindowTitle(kind=="rule-providers"?"规则源（Provider）":"节点源（Provider）"); dialog->resize(680, 410);
    auto* content = new QVBoxLayout(dialog); auto* versions=new CompactComboBox; versions->setObjectName("providerVersion"); versions->addItem("待使用版本",false); versions->addItem("上次使用版本",true); content->addWidget(versions);
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
    connect(policy,&QPushButton::clicked,dialog,[this,id,table,dialog,load] { const auto* item=table->item(table->currentRow(),0); if (!item||item->data(Qt::UserRole).toString().isEmpty()) return; auto* editor=new ProviderPolicyDialog(controller_,id,item->data(Qt::UserRole+1).toJsonObject(),dialog); connect(editor,&QDialog::accepted,dialog,[load] { (*load)(); }); editor->open(); });
    connect(refresh, &QPushButton::clicked, dialog, [this, id, table, waiting] { const auto* selected = table->item(table->currentRow(), 0); if (!selected) return; *waiting = true; controller_->refreshProviders(id, selected->data(Qt::UserRole).toString()); });
    connect(all, &QPushButton::clicked, dialog, [this, id, kind, waiting] { *waiting = true; controller_->refreshProviders(id, kind.isEmpty() ? "*" : "kind:" + kind); });
    connect(controller_, &AppController::operationFinished, dialog, [waiting, guardedHint, load, update](bool ok, const QString& message) { if (!*waiting) return; *waiting = false; if (guardedHint) guardedHint->setText(ok ? "更新已就绪，可查看变化并应用。" : message); (*load)(); update(); });
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
    const auto usage = SubscriptionInfo::summary(profile.subscriptionInfo); auto* info = new QLabel(usage.isEmpty() ? "" : usage + "\n来源快照：" + profile.subscriptionInfo.value("observed_at").toString()); info->setTextFormat(Qt::PlainText); info->setWordWrap(true); info->setVisible(!usage.isEmpty()); layout->addWidget(info);
    auto* maintenance=new QLabel("最近成功："+(profile.updated.isEmpty()?QString("尚无"):profile.updated)+"\n最近尝试："+(profile.lastUpdateAttempt.isEmpty()?QString("尚无"):profile.lastUpdateAttempt)+"\n"+(expected.value("next").toString().isEmpty()?QString("无定时任务"):"下次检查："+expected.value("next").toString())+(profile.updateError.isEmpty()?QString():"\n上次失败："+profile.updateError)); maintenance->setTextFormat(Qt::PlainText); maintenance->setWordWrap(true); layout->addWidget(maintenance);
    auto* note = new QLabel("更新后需手动应用。"); note->setWordWrap(true); note->setObjectName("secondaryText"); layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText("保存"); buttons->button(QDialogButtonBox::Cancel)->setText("取消"); outer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto saving=std::make_shared<bool>(false);
    DialogDraftGuard::protect(dialog,[automatic,hours,download,profile,name,source] { return name->text()!=profile.name || source->text()!=profile.source || automatic->isChecked()!=profile.autoUpdate || (hours && hours->value()!=profile.updateIntervalHours) || download->policy().toJson()!=profile.download.toJson(); },[saving] { return *saving; });
    connect(controller_,&AppController::operationFinished,dialog,[dialog,buttons,note,saving](bool ok,const QString& error) { if (!*saving) return; *saving=false; buttons->setEnabled(true); if (ok) dialog->accept(); else note->setText("保存失败，修改已保留 · "+error); });
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, id, automatic, hours, download, buttons, saving, name, source, expected, interval = profile.updateIntervalHours] { if (!download->policy().valid()) { QMessageBox::warning(dialog, "下载设置", "User-Agent 须为最多 256 个单行 ASCII 字符"); return; } *saving=true; buttons->setEnabled(false); controller_->editProfileSource(id,name->text(),source->text(),automatic->isChecked(),hours?hours->value():interval,download->policy(),expected); }); dialog->open();
}

}
