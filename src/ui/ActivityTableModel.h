#pragma once
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include "app/ConnectionColumns.h"

namespace mlash {
class ActivityTableModel final : public QAbstractTableModel {
public:
    explicit ActivityTableModel(bool connections, QObject* parent = nullptr) : QAbstractTableModel(parent), connections_(connections) {}
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : rows_.size(); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : connections_ ? ConnectionColumns::Count : 3; }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void setRows(QJsonArray rows);
    static QString formatBytes(double value);
    static QString endpoint(QString address,const QString& port);
    static QString processName(const QJsonObject& metadata);
private:
    bool connections_;
    QJsonArray rows_;
};
}
