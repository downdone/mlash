#include "ProxyNodesPage.h"

#include <QtWidgets>
#include "WindowUiFactories.h"

namespace mlash {

ProxyNodesPage::ProxyNodesPage(QWidget* parent) : QWidget(parent) {
    setObjectName("proxyNodesPage");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    empty_ = new QWidget(this);
    empty_->setObjectName("proxyNodesSetup");
    auto* setupLayout = new QVBoxLayout(empty_);
    setupLayout->setContentsMargins(14, 12, 14, 0);
    setupLayout->setSpacing(6);
    title_ = new QLabel(empty_);
    title_->setObjectName("proxyNodesSetupTitle");
    title_->setWordWrap(true);
    setupLayout->addWidget(title_);
    hint_ = new QLabel(empty_);
    hint_->setProperty("secondary", true);
    hint_->setWordWrap(true);
    setupLayout->addWidget(hint_);
    setup_ = new QPushButton(empty_);
    setup_->setObjectName("proxyNodesSetupAction");
    setup_->setProperty("primary", true);
    setupLayout->addWidget(setup_, 0, Qt::AlignLeft);
    layout->addWidget(empty_);

    nodesHost_ = new QWidget(this);
    nodesLayout_ = new QVBoxLayout(nodesHost_);
    nodesLayout_->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(nodesHost_, 1);
    connect(setup_, &QPushButton::clicked, this, &ProxyNodesPage::profileSetupRequested);
}

void ProxyNodesPage::attachNodes(QWidget* nodes) {
    if (nodes->parentWidget() != nodesHost_) nodesLayout_->addWidget(nodes);
}

void ProxyNodesPage::setAvailability(bool hasActive, bool hasProfiles, bool canChooseProfile) {
    const auto copy = WindowParts::profileSetupCopy(hasProfiles);
    title_->setText(copy.title);
    hint_->setText(hasProfiles ? "选择一份代理配置后即可查看节点。" : "导入代理订阅或 YAML 文件后即可查看节点。");
    setup_->setText(copy.action);
    setup_->setEnabled(canChooseProfile);
    empty_->setVisible(!hasActive);
}

} // namespace mlash
