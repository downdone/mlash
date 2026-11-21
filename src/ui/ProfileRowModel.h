#pragma once
#include "domain/Types.h"
#include "profile/SubscriptionInfo.h"
#include <QAbstractListModel>
#include <QDateTime>
#include <QFileInfo>
#include <QUrl>

namespace mlash {
// A single ordered snapshot for both URL and file profiles. UI actions use
// ProfileId, never the row number, because an update can rebuild the model.
class ProfileRowModel final : public QAbstractListModel {
public:
    // data() 的自定义角色；列固定为 1。
    enum Role { RowRole = Qt::UserRole + 1, IsCurrent, Kind, ProfileId, Detail, UsageRatio, UsageText,
                SourceLabel, UpdatedLabel, CanRefresh, HasCandidate, UpdateError };
    explicit ProfileRowModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    void setProfiles(QVector<Profile> profiles) { beginResetModel(); profiles_ = std::move(profiles); endResetModel(); }
    void setState(const QString& activeId, bool ready) {
        if (activeId_ == activeId && ready_ == ready) return;
        activeId_ = activeId; ready_ = ready;
        if (!profiles_.isEmpty()) emit dataChanged(index(0), index(profiles_.size() - 1));
    }
    const Profile* profile(int row) const { return row >= 0 && row < profiles_.size() ? &profiles_[row] : nullptr; }
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : profiles_.size(); }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        const auto* profile = this->profile(index.row());
        if (!profile) return {};
        const bool active = !activeId_.isEmpty() && profile->id == activeId_;
        const bool pending = !profile->active.isEmpty() && !profile->candidate.isEmpty() && profile->candidate != profile->active;
        const auto stamp = QDateTime::fromString(profile->updated, Qt::ISODate).toLocalTime();
        const auto when = stamp.isValid() ? stamp.toString("MM-dd HH:mm") : QStringLiteral("尚未获取");
        switch (role) {
        case Qt::DisplayRole: return profile->name;
        case ProfileId: return profile->id;
        case Kind: return profile->sourceType == QStringLiteral("url") ? QStringLiteral("remote") : QStringLiteral("local");
        case IsCurrent: return active;
        case SourceLabel: {
            if (profile->sourceType != "url")
                return profile->source.isEmpty() ? QStringLiteral("本地快照") : QFileInfo(profile->source).fileName();
            const QUrl source(profile->source);
            if (source.host().isEmpty()) return QStringLiteral("远程订阅");
            QUrl publicUrl;
            publicUrl.setScheme(source.scheme());
            publicUrl.setHost(source.host());
            publicUrl.setPort(source.port());
            return publicUrl.toDisplayString();
        }
        case UpdatedLabel: return stamp.isValid() ? when : QString();
        case CanRefresh: return !profile->source.isEmpty();
        case HasCandidate: return pending;
        case UpdateError: return profile->updateError;
        case Detail: {
            QString detail = profile->sourceType == "url"
                ? QString("远程订阅 · %1 · 每 %2 小时检查").arg(when).arg(profile->updateIntervalHours)
                : QString("本地配置 · %1").arg(when);
            if (active) detail += ready_ ? QStringLiteral(" · 正在运行") : QStringLiteral(" · 已选用 · 未运行");
            else detail += QStringLiteral(" · 尚未使用");
            if (pending) detail += QStringLiteral(" · 有更新待应用");
            if (!profile->updateError.isEmpty()) detail += QStringLiteral(" · 更新失败，保留原版本");
            return detail;
        }
        case UsageRatio: {
            const auto& info = profile->subscriptionInfo;
            if (!SubscriptionInfo::valid(info) || !info.contains("total")) return 0.0;
            const auto total = info.value("total").toString().toLongLong();
            if (total <= 0) return 0.0;
            const auto used = info.value("upload").toString().toLongLong() + info.value("download").toString().toLongLong();
            return double(used) / double(total);
        }
        case UsageText: {
            const auto summary = SubscriptionInfo::summary(profile->subscriptionInfo);
            return summary;
        }
        case Qt::ToolTipRole: return profile->name + "\n点击卡片打开配置内容\n" + data(index, Detail).toString() + (profile->updateError.isEmpty() ? QString() : "\n" + profile->updateError);
        default: return {};
        }
    }
private:
    QVector<Profile> profiles_;
    QString activeId_;
    bool ready_ = false;
};
}
