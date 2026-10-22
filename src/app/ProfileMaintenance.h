#pragma once
#include "Types.h"
#include <QDateTime>
#include <QHash>
#include <QFileSystemWatcher>
#include <QSet>
#include <QTimer>

namespace mlash {
// Scheduling only; AppController owns every download, validation and index mutation.
class ProfileMaintenance : public QObject {
    Q_OBJECT
public:
    explicit ProfileMaintenance(QObject* parent = nullptr);
    void setProfiles(const QVector<Profile>& profiles);
    void setEnabled(bool enabled);
    QString takeDue(const QDateTime& now = QDateTime::currentDateTimeUtc());
    void completed(const QString& id, bool ok, const QDateTime& now = QDateTime::currentDateTimeUtc());
    QJsonObject status(const QString& id) const;
signals:
    void workAvailable();
private:
    QVector<Profile> profiles_;
    QFileSystemWatcher watcher_;
    QTimer tick_, debounce_;
    QSet<QString> dirty_;
    QHash<QString, QDateTime> retryAt_;
    QHash<QString, int> failures_;
    bool enabled_ = false;
};
}
