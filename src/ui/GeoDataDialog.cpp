#include "GeoDataDialog.h"
#include "ContextNote.h"
#include "CompactComboBox.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QDateTime>
namespace mlash {
GeoDataDialog::GeoDataDialog(AppController* controller,QWidget* parent) : QDialog(parent),controller_(controller) {
    setObjectName("geoDataDialog"); setAttribute(Qt::WA_DeleteOnClose); setWindowTitle("Geo 数据"); resize(720,420); auto* layout=new QVBoxLayout(this);
    layout->addWidget(contextNote("更新后重新加载订阅生效。","在线更新从 MetaCubeX GitHub 发布资产直连下载，经内核校验后保存为默认快照。当前运行文件保持不变；重新加载订阅后使用，来源自定义 Geo 地址优先。"));
    auto* toolbar=new QHBoxLayout; file_=new CompactComboBox; file_->setObjectName("geoUpdateFile"); file_->addItems({"country.mmdb","geoip.dat","geosite.dat","asn.mmdb"}); toolbar->addWidget(file_); update_=new QPushButton("下载更新"); update_->setObjectName("updateGeoData"); toolbar->addWidget(update_); import_=new QPushButton("导入本地…"); toolbar->addWidget(import_); toolbar->addStretch(); refresh_=new QPushButton("刷新列表"); toolbar->addWidget(refresh_); layout->addLayout(toolbar);
    table_=new QTableWidget(0,5); table_->setObjectName("geoDataTable"); table_->setHorizontalHeaderLabels({"文件","位置","大小","更新时间","状态"}); table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->verticalHeader()->hide(); table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_,1);
    hint_=new QLabel; hint_->setObjectName("geoDataHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); layout->addWidget(hint_); auto* actions=new QDialogButtonBox(QDialogButtonBox::Close); actions->button(QDialogButtonBox::Close)->setText("关闭"); layout->addWidget(actions); connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(refresh_,&QPushButton::clicked,this,&GeoDataDialog::reload); connect(update_,&QPushButton::clicked,this,[this] { writing_=true; hint_->setText("正在下载、校验并保存默认快照…"); controller_->updateGeoData(file_->currentText()); updateActions(); });
    connect(import_,&QPushButton::clicked,this,[this] { const auto path=QFileDialog::getExistingDirectory(this,"选择Geo数据目录"); if (path.isEmpty()) return; writing_=true; controller_->importGeoData(path); updateActions(); });
    connect(controller_,&AppController::stateChanged,this,&GeoDataDialog::updateActions); connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& error) { if (!writing_) return; writing_=false; if (ok) reload(); else hint_->setText(error); updateActions(); }); reload();
}
void GeoDataDialog::updateActions() { const bool editable=!controller_->state().busy&&!controller_->state().exiting&&!controller_->state().readOnly&&!writing_; update_->setEnabled(editable&&!controller_->settings().corePath.isEmpty()); import_->setEnabled(editable); file_->setEnabled(editable); refresh_->setEnabled(!loading_&&!controller_->state().busy); }
void GeoDataDialog::reload() {
    if (loading_) return; loading_=true; updateActions(); const QPointer<GeoDataDialog> guard(this);
    controller_->inspectGeoData([guard](Result result) {
        if (!guard) return; guard->loading_=false;
        if (!result.ok) guard->hint_->setText(result.error);
        else { const auto rows=result.data.value("files").toArray(); guard->table_->setRowCount(rows.size()); for (int i=0;i<rows.size();++i) { const auto row=rows[i].toObject(); const auto date=QDateTime::fromString(row.value("updated_utc").toString(),Qt::ISODate).toLocalTime(); const bool active=row.value("scope").toString()=="当前运行版本"; const QStringList values{row.value("name").toString(),row.value("scope").toString(),QString::number(row.value("bytes").toDouble()/(1024*1024),'f',2)+" MiB",date.isValid()?date.toString("MM-dd HH:mm"):"未知",active?QString("版本内文件"):row.value("hash_matches").toBool()?QString("哈希一致"):QString("无记录/哈希变化")}; for (int col=0;col<values.size();++col) { auto* item=new QTableWidgetItem(values[col]); item->setToolTip("来源："+row.value("origin").toString()+"\nSHA256："+row.value("sha256").toString()); guard->table_->setItem(i,col,item); } } const auto policy=result.data.value("policy").toString(); guard->hint_->setToolTip(policy); guard->hint_->setText(rows.isEmpty()?QString("尚无 Geo 数据"):policy.contains("自定义")?QString("当前订阅使用自定义来源"):QString()); }
        guard->updateActions();
    });
}
}
