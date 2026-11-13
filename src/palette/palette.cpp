#include "palette/palette.h"
#include "palette/oklab.h"

#include <algorithm>
#include <array>

namespace {

// OKLab lightness of each tone, darkest first, relative to the face at contrast 1.
constexpr std::array lightnessOffsets { -0.376, -0.276, -0.176, -0.106, -0.066, -0.031, 0.0, 0.034,
    0.094, 0.144, 0.200, 0.234 };
static_assert(lightnessOffsets.size() == lightestTone - darkestTone + 1);

// Chroma tapers linearly to zero at black and at white, as mixing in black or white does.
double chromaScale(double lightness, double faceLightness)
{
    if (lightness < faceLightness)
        return lightness / faceLightness;
    if (lightness > faceLightness)
        return (1 - lightness) / (1 - faceLightness);
    return 1;
}

} // namespace

QColor Palette::tone(int number) const
{
    if (const auto it = overrides.find(number); it != overrides.end())
        return it->second;
    const Oklab base = toOklab(face);
    const double offset = contrast * lightnessOffsets[number - darkestTone];
    const double lightness = std::clamp(base.lightness + offset, 0.0, 1.0);
    const double scale = chromaScale(lightness, base.lightness);
    return fromOklab({ lightness, base.a * scale, base.b * scale });
}
