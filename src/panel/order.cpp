#include "panel/order.h"

#include <algorithm>

QStringList placed(QStringList order, const QString& id, const QString& before)
{
    if (id == before)
        return order;
    order.removeAll(id);
    const qsizetype at = order.indexOf(before);
    order.insert(at < 0 ? order.size() : at, id);
    return order;
}

QStringList unplaced(const QStringList& offered, const QStringList& order)
{
    QStringList left;
    std::ranges::copy_if(offered, std::back_inserter(left),
        [&order](const QString& id) { return !order.contains(id); });
    return left;
}

qsizetype gapAt(std::span<const QRect> stack, int y)
{
    return std::ranges::find_if(stack, [y](const QRect& slot) { return y < slot.center().y(); })
        - stack.begin();
}
