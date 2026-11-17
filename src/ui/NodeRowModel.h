#pragma once
#include "NodePresentation.h"
#include <QtWidgets>

namespace mlash {
// 节点行列表（H3 / 原型 01）：一张列表卡片里两段「手动选择 / 自动策略组」。
// 分组不是独立模型：过滤结果（NodeFilter）提供成员，NodeRowModel 把它们拍平成
// 「组头行 + 成员行」交给 QListView。折叠、计数与选路说明因此只在一处计算。
class NodeRowModel final : public QAbstractTableModel {
public:
    // data() 的自定义角色：委托据此区分行类型并取副标题。
    enum Role {
        RowKind = Qt::UserRole + 20,   // 0 组头 / 1 成员
        IsAutoGroup, IsCurrent, RegionCode, LatencyText, Subtitle, GroupNote, GroupCollapsed, SourceName, SourceStamp
    };
    explicit NodeRowModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    // source 为 NodeFilter（任何以 UserRole 0 列给出成员名的模型都可以）。
    void setSource(QAbstractItemModel* source) {
        source_ = source;
        if (source_) {
            connect(source_, &QAbstractItemModel::modelReset, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::layoutChanged, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::rowsInserted, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::rowsRemoved, this, &NodeRowModel::rebuild);
            connect(source_, &QAbstractItemModel::dataChanged, this, [this] { if (!rows_.isEmpty()) emit dataChanged(index(0, 0), index(rows_.size() - 1, 0)); });
        }
        rebuild();
    }
    // 组类型决定整段归入「手动选择」还是「自动策略组」（Selector 手动，URLTest/Fallback 自动）。
    void setGroupAutomatic(bool automatic) { if (groupAutomatic_ == automatic) return; groupAutomatic_ = automatic; rebuild(); }
    void setGroupContext(const QString& name,const QString& current) { if (groupName_==name&&groupCurrent_==current) return; groupName_=name; groupCurrent_=current; if (!rows_.isEmpty()) emit dataChanged(index(0,0),index(0,0)); }
    void setSourceContext(const QString& name,const QString& stamp) { sourceName_=name; sourceStamp_=stamp; if (!rows_.isEmpty()) emit dataChanged(index(0,0),index(0,0)); }
    bool groupAutomatic() const { return groupAutomatic_; }
    void setCollapsed(bool automaticGroup, bool collapsed) {
        auto& value = automaticGroup ? autoCollapsed_ : manualCollapsed_;
        if (value == collapsed) return; value = collapsed; rebuild();
    }
    bool collapsed(bool automaticGroup) const { return automaticGroup ? autoCollapsed_ : manualCollapsed_; }
    void setCurrent(const QString& name) { if (current_ == name) return; current_ = name; if (!rows_.isEmpty()) emit dataChanged(index(0, 0), index(rows_.size() - 1, 0)); }
    QString current() const { return current_; }
    bool rowIsHeader(int row) const { return rows_.value(row).member < 0; }
    bool rowAutomatic(int row) const { return rows_.value(row).group == 1; }
    QString memberAt(int row) const { const auto& value = rows_.value(row); return value.member < 0 ? QString() : members_.value(value.member); }
    int rowOfMember(const QString& name) const { const int member = members_.indexOf(name); if (member < 0) return -1; for (int i = 0; i < rows_.size(); ++i) if (rows_[i].member == member) return i; return -1; }
    QModelIndex indexForRow(int row) const { return index(row, 0); }
    void rebuild() {
        beginResetModel();
        members_.clear();
        if (source_) for (int row = 0; row < source_->rowCount(); ++row) members_.append(source_->data(source_->index(row, 0), Qt::UserRole).toString());
        rows_.clear();
        // 空段不留组头：只有成员非空才写组头。
        const bool automatic = groupAutomatic_;
        if (!members_.isEmpty()) {
            rows_.append({automatic ? 1 : 0, -1});
            if (!collapsed(automatic)) for (int i = 0; i < members_.size(); ++i) rows_.append({automatic ? 1 : 0, i});
        }
        endResetModel();
    }
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : rows_.size(); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 1; }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= rows_.size()) return {};
        const auto& row = rows_[index.row()];
        const bool automatic = row.group == 1;
        if (row.member < 0) {
            switch (role) {
            case RowKind: return 0;
            case Qt::DisplayRole: return groupName_.isEmpty()?(automatic?QString("自动策略组"):QString("手动选择")):groupName_;
            case SourceName: return sourceName_;
            case SourceStamp: return sourceStamp_;
            case GroupNote: return QString("%1 个").arg(members_.size());
            case GroupCollapsed: return collapsed(automatic);
            case Subtitle: return groupCurrent_.isEmpty()?QString("尚未选定成员"):QString("当前 → ")+groupCurrent_;
            case IsAutoGroup: return automatic;
            default: return {};
            }
        }
        const auto name = members_.value(row.member);
        // members_ 与 source_ 的行是同序一一对应（rebuild 按行号填充），所以 row.member
        // 就是源行号：这里必须直接用它，按名字线性查找会让渲染退化成 O(n²)。
        const auto column = [this, &row](int value) { return source_->index(row.member, value); };
        switch (role) {
        case RowKind: return 1;
        case IsAutoGroup: return automatic;
        case IsCurrent: return name == current_;
        case RegionCode: return NodePresentation::location(name).code;
        case LatencyText: return source_->data(column(2), Qt::DisplayRole);
        case Subtitle: {
            const auto type = source_->data(column(1), Qt::DisplayRole).toString();
            return type == QStringLiteral("策略组 ›") ? QString("子策略组") : type;
        }
        case Qt::AccessibleTextRole: return name+" · "+data(index,Subtitle).toString()+" · "+data(index,LatencyText).toString()+(name==current_?QString(" · 当前成员"):QString());
        case Qt::AccessibleDescriptionRole: return automatic?QString("自动选路，Enter 不修改成员；F5 测试，Ctrl+I 查看详情"):QString("Enter 选择成员，F5 测试，Ctrl+I 查看详情");
        case Qt::DisplayRole: return name;
        case Qt::ToolTipRole: return source_->data(column(0), Qt::ToolTipRole);
        default: return {};
        }
    }
private:
    struct Row { int group = 0; int member = -1; };
    QAbstractItemModel* source_ = nullptr;
    QStringList members_;
    QVector<Row> rows_;
    QString current_,groupName_,groupCurrent_,sourceName_,sourceStamp_;
    bool groupAutomatic_ = false, manualCollapsed_ = false, autoCollapsed_ = false;
};
}
