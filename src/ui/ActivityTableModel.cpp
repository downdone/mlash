#include "ActivityTableModel.h"
#include <QJsonObject>
#include <QStringList>
#include <QDateTime>

namespace mlash {
QString ActivityTableModel::endpoint(QString address,const QString& port) { if (address.isEmpty()) return "—"; if (port.isEmpty()) return address; if (address.contains(':')&&!address.startsWith('[')) address='['+address+']'; return address+':'+port; }
QString ActivityTableModel::processName(const QJsonObject& metadata) { auto process=metadata.value("process").toString(); if (process.isEmpty()) { process=metadata.value("processPath").toString(); process.replace('\\','/'); process=process.section('/',-1); } return process.isEmpty()?QString("—"):process; }
void ActivityTableModel::setRows(QJsonArray rows) {
    if (rows == rows_) return;
    bool same = rows.size() == rows_.size();
    for (int i = 0; same && i < rows.size(); ++i) same = connections_ && rows[i].toObject().value("id") == rows_[i].toObject().value("id");
    if (same) { rows_ = std::move(rows); if (!rows_.isEmpty()) emit dataChanged(index(0,0), index(rows_.size()-1,columnCount()-1)); return; }
    beginResetModel(); rows_ = std::move(rows); endResetModel();
}
QString ActivityTableModel::formatBytes(double value) { QStringList units{"B","KiB","MiB","GiB","TiB"}; int unit=0; value=qMax(0.0,value); while (value>=1024 && unit<units.size()-1) { value/=1024; ++unit; } return QString::number(value,'f',unit?1:0)+" "+units[unit]; }
QVariant ActivityTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (connections_ && orientation == Qt::Horizontal && role == Qt::ToolTipRole) return section == 4 ? "点击按下载量排序" : section == 5 ? "点击按下载速率排序；未知速率单独排序" : "点击表头排序；默认顺序按钮恢复内核返回的顺序";
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return (connections_ ? QStringList{"目标", "网络", "命中规则", "代理链", "上传 / 下载", "速率 ↑ / ↓", "状态", "进程", "来源", "目标 IP", "开始时间", "上传量", "下载量", "上传速率", "下载速率"} : QStringList{"规则类型", "匹配内容", "目标策略"}).value(section);
    return {};
}
QVariant ActivityTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size()) return {};
    const auto row = rows_[index.row()].toObject();
    if (role == Qt::UserRole && connections_) return row.value("id").toString();
    if (role == Qt::UserRole+3 && connections_) return QVariant::fromValue(row);
    if (!connections_) {
        if (role != Qt::DisplayRole && role != Qt::ToolTipRole) return {};
        return row.value(QStringList{"type", "payload", "proxy"}.value(index.column())).toString();
    }
    const auto metadata = row.value("metadata").toObject();
    const auto host = metadata.value("host").toString(); const auto ip = metadata.value("destinationIP").toString();
    auto port = metadata.value("destinationPort").toVariant().toString();
    const auto destination = endpoint(host.isEmpty()?ip:host,port);
    QStringList chain; for (const auto& value : row.value("chains").toArray()) chain.append(value.toString());
    const auto rule = row.value("rule").toString(); const auto payload = row.value("rulePayload").toString();
    const auto rate=[&row](const QString& key) { return row.value(key).isDouble()?formatBytes(row.value(key).toDouble())+"/s":QString("—"); };
    const auto process=processName(metadata);
    const auto started=QDateTime::fromString(row.value("start").toString(),Qt::ISODateWithMs);
    const QStringList cells{destination, metadata.value("network").toString().toUpper(), rule.isEmpty() ? "—" : rule + (payload.isEmpty() ? QString() : " · " + payload), chain.join(" › "), formatBytes(row.value("upload").toDouble()) + " / " + formatBytes(row.value("download").toDouble()), rate("_upload_rate")+" / "+rate("_download_rate"), !row.value("_active").toBool(true)?"离开快照":row.value("_paused").toBool()?"暂停快照":"活动", process,endpoint(metadata.value("sourceIP").toString(),metadata.value("sourcePort").toVariant().toString()),ip.isEmpty()?QString("—"):ip,started.isValid()?started.toLocalTime().toString("MM-dd HH:mm:ss"):QString("—"),formatBytes(row.value("upload").toDouble()),formatBytes(row.value("download").toDouble()),rate("_upload_rate"),rate("_download_rate")};
    if (role==Qt::UserRole+2) return (cells.join(' ')+' '+metadata.value("processPath").toString()+' '+metadata.value("process").toString()+' '+metadata.value("sourceIP").toString()+' '+ip).left(4096);
    if (role==Qt::UserRole+1) {
        switch (index.column()) {
        case ConnectionColumns::Bytes: case ConnectionColumns::Download: return row.value("download").toDouble();
        case ConnectionColumns::Upload: return row.value("upload").toDouble();
        case ConnectionColumns::Rates: case ConnectionColumns::DownloadRate: return row.value("_download_rate").toDouble(-1);
        case ConnectionColumns::UploadRate: return row.value("_upload_rate").toDouble(-1);
        case ConnectionColumns::Started: return started.isValid()?started.toMSecsSinceEpoch():qint64(-1);
        default: return cells.value(index.column());
        }
    }
    if (role == Qt::DisplayRole) return cells.value(index.column());
    if (role == Qt::ToolTipRole) {
        const auto known=[](const QString& value) { return value.isEmpty()?QString("—"):value; };
        return QStringList{"目标："+cells[0],"网络："+cells[1]+" / "+known(metadata.value("type").toString()),"来源："+cells[8],"目的 IP："+known(ip),"命中规则："+cells[2],"代理链："+known(cells[3]),"上传 / 下载："+cells[4],"速率 ↑ / ↓："+cells[5],"状态："+cells[6],"进程："+known(metadata.value("processPath").toString(metadata.value("process").toString())),"开始："+known(row.value("start").toString()),"离开快照："+known(row.value("_left_at").toString()),"采样："+known(row.value("_observed_at").toString())}.join('\n');
    }
    return {};
}
}
