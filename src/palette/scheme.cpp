#include "palette/scheme.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

using namespace Qt::StringLiterals;

namespace {

// wallust's darken scales each sRGB channel toward black.
QColor darkened(const QColor& color, double amount)
{
    const auto channel = [amount](int c) { return int(std::lround(c * (1 - amount))); };
    return QColor(channel(color.red()), channel(color.green()), channel(color.blue()));
}

} // namespace

std::optional<Palette> importScheme(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("%s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
        return std::nullopt;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning("%s: %s", qUtf8Printable(path), qUtf8Printable(error.errorString()));
        return std::nullopt;
    }
    const QJsonObject colors = document.object().value("colors"_L1).toObject();
    Palette palette;
    for (const auto& [key, member] :
        { std::pair { "color7", &Palette::face }, { "color0", &Palette::ink },
            { "color10", &Palette::fill }, { "color4", &Palette::selection },
            { "color14", &Palette::pagerDesk }, { "color1", &Palette::chrome } }) {
        palette.*member = QColor::fromString(colors.value(QLatin1StringView(key)).toString());
        if (!(palette.*member).isValid()) {
            qWarning("%s: bad or missing colors/%s", qUtf8Printable(path), key);
            return std::nullopt;
        }
    }
    palette.ink = darkened(palette.ink, 0.9);
    return palette;
}
