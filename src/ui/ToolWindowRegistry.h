#pragma once
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QWidget>
#include <functional>

namespace mlash {

// 工具窗口注册表（界面分层设计 4「工具窗口骨架」/ 评审 N15）。
//
// 现状不一致：TUN、内核更新、本地与远程备份用 findChild 复用，DNS、节点编辑、
// 快捷键等每次 new，同一对象可能同时开出两个窗口，后开的那个还压着先开的。
//
// 复用键 = 工具 + 对象。同一键第二次打开时把已有窗口 raise 起来，不新建；
// 对象被删除或切换数据源时，调用方 closeTool() 让窗口关闭。
class ToolWindowRegistry {
public:
    // factory 只在需要新建时调用。返回的窗口不归注册表所有以外的任何对象，
    // 但注册表只持有 QPointer：窗口自行销毁时条目自动失效。
    QWidget* show(const QString& tool, const QString& objectKey, const std::function<QWidget*()>& factory);
    // 关闭并移除某一工具的全部窗口（objectKey 为空时匹配该工具的所有对象）。
    void close(const QString& tool, const QString& objectKey = QString());
    // 关闭该工具下「对象已不在 alive 中」的窗口：订阅被删后，指向它的工具窗口不能继续存活。
    void closeMissing(const QString& tool, const QSet<QString>& alive);
    bool contains(const QString& tool, const QString& objectKey = QString()) const;
    int count() const { return windows_.size(); }

private:
    struct Entry { QString tool; QString object; QPointer<QWidget> window; };
    static QString key(const QString& tool, const QString& objectKey) { return tool + '\x1f' + objectKey; }
    QHash<QString, Entry> windows_;
};

} // namespace mlash
