#pragma once
#include <QAbstractTableModel>
#include <QJsonObject>
#include <QHash>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QSet>

namespace mlash {
class ProxyTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ProxyTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void setGroup(const QJsonObject& proxies, const QString& group);
    void setDelay(const QString& name, int delay, const QString& error,const QDateTime& time=QDateTime::currentDateTimeUtc());
    void setTestContext(const QString& url) { if (testUrl_!=url) { testUrl_=url; clearDelays(); } }
    void setFavorites(const QSet<QString>& names) { if (favorites_==names) return; favorites_=names; if (!names_.isEmpty()) emit dataChanged(index(0,0),index(names_.size()-1,0)); }
    QString nameAt(int row) const;
    QStringList names() const { return names_; }
    void clearDelays();
private:
    QStringList names_;
    QString now_;
    QJsonObject proxies_;
    QHash<QString, int> delays_;
    QHash<QString, QString> errors_;
    QHash<QString,QDateTime> measured_;
    QString testUrl_;
    QSet<QString> favorites_;
    QSet<QString> publishedStale_;
};
class NodeFilter final : public QSortFilterProxyModel {
public:
    explicit NodeFilter(QObject* parent=nullptr):QSortFilterProxyModel(parent) {}
    void setConditions(const QString& region,const QString& protocol,int health,int maximum,bool favoritesOnly,const QSet<QString>& favorites) {
        if (region_==region && protocol_==protocol && health_==health && maximum_==maximum && favoritesOnly_==favoritesOnly && favorites_==favorites) return;
        beginFilterChange(); region_=region; protocol_=protocol; health_=health; maximum_=maximum; favoritesOnly_=favoritesOnly; favorites_=favorites; endFilterChange(Direction::Rows);
    }
protected:
    bool filterAcceptsRow(int row,const QModelIndex& parent) const override;
private:
    QString region_,protocol_; int health_=0,maximum_=0; bool favoritesOnly_=false; QSet<QString> favorites_;
};
}
