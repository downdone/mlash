#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QToolButton>

namespace mlash {
// Keep the decision-relevant sentence visible; longer guidance is requested explicitly.
inline QWidget* contextNote(const QString& summary,const QString& explanation) {
    auto* surface=new QWidget; surface->setObjectName("contextNote");
    auto* row=new QHBoxLayout(surface); row->setContentsMargins(0,0,0,0); row->setSpacing(6);
    auto* label=new QLabel(summary); label->setObjectName("contextSummary"); label->setTextFormat(Qt::PlainText); label->setWordWrap(true); label->setProperty("secondary",true); label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); row->addWidget(label,1);
    auto* help=new QToolButton; help->setObjectName("contextHelp"); help->setText("?"); help->setProperty("quiet",true); help->setFixedSize(26,26); help->setToolTip("使用说明"); help->setAccessibleName("查看使用说明"); help->setAccessibleDescription(summary); row->addWidget(help,0,Qt::AlignTop);
    auto* detail=new QLabel(explanation); detail->setObjectName("contextExplanation"); detail->setTextFormat(Qt::PlainText); detail->setWordWrap(true); detail->setProperty("secondary",true); detail->hide();
    auto* container=new QWidget; auto* column=new QVBoxLayout(container); column->setContentsMargins(0,0,0,0); column->addWidget(surface); column->addWidget(detail);
    help->setCheckable(true); QObject::connect(help,&QToolButton::toggled,detail,&QWidget::setVisible);
    return container;
}
}
