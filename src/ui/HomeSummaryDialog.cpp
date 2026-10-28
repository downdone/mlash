#include "HomeSummaryDialog.h"
#include "DialogDraftGuard.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QScrollArea>
namespace mlash {
HomeSummaryDialog::HomeSummaryDialog(AppController* controller,QWidget* parent):QDialog(parent) {
    setObjectName("homeSummaryDialog"); setAttribute(Qt::WA_DeleteOnClose); setWindowTitle("首页摘要"); resize(420,360); setMinimumSize(380,300); const int expected=controller->settings().homeSummaryMask;
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(16,16,16,16); layout->setSpacing(12);
    auto* scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); auto* body=new QWidget; auto* fields=new QVBoxLayout(body); fields->setContentsMargins(0,0,0,0); fields->setSpacing(8); scroll->setWidget(body); layout->addWidget(scroll,1);
    auto* note=new QLabel("选择要在“更多运行信息”中显示的项目。"); note->setWordWrap(true); fields->addWidget(note);
    QList<QCheckBox*> choices; const QStringList names{"规则","连接","日志","设置"}; for (int i=0;i<names.size();++i) { auto* choice=new QCheckBox(names[i]); choice->setObjectName("homeSummaryChoice"+QString::number(i)); choice->setChecked(expected&(1<<i)); choices.append(choice); fields->addWidget(choice); }
    auto* restore=new QPushButton("恢复默认"); restore->setObjectName("resetHomeSummary"); fields->addWidget(restore,0,Qt::AlignLeft); fields->addStretch(); connect(restore,&QPushButton::clicked,this,[choices] { for (auto* choice:choices) choice->setChecked(true); });
    auto* hint=new QLabel; hint->setObjectName("homeSummaryHint"); hint->setTextFormat(Qt::PlainText); hint->setWordWrap(true); layout->addWidget(hint);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); auto* save=actions->button(QDialogButtonBox::Save); save->setObjectName("saveHomeSummary"); save->setText("应用显示"); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions); auto saving=std::make_shared<bool>(false);
    const auto mask=[choices] { int value=0; for (int i=0;i<choices.size();++i) if (choices[i]->isChecked()) value|=1<<i; return value; };
    const auto update=[controller,save,restore,choices,saving,mask,expected] { const auto& state=controller->state(); const bool editable=!*saving&&!state.busy&&!state.exiting&&!state.readOnly; for (auto* choice:choices) choice->setEnabled(editable); restore->setEnabled(editable); save->setEnabled(editable&&mask()!=expected); };
    for (auto* choice:choices) connect(choice,&QCheckBox::toggled,this,update); connect(controller,&AppController::stateChanged,this,update); connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(actions,&QDialogButtonBox::accepted,this,[controller,saving,mask,expected,update] { *saving=true; update(); controller->updateHomeSummary(mask(),expected); });
    connect(controller,&AppController::operationFinished,this,[this,saving,hint,update](bool ok,const QString& error) { if (!*saving) return; *saving=false; if (ok) accept(); else { hint->setText(error); update(); } }); DialogDraftGuard::protect(this,[mask,expected] { return mask()!=expected; },[saving] { return *saving; }); update();
}
}
