#include "app/AppController.h"
#include "MainWindow.h"
#include "FeedbackSync.h"
#include "StatusSentence.h"
#include "ViewIds.h"
#include "profile/RuleDraft.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QStyle>
#include <QShortcut>
#include <QMenu>
#include <QScrollArea>
#include <QAction>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QGridLayout>
#include <QTabWidget>
#include <QToolButton>
#include <QTabBar>
#include <QStyledItemDelegate>
#include <QStyleHints>
#include <QStyleOptionButton>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QKeyEvent>
#include <functional>
#include <algorithm>

namespace mlash {
using namespace WindowParts;
void MainWindow::routeOperationFeedback(bool ok, const QString& message, OperationId id) {
    // 页面级结果落到所属页面，不写全局状态栏（P4.1 反馈路由）。目标页此刻不可见时先把
    // 文案缓存进 pageFeedback_，切回该页即可见，因此不会因为「用户当时不在这页」而丢失。
    const auto target = feedbackTarget(id);
    const auto text = ok ? message : QString("操作未完成 · ") + message;
    switch (id) {
    case OperationId::RulesSave:
        // 规则页的结论行是 rulesHint_（未保存/校验错误/读取失败共用），保存结果写回同一处。
        rulesSaving_ = false;
        if (ok) { rulesDirty_ = false; loadRuleDraft(); rulesHint_->clear(); if (route_.view == ViewIds::Rules) controller_->refreshRules(); }
        else rulesHint_->setText(QString("保存失败，修改已保留 · ") + message);
        break;
    case OperationId::SettingsApply:
        settingsApplying_ = false;
        settingsFeedback_ = ok ? QString() : QString("应用失败：") + message;
        break;
    case OperationId::ProxyToggle:
        // 必须先扩到下标 0 再写：pageFeedback_ 首次使用时是空的，operator[] 越界是未定义行为。
        while (pageFeedback_.size() < 1) pageFeedback_.append(QString());
        pageFeedback_[0] = ok ? QString() : text;
        break;
    default: break;
    }
    if (target.kind == FeedbackTarget::PageHint) {
        setPageFeedback(target.page, ok ? QString() : text);
    } else {
        // 跨页操作：仍进「最近操作」段，成功随时间消失，失败保留。
        message_->setText(statusNoticeText(ok, message)); message_->setToolTip(message);
        feedbackTimer_.stop(); recentFeedback_ = ok; feedbackError_ = ok ? QString() : message; noticeOperation_ = id;
        if (ok) feedbackTimer_.start();
    }
    updateState(); updateActions();
}
void MainWindow::setPageFeedback(int page, const QString& text) {
    if (page < 0) return;
    while (pageFeedback_.size() <= page) pageFeedback_.append(QString());
    pageFeedback_[page] = text;
    if (page == 1) {
        // 订阅页的结果写到提交区旁的提示里（profileSubmitHint），对象说明（profileDetails_）
        // 保持不动，因此「哪次更新失败」不会把那行「当前运行版本 · 有无待应用更新」冲掉。
        if (auto* hint = findChild<QLabel*>("profileSubmitHint")) {
            const bool show = route_.view == ViewIds::Profiles && !text.isEmpty();
            if (show && hint->text() != text) hint->setText(text);
            hint->setVisible(show); hint->setToolTip(text);
        }
    }
    if (page == 6 && settingsDraftHint_) settingsDraftHint_->setToolTip(text.isEmpty() ? QString() : text);
    if (page == 0 && homeControlHint_) {
        homeControlHint_->setText(text); homeControlHint_->setToolTip(text); homeControlHint_->setVisible(!text.isEmpty());
        homeControlHint_->setProperty("state", text.isEmpty() ? "idle" : "error");
        homeControlHint_->setAccessibleName(text.isEmpty() ? QString() : QString("代理开关结果：") + text);
    }
    // 连接页的结论行是 connectionsHint_（R5 已建立），这里只借它多带一句操作结论，
    // 采样状态本身仍由 updateConnections 负责，两者不互相覆盖。
    if (page == 4 && connectionsHint_) { connectionFeedback_ = text; updateConnections(); }
}
}
