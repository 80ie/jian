#pragma once

#include "palette/palette.h"

#include <QColor>

#include <span>
#include <vector>

class QImage;

// A color an image is largely made of, and the share of its pixels nearest to it.
struct ImageColor {
    QColor color;
    double share = 0;

    bool operator==(const ImageColor&) const = default;
};

// The colors `image` is made of, most common first: its pixels scaled down and clustered in
// OKLab, leaving out transparent ones. The same image always gives the same colors.
std::vector<ImageColor> imageColors(const QImage& image);

// Palettes built on `colors`, most common face first. Each color that covers enough of the image
// becomes a face, its lightness and chroma brought into the range the ramp works in, unless it
// then comes out like a face before it. The named colors come from the other colors, chosen by
// their contrast with the face; with no other colors they keep their stock values.
std::vector<Palette> candidates(std::span<const ImageColor> colors);
