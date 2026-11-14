#include "panel/slotlook.h"
#include "paint/bevel.h"
#include "palette/palette.h"

#include <QPainter>

namespace {

void tile(QPainter& p, const QRect& r, const Palette& palette)
{
    p.fillRect(r, palette.tone(0));
    paint::raised(p, r, palette);
}

// `raised` turned half round: shaded at the top and left, lit at the bottom and right.
void recessed(QPainter& p, const QRect& r, const Palette& palette)
{
    p.save();
    p.setTransform(QTransform(-1, 0, 0, -1, r.left() + r.right(), r.top() + r.bottom()), true);
    paint::raised(p, r, palette);
    p.restore();
}

void groove(QPainter& p, const QRect& gap, const Palette& palette)
{
    paint::groove(p, QRect(gap.left(), gap.center().y() - 2, gap.width(), 5), palette);
}

} // namespace

SlotLook slotLook(Separation separation)
{
    switch (separation) {
    case Separation::Tiles:
        return { 3, 5, 3, -2, tile, nullptr };
    case Separation::Bays:
        return { 2, 5, 1, 0, recessed, nullptr };
    case Separation::Grooves:
        return { 0, 13, 0, 0, nullptr, groove };
    }
    return { };
}
