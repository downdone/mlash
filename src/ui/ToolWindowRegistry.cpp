#include "ToolWindowRegistry.h"

namespace mlash {

QWidget* ToolWindowRegistry::show(const QString& tool, const QString& objectKey, const std::function<QWidget*()>& factory) {
    const auto id = key(tool, objectKey);
    if (auto it = windows_.find(id); it != windows_.end()) {
        // QPointer 失效说明窗口已自行销毁（WA_DeleteOnClose / 对象被删），当作没有。
        if (it->window) { it->window->show(); it->window->raise(); it->window->activateWindow(); return it->window; }
        windows_.erase(it);
    }
    auto* window = factory ? factory() : nullptr;
    if (!window) return nullptr;
    windows_.insert(id, Entry{tool, objectKey, window});
    return window;
}

void ToolWindowRegistry::close(const QString& tool, const QString& objectKey) {
    QList<QString> remove;
    for (auto it = windows_.begin(); it != windows_.end(); ++it) {
        if (it->tool != tool) continue;
        if (!objectKey.isEmpty() && it->object != objectKey) continue;
        if (it->window) it->window->close();
        remove.append(it.key());
    }
    for (const auto& id : remove) windows_.remove(id);
}

void ToolWindowRegistry::closeMissing(const QString& tool, const QSet<QString>& alive) {
    QList<QString> remove;
    for (auto it = windows_.begin(); it != windows_.end(); ++it) {
        if (it->tool != tool) continue;
        // 单例工具没有对象键，不参与对象存活判断。
        if (it->object.isEmpty()) continue;
        if (alive.contains(it->object)) continue;
        if (it->window) it->window->close();
        remove.append(it.key());
    }
    for (const auto& id : remove) windows_.remove(id);
}

bool ToolWindowRegistry::contains(const QString& tool, const QString& objectKey) const {
    if (objectKey.isEmpty()) {
        for (const auto& entry : windows_) if (entry.tool == tool && entry.window) return true;
        return false;
    }
    const auto it = windows_.constFind(key(tool, objectKey));
    return it != windows_.constEnd() && it->window;
}

} // namespace mlash
