#include "ImportDialog.h"
#include "DownloadPolicyEditor.h"
#include "app/AppController.h"
#include "ElidedLabel.h"
#include "NodeLinksEditor.h"
#include "DialogDraftGuard.h"
#include <QTabWidget>
#include <QTabBar>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QUrl>
#include <QScrollArea>

namespace mlash {
namespace {
// Hidden source forms must not dictate the current source's height.
class ImportSourceTabs final : public QTabWidget {
public:
    ImportSourceTabs() { connect(this,&QTabWidget::currentChanged,this,[this] { fitCurrent(); }); }
    void fitCurrent() {
        for (int i=0;i<count();++i) widget(i)->setSizePolicy(QSizePolicy::Ignored,i==currentIndex()?QSizePolicy::Preferred:QSizePolicy::Ignored);
        updateGeometry();
    }
    QSize sizeHint() const override { return sourceSize(false); }
    QSize minimumSizeHint() const override { return sourceSize(true); }
private:
    QSize sourceSize(bool minimum) const {
        const auto* page=currentWidget(); const auto content=page?(minimum?page->minimumSizeHint():page->sizeHint()):QSize();
        const auto tabs=tabBar()->sizeHint(); return {qMax(content.width()+8,tabs.width()),qMax(0,content.height())+tabs.height()+8};
    }
};
}
ImportDialog::ImportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("添加订阅 · Mlash"); setObjectName("importDialog"); resize(520, 460); setMinimumSize(420,340);
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(16,16,16,14); outer->setSpacing(10);
    auto* title = new QLabel("添加订阅"); title->setObjectName("pageTitle"); outer->addWidget(title);
    auto* scroll=new QScrollArea; scroll->setObjectName("importScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); auto* body=new QWidget; auto* layout=new QVBoxLayout(body); layout->setContentsMargins(0,0,0,0); layout->setSpacing(10); scroll->setWidget(body); outer->addWidget(scroll,1);
    tabs_ = new ImportSourceTabs; tabs_->setObjectName("importTabs");
    auto* urlPage = new QWidget; urlPage->setObjectName("importSource"); urlPage->setAttribute(Qt::WA_StyledBackground, true); auto* urlForm = new QVBoxLayout(urlPage);
    urlForm->addWidget(new QLabel("订阅地址")); url_ = new QLineEdit; url_->setObjectName("subscriptionUrl");
    url_->setPlaceholderText("https://example.com/subscription"); url_->setClearButtonEnabled(true); url_->setEchoMode(QLineEdit::Password);
    urlForm->addWidget(url_); auto* show = new QCheckBox("显示地址"); urlForm->addWidget(show);
    connect(show, &QCheckBox::toggled, this, [this](bool visible) { url_->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password); });
    auto* filePage = new QWidget; filePage->setObjectName("importSource"); filePage->setAttribute(Qt::WA_StyledBackground, true); auto* fileLayout = new QVBoxLayout(filePage); fileLayout->addWidget(new QLabel("配置文件"));
    auto* fileRow = new QHBoxLayout; file_ = new QLineEdit; file_->setObjectName("subscriptionFile"); file_->setPlaceholderText("选择完整 YAML 配置文件"); file_->setClearButtonEnabled(true);
    fileRow->addWidget(file_, 1); auto* browse = new QPushButton("浏览…"); browse->setAutoDefault(false); fileRow->addWidget(browse); fileLayout->addLayout(fileRow);
    connect(browse, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getOpenFileName(this, "选择订阅配置", {}, "YAML (*.yaml *.yml)"); if (!path.isEmpty()) file_->setText(path); });
    urlForm->addStretch(1); fileLayout->addStretch(1); tabs_->addTab(urlPage, "订阅链接"); tabs_->addTab(filePage, "配置文件"); links_=new NodeLinksEditor(nullptr); tabs_->addTab(links_,"节点链接"); static_cast<ImportSourceTabs*>(tabs_)->fitCurrent(); layout->addWidget(tabs_,0,Qt::AlignTop);
    auto* form = new QFormLayout; name_ = new QLineEdit; name_->setObjectName("subscriptionName"); name_->setMaxLength(120); name_->setPlaceholderText("可选，留空使用默认名称"); form->addRow("名称", name_); layout->addLayout(form);
    auto* advanced = new QPushButton("下载设置"); advanced->setCheckable(true); advanced->setAutoDefault(false); layout->addWidget(advanced, 0, Qt::AlignLeft);
    download_ = new DownloadPolicyEditor; download_->hide(); layout->addWidget(download_); connect(advanced, &QPushButton::toggled, download_, &QWidget::setVisible);
    connect(tabs_, &QTabWidget::currentChanged, this, [this, advanced](int index) { advanced->setVisible(index == 0); download_->setVisible(index == 0 && advanced->isChecked()); });
    connect(download_->userAgentEditor(), &QLineEdit::textChanged, this, &ImportDialog::inputChanged);
    hint_ = new ElidedLabel; hint_->setObjectName("importHint"); hint_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); hint_->setMinimumHeight(hint_->fontMetrics().height()+6); outer->addWidget(hint_);
    auto* note = new QLabel("导入后需手动使用。"); note->setToolTip("端口、控制接口和 TUN 由 Mlash 设置管理；仅监听本机。"); note->setObjectName("secondaryText"); note->setWordWrap(true); layout->addWidget(note);
    layout->addStretch(1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    import_ = buttons->addButton("导入并校验", QDialogButtonBox::AcceptRole); import_->setObjectName("importSubmit"); import_->setProperty("primary", true); import_->setDefault(true); outer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(tabs_, &QTabWidget::currentChanged, this, &ImportDialog::inputChanged);
    connect(links_,&NodeLinksEditor::changed,this,&ImportDialog::inputChanged);
    connect(url_, &QLineEdit::textChanged, this, &ImportDialog::inputChanged); connect(file_, &QLineEdit::textChanged, this, &ImportDialog::inputChanged);
    DialogDraftGuard::protect(this,[this] { return isNodeLinks()&&!pending_&&!links_->text().trimmed().isEmpty(); });
    validateInput();
}
bool ImportDialog::isUrl() const { return tabs_->currentIndex() == 0; }
bool ImportDialog::isNodeLinks() const { return tabs_->currentIndex()==2; }
void ImportDialog::bindController(AppController* controller) {
    if (controller_) return;
    controller_=controller;
    links_->bindController(controller);
    const auto update=[this] {
        setImportAvailable(!controller_->state().busy&&!controller_->state().exiting&&!controller_->state().readOnly);
    };
    connect(controller_,&AppController::stateChanged,this,update);
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& message) {
        if (!pending_||controller_->state().busy) return;
        pending_=false;
        tabs_->setEnabled(true); name_->setEnabled(true); download_->setEnabled(true);
        auto* close=findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel);
        close->setText("取消"); close->setToolTip({});
        if (ok) { QDialog::accept(); return; }
        failure_=message; validateInput();
    });
    update();
}
void ImportDialog::accept() {
    if (!controller_) { QDialog::accept(); return; }
    if (!import_->isEnabled()||pending_) return;
    pending_=true; failure_.clear();
    auto* close=findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel);
    close->setText("关闭"); close->setToolTip("关闭窗口后，已提交的导入仍继续；结果显示在状态栏。");
    tabs_->setEnabled(false); name_->setEnabled(false); download_->setEnabled(false);
    import_->setEnabled(false); hint_->setText("正在导入并校验；关闭此窗口后可在状态栏查看结果。");
    if (isNodeLinks()) controller_->importNodeLinks(profileName(),source());
    else if (isUrl()) controller_->importUrl(profileName(),QUrl(source()),downloadPolicy());
    else controller_->importFile(profileName(),source());
}
QString ImportDialog::source() const { return isNodeLinks()?links_->text():(isUrl() ? url_ : file_)->text().trimmed(); }
QString ImportDialog::profileName() const { return name_->text().trimmed(); }
DownloadPolicy ImportDialog::downloadPolicy() const { return download_->policy(); }
void ImportDialog::setSourceTab(bool url) { tabs_->setCurrentIndex(url ? 0 : 1); }
void ImportDialog::setPrefill(const QString& source,bool url) { setSourceTab(url); (url?url_:file_)->setText(source); }
void ImportDialog::setNodeLinks(const QString& text) { tabs_->setCurrentIndex(2); links_->setText(text); }
void ImportDialog::setProtocolSource(const QString& url,const QString& name) { setWindowTitle("确认链接订阅 · Mlash"); setSourceTab(true); url_->setText(url); name_->setText(name); auto* origin=new QLabel("网站请求添加来自 "+QUrl(url).host()+" 的订阅；只有点击“导入并校验”才会下载，不会自动启动或切换订阅。"); origin->setTextFormat(Qt::PlainText); origin->setWordWrap(true); qobject_cast<QVBoxLayout*>(tabs_->parentWidget()->layout())->insertWidget(0,origin); }
void ImportDialog::validateInput() {
    bool valid = !source().isEmpty(); QString hint;
    if (isNodeLinks()) { valid=links_->ready(); hint=valid?"检查已完成；导入后创建本地订阅快照，仍需明确使用。":"请先检查链接并确认节点预览。"; }
    else if (isUrl()) {
        const QUrl url(source());
        valid = valid && url.isValid() && QStringList{"http", "https"}.contains(url.scheme()) && !url.host().isEmpty() && url.userInfo().isEmpty() && !url.hasFragment();
        hint = valid||source().isEmpty()?QString():QString("地址无效：需 HTTP/HTTPS，且不含账户信息或片段。");
    } else hint.clear();
    if (isUrl() && !download_->policy().valid()) { valid = false; hint = "User-Agent 仅支持最多 256 个单行 ASCII 字符"; }
    if (!pending_) {
        hint_->setText(failure_.isEmpty()?hint:failure_);
        hint_->setToolTip(failure_); hint_->setAccessibleDescription(failure_);
    }
    import_->setEnabled(valid&&available_&&!pending_);
}
}
