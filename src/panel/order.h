#pragma once

#include <QRect>
#include <QStringList>

#include <span>

// `order` with `id` taken from where it is, if it is there, and put back before `before`, or
// last when `before` is empty or not in the order.
QStringList placed(QStringList order, const QString& id, const QString& before);
// The widgets in `offered` that `order` does not hold, in the order they are offered.
QStringList unplaced(const QStringList& offered, const QStringList& order);
// Which gap among the slots in `stack`, top to bottom, a drop at `y` goes into: n is the gap before
// the nth slot, and stack.size() the one after the last.
qsizetype gapAt(std::span<const QRect> stack, int y);
