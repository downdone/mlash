#include "HomeSummaryDialog.h"
#include "DialogDraftGuard.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
namespace mlash {
HomeSummaryDialog::HomeSummaryDialog(AppController* controller,QWidget* parent):QDialog(parent) {
    setObjectName("homeSummaryDialog"); setAttribute(Qt::WA_DeleteOnClose); setWindowTitle("首页摘要"); resize(400,280); const int expected=controller->settings().homeSummaryMask;
    auto* layout=new QVBoxLayout(this); auto* note=new QLabel("订阅、当前节点和流量始终优先展示。选择“更多运行信息”中需要的摘要；导航入口保持不变。"); note->setWordWrap(true); layout->addWidget(note);
    QList<QCheckBox*> choices; const QStringList names{"规则","连接","日志","设置"}; for (int i=0;i<names.size();++i) { auto* choice=new QCheckBox(names[i]); choice->setObjectName("homeSummaryChoice"+QString::number(i)); choice->setChecked(expected&(1<<i)); choices.append(choice); layout->addWidget(choice); }
    auto* restore=new QPushButton("恢复默认"); restore->setObjectName("resetHomeSummary"); layout->addWidget(restore,0,Qt::AlignLeft); connect(restore,&QPushButton::clicked,this,[choices] { for (auto* choice:choices) choice->setChecked(true); });
    auto* hint=new QLabel("仅保存显示偏好，不重启内核。"); hint->setObjectName("homeSummaryHint"); hint->setTextFormat(Qt::PlainText); hint->setWordWrap(true); layout->addWidget(hint);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); auto* save=actions->button(QDialogButtonBox::Save); save->setObjectName("saveHomeSummary"); save->setText("应用显示"); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions); auto saving=std::make_shared<bool>(false);
    const auto mask=[choices] { int value=0; for (int i=0;i<choices.size();++i) if (choices[i]->isChecked()) value|=1<<i; return value; };
    const auto update=[controller,save,restore,choices,saving,mask,expected] { const auto& state=controller->state(); const bool editable=!*saving&&!state.busy&&!state.exiting&&!state.readOnly; for (auto* choice:choices) choice->setEnabled(editable); restore->setEnabled(editable); save->setEnabled(editable&&mask()!=expected); };
    for (auto* choice:choices) connect(choice,&QCheckBox::toggled,this,update); connect(controller,&AppController::stateChanged,this,update); connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(actions,&QDialogButtonBox::accepted,this,[controller,saving,mask,expected,update] { *saving=true; update(); controller->updateHomeSummary(mask(),expected); });
    connect(controller,&AppController::operationFinished,this,[this,saving,hint,update](bool ok,const QString& error) { if (!*saving) return; *saving=false; if (ok) accept(); else { hint->setText(error); update(); } }); DialogDraftGuard::protect(this,[mask,expected] { return mask()!=expected; },[saving] { return *saving; }); update();
}
}
