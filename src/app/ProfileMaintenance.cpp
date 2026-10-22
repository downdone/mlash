#include "ProfileMaintenance.h"
#include <QFileInfo>

namespace mlash {
QJsonObject ProfileMaintenance::status(const QString& id) const {
    for (const auto& p:profiles_) if (p.id==id) {
        auto due=QDateTime::fromString(p.updated,Qt::ISODate).addSecs(p.updateIntervalHours*3600);
        if (retryAt_.contains(id) && (failures_.value(id)>0 || !due.isValid() || retryAt_.value(id)>due)) due=retryAt_.value(id);
        return {{"automatic",p.autoUpdate},{"enabled",enabled_},{"next",p.autoUpdate&&p.sourceType=="url"?due.toString(Qt::ISODate):QString()},{"failures",failures_.value(id)},{"watching_file",p.autoUpdate&&p.sourceType=="file"&&enabled_}};
    }
    return {};
}
ProfileMaintenance::ProfileMaintenance(QObject* parent) : QObject(parent) {
    tick_.setInterval(60000); tick_.setTimerType(Qt::VeryCoarseTimer);
    debounce_.setSingleShot(true); debounce_.setInterval(1000);
    connect(&tick_, &QTimer::timeout, this, &ProfileMaintenance::workAvailable);
    connect(&debounce_, &QTimer::timeout, this, &ProfileMaintenance::workAvailable);
    const auto changed = [this](const QString& path) {
        for (const auto& profile : profiles_) if (profile.autoUpdate && profile.sourceType == "file") {
            const QFileInfo file(profile.source);
            if (path == file.absoluteFilePath() || path == file.absolutePath()) dirty_.insert(profile.id);
        }
        if (enabled_) debounce_.start();
    };
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, changed);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, changed);
}
void ProfileMaintenance::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled) tick_.start(); else { tick_.stop(); debounce_.stop(); dirty_.clear(); }
    setProfiles(profiles_);
}
void ProfileMaintenance::setProfiles(const QVector<Profile>& profiles) {
    profiles_ = profiles;
    QSet<QString> wanted, ids;
    for (const auto& profile : profiles_) {
        ids.insert(profile.id);
        if (enabled_ && profile.autoUpdate && profile.sourceType == "file") {
            const QFileInfo source(profile.source);
            wanted.insert(source.absoluteFilePath()); wanted.insert(source.absolutePath());
        }
    }
    dirty_.intersect(ids);
    for (const auto& id : retryAt_.keys()) if (!ids.contains(id)) { retryAt_.remove(id); failures_.remove(id); }
    const auto watching = watcher_.files() + watcher_.directories();
    for (const auto& path : watching) if (!wanted.contains(path)) watcher_.removePath(path);
    for (const auto& path : wanted) if (!watching.contains(path)) watcher_.addPath(path);
}
QString ProfileMaintenance::takeDue(const QDateTime& now) {
    if (!enabled_) return {};
    // Also notices files changed while Mlash was not running; contents are hashed on a worker.
    for (const auto& profile : profiles_) {
        if (!profile.autoUpdate || (retryAt_.contains(profile.id) && retryAt_[profile.id] > now)) continue;
        const auto updated = QDateTime::fromString(profile.updated, Qt::ISODate);
        const bool due = profile.sourceType == "file" ? dirty_.contains(profile.id) || !retryAt_.contains(profile.id)
            : dirty_.contains(profile.id) || !updated.isValid() || updated > now || updated.addSecs(profile.updateIntervalHours*3600) <= now;
        if (!due) continue;
        dirty_.remove(profile.id); retryAt_[profile.id] = now.addSecs(300);
        return profile.id;
    }
    return {};
}
void ProfileMaintenance::completed(const QString& id, bool ok, const QDateTime& now) {
    if (ok) { failures_.remove(id); retryAt_[id] = now; }
    else { const auto failures = qMin(5, failures_.value(id)+1); failures_[id] = failures; retryAt_[id] = now.addSecs(qMin(3600, 300*(1 << (failures-1)))); dirty_.insert(id); }
    // Reattach the file watch after an editor's atomic rename/replacement.
    setProfiles(profiles_);
}
}
