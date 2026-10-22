#pragma once
#include <QLayout>
#include <QList>
#include <QVector>

namespace mlash {
// A bounded toolbar layout: preserve action order, grow the first expanding
// item, and wrap at the actual available width instead of shrinking text.
class WrapLayout final : public QLayout {
public:
    explicit WrapLayout(QWidget* parent=nullptr):QLayout(parent) { setContentsMargins(0,0,0,0); setSpacing(8); }
    ~WrapLayout() override { while (auto* item=takeAt(0)) delete item; }
    void addItem(QLayoutItem* item) override { items_.append(item); }
    int count() const override { return items_.size(); }
    QLayoutItem* itemAt(int index) const override { return items_.value(index); }
    QLayoutItem* takeAt(int index) override { return index>=0&&index<items_.size()?items_.takeAt(index):nullptr; }
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0,0,width,0),false); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize size; for (auto* item:items_) if (!item->isEmpty()) size=size.expandedTo(item->minimumSize());
        const auto m=contentsMargins(); return size+QSize(m.left()+m.right(),m.top()+m.bottom());
    }
    void setGeometry(const QRect& rect) override { QLayout::setGeometry(rect); arrange(rect,true); }
private:
    int arrange(const QRect& rect,bool apply) const {
        const auto m=contentsMargins(); const auto area=rect.adjusted(m.left(),m.top(),-m.right(),-m.bottom());
        int y=area.top(),height=0,width=0; QVector<QLayoutItem*> line;
        const auto preferred=[](QLayoutItem* item) { auto size=item->sizeHint().expandedTo(item->minimumSize()); if (item->expandingDirections().testFlag(Qt::Horizontal)) size.setWidth(qMax(item->minimumSize().width(),120)); return size; };
        const auto flush=[&] {
            if (line.isEmpty()) return;
            int x=area.left(),grow=-1;
            for (int i=0;i<line.size();++i) if (line[i]->expandingDirections().testFlag(Qt::Horizontal)) { grow=i; break; }
            for (int i=0;i<line.size();++i) { const int cell=qMin(area.width(),preferred(line[i]).width()+(i==grow?qMax(0,area.width()-width):0)); if (apply) line[i]->setGeometry(QRect(x,y,cell,height)); x+=cell+spacing(); }
            y+=height+spacing(); line.clear(); height=0; width=0;
        };
        for (auto* item:items_) if (!item->isEmpty()) {
            const auto size=preferred(item); const int next=width+(line.isEmpty()?0:spacing())+size.width();
            if (!line.isEmpty()&&next>area.width()) flush();
            width+=(line.isEmpty()?0:spacing())+size.width(); height=qMax(height,size.height()); line.append(item);
        }
        flush(); return qMax(0,y-area.top()-spacing())+m.top()+m.bottom();
    }
    QList<QLayoutItem*> items_;
};
}
