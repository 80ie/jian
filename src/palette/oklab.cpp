#include "palette/oklab.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

using LinearRgb = std::array<double, 3>;

double toLinear(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }

double toGamma(double c)
{
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1 / 2.4) - 0.055;
}

LinearRgb toLinearRgb(const Oklab& lab)
{
    const double l = std::pow(lab.lightness + 0.3963377774 * lab.a + 0.2158037573 * lab.b, 3);
    const double m = std::pow(lab.lightness - 0.1055613458 * lab.a - 0.0638541728 * lab.b, 3);
    const double s = std::pow(lab.lightness - 0.0894841775 * lab.a - 1.2914855480 * lab.b, 3);
    return { 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s };
}

bool inGamut(const LinearRgb& rgb)
{
    return std::ranges::all_of(rgb, [](double c) { return c >= 0 && c <= 1; });
}

Oklab withChroma(const Oklab& lab, double scale)
{
    return { lab.lightness, lab.a * scale, lab.b * scale };
}

} // namespace

Oklab toOklab(const QColor& color)
{
    const QColor rgb = color.toRgb();
    const double r = toLinear(rgb.redF());
    const double g = toLinear(rgb.greenF());
    const double b = toLinear(rgb.blueF());
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return { 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
        1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
        0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s };
}

QColor fromOklab(const Oklab& lab)
{
    LinearRgb rgb = toLinearRgb(lab);
    if (!inGamut(rgb)) {
        double inside = 0;
        double outside = 1;
        for (int i = 0; i < 24; ++i) {
            const double mid = (inside + outside) / 2;
            (inGamut(toLinearRgb(withChroma(lab, mid))) ? inside : outside) = mid;
        }
        rgb = toLinearRgb(withChroma(lab, inside));
    }
    const auto channel = [](double c) {
        return static_cast<int>(std::lround(toGamma(std::clamp(c, 0.0, 1.0)) * 255));
    };
    return QColor(channel(rgb[0]), channel(rgb[1]), channel(rgb[2]));
}
