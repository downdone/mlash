#include "ProfileCardWidget.h"
#include "ElidedLabel.h"
#include "ProfileRowModel.h"
#include "SummaryCardStyle.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace mlash {
ProfileCardWidget::ProfileCardWidget(QWidget* parent) : QFrame(parent) {
    setObjectName("profileCard");
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(CellInset+HorizontalInset,CellInset+VerticalInset,
                               CellInset+HorizontalInset,CellInset+VerticalInset);
    layout->setSpacing(9);

    auto* header = new QHBoxLayout;
    constexpr int identityGap=10;
    header->setSpacing(identityGap);
    mark_ = new QLabel;
    mark_->setObjectName("profileCardMark");
    mark_->setFixedSize(SummaryCardStyle::MarkSize,SummaryCardStyle::MarkSize);
    mark_->setAlignment(Qt::AlignCenter);
    header->addWidget(mark_);
    title_ = new ElidedLabel;
    title_->setObjectName("profileCardTitle");
    title_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    header->addWidget(title_,1);
    auto* tools = new QHBoxLayout;
    tools->setSpacing(4);
    header->addLayout(tools);
    const auto tool = [this,tools](const QString& name, Action action) {
        auto* control = new QToolButton;
        control->setProperty("profileCardIcon",true);
        control->setFixedSize(28,28);
        control->setIconSize(QSize(18,18));
        control->setFocusPolicy(Qt::StrongFocus);
        control->setToolTip(name);
        control->setAccessibleName(name);
        tools->addWidget(control,0,Qt::AlignVCenter);
        connect(control,&QToolButton::clicked,this,[this,action] { emit actionRequested(action); });
        return control;
    };
    refresh_ = tool("检查更新",Action::Refresh);
    menu_ = tool("配置操作",Action::Menu);
    layout->addLayout(header);

    auto* metadata = new QHBoxLayout;
    metadata->setContentsMargins(SummaryCardStyle::MarkSize+identityGap,0,0,0);
    metadata->setSpacing(12);
    source_ = new ElidedLabel;
    source_->setObjectName("profileCardSource");
    source_->setProperty("caption",true);
    source_->setProperty("secondary",true);
    metadata->addWidget(source_);
    time_ = new QLabel;
    time_->setObjectName("profileCardTime");
    time_->setProperty("caption",true);
    time_->setProperty("secondary",true);
    time_->setAccessibleName("最后更新时间");
    metadata->addWidget(time_);
    metadata->addStretch();
    layout->addLayout(metadata);
    layout->addSpacing(6);
    layout->addStretch();

    auto* footer = new QHBoxLayout;
    footer->setSpacing(8);
    status_ = new QLabel;
    status_->setObjectName("profileCardStatus");
    footer->addWidget(status_,0,Qt::AlignVCenter);
    usage_ = new ElidedLabel;
    usage_->setProperty("caption",true);
    usage_->setProperty("secondary",true);
    usage_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    footer->addWidget(usage_,1);
    const auto command = [this,footer](const QString& name, Action action) {
        auto* control = new QPushButton(name);
        control->setAutoDefault(false);
        control->setProperty("profileCardAction",true);
        control->setProperty("primary",action==Action::Apply || action==Action::Use);
        footer->addWidget(control);
        connect(control,&QPushButton::clicked,this,[this,action] { emit actionRequested(action); });
        return control;
    };
    review_ = command("查看变化",Action::Review);
    apply_ = command("应用更新",Action::Apply);
    use_ = command("使用",Action::Use);
    layout->addLayout(footer);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,&ProfileCardWidget::updateIcons);
}

void ProfileCardWidget::setProfile(const QModelIndex& index) {
    remote_ = index.data(ProfileRowModel::Kind).toString() == "remote";
    title_->setText(index.data(Qt::DisplayRole).toString());
    source_->setText(index.data(ProfileRowModel::SourceLabel).toString());
    time_->setText(index.data(ProfileRowModel::UpdatedLabel).toString());
    const bool candidate = index.data(ProfileRowModel::HasCandidate).toBool();
    const bool current = index.data(ProfileRowModel::IsCurrent).toBool();
    const bool failed = !index.data(ProfileRowModel::UpdateError).toString().isEmpty();
    status_->setText(failed?"更新失败":candidate?"待应用":current?"当前使用":"未使用");
    const QString tone = failed?"error":candidate?"warning":current?"current":"idle";
    if (status_->property("tone") != tone) {
        status_->setProperty("tone",tone);
        status_->style()->unpolish(status_);
        status_->style()->polish(status_);
    }
    usage_->setText(candidate?QString():index.data(ProfileRowModel::UsageText).toString());
    refresh_->setVisible(index.data(ProfileRowModel::CanRefresh).toBool());
    review_->setVisible(candidate);
    apply_->setVisible(candidate);
    use_->setVisible(!candidate && !current);
    setAccessibleName(title_->text());
    setAccessibleDescription(index.data(ProfileRowModel::Detail).toString());
    updateIcons();
}

void ProfileCardWidget::updateIcons() {
    using G = UiIcons::Glyph;
    mark_->setPixmap(UiIcons::icon(remote_?G::Globe:G::Folder,UiTheme::Secondary).pixmap(20,20));
    refresh_->setIcon(UiIcons::icon(G::Refresh,UiTheme::Secondary));
    menu_->setIcon(UiIcons::icon(G::More,UiTheme::Secondary));
}

void ProfileCardWidget::setSelected(bool selected) {
    if (property("selected").toBool() == selected) return;
    setProperty("selected",selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void ProfileCardWidget::setWritable(bool writable) {
    refresh_->setEnabled(writable);
    apply_->setEnabled(writable);
    use_->setEnabled(writable);
}

QAbstractButton* ProfileCardWidget::button(Action action) const {
    switch (action) {
    case Action::Refresh: return refresh_;
    case Action::Menu: return menu_;
    case Action::Review: return review_;
    case Action::Apply: return apply_;
    case Action::Use: return use_;
    }
    return nullptr;
}
}
