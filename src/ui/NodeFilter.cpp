#include "ProxyTableModel.h"
#include "NodePresentation.h"

namespace mlash {
bool NodeFilter::filterAcceptsRow(int row,const QModelIndex& parent) const {
    if (!QSortFilterProxyModel::filterAcceptsRow(row,parent)) return false;
    const auto name=sourceModel()->index(row,0,parent).data(Qt::UserRole).toString(),protocol=sourceModel()->index(row,1,parent).data(Qt::UserRole).toString(); const auto delay=sourceModel()->index(row,2,parent).data(Qt::UserRole).toInt();
    const bool known=delay>0&&delay<1000000;
    return (region_.isEmpty()||NodePresentation::location(name).code==region_)&&(protocol_.isEmpty()||protocol.compare(protocol_,Qt::CaseInsensitive)==0)&&(health_==0||(health_==1&&known)||(health_==2&&delay==1000001)||(health_==3&&delay==1000000))&&(!maximum_||(known&&delay<=maximum_))&&(!favoritesOnly_||favorites_.contains(name));
}
}
