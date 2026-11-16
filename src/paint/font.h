#pragma once

#include <QFont>

namespace paint {

constexpr int textPoints = 10;

// The family and weight of `desktop`, which is the desktop's own font, at `points`. The panel
// takes only those two from the desktop and sets the size itself, as Photon's text is small.
inline QFont font(const QFont& desktop, int points)
{
    return QFont(desktop.family(), points, desktop.weight());
}

} // namespace paint
