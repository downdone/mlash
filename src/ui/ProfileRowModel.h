#pragma once
#include "domain/Types.h"
#include "profile/SubscriptionInfo.h"
#include <QAbstractListModel>
#include <QDateTime>
#include <QSortFilterProxyModel>

namespace mlash {
// K7：配置页的行模型。两张列表（远程订阅 / 本地配置）共用这一份快照，
// 各自套一个按 kind 过滤的代理模型；行内容与是否「当前使用」都在这里算，
// ProfileRowDelegate 只负责绘制，行数据由本模型持有。
class ProfileRowModel final : public QAbstractListModel {
public:
    // data() 的自定义角色；列固定为 1。
    enum Role { RowRole = Qt::UserRole + 1, IsCurrent, Kind, ProfileId, Detail, UsageRatio, UsageText };
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
        const bool pending = profile->candidate != profile->active;
        const auto stamp = QDateTime::fromString(profile->updated, Qt::ISODate).toLocalTime();
        const auto when = stamp.isValid() ? stamp.toString("MM-dd HH:mm") : QStringLiteral("尚未获取");
        switch (role) {
        case Qt::DisplayRole: return profile->name;
        case ProfileId: return profile->id;
        case Kind: return profile->sourceType == QStringLiteral("url") ? QStringLiteral("remote") : QStringLiteral("local");
        case IsCurrent: return active;
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
            return summary.isEmpty() ? (profile->sourceType == "url" ? QString() : QStringLiteral("未指定额度")) : summary;
        }
        case Qt::ToolTipRole: return profile->name + "\n" + data(index, Detail).toString() + (profile->updateError.isEmpty() ? QString() : "\n" + profile->updateError);
        default: return {};
        }
    }
private:
    QVector<Profile> profiles_;
    QString activeId_;
    bool ready_ = false;
};
// 按 Kind 角色分流的代理模型：远程订阅与本地配置各一张列表，数据只有一份。
class ProfileKindFilter final : public QSortFilterProxyModel {
public:
    explicit ProfileKindFilter(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}
    void setKind(const QString& kind) {
        if (kind_ == kind) return;
        beginFilterChange(); kind_ = kind; endFilterChange(Direction::Rows);
    }
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        const auto index = sourceModel()->index(row, 0, parent);
        return index.isValid() && index.data(ProfileRowModel::Kind).toString() == kind_;
    }
private:
    QString kind_;
};
}
