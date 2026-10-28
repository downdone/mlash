#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QToolButton>

namespace mlash {
// Keep the decision-relevant sentence visible; longer guidance is requested explicitly.
inline QWidget* contextNote(const QString& summary,const QString& explanation) {
    auto* surface=new QWidget; surface->setObjectName("contextNote");
    auto* row=new QHBoxLayout(surface); row->setContentsMargins(0,0,0,0); row->setSpacing(6);
    auto* label=new QLabel(summary); label->setObjectName("contextSummary"); label->setTextFormat(Qt::PlainText); label->setWordWrap(true); label->setProperty("secondary",true); label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); row->addWidget(label,1);
    auto* help=new QToolButton; help->setObjectName("contextHelp"); help->setText("?"); help->setProperty("quiet",true); help->setFixedSize(26,26); help->setToolTip("使用说明"); help->setAccessibleName("查看使用说明"); help->setAccessibleDescription(summary); row->addWidget(help,0,Qt::AlignTop);
    QObject::connect(help,&QToolButton::clicked,surface,[surface,explanation] {
        auto* dialog=new QMessageBox(QMessageBox::Information,"使用说明",explanation,QMessageBox::Close,surface);
        dialog->setObjectName("contextHelpDialog"); dialog->setTextFormat(Qt::PlainText); dialog->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->open();
    });
    return surface;
}
}
