#include "DialogDraftGuard.h"
#include "HotkeysDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScrollArea>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QKeySequence>
#include <functional>
#include <utility>
#include <QMap>
#include <QTimer>
namespace mlash {
namespace {
class HotkeyViewport final : public QScrollArea {
public:
    QSize sizeHint() const override { return {480,240}; }
    QSize minimumSizeHint() const override { return {160,80}; }
    bool hasHeightForWidth() const override { return false; }
    int heightForWidth(int) const override { return -1; }
};
class CaptureEdit final : public QLineEdit {
    QPointer<AppController> controller_;
    quint64 token_=0;
public:
    std::function<void(bool)> recordingChanged;
    explicit CaptureEdit(AppController* controller):controller_(controller) {
        setMaxLength(40); setClearButtonEnabled(true); setPlaceholderText("例如 Ctrl+Alt+M；留空关闭");
        connect(controller,&AppController::hotkeyCaptureChanged,this,[this](quint64 token) { if (token_&&token_!=token) stop(); });
        connect(controller,&AppController::hotkeyCaptured,this,[this](quint64 token,const QString& sequence) { if (token_==token&&token_) { stop(); setText(sequence); } });
    }
    ~CaptureEdit() override { recordingChanged={}; stop(); }
    bool recording() const { return token_!=0; }
    bool start() {
        if (!controller_) return false; const auto token=controller_->beginHotkeyCapture(); if (!token) return false;
        token_=token; setReadOnly(true); setFocus(Qt::ShortcutFocusReason); selectAll(); if (recordingChanged) recordingChanged(true); return true;
    }
    void stop() {
        if (!token_) return; const auto token=std::exchange(token_,0); setReadOnly(false); if (controller_) controller_->endHotkeyCapture(token); if (recordingChanged) recordingChanged(false);
    }
protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (!token_) { QLineEdit::keyPressEvent(event); return; }
        if (event->isAutoRepeat()) return;
        if (event->key()==Qt::Key_Escape) { stop(); event->accept(); return; }
        if (event->key()==Qt::Key_Control||event->key()==Qt::Key_Alt||event->key()==Qt::Key_Shift||event->key()==Qt::Key_Meta||event->key()==Qt::Key_AltGr) return;
        const auto sequence=QKeySequence(event->keyCombination()).toString(QKeySequence::PortableText); stop(); setText(sequence); event->accept();
    }
    void focusOutEvent(QFocusEvent* event) override { const auto token=token_; if (token) QTimer::singleShot(0,this,[this,token] { if (token_==token&&!hasFocus()) stop(); }); QLineEdit::focusOutEvent(event); }
    void hideEvent(QHideEvent* event) override { stop(); QLineEdit::hideEvent(event); }
};
}
HotkeysDialog::HotkeysDialog(AppController* controller,QWidget* parent):QDialog(parent) {
    setAttribute(Qt::WA_DeleteOnClose); setObjectName("hotkeysDialog"); setWindowTitle("全局快捷键"); resize(620,440); setMinimumSize(480,340);
    const auto expected=controller->settings().hotkeys; auto* layout=new QVBoxLayout(this);
    auto* note=new QLabel("留空关闭，支持手填或录制单组合。至少包含Ctrl或Alt，可加Shift和字母、数字、F键（F12/Win保留）。网络动作须空闲且状态允许；不自动接管外部代理或安装TUN服务。重载只使用运行版，模式按该版配置初始化。"); note->setWordWrap(true); layout->addWidget(note);
    auto* scroll=new HotkeyViewport; scroll->setObjectName("hotkeyScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); auto* body=new QWidget; auto* fields=new QVBoxLayout(body); fields->setContentsMargins(2,2,2,2); QFormLayout* form=nullptr; bool previousNetwork=true;
    QMap<QString,CaptureEdit*> inputs; QMap<QString,QLabel*> observed; QList<QPushButton*> records;
    for (const auto& action:hotkeyActions()) {
        if (!form||action.network!=previousNetwork) { auto* label=new QLabel(action.network?"网络动作":"打开界面"); label->setObjectName("sectionTitle"); fields->addWidget(label); form=new QFormLayout; form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); fields->addLayout(form); previousNetwork=action.network; }
        auto* row=new QWidget; auto* entry=new QVBoxLayout(row); entry->setContentsMargins(0,0,0,0); entry->setSpacing(2); auto* line=new QHBoxLayout; line->setSpacing(6); auto* input=new CaptureEdit(controller); input->setText(expected.value(action.id).toString()); input->setAccessibleName(action.title+"快捷键"); input->setObjectName(action.id=="show-window"?QString("showWindowHotkey"):action.id=="show-proxy"?QString("showProxyHotkey"):"hotkey_"+action.id); inputs.insert(action.id,input); line->addWidget(input,1);
        auto* record=new QPushButton("录制"); record->setObjectName("recordHotkey_"+action.id); record->setAutoDefault(false); record->setFixedWidth(64); record->setFocusPolicy(Qt::TabFocus); record->setAccessibleName("录制"+action.title); line->addWidget(record); records.append(record); const QPointer<QPushButton> recordGuard(record); input->recordingChanged=[recordGuard,title=action.title](bool recording) { if (recordGuard) { recordGuard->setText(recording?"取消":"录制"); recordGuard->setAccessibleName((recording?QString("取消录制"):QString("录制"))+title); } }; entry->addLayout(line);
        auto* actual=new QLabel; actual->setTextFormat(Qt::PlainText); actual->setProperty("secondary",true); actual->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); actual->setObjectName("registeredHotkey_"+action.id); observed.insert(action.id,actual); entry->addWidget(actual); form->addRow(action.title,row);
    }
    fields->addStretch(); scroll->setWidget(body); layout->addWidget(scroll,1);
    auto* hint=new QLabel; hint->setObjectName("hotkeyHint"); hint->setTextFormat(Qt::PlainText); hint->setWordWrap(true); hint->setMinimumHeight(fontMetrics().lineSpacing()*2+4); layout->addWidget(hint);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); auto* save=actions->button(QDialogButtonBox::Save); save->setText("应用快捷键"); save->setObjectName("saveHotkeys"); actions->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(actions); auto saving=std::make_shared<bool>(false); auto error=std::make_shared<QString>();
    const auto draft=[inputs] { QJsonObject value; for (auto it=inputs.begin();it!=inputs.end();++it) value[it.key()]=it.value()->text(); return value; };
    const auto update=[controller,inputs,observed,records,save,saving,error,hint,draft] {
        QList<HotkeyBinding> parsed; const auto checked=parseHotkeys(draft(),parsed); const auto& state=controller->state(); const bool editable=!*saving&&!state.busy&&!state.exiting&&!state.readOnly;
        for (auto* input:inputs) input->setEnabled(editable); for (auto* record:records) record->setEnabled(editable);
        save->setEnabled(editable&&!controller->capturingHotkey()&&(!checked.ok||checked.data.value("bindings").toObject()!=controller->settings().hotkeys||checked.data.value("bindings").toObject()!=controller->activeHotkeys()||!controller->hotkeyError().isEmpty()));
        for (auto it=observed.begin();it!=observed.end();++it) { const auto actual=controller->activeHotkeys().value(it.key()).toString(); it.value()->setText(actual.isEmpty()?QString("当前未注册"):"已注册："+actual); it.value()->setToolTip(it.value()->text()); }
        const auto status=controller->capturingHotkey()?QString("录制中：按下组合键；Esc取消，移开焦点结束。已绑定键只作输入。"):!error->isEmpty()?*error:!checked.ok?checked.error:!controller->hotkeyError().isEmpty()?controller->hotkeyError():QString("实际已注册 %1 项；未保存内容只在本窗口。留空关闭对应动作。").arg(controller->activeHotkeys().size()); hint->setText(status); hint->setToolTip(status);
    };
    for (auto it=inputs.begin();it!=inputs.end();++it) { auto* input=it.value(); connect(input,&QLineEdit::textChanged,this,[error,update] { error->clear(); update(); }); auto* record=findChild<QPushButton*>("recordHotkey_"+it.key()); connect(record,&QPushButton::clicked,this,[input,error,update] { if (input->recording()) input->stop(); else if (!input->start()) *error="当前不能录制，请等待正在执行的任务或注册完成。"; update(); }); }
    connect(controller,&AppController::stateChanged,this,update); connect(controller,&AppController::hotkeyCaptureChanged,this,[update](quint64) { update(); }); connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(actions,&QDialogButtonBox::accepted,this,[=] { *saving=true; error->clear(); update(); controller->updateHotkeys(draft(),expected); });
    connect(controller,&AppController::operationFinished,this,[=,this](bool ok,const QString& message) { if (!*saving) return; *saving=false; if (ok) accept(); else { *error=message; update(); } });
    DialogDraftGuard::protect(this,[draft,expected] { QList<HotkeyBinding> parsed; const auto checked=parseHotkeys(draft(),parsed); return !checked.ok||checked.data.value("bindings").toObject()!=expected; },[saving] { return *saving; }); update();
}
}
