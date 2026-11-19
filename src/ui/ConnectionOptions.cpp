#include "app/AppController.h"
#include "ConnectionOptions.h"
#include "ConnectionColumnsPopup.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "domain/ConnectionColumns.h"
#include "storage/Storage.h"
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QEvent>
#include <QFontMetrics>
#include <QSignalBlocker>
namespace mlash {
namespace {
QList<int> defaultConnectionWidths(const QFontMetrics& metrics) {
    QList<int> widths{180,36,95,105,112,112,70,130,150,140,130,100,100,100,100};
    widths[1]=qMax(36,metrics.horizontalAdvance("网络")+12);
    widths[6]=qMax(70,metrics.horizontalAdvance("暂停快照")+16);
    return widths;
}
}
ConnectionOptions::ConnectionOptions(AppController* controller,QTableView* table,QSortFilterProxyModel* filter,QLineEdit* search,QLabel* error,QWidget* parent) : QWidget(parent),controller_(controller),table_(table),filter_(filter),search_(search),error_(error) {
    setObjectName("connectionOptions"); search_->setMaxLength(256); if (error_) { error_->setObjectName("connectionsSearchError"); error_->setTextFormat(Qt::PlainText); error_->setWordWrap(true); error_->hide(); } auto* layout=new QHBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(3);
    const auto toggle=[&](const QString& title,const QString& name,const QString& tooltip) { auto* control=new QToolButton; control->setText(title); control->setObjectName(name); control->setCheckable(true); control->setProperty("quiet",true); control->setToolTip(tooltip); control->setAccessibleName(tooltip); layout->addWidget(control); return control; };
    case_=toggle("Aa","connectionCase","区分大小写"); word_=toggle("整词","connectionWholeWord","按Unicode单词边界匹配"); regex_=toggle(".*","connectionRegex","使用正则表达式"); auto* columns=new QToolButton; columns->setText("列与视图"); columns->setObjectName("connectionColumns"); layout->addWidget(columns);
    columns->setIcon(UiIcons::icon(UiIcons::Glyph::Settings,UiTheme::Brand)); columns->setToolButtonStyle(Qt::ToolButtonIconOnly); columns->setToolTip("连接列设置"); columns->setProperty("prototypeIcon",true);
    columnsPopup_=new ConnectionColumnsPopup(columns);
    auto* header=table_->horizontalHeader(); header->setSectionsMovable(true); header->setMinimumSectionSize(24); header->setStretchLastSection(false); header->setSectionResizeMode(QHeaderView::Interactive); defaults();
    const auto saved=controller_->settings().connectionView; case_->setChecked(saved.value("case_sensitive").toBool()); word_->setChecked(saved.value("whole_word").toBool()); regex_->setChecked(saved.value("regex").toBool()); if (validateConnectionView(saved).ok&&saved.contains("columns")) { custom_=true; const auto rows=saved.value("columns").toArray(); for (int visual=0;visual<rows.size();++visual) { const auto row=rows[visual].toObject(); const auto id=row.value("id").toInt(); header->moveSection(header->visualIndex(id),visual); widths_[id]=row.value("width").toInt(); table_->setColumnWidth(id,widths_[id]); table_->setColumnHidden(id,!row.value("visible").toBool()); } }
    for (int col=0;col<ConnectionColumns::Count;++col) {
        auto* option=columnsPopup_->addColumn(col,table_->model()->headerData(col,Qt::Horizontal).toString(),!table_->isColumnHidden(col),col==ConnectionColumns::Target);
        columnChecks_.append(option);
        connect(option,&QCheckBox::toggled,this,[this,col](bool visible) { table_->setColumnHidden(col,!visible); custom_=true; changed(); });
    }
    auto* reset=columnsPopup_->addFooterAction("恢复默认列","resetConnectionColumns");
    connect(reset,&QPushButton::clicked,this,[this] {
        applying_=true; defaults(); applying_=false; fitDefault();
        for (int col=0;col<columnChecks_.size();++col) { const QSignalBlocker block(columnChecks_[col]); columnChecks_[col]->setChecked(!table_->isColumnHidden(col)); }
        changed();
    });
    connect(columns,&QToolButton::clicked,this,[this,columns] {
        for (int col=0;col<columnChecks_.size();++col) { const QSignalBlocker block(columnChecks_[col]); columnChecks_[col]->setChecked(!table_->isColumnHidden(col)); }
        columnsPopup_->openBelow(columns);
    });
    debounce_.setSingleShot(true); debounce_.setInterval(400); connect(&debounce_,&QTimer::timeout,this,&ConnectionOptions::save); connect(controller_,&AppController::stateChanged,this,[this] { if (dirty_&&!controller_->state().busy&&!controller_->state().exiting) debounce_.start(); });
    for (auto* option:{case_,word_,regex_}) connect(option,&QToolButton::toggled,this,[this] { applySearch(); changed(); }); connect(search_,&QLineEdit::textChanged,this,&ConnectionOptions::applySearch); connect(header,&QHeaderView::sectionMoved,this,[this] { if (!applying_) { custom_=true; changed(); } }); connect(header,&QHeaderView::sectionResized,this,[this](int id,int,int size) { if (applying_||size<24) return; widths_[id]=qBound(24,size,1000); custom_=true; changed(); }); table_->viewport()->installEventFilter(this); table_->installEventFilter(this); applying_=false; fitDefault(); applySearch();
}
ConnectionOptions::~ConnectionOptions() { debounce_.stop(); }
void ConnectionOptions::addViewAction(QAction* action) {
    if (!action || !columnsPopup_) return;
    auto* button=columnsPopup_->addFooterAction(action->text(),"connectionViewAction");
    button->setEnabled(action->isEnabled());
    connect(action,&QAction::changed,button,[action,button] { button->setEnabled(action->isEnabled()); button->setText(action->text()); });
    connect(button,&QPushButton::clicked,action,&QAction::trigger);
}
void ConnectionOptions::flushPreferences() { debounce_.stop(); if (dirty_&&!controller_->state().exiting&&!controller_->state().readOnly) { const auto value=preferences(); dirty_=false; controller_->updateConnectionView(value); } }
void ConnectionOptions::defaults() { custom_=false; const auto sizes=defaultConnectionWidths(table_->fontMetrics()); auto* header=table_->horizontalHeader(); for (int id=0;id<ConnectionColumns::Count;++id) { header->moveSection(header->visualIndex(id),id); table_->setColumnHidden(id,id!=ConnectionColumns::Target&&id!=ConnectionColumns::Rule&&id!=ConnectionColumns::Chain&&id!=ConnectionColumns::State); table_->setColumnWidth(id,sizes[id]); widths_[id]=sizes[id]; } }
void ConnectionOptions::fitDefault() {
    if (custom_||applying_) return; applying_=true; auto sizes=defaultConnectionWidths(table_->fontMetrics());
    const int available=qMax(180,table_->viewport()->width());
    const QList<int> columns{ConnectionColumns::Target,ConnectionColumns::Rule,ConnectionColumns::Chain,ConnectionColumns::State};
    const QList<double> ratios{0.34,0.23,0.28,0.15}; int used=0;
    for (int i=0;i<columns.size();++i) { const int width=i==columns.size()-1?available-used:qMax(24,int(available*ratios[i])); widths_[columns[i]]=width; table_->setColumnWidth(columns[i],width); used+=width; }
    applying_=false;
}
bool ConnectionOptions::eventFilter(QObject* watched,QEvent* event) { if (watched==table_&&(event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange)&&!custom_&&!applying_) { applying_=true; defaults(); applying_=false; fitDefault(); } if (watched==table_->viewport()&&event->type()==QEvent::Resize) fitDefault(); return QWidget::eventFilter(watched,event); }
QJsonObject ConnectionOptions::preferences() const { QJsonObject object{{"case_sensitive",case_->isChecked()},{"whole_word",word_->isChecked()},{"regex",regex_->isChecked()}}; if (custom_) { QJsonArray columns; for (int visual=0;visual<ConnectionColumns::Count;++visual) { const int id=table_->horizontalHeader()->logicalIndex(visual); columns.append(QJsonObject{{"id",id},{"width",qBound(24,widths_.value(id,80),1000)},{"visible",!table_->isColumnHidden(id)}}); } object["columns"]=columns; } return object; }
void ConnectionOptions::changed() { if (applying_) return; dirty_=preferences()!=controller_->settings().connectionView; if (dirty_) debounce_.start(); }
void ConnectionOptions::save() { if (!dirty_||controller_->state().busy||controller_->state().exiting||controller_->state().readOnly) return; const auto value=preferences(); dirty_=false; controller_->updateConnectionView(value); }
void ConnectionOptions::reportSearchError(const QString& error) { if (error_) { error_->setText(error); error_->setVisible(!error.isEmpty()); } if (searchError_!=error) { searchError_=error; emit searchFeedback(error); } }
void ConnectionOptions::applySearch() {
    const auto text=search_->text(); QString pattern=regex_->isChecked()?text:QRegularExpression::escape(text);
    if (regex_->isChecked()&&text.contains("(*")) { reportSearchError("不支持正则控制指令，请使用普通匹配表达式"); return; }
    const auto options=QRegularExpression::UseUnicodePropertiesOption|(case_->isChecked()?QRegularExpression::NoPatternOption:QRegularExpression::CaseInsensitiveOption);
    // Validate the user's expression before adding word-boundary syntax. A
    // dangling '[' can consume a bracket in our suffix and become valid syntax.
    const QRegularExpression input(pattern,options);
    if (!input.isValid()) { reportSearchError("正则表达式无效："+input.errorString()+"；保留上一次有效结果"); return; }
    if (word_->isChecked()&&!text.isEmpty()) pattern="(?<![\\p{L}\\p{N}_])(?:"+pattern+")(?![\\p{L}\\p{N}_])";
    QRegularExpression expression("(*LIMIT_MATCH=1000)(*LIMIT_DEPTH=64)"+pattern,options);
    if (!expression.isValid()) { reportSearchError("正则表达式无效："+expression.errorString()+"；保留上一次有效结果"); return; } reportSearchError({}); filter_->setFilterRegularExpression(expression);
}
}
