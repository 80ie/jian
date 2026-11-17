#pragma once

#include <QRect>
#include <QString>

class QPainter;
struct Palette;

namespace paint {

// A closed combo box showing `text`, with the wedge that opens its menu.
void dropDown(
    QPainter& p, const QRect& r, const Palette& palette, const QString& text, bool enabled);

} // namespace paint
