#pragma once
#include "app/Types.h"
#include <QSet>
#include <QStringList>
#include <QHash>
#include <functional>
namespace mlash {
// Presentation only. Group routes remain unresolved until runtime.
inline Result chainPreview(const QString& selected,const QJsonObject& source,const QJsonObject& draft,const QSet<QString>& nodes,const QSet<QString>& targets,const QJsonObject& groups={}) {
    if (selected.isEmpty()) return {true,{},{{"preview","选择节点以预览草稿路径"}}};
    QHash<QString,int> marks; std::function<bool(const QString&,int)> check=[&](const QString& node,int depth) { if (depth>64||marks.value(node)==1) return false; if (marks.value(node)==2) return true; marks[node]=1; if (nodes.contains(node)) { const auto upstream=draft.contains(node)?draft.value(node).toString():source.value(node).toString(); if (!upstream.isEmpty()&&upstream!="DIRECT"&&!check(upstream,depth+1)) return false; } else for (const auto& value:groups.value(node).toArray()) if (value.toString()!="DIRECT"&&!check(value.toString(),depth+1)) return false; marks[node]=2; return true; };
    if (!check(selected,0)) return Result::fail("草稿链或策略组存在循环或超过64层；请调整上游关系");
    QStringList path{selected}; QSet<QString> visited{selected}; QString cursor=selected,group;
    for (int hop=0;;++hop) {
        const auto next=draft.contains(cursor)?draft.value(cursor).toString():source.value(cursor).toString(); if (next.isEmpty()||next=="DIRECT") break; if (hop>=64) return Result::fail("链超过64层；请调整上游关系");
        if (visited.contains(next)) return Result::fail("草稿链存在循环；请调整上游关系");
        if (!targets.contains(next)) return Result::fail("上游不在当前节点或策略组中；请重新选择");
        visited.insert(next); path.prepend(next); if (!nodes.contains(next)) { group=next; break; } cursor=next;
    }
    QStringList display; for (int i=0;i<path.size();++i) { if (path.size()>4&&i==2) { display.append(QString("… %1 级 …").arg(path.size()-4)); i=path.size()-3; continue; } const auto label=path[i]; display.append((label.size()>24?label.left(23)+"…":label)+(label==group?QString("（策略组）"):QString())); }
    return {true,{},{{"preview","应用 → "+display.join(" → ")+" → 目标"},{"full","应用 → "+path.join(" → ")+" → 目标"+(group.isEmpty()?QString():QString("\n策略组由运行时选择，不推断具体节点。"))}}};
}
}
