#include "MainWindow.h"
#include "ViewIds.h"
#include "WindowUiFactories.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "app/AppController.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QHeaderView>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTableWidget>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <algorithm>

namespace mlash {
namespace {
using Glyph=UiIcons::Glyph;
enum class Tone { Idle, Ready, Busy, Error };

struct Activity {
    QString text;
    Tone tone;
};

Activity proxyActivity(const AppState& state) {
    if (state.core==CoreState::Starting || (state.core==CoreState::Running&&!state.ready))
        return {"正在启动",Tone::Busy};
    if (state.core==CoreState::Stopping) return {"正在停止",Tone::Busy};
    if (state.core==CoreState::Failed) return {"启动或运行失败",Tone::Error};
    return state.ready?Activity{"运行中",Tone::Ready}:Activity{"未启动",Tone::Idle};
}

Activity tunnelActivity(const QJsonObject& observation) {
    const auto phase=observation.value("phase").toString();
    if (phase=="connecting") return {"正在连接",Tone::Busy};
    if (phase=="disconnecting") return {"正在清理",Tone::Busy};
    if (observation.value("cleanup_pending").toBool()) return {"待清理",Tone::Error};
    if (observation.value("active").toBool()) return {"已连接",Tone::Ready};
    if (!observation.value("error").toString().isEmpty()) return {"连接失败",Tone::Error};
    return {"未连接",Tone::Idle};
}

class WorkspaceChoice final : public QToolButton {
public:
    WorkspaceChoice(const QString& name,Glyph glyph,QWidget* parent)
        : QToolButton(parent), name_(name), glyph_(glyph) {
        setText(name);
        setCheckable(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        setFixedHeight(UiTheme::SidebarItemHeight);
    }

    void setPresentation(bool collapsed,const Activity& activity) {
        collapsed_=collapsed;
        tone_=activity.tone;
        const auto description=name_+" · "+activity.text;
        if (accessibleName()!=description) {
            setAccessibleName(description);
            setToolTip(description);
            setProperty("observedStatus",activity.text);
        }
        update();
    }
    void setPeer(WorkspaceChoice* peer,bool left) { peer_=peer; left_=left; }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        const int towardPeer=collapsed_?(left_?Qt::Key_Down:Qt::Key_Up):(left_?Qt::Key_Right:Qt::Key_Left);
        if (peer_&&event->key()==towardPeer) {
            peer_->setFocus(Qt::TabFocusReason);
            peer_->click();
            event->accept();
            return;
        }
        QToolButton::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto inset=QRectF(rect()).adjusted(.5,.5,-.5,-.5);
        if (hasFocus()&&property("keyboardFocus").toBool()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QColor(UiTheme::Focus));
            painter.drawRoundedRect(inset,6,6);
        }
        auto font=UiTheme::rowCaptionFont(UiTheme::bodyFont());
        font.setWeight(isChecked()?QFont::DemiBold:QFont::Normal);
        painter.setFont(font);
        const QColor ink(isChecked()?UiTheme::Brand:UiTheme::Ink);
        const int labelWidth=QFontMetrics(font).horizontalAdvance(name_);
        const bool drawLabel=!collapsed_;
        const bool drawIcon=collapsed_||labelWidth+30<=width();
        const int iconSize=drawLabel?12:15;
        const int iconLeft=drawLabel?(width()-(iconSize+3+labelWidth+10))/2:(width()-iconSize)/2;
        if (drawIcon)
            UiIcons::icon(glyph_,ink.name()).paint(&painter,QRect(iconLeft,(height()-iconSize)/2,iconSize,iconSize));
        if (drawLabel) {
            painter.setPen(ink);
            const int labelLeft=drawIcon?iconLeft+iconSize+3:(width()-labelWidth-8)/2;
            painter.drawText(QRect(labelLeft,0,labelWidth+1,height()),Qt::AlignVCenter,name_);
        }
        const QPointF dot(width()-(drawLabel?6:7),drawLabel?height()/2.0:8);
        if (tone_==Tone::Error) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(UiTheme::Error));
            const qreal r=4;
            painter.drawPolygon(QPolygonF{dot+QPointF(0,-r),dot+QPointF(r,0),dot+QPointF(0,r),dot+QPointF(-r,0)});
        } else {
            const QColor color(tone_==Tone::Ready?UiTheme::Success:tone_==Tone::Busy?UiTheme::Warning:UiTheme::Secondary);
            painter.setPen(QPen(color,1.2));
            painter.setBrush(tone_==Tone::Ready?QBrush(color):QBrush(Qt::NoBrush));
            painter.drawEllipse(dot,3,3);
            if (tone_==Tone::Busy) painter.drawLine(dot+QPointF(-1.3,0),dot+QPointF(1.3,0));
        }
    }
private:
    QString name_;
    Glyph glyph_;
    Tone tone_ = Tone::Idle;
    bool collapsed_ = false;
    WorkspaceChoice* peer_ = nullptr;
    bool left_ = true;
};

class WorkspaceSelector final : public QWidget {
public:
    explicit WorkspaceSelector(QWidget* parent) : QWidget(parent) {
        motion_.setDuration(145);
        motion_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&motion_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value) {
            position_=value.toReal();
            update();
        });
    }

    void select(int index,bool collapsed) {
        if (index!=selected_ || collapsed!=collapsed_) {
            motion_.stop();
            const bool slide=isVisible()&&!collapsed&&!collapsed_;
            selected_=index;
            collapsed_=collapsed;
            if (slide) {
                motion_.setStartValue(position_);
                motion_.setEndValue(qreal(selected_));
                motion_.start();
            } else position_=selected_;
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        const QRectF track(rect());
        painter.setBrush(QColor(UiTheme::Control));
        painter.drawRoundedRect(track,8,8);
        QRectF selection=track.adjusted(3,3,-3,-3);
        if (collapsed_) {
            selection.setHeight(selection.height()/2);
            selection.translate(0,selection.height()*position_);
        } else {
            selection.setWidth(selection.width()/2);
            selection.translate(selection.width()*position_,0);
        }
        painter.setPen(QPen(QColor(UiTheme::Border),1));
        painter.setBrush(QColor(UiTheme::Window));
        painter.drawRoundedRect(selection.adjusted(.5,.5,-.5,-.5),6,6);
    }
private:
    QVariantAnimation motion_;
    qreal position_=0;
    int selected_=0;
    bool collapsed_=false;
};

int workspaceIndex(WorkspaceArea area) { return area==WorkspaceArea::Tunnel?1:0; }
}

QWidget* MainWindow::workspaceNavigation() {
    auto* box=new QWidget;
    box->setObjectName("workspaceNavigation");
    auto* layout=new QVBoxLayout(box);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(4);
    auto* separator=new QFrame;
    separator->setObjectName("sidebarDivider");
    separator->setFixedHeight(1);
    layout->addWidget(separator);
    layout->addSpacing(4);
    workspaceSelector_=new WorkspaceSelector(box);
    workspaceSelector_->setObjectName("workspaceSelector");
    workspaceSelector_->setAccessibleName("切换工作区");
    workspaceSegments_=new QBoxLayout(QBoxLayout::LeftToRight,workspaceSelector_);
    workspaceSegments_->setContentsMargins(3,3,3,3);
    workspaceSegments_->setSpacing(0);
    const std::array areas{WorkspaceArea::Proxy,WorkspaceArea::Tunnel};
    for (const auto area:areas) {
        const int index=workspaceIndex(area);
        auto* button=new WorkspaceChoice(index?"隧道":"代理",index?Glyph::Link:Glyph::Globe,workspaceSelector_);
        workspaceChoices_[index]=button;
        button->setObjectName(index?"tunnelWorkspaceChoice":"proxyWorkspaceChoice");
        workspaceSegments_->addWidget(button,1);
        connect(button,&QToolButton::clicked,this,[this,index] { navigate(workspace_.routes[index]); });
    }
    static_cast<WorkspaceChoice*>(workspaceChoices_[0])->setPeer(static_cast<WorkspaceChoice*>(workspaceChoices_[1]),true);
    static_cast<WorkspaceChoice*>(workspaceChoices_[1])->setPeer(static_cast<WorkspaceChoice*>(workspaceChoices_[0]),false);
    layout->addWidget(workspaceSelector_);
    const std::array sections{WorkspacePage::Run,WorkspacePage::Members,WorkspacePage::Traffic,WorkspacePage::Logs};
    const std::array names{"workspaceRun","workspaceMembers","workspaceTraffic","workspaceLogs"};
    for (int i=0;i<int(sections.size());++i) {
        auto* button=new QToolButton(box);
        workspaceLinks_[i]=button;
        button->setObjectName(names[i]);
        button->setProperty("workspaceLink",true);
        button->setCheckable(true);
        button->setIconSize(QSize(16,16));
        button->setFocusPolicy(Qt::StrongFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        button->setFixedHeight(32);
        layout->addWidget(button);
        connect(button,&QToolButton::clicked,this,[this,page=sections[i]] {
            navigate(workspaceRoute(workspace_.area,page));
        });
    }
    return box;
}

void MainWindow::resizeWorkspaceNavigation() {
    if (!workspaceSelector_) return;
    workspaceSegments_->setDirection(sidebarCollapsed_?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
    workspaceSelector_->setFixedHeight((sidebarCollapsed_?2:1)*UiTheme::SidebarItemHeight+6);
    for (auto* button:workspaceLinks_) button->setToolButtonStyle(sidebarCollapsed_?Qt::ToolButtonIconOnly:Qt::ToolButtonTextBesideIcon);
    updateWorkspaceNavigation();
}

void MainWindow::updateWorkspaceNavigation() {
    if (!workspaceSelector_) return;
    const int index=workspaceIndex(workspace_.area);
    static_cast<WorkspaceSelector*>(workspaceSelector_)->select(index,sidebarCollapsed_);
    const auto proxy=proxyActivity(controller_->state());
    const auto tunnel=tunnelActivity(controller_->wireGuardObservation());
    for (int i=0;i<int(workspaceChoices_.size());++i) {
        workspaceChoices_[i]->setChecked(index==i);
        static_cast<WorkspaceChoice*>(workspaceChoices_[i])->setPresentation(sidebarCollapsed_,i?tunnel:proxy);
    }
    const std::array labels{QString("运行"),index?QString("Peer"):QString("节点"),QString("流量"),QString("日志")};
    const std::array glyphs{Glyph::Rocket,index?Glyph::Link:Glyph::Layers,Glyph::Chart,Glyph::List};
    const int page=route_.area==WorkspaceArea::Global?-1:int(workspacePage(route_));
    for (int i=0;i<int(workspaceLinks_.size());++i) {
        auto* button=workspaceLinks_[i];
        const bool active=page==i;
        button->setChecked(active);
        button->setText(labels[i]);
        button->setAccessibleName(labels[i]);
        button->setToolTip(labels[i]);
        button->setIcon(UiIcons::icon(glyphs[i],active?UiTheme::Brand:UiTheme::Secondary));
    }
}

void MainWindow::updateWorkspacePage() {
    if (tunnelControl_&&route_.view==ViewIds::Tunnel) {
        const bool members=route_.anchor==HomeAnchor::Nodes;
        tunnelControl_->setVisible(!members);
        if (auto* peers=tunnelScroll_->findChild<QTableWidget*>("workspaceTunnelPeers")) {
            peers->setColumnHidden(2,!members);
            peers->setColumnHidden(3,!members);
            peers->horizontalHeader()->setVisible(members);
            peers->setFixedHeight((members?peers->horizontalHeader()->sizeHint().height():0)+
                qMin(peers->rowCount(),7)*peers->verticalHeader()->defaultSectionSize()+2);
            for (int row=0;row<peers->rowCount();++row)
                if (auto* handshake=peers->item(row,1)) handshake->setTextAlignment((members?Qt::AlignLeft:Qt::AlignRight)|Qt::AlignVCenter);
        }
    }
    if (dataTabs_ && route_.view==ViewIds::Data) {
        const QSignalBlocker guard(dataTabs_);
        const bool shared=route_.area!=WorkspaceArea::Tunnel;
        dataTabs_->setTabVisible(int(ViewIds::DataTab::Connections),shared);
        dataTabs_->setTabVisible(int(ViewIds::DataTab::Tests),shared);
        if (route_.tab>=0) dataTabs_->setCurrentIndex(route_.tab);
    }
    if (route_.view==ViewIds::Data && logArea_!=route_.area) {
        logQueries_[int(logArea_)]=logSearch_->text();
        logLevels_[int(logArea_)]=logFilter_->currentIndex();
        logArea_=route_.area;
        const QSignalBlocker searchGuard(logSearch_);
        const QSignalBlocker filterGuard(logFilter_);
        logSearch_->setText(logQueries_[int(logArea_)]);
        logFilter_->setCurrentIndex(logLevels_[int(logArea_)]);
        if (auto* filterList=qobject_cast<QListView*>(logFilter_->view())) {
            for (const auto* filter:{"app","wireguard"}) {
                const int index=logFilter_->findData(QString::fromLatin1(filter));
                if (index>=0) filterList->setRowHidden(index,logArea_!=WorkspaceArea::Global);
            }
        }
    }
    emit displayRouteChanged();
}

WorkspaceBookmark MainWindow::workspaceBookmark() const {
    auto bookmark=workspace_;
    bookmark.collapsed=sidebarCollapsed_;
    if (const auto* scroll=qobject_cast<QScrollArea*>(homeScroll_.data())) bookmark.proxyScroll=scroll->verticalScrollBar()->value();
    if (tunnelScroll_) bookmark.tunnelScroll=tunnelScroll_->verticalScrollBar()->value();
    return bookmark;
}

void MainWindow::restoreWorkspaceBookmark(const WorkspaceBookmark& value) {
    workspace_=value;
    sidebarCollapsed_=value.collapsed;
    updateSidebar();
    QTimer::singleShot(0,this,[this,value] {
        if (auto* scroll=qobject_cast<QScrollArea*>(homeScroll_.data())) scroll->verticalScrollBar()->setValue(value.proxyScroll);
        if (tunnelScroll_) tunnelScroll_->verticalScrollBar()->setValue(value.tunnelScroll);
    });
}

QWidget* MainWindow::overviewPage() {
    auto* page=new QWidget;
    page->setObjectName("workspaceOverview");
    auto* content=new QVBoxLayout(page);
    content->setContentsMargins(0,0,0,0);
    content->setSpacing(UiTheme::PanelGap);
    auto* cards=new QHBoxLayout;
    cards->setSpacing(UiTheme::PanelGap);
    struct Summary {
        QLabel* status;
        std::array<QLabel*,3> fields{};
    };
    const auto addSummary=[this,cards](const QString& name,Glyph glyph,WorkspaceArea area,
                                      const char* statusName,const std::array<const char*,3>& keys,
                                      const std::array<const char*,3>& fieldNames) {
        auto* card=WindowParts::panel();
        auto* layout=new QVBoxLayout(card);
        layout->setContentsMargins(14,10,14,12);
        layout->setSpacing(7);
        auto* line=new QHBoxLayout;
        auto* icon=new QLabel;
        icon->setFixedSize(20,20);
        const auto redraw=[icon,glyph] { icon->setPixmap(UiIcons::icon(glyph,UiTheme::Brand).pixmap(20,20)); };
        redraw();
        connect(UiTheme::notifier(),&UiTheme::AppearanceNotifier::changed,icon,redraw);
        line->addWidget(icon);
        auto* title=new QLabel(name);
        title->setObjectName("sectionTitle");
        line->addWidget(title);
        line->addStretch();
        auto* button=new QPushButton("运行 ›");
        button->setProperty("quiet",true);
        button->setAccessibleName(area==WorkspaceArea::Proxy?"打开代理运行页":"打开隧道运行页");
        line->addWidget(button);
        connect(button,&QPushButton::clicked,this,[this,area] { navigate(workspaceRoute(area,WorkspacePage::Run)); });
        layout->addLayout(line);
        auto* status=new QLabel;
        status->setObjectName(statusName);
        status->setTextFormat(Qt::PlainText);
        status->setProperty("secondary",true);
        layout->addWidget(status);
        auto* divider=new QFrame;
        divider->setObjectName("sidebarDivider");
        divider->setFixedHeight(1);
        layout->addWidget(divider);
        Summary summary{status};
        for (int i=0;i<int(keys.size());++i) {
            auto* field=new QHBoxLayout;
            auto* label=new QLabel(QString::fromUtf8(keys[i]));
            label->setProperty("secondary",true);
            field->addWidget(label);
            auto* value=new QLabel;
            value->setObjectName(fieldNames[i]);
            value->setTextFormat(Qt::PlainText);
            value->setWordWrap(true);
            value->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
            value->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
            field->addWidget(value,1);
            layout->addLayout(field);
            summary.fields[i]=value;
        }
        layout->addStretch();
        cards->addWidget(card,1);
        return summary;
    };
    const auto proxy=addSummary("本地代理",Glyph::Globe,WorkspaceArea::Proxy,"overviewProxyStatus",
        {"配置","路由 / 节点","本地入口"},{"overviewProxyProfile","overviewProxyRoute","overviewProxyEndpoint"});
    const auto tunnel=addSummary("分流隧道",Glyph::Link,WorkspaceArea::Tunnel,"overviewTunnelStatus",
        {"配置","分流网段","最近握手"},{"overviewTunnelProfile","overviewTunnelRoutes","overviewTunnelHandshake"});
    content->addLayout(cards);
    auto* footer=new QHBoxLayout;
    footer->setContentsMargins(2,0,2,0);
    auto* direct=new QLabel("Windows 直连 · 系统选路 · 未统计");
    direct->setObjectName("overviewDirectStatus");
    direct->setProperty("secondary",true);
    direct->setWordWrap(true);
    footer->addWidget(direct,1);
    auto* traffic=new QPushButton("流量总览 ›");
    traffic->setProperty("quiet",true);
    footer->addWidget(traffic);
    content->addLayout(footer);
    connect(traffic,&QPushButton::clicked,this,[this] { navigate(Route{ViewIds::Data,kNoCategory,HomeAnchor::Control,int(ViewIds::DataTab::Traffic)}); });
    content->addStretch(1);
    const auto refresh=[this,proxy,tunnel] {
        const auto& app=controller_->state();
        const auto setFields=[](const Summary& card,const std::array<QString,3>& values) {
            for (int i=0;i<int(values.size());++i) {
                card.fields[i]->setText(values[i]);
                card.fields[i]->setToolTip(values[i]);
            }
        };
        proxy.status->setText(proxyActivity(app).text);
        const auto profileName=activeProfileName();
        const auto mode=app.mode=="rule"?QString("规则模式"):app.mode=="global"?QString("全局模式"):
            app.mode=="direct"?QString("直连模式"):QString("模式未确认");
        const auto node=controller_->homeNode();
        setFields(proxy,{
            profileName.isEmpty()?WindowParts::profileSetupCopy(!controller_->profiles().isEmpty()).title:
                app.ready?profileName:"已选用 · "+profileName,
            app.ready?mode+(node.isEmpty()?QString():" · "+node):QString("未生效"),
            app.ready?QString("127.0.0.1:%1").arg(controller_->settings().mixedPort):QString("未监听")});

        const auto seen=controller_->wireGuardObservation();
        const auto& catalog=controller_->wireGuardProfiles();
        tunnel.status->setText(tunnelActivity(seen).text);
        const bool active=seen.value("active").toBool()&&seen.value("phase").toString().isEmpty();
        const auto id=seen.value("profile_id").toString();
        const auto running=std::find_if(catalog.cbegin(),catalog.cend(),[&id](const auto& item) { return item.id==id; });
        QDateTime latest;
        if (active) for (const auto& peer:seen.value("observation").toObject().value("peers").toArray()) {
            const auto stamp=QDateTime::fromString(peer.toObject().value("last_handshake").toString(),Qt::ISODate);
            if (stamp.isValid()&&(!latest.isValid()||stamp>latest)) latest=stamp;
        }
        setFields(tunnel,{
            active?(running!=catalog.cend()?running->name:QString("运行配置未确认")):
                catalog.isEmpty()?QString("还没有配置"):QString("已保存 %1 份").arg(catalog.size()),
            active?QString("已回读 %1 条").arg(seen.value("routes").toArray().size()):
                seen.value("cleanup_pending").toBool()?QString("清理待确认"):QString("未生效"),
            active?(latest.isValid()?latest.toLocalTime().toString("MM-dd HH:mm:ss"):QString("等待握手")):QString("—")});
    };
    connect(controller_,&AppController::stateChanged,page,refresh);
    connect(controller_,&AppController::profilesChanged,page,refresh);
    connect(controller_,&AppController::proxiesChanged,page,refresh,Qt::QueuedConnection);
    connect(controller_,&AppController::wireGuardChanged,page,refresh);
    connect(this,&MainWindow::displayRouteChanged,page,refresh);
    refresh();
    return page;
}
} // namespace mlash
