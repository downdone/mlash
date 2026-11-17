#pragma once
#include "UiTheme.h"
#include "UiIcons.h"
#include "UiHairline.h"
#include <QtWidgets>

namespace mlash::WindowParts {
class SettingRowSurface final : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOption option; option.initFrom(this);
        QPainter painter(this);
        style()->drawPrimitive(QStyle::PE_Widget,&option,&painter,this);
        if (!property("last").toBool()) UiHairline::bottom(&painter,rect(),46,QColor(UiTheme::Separator));
    }
};
inline void settingRow(QVBoxLayout* section, const QString& name, const QString& description, QWidget* control,bool stacked=false, const QString& currentValue = QString(), UiIcons::Glyph glyph = UiIcons::Glyph::Settings) {
    auto* row = new SettingRowSurface; row->setObjectName("settingRow"); row->setProperty("settingSearchName",name); row->setProperty("settingSearchTerms",name+" "+description);
    // K8：行首 21px 图标位（原型 .setting-icon）。下拉/数字框本身已经显示值，不再重复写在行里。
    auto* labels = new QVBoxLayout; labels->setSpacing(2); auto* title = new QLabel(name); title->setObjectName("settingName"); labels->addWidget(title);
    // 行内「当前值」（原型 05a/05b / 切片 V3）：值不在控件里的那些行（启动页面、TUN 状态、
    // 内核路径…）用一行次要文字说明现状，控件本身仍只负责「改」。
    // 文案为空表示这行的控件已经显示了值（下拉、数字框），不重复。
    QLabel* current = nullptr;
    if (!currentValue.isEmpty()) {
        current = new QLabel(currentValue); current->setObjectName("settingValue");
        current->setProperty("secondary", true); current->setTextFormat(Qt::PlainText);
        current->setWordWrap(false); current->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        current->setToolTip(currentValue);
        current->setAccessibleName(name + " 当前值");
        labels->addWidget(current);
    }
    const bool explain=QStringList{"启动时恢复代理开关","关闭窗口时释放界面"}.contains(name);
    if (!description.isEmpty()) { control->setToolTip(control->toolTip().isEmpty()?description:control->toolTip()+"\n"+description); }
    if (explain&&!description.isEmpty()) { auto* detail = new QLabel(description); detail->setObjectName("secondaryText"); detail->setWordWrap(true); detail->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); labels->addWidget(detail); }
    if (auto* button=qobject_cast<QPushButton*>(control)) button->setProperty("quiet",true);
    if (control->layout()) control->layout()->setSizeConstraint(QLayout::SetMinimumSize);
    control->setAccessibleName(name); control->setAccessibleDescription(description);
    row->setProperty("settingKind",stacked?"stacked":explain?"explained":"single");
    auto* icon = new QLabel; icon->setObjectName("settingIcon"); icon->setFixedSize(21,21); icon->setPixmap(UiIcons::icon(glyph,UiTheme::Secondary).pixmap(16,16)); icon->setAlignment(Qt::AlignCenter);
    QObject::connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,row,[icon,glyph] {
        icon->setPixmap(UiIcons::icon(glyph,UiTheme::Secondary).pixmap(16,16));
    });
    if (stacked) {
        auto* layout=new QVBoxLayout(row); layout->setContentsMargins(0,7,0,7); layout->setSpacing(6);
        auto* head=new QHBoxLayout; head->setSpacing(13); head->addWidget(icon,0,Qt::AlignVCenter); head->addLayout(labels,1);
        layout->addLayout(head); layout->addWidget(control);
    }
    else {
        auto* layout=new QHBoxLayout(row); layout->setContentsMargins(0,5,0,5); layout->setSpacing(10);
        layout->addWidget(icon,0,Qt::AlignVCenter); layout->addLayout(labels,1);
        // 右侧「行尾」：值（若有）+ 控件 + ›。
        auto* trail=new QHBoxLayout; trail->setSpacing(8); if (current) { labels->removeWidget(current); trail->addWidget(current,0,Qt::AlignVCenter); }
        trail->addWidget(control,0,Qt::AlignVCenter);
        // Form editors use their controls directly; directory chevrons belong to ActionRow only.
        layout->addLayout(trail);
        row->setMinimumHeight(qMax(UiTheme::SettingRowHeight,title->fontMetrics().height()+14));
    }
    section->addWidget(row);
}
}
