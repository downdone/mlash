#include "WebsiteTestsPage.h"
#include "NodePresentation.h"
#include "WidgetStyle.h"
#include "UiTheme.h"
#include "CompactComboBox.h"
#include "UiIcons.h"
#include "WebsiteTargetsDialog.h"
#include "ElidedLabel.h"
#include <QComboBox>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTextOption>
#include <QToolButton>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QPainter>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QScrollBar>
#include <QStyle>

namespace mlash {
WebsiteTestsPage::WebsiteTestsPage(AppController* controller,QWidget* parent):QWidget(parent),controller_(controller) {
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(10);
    auto* heading=new QHBoxLayout; auto* title=new QLabel("测速"); title->setObjectName("pageTitle"); heading->addWidget(title); heading->addStretch(); auto* edit=new QPushButton("编辑目标"); edit->setObjectName("editWebsiteTargets"); heading->addWidget(edit); layout->addLayout(heading); connect(edit,&QPushButton::clicked,this,[this] { (new WebsiteTargetsDialog(controller_,this))->show(); });
    auto* controls=new QFrame; controls->setObjectName("panel"); auto* controlLayout=new QVBoxLayout(controls); controlLayout->setContentsMargins(12,10,12,10); controlLayout->setSpacing(8);
    auto* row=new QHBoxLayout; row->setSpacing(8); row->addWidget(new QLabel("测试节点")); nodes_=new CompactComboBox; nodes_->setPlaceholderText("请选择具体节点"); nodes_->setObjectName("websiteNode"); nodes_->setAccessibleName("网站测速节点"); nodes_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed); row->addWidget(nodes_,1);
    start_=new QPushButton("开始测速"); start_->setObjectName("startWebsiteTests"); start_->setProperty("primary",true); row->addWidget(start_);
    clear_=new QPushButton("清空结果"); clear_->setObjectName("clearWebsiteTests"); row->addWidget(clear_); controlLayout->addLayout(row);
    layout->addWidget(controls);
    auto* scroll=new QScrollArea; scroll->setObjectName("websiteScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cards_=new QWidget; cards_->setObjectName("websiteSurface"); cards_->setAttribute(Qt::WA_StyledBackground); grid_=new QGridLayout(cards_); grid_->setContentsMargins(0,0,0,0); grid_->setSpacing(10); scroll->setWidget(cards_);
    auto* summary=new QHBoxLayout; progress_=new ElidedLabel; progress_->setObjectName("websiteProgress"); progress_->setAccessibleName("测速进度或阻塞原因"); progress_->setProperty("secondary",true); progress_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); summary->addWidget(progress_,1); lastTestIcon_=new QLabel; lastTestIcon_->setFixedSize(16,16); summary->addWidget(lastTestIcon_); lastTest_=new QLabel; lastTest_->setObjectName("lastWebsiteTest"); lastTest_->setProperty("secondary",true); summary->addWidget(lastTest_); layout->addLayout(summary); layout->addWidget(scroll,1);
    scroll->viewport()->installEventFilter(this); rebuildCards();
    connect(controller_->websiteTests(),&WebsiteTests::targetsChanged,this,&WebsiteTestsPage::rebuildCards);
    auto* method=new QToolButton; method->setObjectName("websiteMethod"); method->setProperty("quiet",true); method->setText("HTTP 响应测试 · 方法与含义"); layout->addWidget(method,0,Qt::AlignLeft);
    connect(method,&QToolButton::clicked,this,[this] { QMessageBox::information(this,"测速方法","通过受控内核的指定节点请求目标地址，记录本次 HTTP 响应延迟。\n已响应不代表解锁、登录可用或带宽；HTTP 状态未提供时显示未知。\n停止、离开页面或隐藏窗口会取消未完成项；已发出的内核请求可能持续到5秒超时。\n测试不主动修改策略组；每卡详情包含指定节点、目标、时间与错误依据。"); });
    connect(start_,&QPushButton::clicked,this,[this] { if (controller_->websiteTests()->running()) controller_->cancelWebsiteTests(); else controller_->startWebsiteTests(nodes_->currentData().toString()); });
    connect(clear_,&QPushButton::clicked,controller_,&AppController::clearWebsiteTests);
    connect(nodes_,&QComboBox::activated,this,[this] { userPicked_=true; controller_->clearWebsiteTests(); updateResults(); });
    connect(controller_,&AppController::stateChanged,this,&WebsiteTestsPage::updateNodes); connect(controller_,&AppController::proxiesChanged,this,&WebsiteTestsPage::updateNodes);
    connect(controller_->websiteTests(),&WebsiteTests::changed,this,&WebsiteTestsPage::updateResults);
    connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,this,[this] { clear_->setIcon(UiIcons::icon(UiIcons::Glyph::Clear)); updateResults(); reflow(); });
    start_->setIcon(UiIcons::icon(UiIcons::Glyph::Play,UiTheme::OnPrimary)); clear_->setIcon(UiIcons::icon(UiIcons::Glyph::Clear)); updateNodes(); reflow();
}
void WebsiteTestsPage::rebuildCards() {
    if (resultDetails_) resultDetails_->close();
    for (const auto& card : cardWidgets_) { grid_->removeWidget(card.box); delete card.box; } cardWidgets_.clear(); columns_=0;
    for (const auto& target:controller_->websiteTests()->targets()) {
        auto* box=new QFrame(cards_); box->setObjectName("websiteCard"); box->setProperty("targetId",target.id); box->setMinimumHeight(100); box->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        auto* content=new QVBoxLayout(box); content->setContentsMargins(12,10,12,10); content->setSpacing(6); auto* top=new QHBoxLayout; top->setSpacing(8);
        auto* mark=new QLabel; mark->setObjectName("websiteMark"); mark->setFixedSize(36,36); mark->setAlignment(Qt::AlignCenter); mark->setPixmap(UiIcons::service(target.id).pixmap(26,26)); mark->setAccessibleName(target.name+" 图标"); top->addWidget(mark);
        auto* names=new QVBoxLayout; names->setSpacing(1); auto* name=new ElidedLabel(target.name); name->setObjectName("websiteName"); name->setTextFormat(Qt::PlainText); name->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); name->setToolTip(target.name); auto* host=new ElidedLabel(QUrl(target.url).host()); host->setObjectName("secondaryText"); host->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); host->setToolTip(target.url); names->addWidget(name); names->addWidget(host); top->addLayout(names,1);
        auto* details=new QToolButton; details->setObjectName("websiteResultDetails"); details->setProperty("quiet",true); details->setProperty("targetId",target.id); details->setMinimumSize(28,28); details->setIcon(UiIcons::icon(UiIcons::Glyph::Info)); details->setAccessibleName("查看 "+target.name+" 测速详情"); details->setToolTip("查看测试详情"); top->addWidget(details,0,Qt::AlignTop); content->addLayout(top); connect(details,&QToolButton::clicked,this,[this,id=target.id] { showResult(id); });
        auto* status=new QHBoxLayout; auto* value=new QLabel("—"); value->setObjectName("websiteDelay"); auto* state=new QLabel("未测试"); state->setObjectName("secondaryText"); status->setSpacing(6); status->addWidget(value); auto* dot=new QLabel; dot->setObjectName("websiteStatusDot"); dot->setFixedSize(8,8); status->addWidget(dot); status->addWidget(state,1);
        auto* test=new QPushButton("测试"); test->setObjectName("testWebsite"); test->setProperty("targetId",target.id); test->setAccessibleName("测试 "+target.name); status->addWidget(test); content->addLayout(status);
        cardWidgets_.append({target.id,box,value,state,dot,test,details});
        connect(test,&QPushButton::clicked,this,[this,id=target.id] { controller_->startWebsiteTests(nodes_->currentData().toString(),id); });
    }
    reflow(); updateResults();
}
void WebsiteTestsPage::showResult(const QString& id) {
    if (!active_||!isVisible()) return;
    WebsiteTarget target; bool found=false; for (const auto& item:controller_->websiteTests()->targets()) if (item.id==id) { target=item; found=true; } if (!found) return;
    if (resultDetails_) resultDetails_->close(); const auto result=controller_->websiteTests()->result(id); const auto node=controller_->websiteTests()->node();
    QString state; switch (result.status) { case WebsiteResult::Queued: state="等待中"; break; case WebsiteResult::Testing: state="测试中"; break; case WebsiteResult::Responded: state="已响应"; break; case WebsiteResult::Failed: state="未响应"; break; case WebsiteResult::Cancelled: state="已取消"; break; default: state="未测试"; }
    const QString text=QStringList{"目标名称："+target.name,"目标地址："+target.url,"指定节点："+(node.isEmpty()?QString("未知 · 尚未开始测试"):node),"结果："+state,"响应延迟："+(result.delay>=0?QString::number(result.delay)+" ms":QString("未知")),"测试时间："+(result.measured.isValid()?result.measured.toLocalTime().toString("yyyy-MM-dd HH:mm:ss t"):QString("未知")),"HTTP 状态：未知（核心 delay 接口未提供）","错误："+(result.error.isEmpty()?QString("无已报告错误"):result.error),"", "方法：经受控内核的指定节点请求以上目标；不主动修改策略组。", "已响应只表示本次 HTTP 响应延迟，不证明解锁、账号可用或带宽。"}.join('\n');
    auto* dialog=new QDialog(this); resultDetails_=dialog; dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setObjectName("websiteResultSnapshot"); dialog->setProperty("evidenceNode",node); dialog->setWindowTitle("测速详情 · 打开时快照"); dialog->resize(540,370); auto* layout=new QVBoxLayout(dialog); auto* note=new QLabel("打开时的结果快照；后续重测不会改变此处内容。"); note->setWordWrap(true); layout->addWidget(note); auto* details=new QPlainTextEdit(text); details->setObjectName("websiteResultEvidence"); details->setReadOnly(true); details->setWordWrapMode(QTextOption::WrapAnywhere); details->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); layout->addWidget(details,1); auto* actions=new QDialogButtonBox(QDialogButtonBox::Close); actions->button(QDialogButtonBox::Close)->setText("完成"); auto* copy=actions->addButton("复制详情",QDialogButtonBox::ActionRole); copy->setObjectName("copyWebsiteResult"); layout->addWidget(actions); connect(actions,&QDialogButtonBox::rejected,dialog,&QDialog::reject); connect(copy,&QPushButton::clicked,dialog,[text] { QApplication::clipboard()->setText(text); }); dialog->show();
}
void WebsiteTestsPage::setActive(bool active,const QString& suggestedGroup) {
    if (active && !active_) userPicked_=false;
    if (!suggestedGroup.isEmpty()) suggestedGroup_=suggestedGroup;
    if (!active&&resultDetails_) resultDetails_->close();
    active_=active; controller_->setWebsiteTestsVisible(active); updateNodes();
    updateResults();
}
void WebsiteTestsPage::suggestGroup(const QString& group) { suggestedGroup_=group; if (active_) updateNodes(); }
void WebsiteTestsPage::updateNodes() {
    const auto previous=nodes_->currentData().toString(); const QSignalBlocker block(nodes_);
    const auto& state=controller_->state();
    QStringList identities;
    if (state.ready) for (auto it=controller_->proxies().begin();it!=controller_->proxies().end();++it) if (controller_->websiteNode(it.key())==it.key()) identities.append(it.key());
    const bool changed=nodes_->property("nodeIdentities").toStringList()!=identities;
    if (changed) {
        nodes_->setProperty("nodeIdentities",identities); nodes_->clear();
        for (const auto& name:identities) nodes_->addItem(NodePresentation::icon(name),name=="DIRECT"?"直连 · DIRECT":name,name);
    }
    const auto suggested=controller_->websiteNode(suggestedGroup_);
    const auto preferred=!controller_->websiteTests()->node().isEmpty()?controller_->websiteTests()->node():!userPicked_&&!suggested.isEmpty()?suggested:previous;
    const auto index=nodes_->findData(preferred);
    if (nodes_->currentIndex()!=index && (changed||!nodes_->view()->isVisible())) nodes_->setCurrentIndex(index);
    updateResults();
}
void WebsiteTestsPage::updateResults() {
    const auto* tests=controller_->websiteTests(); const auto& state=controller_->state(); const bool running=tests->running();
    const bool allowed=active_ && isVisible() && state.ready && !state.busy && !state.exiting && !state.readOnly && !nodes_->currentData().toString().isEmpty();
    nodes_->setEnabled(state.ready && !state.busy && !running && !state.exiting); nodes_->setToolTip(nodes_->currentText()); start_->setEnabled(running||allowed); clear_->setEnabled(!running && !tests->node().isEmpty());
    start_->setText(running?"停止测试":"开始测速"); start_->setIcon(UiIcons::icon(running?UiIcons::Glyph::Close:UiIcons::Glyph::Play,running?UiTheme::Secondary:UiTheme::OnPrimary));
    start_->setMinimumWidth(start_->fontMetrics().horizontalAdvance("开始测速")+48);
    if (start_->property("primary").toBool()==running) { start_->setProperty("primary",!running); start_->style()->unpolish(start_); start_->style()->polish(start_); }
    const QString reason=!state.ready?"内核尚未就绪 · 先使用订阅并启动内核":nodes_->currentIndex()<0?"请选择具体节点；不会修改策略组":state.readOnly?"只读保护中，暂时无法测试":state.busy?"等待当前操作完成":QString();
    if (resultDetails_&&!resultDetails_->property("evidenceNode").toString().isEmpty()&&resultDetails_->property("evidenceNode").toString()!=tests->node()) resultDetails_->close();
    int completed=0,responded=0,cancelled=0; QDateTime last;
    for (const auto& card:cardWidgets_) {
        card.details->setIcon(UiIcons::icon(UiIcons::Glyph::Info));
        const auto result=tests->result(card.id); QString label="未测试",color=UiTheme::Secondary;
        switch(result.status) {
        case WebsiteResult::Queued: label="等待中"; color=UiTheme::Brand; break;
        case WebsiteResult::Testing: label="测试中…"; color=UiTheme::Brand; break;
        case WebsiteResult::Responded: label="已响应"; color=UiTheme::Success; ++completed; ++responded; break;
        case WebsiteResult::Failed: label=result.error.contains("超时") || result.error.contains("504") || result.error.contains("timeout",Qt::CaseInsensitive) || result.error.contains("deadline exceeded",Qt::CaseInsensitive)?"超时":"未响应"; color=UiTheme::Error; ++completed; break;
        case WebsiteResult::Cancelled: label="已取消"; ++cancelled; break;
        default: break;
        }
        QPixmap dotImage(16,16); dotImage.setDevicePixelRatio(2); dotImage.fill(Qt::transparent); { QPainter painter(&dotImage); painter.setRenderHint(QPainter::Antialiasing); painter.setPen(Qt::NoPen); painter.setBrush(QColor(color)); painter.drawEllipse(QRectF(0,0,8,8)); } card.dot->setPixmap(dotImage);
        if (result.measured>last) last=result.measured;
        card.box->setAccessibleName(card.test->accessibleName().mid(3)+" · "+label+(result.delay>=0?QString(" · %1 毫秒").arg(result.delay):QString()));
        card.value->setText(result.delay>=0?QString::number(result.delay)+" ms":"—"); card.value->setStyleSheet(QString("color:%1;font-weight:600;").arg(color)); card.state->setText(label);
        card.box->setToolTip((result.error.isEmpty()?QString("通过指定节点测试 HTTP 响应，不判断服务账号或地区限制"):result.error)+(result.measured.isValid()?"\n测试时间："+result.measured.toString("HH:mm:ss"):QString())); card.test->setText(result.status==WebsiteResult::Untested?"测试":"重测"); card.test->setEnabled(allowed && !running);
    }
    lastTestIcon_->setPixmap(UiIcons::icon(UiIcons::Glyph::Clock).pixmap(14,14));
    lastTest_->setText(last.isValid()?"上次测试 "+last.toString("MM-dd HH:mm:ss"):QString("尚未测试"));
    const auto summary=tests->node().isEmpty()?QString("%1 个常见目标 · 按需测试").arg(cardWidgets_.size()):QString("%1 个目标 · %2 项完成 · %3 项已响应").arg(cardWidgets_.size()).arg(completed).arg(responded)+(cancelled?QString(" · %1 项取消").arg(cancelled):QString());
    progress_->setText(reason.isEmpty()?summary:reason);
}
void WebsiteTestsPage::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); reflow(); }
bool WebsiteTestsPage::eventFilter(QObject* watched,QEvent* event) {
    if (event->type()==QEvent::Resize) reflow();
    return QWidget::eventFilter(watched,event);
}
void WebsiteTestsPage::reflow() {
    auto* scroll=findChild<QScrollArea*>("websiteScroll"); const int available=scroll?scroll->viewport()->width():width();
    const int cardWidth=qMax(236,fontMetrics().horizontalAdvance("9999 ms 已响应 重测")+80);
    const int columns=qBound(1,(available+12)/(cardWidth+12),3); if (columns==columns_) return; columns_=columns;
    cards_->setProperty("columns",columns); grid_->setSpacing(12);
    for (const auto& card:cardWidgets_) grid_->removeWidget(card.box);
    for (int c=0;c<4;++c) grid_->setColumnStretch(c,c<columns?1:0);
    for (int i=0;i<cardWidgets_.size();++i) grid_->addWidget(cardWidgets_[i].box,i/columns,i%columns);
    grid_->setRowStretch(12,1);
}
}
