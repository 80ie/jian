#pragma once

#include <QColor>

struct Oklab {
    double lightness = 0;
    double a = 0;
    double b = 0;
};

Oklab toOklab(const QColor& color);

// Outside sRGB, chroma is given up until it fits; lightness and hue are kept.
QColor fromOklab(const Oklab& lab);
