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
namespace {
class ProfileImportInput final : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
protected:
    void resizeEvent(QResizeEvent* event) override {
        QLineEdit::resizeEvent(event);
        updatePrompt();
    }
    void changeEvent(QEvent* event) override {
        QLineEdit::changeEvent(event);
        if (event->type()==QEvent::FontChange) updatePrompt();
    }
private:
    void updatePrompt() {
        const auto full=QStringLiteral("粘贴订阅 URL，或拖拽 / 选择 YAML、CONF");
        setPlaceholderText(width()>=fontMetrics().horizontalAdvance(full)+36
            ? full : QStringLiteral("订阅 URL 或配置文件"));
    }
};

class ProfileCardCell final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool setColumnSizes(int width,QList<int> heights) {
        if (width_==width && heights_==heights) return false;
        width_=width;
        heights_=std::move(heights);
        return true;
    }
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex& index) const override {
        const auto* list=qobject_cast<const QListView*>(parent());
        if (list && list->gridSize().isValid()) return list->gridSize();
        return {width_,heights_.value(index.row(),ProfileCardWidget::CellHeight)};
    }
    void paint(QPainter*,const QStyleOptionViewItem&,const QModelIndex&) const override {}
private:
    int width_=320;
    QList<int> heights_;
};
}

// The subscription surface follows ADR 0057: URL and file profiles share one
// ordered card model. Card body, refresh, candidate and menu have distinct hits.
QWidget* MainWindow::profilesPage() {
    using namespace PrototypeUi;
    auto* page = new QWidget;
    page->setObjectName("profilesSurface");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8,12,8,18);
    layout->setSpacing(10);

    auto* quick = new QWidget(page);
    quick->setObjectName("profileQuickImport");
    quick->setAttribute(Qt::WA_StyledBackground);
    auto* quickRow = new QHBoxLayout(quick);
    quickRow->setContentsMargins(12,8,12,8);
    quickRow->setSpacing(8);
    auto* field = new QWidget(quick);
    field->setObjectName("profileImportField");
    field->setAttribute(Qt::WA_StyledBackground);
    auto* fieldRow = new QHBoxLayout(field);
    fieldRow->setContentsMargins(9,0,6,0);
    fieldRow->setSpacing(6);
    auto* link = new QLabel(field);
    link->setObjectName("profileImportLink");
    link->setFixedSize(18,18);
    fieldRow->addWidget(link);
    quickImport_ = new ProfileImportInput(field);
    quickImport_->setObjectName("profileImportInput");
    quickImport_->setAccessibleName("订阅地址或 YAML 文件路径");
    quickImport_->setPlaceholderText("订阅 URL 或 YAML 文件");
    quickImport_->setMaxLength(4096);
    quickImport_->setClearButtonEnabled(true);
    quickImport_->setAcceptDrops(false);
    fieldRow->addWidget(quickImport_,1);
    auto* browse = new QToolButton(field);
    browse->setObjectName("profileBrowseImport");
    browse->setToolTip("选择 YAML 或 WireGuard CONF 文件");
    browse->setAccessibleName(browse->toolTip());
    browse->setIconSize(QSize(16,16));
    browse->setFocusPolicy(Qt::StrongFocus);
    browse->setProperty("quiet",true);
    fieldRow->addWidget(browse);
    quickRow->addWidget(field,1);
    auto* submit = new QPushButton("导入",quick);
    submit->setObjectName("profileQuickSubmit");
    submit->setProperty("primary",true);
    quickRow->addWidget(submit);
    auto* advanced = new QPushButton("高级",quick);
    advanced->setObjectName("importSubscription");
    advanced->setAccessibleName("高级新增配置");
    advanced->setLayoutDirection(Qt::RightToLeft);
    advanced->setIconSize(QSize(14,14));
    const auto updateIcons=[link,browse,advanced] {
        link->setPixmap(UiIcons::icon(UiIcons::Glyph::Link,UiTheme::Brand).pixmap(18,18));
        browse->setIcon(UiIcons::icon(UiIcons::Glyph::Folder,UiTheme::Secondary));
        advanced->setIcon(UiIcons::icon(UiIcons::Glyph::ChevronRight,UiTheme::Secondary));
    };
    updateIcons();
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,field,updateIcons);
    connect(qApp,&QApplication::focusChanged,field,[field,input=quickImport_](QWidget* previous,QWidget* next) {
        if (previous!=input && next!=input) return;
        field->setProperty("editing",next==input);
        field->style()->unpolish(field);
        field->style()->polish(field);
        field->update();
    });
    quickRow->addWidget(advanced);
    layout->addWidget(quick);
    auto* submitHint = new ElidedLabel;
    submitHint->setObjectName("profileSubmitHint");
    submitHint->setProperty("secondary",true);
    submitHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    submitHint->setAccessibleName("订阅操作结果");
    submitHint->hide();
    layout->addWidget(submitHint);
    layout->addSpacing(8);

    connect(browse,&QToolButton::clicked,this,[this] {
        const auto path=QFileDialog::getOpenFileName(this,"选择配置文件",{},"配置文件 (*.yaml *.yml *.conf);;所有文件 (*)");
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
    connect(controller_,&AppController::operationFinished,quick,[this,syncImport](bool ok,const QString& notice,OperationId operation) {
        if (!quickImportPending_ || operation!=OperationId::ProfileImport) return;
        quickImportPending_=false;
        if (ok) {
            quickImport_->clear();
            if (notice.startsWith("WireGuard")&&!controller_->wireGuardProfiles().isEmpty())
                selectProfile(controller_->wireGuardProfiles().back().id);
            else if (!controller_->profiles().isEmpty()) selectProfile(controller_->profiles().back().id);
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
    connect(batch, &QPushButton::clicked, this, &MainWindow::showProfileBatch);
    connect(controller_, &AppController::profileBatchChanged, this, &MainWindow::updateProfileBatchAction);

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
    profiles_->setAccessibleDescription("方向键选择卡片，回车打开配置内容，菜单键打开操作，F5 检查更新，Ctrl+Shift+V 或 Ctrl+Shift+A 查看待应用更新");
    layout->addWidget(profiles_, 1);
    emptyState(profiles_, profileSetupCopy(false).title, true);
    wireProfileList(profiles_);
    connect(controller_,&AppController::wireGuardChanged,profiles_,[this] {
        profileRowsModel_->setWireGuardState(controller_->runningWireGuardProfile());
    });
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,&MainWindow::updateProfileGrid);
    updateProfileGrid();
    return page;
}

void MainWindow::updateProfileGrid() {
    if (!profiles_) return;
    // Reserve scrollbar space in both states so the column width does not
    // change when adding a card makes the list start scrolling.
    const int scrollbar=profiles_->style()->pixelMetric(QStyle::PM_ScrollBarExtent,nullptr,profiles_);
    const int available=qMax(1,profiles_->maximumViewportSize().width()-scrollbar-2);
    QList<ProfileCardWidget*> cards;
    for (int row=0;row<profiles_->model()->rowCount();++row) {
        auto* card=qobject_cast<ProfileCardWidget*>(profiles_->indexWidget(profiles_->model()->index(row,0)));
        if (card) cards.append(card);
    }
    const auto computeExtra=[&cards](int width) {
        int extra=0;
        for (auto* card:cards) extra=qMax(extra,card->extraTextHeight(width));
        return extra;
    };
    const int minimumCellWidth=288;
    int width=available>=2*minimumCellWidth?available/2:available;
    // Prefer a full-width column when wrapping would make every card tall.
    if (width!=available && (computeExtra(width)>72 ||
        std::any_of(cards.cbegin(),cards.cend(),[width](const auto* card) {
            return width<card->minimumWidthForMetadata();
        }))) width=available;
    const bool single=width==available;
    const int base=ProfileCardWidget::cellHeightForWidth(width);
    int height=base;
    QList<int> columnHeights;
    for (auto* card:cards) {
        const int cardHeight=qMax(base,card->fitText(width));
        height=qMax(height,cardHeight);
        if (single) columnHeights.append(cardHeight);
    }
    const auto mode=single?QListView::ListMode:QListView::IconMode;
    if (profiles_->viewMode()!=mode) {
        profiles_->setViewMode(mode);
        profiles_->setFlow(single?QListView::TopToBottom:QListView::LeftToRight);
        profiles_->setWrapping(!single);
        profiles_->setUniformItemSizes(!single);
    }
    const QSize size=single?QSize():QSize(width,height);
    if (profiles_->gridSize() != size) profiles_->setGridSize(size);
    auto* delegate=static_cast<ProfileCardCell*>(profiles_->itemDelegate());
    if (delegate->setColumnSizes(width,std::move(columnHeights)) && single)
        QMetaObject::invokeMethod(profiles_,&QListView::doItemsLayout,Qt::QueuedConnection);
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
void MainWindow::openProfileSetup() {
    if (controller_->profiles().isEmpty()) {
        addUrl();
        return;
    }
    navigate(Route{ViewIds::Profiles});
    if (!profiles_->isVisible() || profiles_->model()->rowCount()==0) return;
    auto index = profiles_->currentIndex();
    if (!index.isValid()) index = profiles_->model()->index(0,0);
    selectProfile(profileAt(index));
    auto* card = qobject_cast<ProfileCardWidget*>(profiles_->indexWidget(index));
    if (card) {
        const auto action = index.data(ProfileRowModel::HasCandidate).toBool()
            ? ProfileCardWidget::Action::ReviewUpdate : ProfileCardWidget::Action::Menu;
        auto* button = card->button(action);
        if (button->isVisible() && button->isEnabled()) {
            button->setFocus(Qt::ShortcutFocusReason);
            return;
        }
    }
    profiles_->setFocus(Qt::ShortcutFocusReason);
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
                case Action::ReviewUpdate:
                    if (index.data(ProfileRowModel::HasCandidate).toBool()) openProfileTask(id,"candidate");
                    break;
                case Action::Open:
                    openProfileTask(id,"nodes");
                    break;
                case Action::Refresh:
                    if (writable() && index.data(ProfileRowModel::CanRefresh).toBool()) controller_->updateProfile(id);
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
    connect(controller_,&AppController::stateChanged,list,[this] {
        const auto& state=controller_->state();
        profileRowsModel_->setState(state.activeProfile,state.ready);
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
    for (const auto key : {Qt::Key_V,Qt::Key_A}) {
        auto* review = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | key),list);
        review->setContext(Qt::WidgetWithChildrenShortcut);
        connect(review,&QShortcut::activated,this,[this,list] {
            const auto index = list->currentIndex();
            if (index.isValid() && index.data(ProfileRowModel::HasCandidate).toBool())
                openProfileTask(profileAt(index),"candidate");
        });
    }
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
        profileRowsModel_->setProfiles(controller_->profiles(),controller_->wireGuardProfiles());
        profileRowsModel_->setState(controller_->state().activeProfile, controller_->state().ready);
        profileRowsModel_->setWireGuardState(controller_->runningWireGuardProfile());
        updateProfileGrid();
    }
    if (auto* count=findChild<QLabel*>("profileCollectionCount"))
        count->setText(QString("%1 个").arg(controller_->profiles().size()+controller_->wireGuardProfiles().size()));
    if (!selected.isEmpty()) selectProfile(selected);
    updateProfileBatchAction();
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
        const auto matches=[this](const auto& profile) { return profile.id==profileDetailId_; };
        const auto& mihomo=controller_->profiles();
        const auto& wireGuard=controller_->wireGuardProfiles();
        const bool found=std::any_of(mihomo.cbegin(),mihomo.cend(),matches)||std::any_of(wireGuard.cbegin(),wireGuard.cend(),matches);
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
}