#include "ProxyCoordinator.h"
#include "storage/Storage.h"
#include <QFile>
#include <QJsonDocument>
#include <QDateTime>

namespace mlash {
namespace {
bool validProxySnapshot(const QJsonObject& snapshot) {
    const auto flags = snapshot.value("flags");
    if (snapshot.size() != 4 || !flags.isDouble() || flags.toDouble() < 0 || flags.toDouble() > 15 || flags.toDouble() != flags.toInt()) return false;
    for (const auto* key : {"server", "bypass", "pac"}) if (!snapshot.value(key).isString() || snapshot.value(key).toString().size() > 32768 || snapshot.value(key).toString().contains(QChar::Null)) return false;
    return true;
}
}
ProxyCoordinator::ProxyCoordinator(QString root, std::shared_ptr<PlatformServices> platform)
    : journal_(root + "/recovery/proxy-journal.dat"), platform_(std::move(platform)) {}
bool ProxyCoordinator::hasJournal() const { return QFile::exists(journal_); }
Result ProxyCoordinator::readJournal(QJsonObject& journal) {
    QByteArray bytes, plain;
    auto r = readFile(journal_, bytes); if (!r.ok) return r;
    r = platform_->open(bytes, plain); if (!r.ok) return r;
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(plain, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return Result::fail("恢复日志损坏，未修改系统代理");
    journal = doc.object();
    if (!journal.value("schema").isDouble() || journal.value("schema").toDouble() != 1 || !isId(journal.value("token").toString()) ||
        !QDateTime::fromString(journal.value("time").toString(), Qt::ISODate).isValid() || !journal.value("before").isObject() || !journal.value("written").isObject() ||
        !validProxySnapshot(journal.value("before").toObject()) || !validProxySnapshot(journal.value("written").toObject()) ||
        (journal.contains("previous_written")&&(!journal.value("previous_written").isObject()||!validProxySnapshot(journal.value("previous_written").toObject()))))
        return Result::fail("恢复日志格式不受支持");
    return {};
}
Result ProxyCoordinator::saveJournal(const QJsonObject& journal) {
    QByteArray bytes;
    auto r = platform_->seal(QJsonDocument(journal).toJson(QJsonDocument::Compact), bytes);
    return r.ok ? writeAtomic(journal_, bytes) : r;
}
Result ProxyCoordinator::inspect() {
    auto current = platform_->readProxy(); if (!current.ok) return current;
    QString state = platform_->proxyEnabled(current.data) ? "external" : "disabled";
    if (hasJournal()) {
        QJsonObject journal;
        auto r = readJournal(journal); if (!r.ok) return r;
        if (current.data == journal.value("written").toObject() || (journal.contains("previous_written")&&current.data==journal.value("previous_written").toObject())) state = "owned";
    }
    return {true, {}, {{"state", state}}};
}
Result ProxyCoordinator::replaceOwned(QJsonObject journal,const QJsonObject& current,const QJsonObject& target) {
    auto previous=journal; previous["written"]=current; previous.remove("previous_written");
    journal["previous_written"]=current; journal["written"]=target; auto result=saveJournal(journal); if (!result.ok) { result.data["state"]="owned"; return result; }
    const auto recheck=platform_->readProxy(); if (!recheck.ok||recheck.data!=current) return {false,"更新前代理状态变化，保留恢复日志且未覆盖",{{"state",recheck.ok?"external":"unknown"},{"conflict",recheck.ok}}};
    const auto written=platform_->writeProxy(target); auto observed=platform_->readProxy();
    if (written.ok&&observed.ok&&observed.data==target) { journal.remove("previous_written"); result=saveJournal(journal); if (result.ok) return {true,{},{{"state","owned"}}}; return {false,"代理已写入但日志确认失败，保留过渡日志",{{"state","owned"}}}; }
    if (observed.ok&&observed.data==target) { const auto again=platform_->readProxy(); if (again.ok&&again.data==target) { const auto restored=platform_->writeProxy(current); observed=platform_->readProxy(); if (!restored.ok) return {false,"代理更新与旧目标恢复失败，保留日志",{{"state","unknown"}}}; } else return {false,"恢复更新前代理时检测到外部修改，保留日志",{{"state","external"},{"conflict",true}}}; }
    if (observed.ok&&observed.data==current) { result=saveJournal(previous); return {false,result.ok?"代理更新失败，旧目标保持":"代理更新失败且旧目标保持，过渡日志待恢复",{{"state","owned"}}}; }
    return {false,"代理更新回读不一致，保留外部状态与恢复日志",{{"state",observed.ok?"external":"unknown"},{"conflict",observed.ok}}};
}
Result ProxyCoordinator::acquire(int port, bool allowExternal,const std::optional<QString>& bypass) {
    if (port < 1024 || port > 65535) return Result::fail("系统代理端口不合法");
    auto target=platform_->proxyTarget(port); if (bypass) { const auto checked=normalizeProxyBypass(*bypass); if (!checked.ok) return checked; target["bypass"]=checked.data.value("bypass"); } if (!validProxySnapshot(target)) return Result::fail("系统代理目标格式无效");
    if (hasJournal()) {
        auto r = inspect();
        if (!r.ok) return r;
        QJsonObject journal; r = readJournal(journal); if (!r.ok) return r;
        const auto current=platform_->readProxy(); if (!current.ok) return current;
        if (current.data==journal.value("written").toObject() || (journal.contains("previous_written")&&current.data==journal.value("previous_written").toObject())) { if (current.data==target) return {true,{},{{"state","owned"}}}; return replaceOwned(journal,current.data,target); }
        r = release();
        if (!r.ok) {
            if (!allowExternal || !r.data.value("conflict").toBool()) return r;
            // Explicit new takeover snapshots the external writer's current state, not the old journal.
            if (!QFile::rename(journal_, journal_ + ".superseded-" + newId())) return Result::fail("无法保留旧恢复日志，未接管");
        }
    }
    auto before = platform_->readProxy(); if (!before.ok) return before;
    if (platform_->proxyEnabled(before.data) && !allowExternal)
        return {false, "系统已有代理/PAC/自动检测，需明确允许接管", {{"state", "external"}, {"conflict", true}}};
    if (!validProxySnapshot(before.data) || !validProxySnapshot(target)) return Result::fail("系统代理快照格式不受支持，未写入");
    QJsonObject journal{{"schema", 1}, {"token", newId()}, {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"before", before.data}, {"written", target}};
    auto r = saveJournal(journal); if (!r.ok) return r;
    const auto recheck = platform_->readProxy();
    if (!recheck.ok || recheck.data != before.data) {
        if (!QFile::remove(journal_)) return Result::fail("接管冲突且恢复日志清理失败");
        return {false, "接管前代理被外部修改，未覆盖", {{"state", "external"}}};
    }
    r = platform_->writeProxy(target);
    const auto observed = platform_->readProxy();
    if (r.ok && observed.ok && observed.data == target) return {true, {}, {{"state", "owned"}}};
    auto restore = release();
    return {false, (r.ok ? QString("系统代理写入后回读不一致") : r.error) + (restore.ok ? "；已执行安全恢复" : "；" + restore.error),
        {{"state", restore.ok ? restore.data.value("state").toString("unknown") : "unknown"}}};
}
Result ProxyCoordinator::release() {
    if (!hasJournal()) return inspect();
    QJsonObject journal;
    auto r = readJournal(journal); if (!r.ok) return r;
    const auto observed = platform_->readProxy(); if (!observed.ok) return observed;
    const auto before = journal.value("before").toObject(), written = journal.value("written").toObject();
    if (observed.data != written && (!journal.contains("previous_written")||observed.data!=journal.value("previous_written").toObject())) {
        if (observed.data == before) {
            if (!QFile::remove(journal_)) return Result::fail("恢复日志清理失败");
            return inspect();
        }
        // A new writer has taken ownership. Retain the journal for diagnosis, never overwrite it.
        return {false, "代理已被外部修改，保留外部设置与恢复日志", {{"state", "external"}, {"conflict", true}}};
    }
    const auto recheck = platform_->readProxy();
    if (!recheck.ok || recheck.data != observed.data) return {false, "恢复前代理被外部修改", {{"state", "external"}}};
    r = platform_->writeProxy(before); if (!r.ok) return r;
    const auto confirmed = platform_->readProxy();
    if (!confirmed.ok || confirmed.data != before) return Result::fail("代理恢复回读失败，保留恢复日志");
    if (!QFile::remove(journal_)) return Result::fail("代理恢复完成但日志清理失败");
    return inspect();
}
}
