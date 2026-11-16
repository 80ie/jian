#pragma once

#include <QFont>

namespace paint {

constexpr int textPoints = 10;

// Only the family and weight follow the desktop: Photon's text is smaller than a desktop's.
inline QFont font(const QFont& desktop, int points)
{
    return QFont(desktop.family(), points, desktop.weight());
}

} // namespace paint
