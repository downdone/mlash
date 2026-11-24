#include "ProfileCardWidget.h"
#include "ProfileRowModel.h"
#include "StatusShape.h"
#include "SummaryCardStyle.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "WrapLayout.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace mlash {
namespace {
constexpr int actionGap = 6;
constexpr int actionPadding = 22;
constexpr int toolSize = 30;
constexpr int toolGap = 10;
constexpr int markSize = 40;
constexpr int markGap = 16;
constexpr int toolInset = 8;
constexpr int identityInset = markSize+markGap;
constexpr int metadataGap = 4;
constexpr int stateInset = 6;
constexpr int stateVerticalInset = 2;
constexpr int stateGap = 3;
const QString reviewUpdateLabel = QStringLiteral("查看更新…");

QFont captionFont(bool emphasized = false) {
    auto font = UiTheme::rowCaptionFont(UiTheme::bodyFont());
    if (emphasized) font.setWeight(QFont::DemiBold);
    return font;
}

bool needsStackedFooter(int width) {
    const QFontMetrics regular(captionFont()), emphasized(captionFont(true));
    const int actionWidth = emphasized.horizontalAdvance(reviewUpdateLabel) + actionPadding;
    const int statusWidth = identityInset + regular.horizontalAdvance(QStringLiteral("当前使用")) + 17 + 2*stateInset;
    const int insets = 2*(ProfileCardWidget::CellInset+ProfileCardWidget::HorizontalInset);
    return width < actionWidth + statusWidth + insets + 2*actionGap + 6;
}
}

class ProfileCardText final : public QLabel {
public:
    ProfileCardText() { setTextFormat(Qt::PlainText); setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed); }

    int heightForWidth(int width) const override {
        QTextLayout layout(displayText(),font(),this);
        return arrange(layout,width);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        QTextLayout layout(displayText(),font(),this);
        const auto content = contentsRect();
        const auto height = arrange(layout,content.width());
        layout.draw(&painter,content.topLeft()+QPoint(0,qMax(0,(content.height()-height)/2)));
    }

private:
    QString displayText() const {
        return QString(text()).replace(QStringLiteral("\r\n"),QStringLiteral("\n"))
            .replace(u'\r',QChar::LineSeparator).replace(u'\n',QChar::LineSeparator);
    }

    static int arrange(QTextLayout& layout,int width) {
        if (layout.text().isEmpty()) return 0;
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        qreal height = 0;
        layout.beginLayout();
        for (auto line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(qMax(1,width));
            line.setPosition(QPointF(0,height));
            height += line.height();
        }
        layout.endLayout();
        return qCeil(height);
    }
};

int ProfileCardWidget::cellHeightForWidth(int width) {
    const int scale = UiTheme::textScale();
    const int extraLine = QFontMetrics(captionFont()).height()+8;
    return CellHeight + qMax(0,scale-100)/2 + (needsStackedFooter(width) ? extraLine : 0);
}

int ProfileCardWidget::identityWidth(int width) const {
    const auto margin = cardLayout_->contentsMargins();
    const auto frame = contentsMargins();
    const int inner = width-margin.left()-margin.right()-frame.left()-frame.right();
    return inner-identityInset-toolInset-2*toolSize-toolGap;
}

int ProfileCardWidget::minimumWidthForMetadata() const {
    return qMax(source_->sizeHint().width(),time_->sizeHint().width())-identityWidth(0);
}

ProfileCardWidget::TextHeights ProfileCardWidget::textHeights(int width) const {
    const int identity = identityWidth(width);
    return {title_->heightForWidth(identity),tags_->heightForWidth(identity)};
}

int ProfileCardWidget::extraTextHeight(int width) const {
    const auto required = textHeights(width);
    return qMax(0,required.title-title_->fontMetrics().height())
        + qMax(0,required.metadata-qMax(source_->sizeHint().height(),time_->sizeHint().height()));
}

int ProfileCardWidget::fitText(int width) {
    ensurePolished();
    title_->ensurePolished();
    source_->ensurePolished();
    time_->ensurePolished();
    const auto required = textHeights(width);
    title_->setFixedHeight(required.title);
    metadata_->setFixedHeight(required.metadata);
    cardLayout_->invalidate();
    const auto frame = contentsMargins();
    const auto insets = cardLayout_->contentsMargins();
    const int header = qMax(markSize,required.title+metadataGap+required.metadata);
    const auto* visibleAction=reviewUpdate_->isHidden()?open_:reviewUpdate_;
    const int action=visibleAction->isHidden()?0:visibleAction->sizeHint().height();
    int status = statusLine_->isHidden() ? 0 : qMax(12,status_->fontMetrics().height())+2*stateVerticalInset;
    if (!updateStatus_->parentWidget()->isHidden())
        status += stateGap+qMax(12,updateStatus_->fontMetrics().height())+2*stateVerticalInset;
    const int gap = cardLayout_->spacing();
    const int footer = (needsStackedFooter(width) && status && action) ? status+gap+action : qMax(status,action);
    return frame.top()+frame.bottom()+insets.top()+insets.bottom()+header+(footer?gap+footer:0);
}

ProfileCardWidget::ProfileCardWidget(QWidget* parent) : QFrame(parent) {
    setObjectName("profileCard");
    setProperty("summaryCard",true);
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(false);
    auto* layout = new QVBoxLayout(this);
    cardLayout_ = layout;
    layout->setContentsMargins(HorizontalInset,VerticalInset,HorizontalInset,VerticalInset);
    layout->setSpacing(SummaryCardStyle::RowGap);

    auto* header = new QHBoxLayout;
    header->setSpacing(0);
    mark_ = new QLabel;
    mark_->setObjectName("profileCardMark");
    mark_->setProperty("summaryMark",true);
    mark_->setFixedSize(markSize,markSize);
    mark_->setAlignment(Qt::AlignCenter);
    header->addWidget(mark_,0,Qt::AlignTop);
    header->addSpacing(markGap);
    auto* identity = new QVBoxLayout;
    identity->setSpacing(metadataGap);
    title_ = new ProfileCardText;
    title_->setObjectName("profileCardTitle");
    title_->setProperty("summaryTitle",true);
    identity->addWidget(title_);
    metadata_ = new QWidget;
    tags_ = new WrapLayout(metadata_);
    tags_->setSpacing(8);
    const auto makeTag=[this](const QString& name) {
        auto* tag=new QLabel;
        tag->setObjectName(name);
        tag->setTextFormat(Qt::PlainText);
        tag->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        tag->setProperty("profileCardMetadataTag",true);
        tags_->addWidget(tag);
        return tag;
    };
    source_=makeTag("profileCardSource");
    time_=makeTag("profileCardTime");
    time_->setProperty("profileCardMetadataTag",false);
    identity->addWidget(metadata_);
    header->addLayout(identity,1);
    header->addSpacing(toolInset);
    auto* tools = new QHBoxLayout;
    tools->setSpacing(toolGap);
    header->addLayout(tools);
    header->setAlignment(tools,Qt::AlignTop);
    const auto tool = [this,tools](const QString& name, Action action) {
        auto* control = new QToolButton;
        control->setProperty("profileCardIcon",true);
        control->setProperty("quiet",true);
        control->setFixedSize(toolSize,toolSize);
        control->setIconSize(QSize(16,16));
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
    layout->addStretch(1);

    auto* footer = new QHBoxLayout;
    footer_ = footer;
    footer->setSpacing(actionGap);
    statusLine_ = new QWidget;
    statusLine_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    auto* statusLayout = new QVBoxLayout(statusLine_);
    statusLayout->setContentsMargins(identityInset,0,0,0);
    statusLayout->setSpacing(stateGap);
    const auto addState = [statusLayout](QLabel*& mark,QLabel*& text,const QString& name) {
        auto* tag=new QWidget;
        tag->setProperty("profileCardStateTag",true);
        tag->setAttribute(Qt::WA_StyledBackground);
        tag->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        auto* row=new QHBoxLayout(tag);
        row->setContentsMargins(stateInset,stateVerticalInset,stateInset,stateVerticalInset);
        row->setSpacing(5);
        mark=new QLabel;
        mark->setFixedSize(12,12);
        row->addWidget(mark,0,Qt::AlignVCenter);
        text=new QLabel;
        text->setObjectName(name);
        text->setProperty("cardState",true);
        text->setTextFormat(Qt::PlainText);
        text->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        row->addWidget(text,0,Qt::AlignVCenter);
        statusLayout->addWidget(tag,0,Qt::AlignLeft);
    };
    addState(statusMark_,status_,"profileCardStatus");
    addState(updateMark_,updateStatus_,"profileCardUpdate");
    footer->addWidget(statusLine_,1);
    const auto command = [this,footer](const QString& name, Action action) {
        auto* control = new QPushButton(name);
        control->setAutoDefault(false);
        control->setProperty("profileCardAction",true);
        control->setProperty("quiet",true);
        footer->addWidget(control,0,Qt::AlignRight|Qt::AlignVCenter);
        connect(control,&QPushButton::clicked,this,[this,action] { emit actionRequested(action); });
        return control;
    };
    reviewUpdate_ = command(reviewUpdateLabel,Action::ReviewUpdate);
    open_=command("详情 ›",Action::Open);
    open_->setObjectName("openProfileCardDetails");
    layout->addLayout(footer);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,&ProfileCardWidget::updateAppearance);
}

void ProfileCardWidget::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    arrangeFooter();
    fitText(event->size().width());
    cardLayout_->activate();
}

void ProfileCardWidget::arrangeFooter() {
    const bool stack = needsStackedFooter(width());
    if (stack == stackedFooter_) return;
    stackedFooter_ = stack;
    if (stack) {
        footer_->removeWidget(statusLine_);
        cardLayout_->insertWidget(cardLayout_->count()-1,statusLine_);
    } else {
        cardLayout_->removeWidget(statusLine_);
        footer_->insertWidget(0,statusLine_,1);
    }
}

void ProfileCardWidget::setProfile(const QModelIndex& index) {
    const auto kind = index.data(ProfileRowModel::Kind).toString();
    const bool remote = kind == "remote";
    const bool tunnel = kind == "wireguard";
    setProperty("wireGuardCard",tunnel);
    title_->setText(index.data(Qt::DisplayRole).toString());
    const auto source = index.data(ProfileRowModel::SourceLabel).toString();
    canRefresh_ = index.data(ProfileRowModel::CanRefresh).toBool();
    const auto timestamp=index.data(ProfileRowModel::UpdatedLabel).toString();
    source_->setText(tunnel?"WireGuard":index.data(ProfileRowModel::MixedEngines).toBool()
        ?(remote?"代理订阅":"代理配置"):(remote?"订阅链接":"本地文件"));
    source_->setToolTip(source);
    time_->setText(timestamp.isEmpty() ? QStringLiteral("尚未更新") : timestamp);
    const auto refreshHint = canRefresh_ ? QStringLiteral("检查更新") : QStringLiteral("此配置没有可刷新的来源");
    refresh_->setToolTip(refreshHint);
    refresh_->setAccessibleDescription(refreshHint);
    const bool candidate = index.data(ProfileRowModel::HasCandidate).toBool();
    const bool current = index.data(ProfileRowModel::IsCurrent).toBool();
    const bool running = index.data(ProfileRowModel::IsRunning).toBool();
    const bool failed = !index.data(ProfileRowModel::UpdateError).toString().isEmpty();
    const auto setState=[](QLabel* label,const QString& text,const QString& tone) {
        label->setText(text);
        if (label->property("tone") != tone) {
            label->setProperty("tone",tone);
            auto* tag=label->parentWidget();
            tag->setProperty("tone",tone);
            tag->style()->unpolish(tag);
            tag->style()->polish(tag);
            label->style()->unpolish(label);
            label->style()->polish(label);
        }
    };
    if (running) setState(status_,tunnel?"隧道已连接":"运行中","success");
    else if (current) setState(status_,"已选配置","idle");
    else if (candidate && !failed) setState(status_,"待应用","warning");
    else if (failed) setState(status_,"更新失败","error");
    else setState(status_,tunnel?"未连接":"未启用","idle");
    const bool separateUpdate=(current && candidate) || (failed && (current || candidate));
    setState(updateStatus_,failed && current?"更新失败":"待应用",failed && current?"error":"warning");
    updateStatus_->parentWidget()->setVisible(separateUpdate);
    reviewUpdate_->setVisible(candidate);
    open_->setVisible(!candidate);
    open_->setAccessibleName("打开配置详情 · "+title_->text());
    setWritable(writable_);
    setAccessibleName(title_->text());
    setAccessibleDescription(index.data(ProfileRowModel::Detail).toString());
    updateAppearance();
}

void ProfileCardWidget::updateAppearance() {
    using G = UiIcons::Glyph;
    mark_->setPixmap(UiIcons::icon(property("wireGuardCard").toBool()?G::Link:G::Folder,UiTheme::Brand).pixmap(23,23));
    statusMark_->setPixmap(StatusShape::pixmap(status_->property("tone").toString(),devicePixelRatioF()));
    updateMark_->setPixmap(StatusShape::pixmap(updateStatus_->property("tone").toString(),devicePixelRatioF()));
    int statusWidth=0;
    for (auto* state:{status_,updateStatus_}) {
        state->ensurePolished();
        state->setFixedSize(state->fontMetrics().size(Qt::TextSingleLine,state->text()));
        auto* tag=state->parentWidget();
        const int width=state->width()+2*stateInset+17;
        tag->setFixedWidth(width);
        if (!tag->isHidden()) statusWidth=qMax(statusWidth,width);
    }
    statusLine_->setMinimumWidth(identityInset+statusWidth);
    refresh_->setIcon(UiIcons::icon(G::Refresh,UiTheme::Secondary));
    menu_->setIcon(UiIcons::icon(G::More,UiTheme::Secondary));
    reviewUpdate_->ensurePolished();
    reviewUpdate_->setMinimumWidth(qMax(reviewUpdate_->sizeHint().width(),
                                      reviewUpdate_->fontMetrics().horizontalAdvance(reviewUpdate_->text())+actionPadding));
    arrangeFooter();
    fitText(width());
}

void ProfileCardWidget::setSelected(bool selected) {
    if (property("selected").toBool() == selected) return;
    setProperty("selected",selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void ProfileCardWidget::setWritable(bool writable) {
    writable_ = writable;
    refresh_->setEnabled(writable && canRefresh_);
}

QAbstractButton* ProfileCardWidget::button(Action action) const {
    switch (action) {
    case Action::Refresh: return refresh_;
    case Action::Menu: return menu_;
    case Action::ReviewUpdate: return reviewUpdate_;
    case Action::Open: return open_;
    }
    return nullptr;
}
}
