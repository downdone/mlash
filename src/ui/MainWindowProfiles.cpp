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

namespace mlash {
using namespace WindowParts;
namespace {
class ProfileCardCell final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex&) const override {
        const auto* list=qobject_cast<const QListView*>(option.widget);
        return list ? list->gridSize() : QSize(320,ProfileCardWidget::CellHeight);
    }
    void paint(QPainter*,const QStyleOptionViewItem&,const QModelIndex&) const override {}
};
}

// The subscription surface follows ADR 0057: URL and file profiles share one
// ordered card model. Card body, refresh, candidate and menu have distinct hits.
QWidget* MainWindow::profilesPage() {
    using namespace PrototypeUi;
    auto* page = new QWidget;
    page->setObjectName("profilesSurface");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(10);

    auto* quick = new QWidget(page);
    quick->setObjectName("profileQuickImport");
    auto* quickRow = new QHBoxLayout(quick);
    quickRow->setContentsMargins(0,0,0,0);
    quickRow->setSpacing(8);
    quickImport_ = new QLineEdit(quick);
    quickImport_->setObjectName("profileImportInput");
    quickImport_->setAccessibleName("订阅地址或 YAML 文件路径");
    quickImport_->setPlaceholderText("粘贴订阅 URL 或 YAML 文件路径");
    quickImport_->setMaxLength(4096);
    quickImport_->setClearButtonEnabled(true);
    quickRow->addWidget(quickImport_,1);
    auto* browse = new QPushButton("选文件…",quick);
    browse->setObjectName("profileBrowseImport");
    quickRow->addWidget(browse);
    auto* submit = new QPushButton("导入",quick);
    submit->setObjectName("profileQuickSubmit");
    submit->setProperty("primary",true);
    quickRow->addWidget(submit);
    auto* advanced = new QPushButton("高级新增…",quick);
    advanced->setObjectName("importSubscription");
    quickRow->addWidget(advanced);
    layout->addWidget(quick);
    auto* submitHint = new ElidedLabel;
    submitHint->setObjectName("profileSubmitHint");
    submitHint->setProperty("secondary",true);
    submitHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    submitHint->setAccessibleName("订阅操作结果");
    submitHint->hide();
    layout->addWidget(submitHint);

    connect(browse,&QPushButton::clicked,this,[this] {
        const auto path=QFileDialog::getOpenFileName(this,"选择配置文件",{},"YAML (*.yaml *.yml)");
        if (!path.isEmpty()) quickImport_->setText(QDir::toNativeSeparators(path));
    });
    connect(submit,&QPushButton::clicked,this,&MainWindow::submitQuickImport);
    connect(quickImport_,&QLineEdit::returnPressed,this,&MainWindow::submitQuickImport);
    connect(advanced,&QPushButton::clicked,this,&MainWindow::openAdvancedImport);
    const auto syncImport=[this,browse,submit,advanced] {
        const auto& state=controller_->state();
        const bool enabled=!state.busy&&!state.readOnly&&!state.exiting&&!quickImportPending_;
        quickImport_->setReadOnly(!enabled);
        browse->setEnabled(enabled);
        advanced->setEnabled(enabled);
        submit->setEnabled(enabled&&!quickImport_->text().trimmed().isEmpty());
        submit->setText(quickImportPending_?"导入中…":"导入");
    };
    connect(controller_,&AppController::stateChanged,quick,syncImport);
    connect(quickImport_,&QLineEdit::textChanged,quick,[this,syncImport](const QString& text) {
        auto input=text.trimmed();
        if (input.startsWith('"')) input.remove(0,1);
        const auto scheme=input.section(':',0,0).toLower();
        const bool sensitive=scheme=="https" || scheme=="http" || NodeLinks::acceptsScheme(scheme);
        if (quickImport_->echoMode()!=(sensitive?QLineEdit::Password:QLineEdit::Normal))
            quickImport_->setEchoMode(sensitive?QLineEdit::Password:QLineEdit::Normal);
        syncImport();
    });
    connect(controller_,&AppController::operationFinished,quick,[this,syncImport](bool ok,const QString&,OperationId operation) {
        if (!quickImportPending_ || operation!=OperationId::ProfileImport) return;
        quickImportPending_=false;
        if (ok) {
            quickImport_->clear();
            if (!controller_->profiles().isEmpty()) selectProfile(controller_->profiles().back().id);
        }
        syncImport();
    });
    syncImport();

    auto* listTools = new QHBoxLayout;
    auto* heading = new QLabel("我的配置",page);
    heading->setObjectName("profileCollectionHeading");
    listTools->addWidget(heading);
    auto* count = new QLabel(page);
    count->setObjectName("profileCollectionCount");
    listTools->addWidget(count);
    listTools->addStretch();
    auto* order = new QToolButton(page);
    order->setObjectName("profileOrder");
    order->setText("排序");
    order->hide();
    order->setToolTip("调整当前选中配置在列表中的位置");
    order->setPopupMode(QToolButton::InstantPopup);
    auto* orderMenu = new QMenu(order);
    orderMenu->setObjectName("profileOrderMenu");
    auto* moveUp = orderMenu->addAction("上移");
    auto* moveDown = orderMenu->addAction("下移");
    order->setMenu(orderMenu);
    listTools->addWidget(order);
    connect(orderMenu, &QMenu::aboutToShow, this, [this, moveUp, moveDown] {
        const int row = profiles_ && profiles_->currentIndex().isValid() ? profiles_->currentIndex().row() : -1;
        const bool canEdit = !controller_->state().busy && !controller_->state().readOnly;
        const QString id = row >= 0 ? profileAt(profiles_->model()->index(row, 0)) : QString();
        moveUp->setData(id); moveDown->setData(id);
        moveUp->setEnabled(canEdit && row > 0);
        moveDown->setEnabled(canEdit && row >= 0 && row + 1 < profiles_->model()->rowCount());
    });
    connect(moveUp, &QAction::triggered, this, [this, moveUp] { controller_->moveProfile(moveUp->data().toString(), -1); });
    connect(moveDown, &QAction::triggered, this, [this, moveDown] { controller_->moveProfile(moveDown->data().toString(), 1); });
    auto* batch = new QPushButton("更新全部", page);
    batch->setObjectName("batchProfiles");
    batch->hide();
    listTools->addWidget(batch);
    layout->addLayout(listTools);
    connect(batch, &QPushButton::clicked, this, [this] { showProfileBatch(false); });
    mutations_ += {batch};

    profileRowsModel_ = new ProfileRowModel(this);
    profileRowsModel_->setObjectName("profileRows");
    profiles_ = new QListView(page);
    profiles_->setObjectName("profileList");
    profiles_->setModel(profileRowsModel_);
    profiles_->setItemDelegate(new ProfileCardCell(profiles_));
    profiles_->setFrameShape(QFrame::NoFrame);
    profiles_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    profiles_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    profiles_->setSelectionBehavior(QAbstractItemView::SelectRows);
    profiles_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    profiles_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    profiles_->setViewMode(QListView::IconMode);
    profiles_->setFlow(QListView::LeftToRight);
    profiles_->setWrapping(true);
    profiles_->setResizeMode(QListView::Adjust);
    profiles_->setMovement(QListView::Static);
    profiles_->setSpacing(0);
    profiles_->setUniformItemSizes(true);
    profiles_->setGridSize(QSize(320, ProfileCardWidget::CellHeight));
    profiles_->setMouseTracking(true);
    profiles_->installEventFilter(this);
    profiles_->viewport()->installEventFilter(this);
    profiles_->setAccessibleName("订阅卡片");
    profiles_->setAccessibleDescription("方向键选择卡片，回车打开配置内容，菜单键可使用此配置，F5 检查更新；Ctrl+Shift+V 查看变化，Ctrl+Shift+A 应用更新");
    layout->addWidget(profiles_, 1);
    emptyState(profiles_, "还没有配置");
    wireProfileList(profiles_);
    updateProfileGrid();
    return page;
}

void MainWindow::updateProfileGrid() {
    if (!profiles_) return;
    // Reserve scrollbar space in both states so the column width does not
    // change when adding a card makes the list start scrolling.
    constexpr int minimumCardWidth=288;
    const int scrollbar=profiles_->style()->pixelMetric(QStyle::PM_ScrollBarExtent,nullptr,profiles_);
    const int available=qMax(1,profiles_->maximumViewportSize().width()-scrollbar-2);
    const int columns=available>=2*(minimumCardWidth+8)?2:1;
    const int width=available/columns;
    const QSize size(width,ProfileCardWidget::CellHeight);
    if (profiles_->gridSize() != size) profiles_->setGridSize(size);
}

QString MainWindow::profileAt(const QModelIndex& index) {
    return index.isValid() ? index.data(ProfileRowModel::ProfileId).toString() : QString();
}
QStringList MainWindow::selectedProfileIds() const {
    QStringList ids;
    if (profiles_ && profiles_->selectionModel())
        for (const auto& index : profiles_->selectionModel()->selectedRows())
            ids.append(profileAt(index));
    return ids;
}
void MainWindow::selectProfile(const QString& id) {
    if (!profiles_) return;
    for (int row = 0; row < profiles_->model()->rowCount(); ++row) {
        const auto index = profiles_->model()->index(row, 0);
        if (profileAt(index) != id) continue;
        profiles_->setCurrentIndex(index);
        profiles_->scrollTo(index, QAbstractItemView::EnsureVisible);
        updateActions();
        return;
    }
}
void MainWindow::showProfileMenu(const QString& id, const QPoint& globalPoint) {
    if (id.isEmpty()) return;
    bool remote = false, refreshable = false;
    for (const auto& profile : controller_->profiles()) if (profile.id == id) {
        remote = profile.sourceType == "url";
        refreshable = !profile.source.isEmpty();
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
    if (id!=controller_->state().activeProfile) {
        auto* use=menu->addAction("使用此配置");
        use->setEnabled(!controller_->state().busy && !controller_->state().readOnly);
        connect(use,&QAction::triggered,this,[this,id] { controller_->activateProfile(id); });
    }
    if (remote) {
        auto* share = menu->addAction("分享配置…");
        connect(share, &QAction::triggered, this, [this, id] {
            selectProfile(id);
            shareProfileScheme();
        });
    }
    Q_UNUSED(refreshable);
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
void MainWindow::wireProfileList(QListView* list) {
    using Action = ProfileCardWidget::Action;
    const auto writable=[this] {
        const auto& state=controller_->state();
        return !state.busy&&!state.readOnly&&!state.exiting;
    };
    const auto syncCard=[list,writable](const QModelIndex& index) {
        if (auto* card=qobject_cast<ProfileCardWidget*>(list->indexWidget(index))) {
            card->setProfile(index);
            card->setWritable(writable());
            card->setSelected(list->selectionModel()->isSelected(index));
        }
    };
    connect(profileRowsModel_,&QAbstractItemModel::modelReset,list,[this,list,syncCard,writable] {
        for (int row=0;row<list->model()->rowCount();++row) {
            const QPersistentModelIndex index=list->model()->index(row,0);
            auto* card=new ProfileCardWidget;
            list->setIndexWidget(index,card);
            syncCard(index);
            connect(card,&ProfileCardWidget::actionRequested,this,[this,list,card,index,writable](Action action) {
                if (!index.isValid()) return;
                const auto id=profileAt(index);
                list->setCurrentIndex(index);
                switch (action) {
                case Action::Menu: {
                    auto* button=card->button(action);
                    showProfileMenu(id,button->mapToGlobal(QPoint(0,button->height())));
                    break;
                }
                case Action::Review:
                    if (index.data(ProfileRowModel::HasCandidate).toBool()) openProfileTask(id,"candidate");
                    break;
                case Action::Refresh:
                    if (writable() && index.data(ProfileRowModel::CanRefresh).toBool()) controller_->updateProfile(id);
                    break;
                case Action::Apply:
                    if (writable() && index.data(ProfileRowModel::HasCandidate).toBool()) controller_->activateProfile(id);
                    break;
                case Action::Use:
                    if (writable() && !index.data(ProfileRowModel::IsCurrent).toBool()) controller_->activateProfile(id);
                    break;
                }
            });
        }
    });
    connect(profileRowsModel_,&QAbstractItemModel::dataChanged,list,[list,syncCard](const QModelIndex& first,const QModelIndex& last) {
        for (int row=first.row();row<=last.row();++row) syncCard(list->model()->index(row,0));
    });
    connect(list->selectionModel(),&QItemSelectionModel::selectionChanged,list,
            [list](const QItemSelection& selected,const QItemSelection& deselected) {
        for (const auto& index:deselected.indexes())
            if (auto* card=qobject_cast<ProfileCardWidget*>(list->indexWidget(index))) card->setSelected(false);
        for (const auto& index:selected.indexes())
            if (auto* card=qobject_cast<ProfileCardWidget*>(list->indexWidget(index))) card->setSelected(true);
    });
    connect(controller_,&AppController::stateChanged,list,[list,writable] {
        const bool enabled=writable();
        for (int row=0;row<list->model()->rowCount();++row)
            if (auto* card=qobject_cast<ProfileCardWidget*>(list->indexWidget(list->model()->index(row,0)))) card->setWritable(enabled);
    });
    connect(qApp,&QApplication::focusChanged,list,[list](QWidget*,QWidget* focused) {
        const auto* card=focused?qobject_cast<ProfileCardWidget*>(focused->parentWidget()):nullptr;
        if (!card || card->parentWidget()!=list->viewport()) return;
        for (int row=0;row<list->model()->rowCount();++row) {
            const auto index=list->model()->index(row,0);
            if (list->indexWidget(index)!=card) continue;
            list->selectionModel()->setCurrentIndex(index,QItemSelectionModel::NoUpdate);
            list->scrollTo(index);
            break;
        }
    });
    profileContextMenu_ = new QMenu(this);
    profileContextMenu_->setObjectName("profileContextMenu");
    connect(list, &QListView::clicked, this, [this](const QModelIndex& index) {
        if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) return;
        const auto id = profileAt(index);
        if (!id.isEmpty()) openProfileTask(id,"nodes");
    });
    connect(list, &QAbstractItemView::activated, this, [this](const QModelIndex& index) {
        const auto id = profileAt(index);
        if (!id.isEmpty()) openProfileTask(id,"nodes");
    });
    connect(list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateActions);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QWidget::customContextMenuRequested, this, [this, list](const QPoint& point) {
        const auto index = list->indexAt(point);
        if (index.isValid() && ProfileCardWidget::surfaceRect(list->visualRect(index)).contains(point)) {
            list->setCurrentIndex(index);
            showProfileMenu(profileAt(index), list->viewport()->mapToGlobal(point));
        }
    });
    for (const auto& key : {QKeySequence(Qt::Key_Menu), QKeySequence(Qt::SHIFT | Qt::Key_F10)}) {
        auto* shortcut = new QShortcut(key, list);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, list] {
            const auto index = list->currentIndex();
            if (index.isValid()) showProfileMenu(profileAt(index), list->viewport()->mapToGlobal(list->visualRect(index).center()));
        });
    }
    auto* refresh = new QShortcut(QKeySequence(Qt::Key_F5), list);
    refresh->setContext(Qt::WidgetWithChildrenShortcut);
    connect(refresh, &QShortcut::activated, this, [this, list] {
        const auto index = list->currentIndex();
        if (index.isValid() && index.data(ProfileRowModel::CanRefresh).toBool()) controller_->updateProfile(profileAt(index));
    });
    auto* review = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), list);
    review->setContext(Qt::WidgetWithChildrenShortcut);
    connect(review, &QShortcut::activated, this, [this, list] {
        const auto index = list->currentIndex();
        if (index.isValid() && index.data(ProfileRowModel::HasCandidate).toBool())
            openProfileTask(profileAt(index), "candidate");
    });
    auto* applyCandidate = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A), list);
    applyCandidate->setContext(Qt::WidgetWithChildrenShortcut);
    connect(applyCandidate, &QShortcut::activated, this, [this, list] {
        const auto index = list->currentIndex();
        if (index.isValid() && index.data(ProfileRowModel::HasCandidate).toBool() &&
            !controller_->state().busy && !controller_->state().readOnly)
            controller_->activateProfile(profileAt(index));
    });
}
void MainWindow::openProfileTask(const QString& id, const QString& task) {
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
    if (!hasDisplayedSettings_||windowChrome_->currentData().toString()==displayedSettings_.windowChrome) windowChrome_->setCurrentIndex(windowChrome_->findData(s.windowChrome));
    settingsSyncing_ = false;
    displayedSettings_ = s; hasDisplayedSettings_ = true;
    const auto selected = selectedProfile();
    const auto selectedCombo = profilesCombo_->currentData().toString();
    const QSignalBlocker comboBlock(profilesCombo_);
    profilesCombo_->clear();
    for (const auto& p : controller_->profiles())
        profilesCombo_->addItem(p.name + (p.id == controller_->state().activeProfile ? " · 当前" : ""), p.id);
    const int restore = !selectedCombo.isEmpty() ? profilesCombo_->findData(selectedCombo)
        : profilesCombo_->findData(controller_->state().activeProfile);
    profilesCombo_->setCurrentIndex(restore >= 0 ? restore : (profilesCombo_->count() ? 0 : -1));
    const int scrollPosition = profiles_ ? profiles_->verticalScrollBar()->value() : 0;
    if (profileRowsModel_) {
        profileRowsModel_->setProfiles(controller_->profiles());
        profileRowsModel_->setState(controller_->state().activeProfile, controller_->state().ready);
        updateProfileGrid();
    }
    if (auto* count=findChild<QLabel*>("profileCollectionCount"))
        count->setText(QString("%1 个").arg(controller_->profiles().size()));
    if (auto* batch = findChild<QPushButton*>("batchProfiles")) {
        int refreshable = 0;
        for (const auto& profile : controller_->profiles()) if (!profile.source.isEmpty()) ++refreshable;
        batch->setVisible(refreshable > 1);
        batch->setEnabled(refreshable > 1 && !controller_->state().busy && !controller_->state().readOnly);
    }
    if (!selected.isEmpty()) selectProfile(selected);
    if (profiles_) {
        if (auto* label = profiles_->findChild<QLabel*>("emptyState"))
            label->setVisible(profiles_->model()->rowCount() == 0);
        setEmptyStateAction(profiles_, {}, {});
        QPointer<QListView> list = profiles_;
        QTimer::singleShot(0, this, [list, scrollPosition] {
            if (list) list->verticalScrollBar()->setValue(scrollPosition);
        });
    }
    if (!profileDetailId_.isEmpty()) {
        bool found = false;
        for (const auto& profile : controller_->profiles()) if (profile.id == profileDetailId_) found = true;
        if (!found) { profileDetailId_.clear(); returnFromWorkspace(); }
    }    // 行内「当前值」（V3）：只更新已有 settingValue 的行；控件本身就显示值的行不建这个标签。
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
    if (contentStack_ && contentStack_->currentIndex() > 0 && !profileDetailId_.isEmpty())
        return profileDetailId_;
    if (profiles_ && profiles_->currentIndex().isValid())
        return profiles_->currentIndex().data(ProfileRowModel::ProfileId).toString();
    return {};
}
namespace {
struct ImportSource {
    QString value;
    bool remote = false;
};
ImportSource classifyImport(const QString& input) {
    auto text=input.trimmed();
    if (text.size()>4096 || text.contains('\n') || text.contains('\r')) return {};
    if (text.size()>1 && text.startsWith('"') && text.endsWith('"')) text=text.mid(1,text.size()-2);
    const QUrl url(text);
    if (ImportDialog::acceptsUrl(url)) return {text,true};
    const auto path=url.isLocalFile()?url.toLocalFile():text;
    if (QFileInfo(path).isAbsolute() && QStringList{"yaml","yml"}.contains(QFileInfo(path).suffix().toLower()))
        return {path,false};
    return {};
}
bool isNodeImport(const QString& value) {
    const auto scheme=value.section(':',0,0).toLower();
    return value.size()<=64*1024 && scheme!="http" && scheme!="https" && NodeLinks::acceptsScheme(scheme);
}
}
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
            "节点链接请从「高级新增」检查后导入":"请输入 HTTP/HTTPS 订阅地址或本地 YAML 文件的完整路径");
        return;
    }
    quickImportPending_=true;
    setPageFeedback(ViewIds::Profiles,{});
    if (source.remote) controller_->importUrl({},QUrl(source.value));
    else controller_->importFile({},source.value);
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
        setPageFeedback(ViewIds::Profiles,"仅支持一个 HTTP/HTTPS 订阅或本地 YAML 文件");
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

void MainWindow::showProfileBatch(bool start) {
    if (auto* existing=findChild<QDialog*>("profileBatchDialog")) { existing->show(); existing->raise(); return; }
    QStringList ids; const auto selected=selectedProfileIds();
    if (selected.size()>1) ids=selected; else for (const auto& p:controller_->profiles()) ids.append(p.id);
    const auto before=ids.size(); ids.removeIf([this](const QString& id) { for (const auto& profile:controller_->profiles()) if (profile.id==id) return profile.source.isEmpty(); return true; }); const auto local=before-ids.size();
    auto* dialog=new TaskDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("profileBatchDialog"); dialog->setWindowTitle("批量更新订阅"); dialog->resize(600,400); auto* box=new QVBoxLayout(dialog);
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
    connect(controller_,&AppController::profileBatchChanged,dialog,refresh); connect(controller_,&AppController::stateChanged,dialog,refresh); connect(run,&QPushButton::clicked,dialog,[this,ids] { controller_->updateProfilesBatch(ids); }); connect(cancel,&QPushButton::clicked,controller_,&AppController::cancelProfileBatch); connect(close,&QPushButton::clicked,dialog,&QDialog::close); presentTask(dialog); refresh(); if (start&&run->isEnabled()) run->click();
}

void MainWindow::presentImport(ImportDialog* dialog) {
    QSet<QString> before; for (const auto& profile:controller_->profiles()) before.insert(profile.id);
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->bindController(controller_);
    // 导入成功后在统一卡片网格中定位新对象，不以来源类型改变落点。
    connect(dialog,&QDialog::accepted,this,[this,before] { navigate(Route{ViewIds::Profiles}); if (route_.view!=ViewIds::Profiles) return; for (const auto& profile:controller_->profiles()) if (!before.contains(profile.id)) { selectProfile(profile.id); break; } });
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
    const auto text=quickImport_->text().trimmed();
    const auto source=classifyImport(text);
    if (!source.value.isEmpty()) dialog->setPrefill(source.value,source.remote);
    else if (isNodeImport(text)) dialog->setNodeLinks(text);
    presentImport(dialog);
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
    DialogDraftGuard::protect(dialog,[automatic,hours,download,profile,name,source] { return name->text()!=profile.name || source->text()!=profile.source || automatic->isChecked()!=profile.autoUpdate || (hours && hours->value()!=profile.updateIntervalHours) || download->policy().toJson()!=profile.download.toJson(); },[saving] { return *saving; });
    if (rollback) {
        auto rolling=std::make_shared<bool>(false);
        connect(rollback,&QPushButton::clicked,dialog,[this,dialog,rollback,rolling,id,profile,name,source,automatic,hours,download] {
            const bool dirty=name->text()!=profile.name || source->text()!=profile.source ||
                automatic->isChecked()!=profile.autoUpdate ||
                (hours && hours->value()!=profile.updateIntervalHours) ||
                download->policy().toJson()!=profile.download.toJson();
            const QString message=dirty
                ? "未保存的来源修改将被放弃。确定回滚此配置到上次良好版本？"
                : "确定回滚此配置到上次良好版本？";
            if (QMessageBox::question(dialog,"回滚配置",message,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
            *rolling=true; rollback->setEnabled(false); controller_->activateProfile(id,true);
        });
        connect(controller_,&AppController::operationFinished,dialog,[dialog,rollback,rolling,note](bool ok,const QString& error) {
            if (!*rolling) return;
            *rolling=false;
            if (ok) dialog->accept(); else { rollback->setEnabled(true); note->setText("回滚失败，运行版本未变 · "+error); }
        });
    }
    connect(controller_,&AppController::operationFinished,dialog,[dialog,buttons,note,saving](bool ok,const QString& error) { if (!*saving) return; *saving=false; buttons->setEnabled(true); if (ok) dialog->accept(); else note->setText("保存失败，修改已保留 · "+error); });
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, id, automatic, hours, download, buttons, saving, name, source, expected, note, interval = profile.updateIntervalHours] { if (!download->policy().valid()) { note->setText("User-Agent 须为最多 256 个单行 ASCII 字符"); return; } *saving=true; buttons->setEnabled(false); controller_->editProfileSource(id,name->text(),source->text(),automatic->isChecked(),hours?hours->value():interval,download->policy(),expected); }); presentTask(dialog);
}

}
