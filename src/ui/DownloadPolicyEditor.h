#pragma once
#include "app/Types.h"
#include "CompactComboBox.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

namespace mlash {
class DownloadPolicyEditor final : public QWidget {
public:
    explicit DownloadPolicyEditor(const DownloadPolicy& policy = {}, QWidget* parent = nullptr) : QWidget(parent) {
        auto* form = new QFormLayout(this); form->setContentsMargins(0, 0, 0, 0);
        route_ = new CompactComboBox; route_->setObjectName("downloadRoute"); route_->addItem("直连", "direct"); route_->addItem("经本地代理", "core"); route_->setCurrentIndex(route_->findData(policy.route));
        route_->setToolTip("经本地代理需要内核已就绪；不会自动回退到其他路径。DNS 在本机检查并固定目标地址。"); form->addRow("下载路径", route_);
        agent_ = new QLineEdit(policy.userAgent); agent_->setObjectName("downloadUserAgent"); agent_->setMaxLength(256); agent_->setPlaceholderText("默认 Mlash User-Agent"); form->addRow("User-Agent", agent_);
        timeout_ = new QSpinBox; timeout_->setObjectName("downloadTimeout"); timeout_->setButtonSymbols(QAbstractSpinBox::NoButtons); timeout_->setRange(5, 120); timeout_->setSuffix(" 秒"); timeout_->setValue(policy.timeoutSeconds); form->addRow("总超时", timeout_);
    }
    DownloadPolicy policy() const { DownloadPolicy policy; policy.route = route_->currentData().toString(); policy.userAgent = agent_->text(); policy.timeoutSeconds = timeout_->value(); return policy; }
    QLineEdit* userAgentEditor() const { return agent_; }
private:
    QComboBox* route_;
    QLineEdit* agent_;
    QSpinBox* timeout_;
};
}
