#include "paint/dropdown.h"
#include "palette/palette.h"

#include <QPainter>

namespace {

constexpr int textLeft = 4;
constexpr int wedgeCellWidth = 15;

} // namespace

void paint::dropDown(
    QPainter& p, const QRect& r, const Palette& palette, const QString& text, bool enabled)
{
    p.fillRect(r, palette.tone(5));
    p.setPen(palette.tone(-6));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    const int cell = r.right() - wedgeCellWidth - 1;
    p.drawLine(cell, r.top(), cell, r.bottom());
    const QPoint centre(r.right() - 7, r.top() + r.height() / 2);
    for (int row = 0; row < 4; ++row)
        p.fillRect(centre.x() - 4 + row, centre.y() - 2 + row, 8 - 2 * row, 1, palette.ink);

    const QRect field(r.left() + textLeft, r.top() + 1, cell - r.left() - textLeft, r.height() - 2);
    // Clipped as a combo box clips its label, which trims the first glyph's fringe.
    p.save();
    p.setClipRect(field);
    p.setPen(enabled ? palette.ink : palette.tone(-4));
    p.drawText(field, Qt::AlignLeft | Qt::AlignVCenter,
        p.fontMetrics().elidedText(text, Qt::ElideRight, field.width()));
    p.restore();
}
