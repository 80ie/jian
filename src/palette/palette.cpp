#include "palette/palette.h"
#include "palette/oklab.h"

#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <array>
#include <utility>

using namespace Qt::StringLiterals;

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

constexpr std::pair<QLatin1StringView, QColor Palette::*> colorKeys[] {
    { "palette/face"_L1, &Palette::face },
    { "palette/ink"_L1, &Palette::ink },
    { "palette/fill"_L1, &Palette::fill },
    { "palette/selection"_L1, &Palette::selection },
    { "palette/pager_desk"_L1, &Palette::pagerDesk },
};

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

std::optional<Palette> loadPalette(const QString& path)
{
    if (!QFileInfo(path).isReadable()) {
        qWarning("%s: cannot read", qUtf8Printable(path));
        return std::nullopt;
    }
    const auto bad = [&path](QAnyStringView key) {
        qWarning("%s: bad or missing %s", qUtf8Printable(path), qUtf8Printable(key.toString()));
        return std::nullopt;
    };

    QSettings ini(path, QSettings::IniFormat);
    Palette palette;
    for (const auto& [key, member] : colorKeys) {
        palette.*member = QColor::fromString(ini.value(key).toString());
        if (!(palette.*member).isValid())
            return bad(key);
    }
    bool ok = false;
    palette.contrast = ini.value("palette/contrast").toDouble(&ok);
    if (!ok)
        return bad(u"palette/contrast");

    ini.beginGroup("override");
    for (const QString& key : ini.childKeys()) {
        const int number = key.mid(4).toInt(&ok);
        const QColor color = QColor::fromString(ini.value(key).toString());
        if (!key.startsWith("tone"_L1) || !ok || number < darkestTone || number > lightestTone
            || !color.isValid())
            return bad(u"override/"_s + key);
        palette.overrides[number] = color;
    }
    return palette;
}

bool savePalette(const Palette& palette, const QString& path)
{
    QSettings ini(path, QSettings::IniFormat);
    ini.clear();
    for (const auto& [key, member] : colorKeys)
        ini.setValue(key, (palette.*member).name());
    ini.setValue("palette/contrast", palette.contrast);
    for (const auto& [number, color] : palette.overrides)
        ini.setValue(u"override/tone"_s + QString::number(number), color.name());
    ini.sync();
    if (ini.status() == QSettings::NoError)
        return true;
    qWarning("%s: cannot write", qUtf8Printable(path));
    return false;
}
