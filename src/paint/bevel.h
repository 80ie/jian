#pragma once

#include <QColor>
#include <QPoint>
#include <QRect>

class QPainter;
struct Palette;

namespace paint {

constexpr int frameWidth = 7;

// Each painter takes the rect it occupies, its bevel included.
void raised(QPainter& p, const QRect& r, const Palette& palette);
// The last row and column are a light lip outside the outline.
void sunken(QPainter& p, const QRect& r, const Palette& palette, const QColor& floor);
void trough(QPainter& p, const QRect& r, const Palette& palette);
// Five rows tall.
void groove(QPainter& p, const QRect& r, const Palette& palette);
void button(QPainter& p, const QRect& r, const Palette& palette, bool down);
void thumb(QPainter& p, const QRect& r, const Palette& palette);
// The inner edge of a panel docked to `side`: two nested bevels around a face channel.
void frame(QPainter& p, const QRect& r, const Palette& palette, Qt::Edge side);
// Two rows tall.
void divider(QPainter& p, const QRect& r, const Palette& palette);
// One 2x2 mark of a scale.
void tick(QPainter& p, QPoint at, const Palette& palette);

} // namespace paint
