#pragma once

#include <QImage>

struct Palette;

namespace paint {

// The panel's frame holding one labelled sample of each painter, then the ramp.
QImage sheet(const Palette& palette);

} // namespace paint
