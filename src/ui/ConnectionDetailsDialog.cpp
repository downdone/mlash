#include "ConnectionDetailsDialog.h"
#include "ActivityTableModel.h"
#include "CompactComboBox.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHostAddress>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
namespace mlash {
namespace {
class DetailViewport final : public QScrollArea {
public:
    QSize sizeHint() const override { return {480,240}; }
    QSize minimumSizeHint() const override { return {160,80}; }
    bool hasHeightForWidth() const override { return false; }
    int heightForWidth(int) const override { return -1; }
};
}
QJsonArray ConnectionDetailsDialog::ruleSuggestions(const QJsonObject& row) {
    QJsonArray values; const auto metadata=row.value("metadata").toObject(); auto host=metadata.value("host").toString();
    if (host.endsWith('.')) host.chop(1);
    const auto ace=host.size()<=253?QString::fromLatin1(QUrl::toAce(host)):QString(); QHostAddress hostAddress;
    static const QRegularExpression domain("^(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)(?:\\.[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)*$");
    if (!ace.isEmpty()&&ace.size()<=253&&!hostAddress.setAddress(host)&&domain.match(ace).hasMatch()) values.append(QJsonObject{{"label","按域名创建规则…"},{"type","DOMAIN"},{"payload",ace.toLower()}});
    const auto ip=metadata.value("destinationIP").toString(); QHostAddress address; if (ip.size()<=64&&address.setAddress(ip)&&address.scopeId().isEmpty()) { const bool v6=address.protocol()==QAbstractSocket::IPv6Protocol; values.append(QJsonObject{{"label","按目标 IP 创建规则…"},{"type",v6?"IP-CIDR6":"IP-CIDR"},{"payload",address.toString()+(v6?"/128":"/32")},{"option","no-resolve"}}); }
    const auto process=ActivityTableModel::processName(metadata); bool safe=process!="—"&&!process.isEmpty()&&process.size()<=256&&process.trimmed()==process;
    for (const auto ch:process) if (ch==','||ch.isNull()||ch.category()==QChar::Other_Control) safe=false;
    if (safe) values.append(QJsonObject{{"label","按进程创建规则…"},{"type","PROCESS-NAME"},{"payload",process}});
    return values;
}
ConnectionDetailsDialog::ConnectionDetailsDialog(AppController* controller,QJsonObject row,QString profile,QString version,QWidget* parent)
    :QDialog(parent),controller_(controller),row_(std::move(row)),profile_(std::move(profile)),version_(std::move(version)) {
    setObjectName("connectionDetails"); setAttribute(Qt::WA_DeleteOnClose); setWindowTitle("连接详情 · 打开时快照"); resize(620,510);
    auto* layout=new QVBoxLayout(this); auto* note=new QLabel("打开时的连接快照"); note->setWordWrap(true); layout->addWidget(note);
    auto* scroll=new DetailViewport; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); auto* body=new QWidget; auto* fields=new QVBoxLayout(body); fields->setContentsMargins(4,4,4,4);
    QFormLayout* form=nullptr;
    const auto section=[&](const QString& title) { auto* label=new QLabel(title); label->setObjectName("sectionTitle"); fields->addWidget(label); form=new QFormLayout; form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); fields->addLayout(form); };
    const auto field=[&](const QString& key,const QString& label,const QString& value) {
        auto* row=new QWidget; auto* line=new QHBoxLayout(row); line->setContentsMargins(0,0,0,0); auto* text=new QLabel(value.isEmpty()?QString("—"):value.left(512)+(value.size()>512?QString("…"):QString())); text->setTextFormat(Qt::PlainText); text->setWordWrap(true); text->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard); text->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); text->setObjectName("connectionField_"+key); text->setAccessibleName(label); text->setToolTip("<qt>"+value.left(4096).toHtmlEscaped()+(value.size()>512?QString("<br>显示已省略；复制取得完整原值。"):QString())+"</qt>"); line->addWidget(text,1);
        auto* copy=new QToolButton; copy->setObjectName("copyConnection_"+key); copy->setIcon(UiIcons::icon(UiIcons::Glyph::Copy)); copy->setAccessibleName("复制"+label); copy->setToolTip("复制完整原值"); copy->setEnabled(!value.isEmpty()&&value!="—"); copies_.append(copy); line->addWidget(copy); connect(copy,&QToolButton::clicked,this,[this,value,label] { QApplication::clipboard()->setText(value); feedback_="已复制"+label; updateContext(); }); form->addRow(label,row);
    };
    const auto metadata=row_.value("metadata").toObject();
    section("目标"); field("host","主机",metadata.value("host").toString()); field("destination","目标 IP / 端口",ActivityTableModel::endpoint(metadata.value("destinationIP").toString(),metadata.value("destinationPort").toVariant().toString())); field("network","网络",metadata.value("network").toString().toUpper()); field("inbound","入站类型",metadata.value("type").toString());
    section("来源与进程"); field("source","来源",ActivityTableModel::endpoint(metadata.value("sourceIP").toString(),metadata.value("sourcePort").toVariant().toString())); field("process","进程",ActivityTableModel::processName(metadata)); field("processPath","进程路径",metadata.value("processPath").toString());
    section("规则与链"); QString profileName; for (const auto& profile:controller_->profiles()) if (profile.id==profile_) profileName=profile.name; field("profile","来源订阅",profileName); field("rule","命中规则",row_.value("rule").toString()); field("rulePayload","匹配内容",row_.value("rulePayload").toString()); QStringList chain; for (const auto& item:row_.value("chains").toArray()) chain.append(item.toString()); field("chain","核心返回链",chain.join(" › "));
    section("流量与采样"); const auto bytes=[&](const QString& key) { return row_.value(key).isDouble()?ActivityTableModel::formatBytes(row_.value(key).toDouble()):QString(); }; const auto rate=[&](const QString& key) { return row_.value(key).isDouble()?ActivityTableModel::formatBytes(row_.value(key).toDouble())+"/s":QString(); };
    field("state","记录状态",!row_.value("_active").toBool(true)?"离开快照":row_.value("_paused").toBool()?"暂停快照":"活动观察快照"); field("upload","上传量",bytes("upload")); field("download","下载量",bytes("download")); const auto up=rate("_upload_rate"),down=rate("_download_rate"); field("rates","上传 / 下载速率",up.isEmpty()&&down.isEmpty()?QString():(up.isEmpty()?QString("—"):up)+" / "+(down.isEmpty()?QString("—"):down)); field("start","开始时间",row_.value("start").toString()); field("observed","采样时间",row_.value("_observed_at").toString()); field("left","离开观察时间",row_.value("_left_at").toString()); field("id","连接 ID",row_.value("id").toString()); fields->addStretch(); scroll->setWidget(body); layout->addWidget(scroll,1);
    auto* route=new QWidget; auto* routeLayout=new QHBoxLayout(route); routeLayout->setContentsMargins(0,0,0,0); groups_=new CompactComboBox; groups_->setObjectName("connectionGroups"); for (const auto& name:chain) if (controller_->proxies().value(name).toObject().value("all").isArray()&&groups_->findData(name)<0) groups_->addItem(name,name); routeLayout->addWidget(groups_,1); locate_=new QPushButton("定位策略组"); locate_->setObjectName("locateConnectionGroup"); routeLayout->addWidget(locate_); route->setVisible(groups_->count()>0); layout->addWidget(route);
    hint_=new QLabel; hint_->setObjectName("connectionDetailsHint"); hint_->setTextFormat(Qt::PlainText); hint_->setWordWrap(true); hint_->setMinimumHeight(fontMetrics().lineSpacing()*2+4); layout->addWidget(hint_);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Close); actions->button(QDialogButtonBox::Close)->setText("完成"); rule_=actions->addButton("添加规则…",QDialogButtonBox::ActionRole); rule_->setObjectName("createConnectionRule"); disconnect_=actions->addButton("断开连接",QDialogButtonBox::ActionRole); disconnect_->setObjectName("disconnectConnection"); disconnect_->setProperty("danger",true); layout->addWidget(actions); connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    auto* menu=new QMenu(rule_); menu->setObjectName("connectionRuleMenu"); for (const auto& value:ruleSuggestions(row_)) { const auto candidate=value.toObject(); auto* action=menu->addAction(candidate.value("label").toString()); action->setObjectName("connectionRule_"+candidate.value("type").toString()); connect(action,&QAction::triggered,this,[this,candidate] { if (!contextCurrent()) return; auto suggestion=candidate; suggestion["target"]=target(); suggestion["profile"]=profile_; suggestion["active_version"]=version_; suggestion["observed_at"]=row_.value("_observed_at"); emit createRule(suggestion); }); }
    rule_->setProperty("hasSuggestions",!menu->actions().isEmpty()); connect(rule_,&QPushButton::clicked,this,[this,menu] { menu->popup(rule_->mapToGlobal(QPoint(0,rule_->height()))); });
    connect(locate_,&QPushButton::clicked,this,[this] { const auto group=groups_->currentData().toString(); if (contextCurrent()&&controller_->proxies().value(group).toObject().value("all").isArray()) { emit browseGroup(group); accept(); } });
    connect(disconnect_,&QPushButton::clicked,this,[this] { if (!disconnect_->isEnabled()) return; disconnecting_=true; feedback_.clear(); controller_->closeConnection(row_.value("id").toString()); });
    connect(controller_,&AppController::operationFinished,this,[this](bool ok,const QString& message) { if (!disconnecting_) return; disconnecting_=false; feedback_=ok?QString("连接关闭已回读确认"):message; updateContext(); }); connect(controller_,&AppController::stateChanged,this,&ConnectionDetailsDialog::updateContext); connect(controller_,&AppController::connectionsChanged,this,&ConnectionDetailsDialog::updateContext); connect(controller_,&AppController::proxiesChanged,this,&ConnectionDetailsDialog::updateContext); connect(groups_,&QComboBox::currentIndexChanged,this,&ConnectionDetailsDialog::updateContext);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,[this] { for (auto* copy:copies_) copy->setIcon(UiIcons::icon(UiIcons::Glyph::Copy)); }); updateContext();
}
bool ConnectionDetailsDialog::contextCurrent() const { if (!controller_->state().ready||controller_->state().activeProfile!=profile_||profile_.isEmpty()||version_.isEmpty()) return false; for (const auto& profile:controller_->profiles()) if (profile.id==profile_) return profile.active==version_; return false; }
QString ConnectionDetailsDialog::target() const {
    auto target=groups_->currentData().toString(); if (target.isEmpty()) for (const auto& value:row_.value("chains").toArray()) { const auto name=value.toString(); if (name=="DIRECT"||name=="REJECT"||controller_->proxies().contains(name)) { target=name; break; } }
    if (target=="GLOBAL"||target.contains(',')||target.contains('\n')||target.contains('\r')||target.contains(QChar::Null)) return {}; return target;
}
void ConnectionDetailsDialog::updateContext() {
    const bool context=contextCurrent(),idle=!controller_->state().busy&&!controller_->state().exiting; bool present=false; for (const auto& value:controller_->connections()) if (value.toObject().value("id")==row_.value("id")) present=true;
    const bool group=context&&controller_->proxies().value(groups_->currentData().toString()).toObject().value("all").isArray(); groups_->setEnabled(context&&idle); locate_->setEnabled(group&&idle); rule_->setEnabled(context&&idle&&!controller_->state().readOnly&&rule_->property("hasSuggestions").toBool()); disconnect_->setEnabled(context&&idle&&!controller_->state().readOnly&&controller_->connectionsCurrent()&&present&&!disconnecting_);
    const auto status=!context?QString("来源上下文已变化或内核未就绪；仅查看快照"):!controller_->connectionsCurrent()?QString("仅查看快照；当前活动状态未确认"):!present?QString("该 ID 已不在当前观察快照中"):QString("当前观察仍包含此 ID；字段保持打开时值"); hint_->setText(status+(feedback_.isEmpty()?QString():"\n"+feedback_));
}
}
