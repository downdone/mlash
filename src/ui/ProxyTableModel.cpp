#include "ProxyTableModel.h"
#include "UiTheme.h"
#include <QJsonArray>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QGuiApplication>

namespace mlash {
int ProxyTableModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : names_.size(); }
int ProxyTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : 4; }
QVariant ProxyTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return QStringList{"成员名称", "类型", "延迟", "状态"}.value(section);
    if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole) return int((section == 2 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    return {};
}
QVariant ProxyTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= names_.size()) return {};
    const auto name = names_[index.row()];
    const bool group = proxies_.value(name).toObject().value("all").isArray();
    if (role==Qt::UserRole+8) return favorites_.contains(name);
    if (role == Qt::TextAlignmentRole) return int((index.column() == 2 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    if (role == Qt::ForegroundRole && index.column() == 2 && delays_.contains(name)) {
        if (measured_.value(name).secsTo(QDateTime::currentDateTimeUtc())>=60) return QBrush(QColor(UiTheme::Secondary));
        const auto delay = delays_[name]; return QBrush(QColor(delay < 0 ? UiTheme::Error : delay <= 200 ? UiTheme::Success : UiTheme::Warning));
    }
    if (role == Qt::UserRole) {
        if (index.column() == 2) return !delays_.contains(name) ? 1000000 : delays_[name] <= 0 ? 1000001 : delays_[name];
        if (index.column() == 3) return delays_.contains(name) ? delays_[name] > 0 ? 2 : 3 : group ? 1 : 0;
        return index.column() == 0 ? name : proxies_.value(name).toObject().value("type").toString();
    }
    if (role == Qt::ToolTipRole) return name + "\n类型：" + proxies_.value(name).toObject().value("type").toString("未知") + (measured_.contains(name)?"\n测于："+measured_.value(name).toLocalTime().toString("yyyy-MM-dd HH:mm:ss")+" · 60 秒后视为旧结果":QString()) + (errors_.value(name).isEmpty() ? QString() : "\n" + errors_.value(name));
    if (role == Qt::FontRole && name == now_) { auto font=QGuiApplication::font(); font.setBold(true); return font; }
    if (role != Qt::DisplayRole) return {};
    if (index.column() == 0) return (name == now_ ? "● " : "   ") + (favorites_.contains(name)?QString("★ "):QString()) + name;
    if (index.column() == 1) return group ? "策略组 ›" : proxies_.value(name).toObject().value("type").toString("未知");
    if (index.column() == 3) return (name == now_ ? "当前 · " : "") + (delays_.contains(name) ? measured_.value(name).secsTo(QDateTime::currentDateTimeUtc())>=60?QString("已过期"):delays_[name] > 0 ? QString("已测") : QString("失败") : group ? QString("子策略组") : QString("未测速"));
    if (!delays_.contains(name)) return "—";
    return delays_[name] > 0 ? QString("%1 ms").arg(delays_[name]) : "失败";
}
QString ProxyTableModel::nameAt(int row) const { return names_.value(row); }
void ProxyTableModel::setGroup(const QJsonObject& proxies, const QString& group) {
    const auto object = proxies.value(group).toObject(); QStringList names;
    for (const auto& value : object.value("all").toArray()) names.append(value.toString());
    if (names == names_) {
        const auto previous=proxies_;
        const auto previousNow=now_;
        proxies_=proxies; now_=object.value("now").toString();
        const auto time=QDateTime::currentDateTimeUtc();
        for (int row=0;row<names_.size();++row) {
            const auto& name=names_[row];
            const auto before=previous.value(name).toObject(),after=proxies.value(name).toObject();
            const bool stale=delays_.contains(name)&&measured_.value(name).secsTo(time)>=60;
            const bool expired=stale!=publishedStale_.contains(name);
            if (stale) publishedStale_.insert(name); else publishedStale_.remove(name);
            if ((previousNow!=now_&&(name==previousNow||name==now_)) || before.value("type")!=after.value("type") || before.value("all").isArray()!=after.value("all").isArray())
                emit dataChanged(index(row,0),index(row,3));
            else if (expired) emit dataChanged(index(row,2),index(row,3));
        }
        return;
    }
    beginResetModel(); proxies_ = proxies; names_ = names; now_ = object.value("now").toString(); endResetModel();
}
void ProxyTableModel::setDelay(const QString& name, int delay, const QString& error,const QDateTime& time) {
    if (delays_.contains(name)&&delays_.value(name)==delay&&errors_.value(name)==error&&measured_.value(name)==time) return;
    delays_[name] = delay; errors_[name] = error; measured_[name]=time;
    if (time.secsTo(QDateTime::currentDateTimeUtc())>=60) publishedStale_.insert(name); else publishedStale_.remove(name);
    const int row = names_.indexOf(name); if (row >= 0) emit dataChanged(index(row, 2), index(row, 3));
}
void ProxyTableModel::clearDelays() { if (delays_.isEmpty()) return; delays_.clear(); errors_.clear(); measured_.clear(); publishedStale_.clear(); if (!names_.isEmpty()) emit dataChanged(index(0, 2), index(names_.size() - 1, 3)); }

}
