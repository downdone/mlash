#include "GlobalHotkeys.h"
#include <QSet>
#include <QRegularExpression>
namespace mlash {
const QList<HotkeyAction>& hotkeyActions() {
    // Append only: Windows registration IDs retain these indices.
    static const QList<HotkeyAction> actions{{"show-window","打开主窗口"},{"show-proxy","打开代理页"},{"show-connections","打开连接页"},{"cycle-mode","循环代理模式",true},{"toggle-system-proxy","切换本程序系统代理",true},{"toggle-tun","切换 TUN",true},{"reload-active","重载当前运行版",true}}; return actions;
}
int hotkeyActionIndex(const QString& action) { const auto& values=hotkeyActions(); for (int i=0;i<values.size();++i) if (values[i].id==action) return i; return -1; }

Result parseHotkeys(const QJsonObject& bindings,QList<HotkeyBinding>& parsed) {
    parsed.clear(); QJsonObject normalized; QSet<QString> used;
    if (bindings.size()>hotkeyActions().size()) return Result::fail("超过允许的全局快捷键动作数量");
    for (auto it=bindings.begin();it!=bindings.end();++it) {
        if (hotkeyActionIndex(it.key())<0||!it.value().isString()||it.value().toString().size()>40) return Result::fail("快捷键动作或格式无效");
        const auto value=it.value().toString().trimmed().toUpper(); if (value.isEmpty()) continue;
        const auto parts=value.split('+'); if (parts.size()<2||parts.size()>4) return Result::fail("快捷键使用Ctrl/Alt/Shift加字母、数字或F键");
        HotkeyBinding binding; binding.action=it.key(); binding.key=parts.last();
        if (!QRegularExpression("^[A-Z0-9]$|^F([1-9]|1[0-9]|2[0-4])$").match(binding.key).hasMatch()||binding.key=="F12") return Result::fail("主键需要字母、数字或F1–F24（F12由系统保留）");
        QSet<QString> modifiers;
        for (int i=0;i<parts.size()-1;++i) { const auto mod=parts[i]; if (!QStringList{"CTRL","ALT","SHIFT"}.contains(mod)||modifiers.contains(mod)) return Result::fail("只支持不重复的Ctrl/Alt/Shift修饰键"); modifiers.insert(mod); }
        binding.control=modifiers.contains("CTRL"); binding.alt=modifiers.contains("ALT"); binding.shift=modifiers.contains("SHIFT"); if (!binding.control&&!binding.alt) return Result::fail("全局快捷键至少包含Ctrl或Alt");
        QStringList canonical; if (binding.control) canonical.append("Ctrl"); if (binding.alt) canonical.append("Alt"); if (binding.shift) canonical.append("Shift"); canonical.append(binding.key); binding.sequence=canonical.join('+'); if (used.contains(binding.sequence)) return Result::fail("多个动作不能使用同一快捷键"); used.insert(binding.sequence); normalized[binding.action]=binding.sequence; parsed.append(binding);
    }
    return {true,{},{{"bindings",normalized}}};
}
}
