#include "paint/sheet.h"
#include "palette/palette.h"

#include <QApplication>
#include <QCommandLineParser>

#include <cstdio>
#include <optional>

using namespace Qt::StringLiterals;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCommandLineParser parser;
    const QCommandLineOption renderOption(
        u"render"_s, u"Draw the painter sheet to <png>."_s, u"png"_s);
    const QCommandLineOption paletteOption(
        u"palette"_s, u"Draw it with the palette in <ini>."_s, u"ini"_s);
    parser.addOptions({ renderOption, paletteOption });
    if (!parser.parse(app.arguments()) || !parser.isSet(renderOption)
        || !parser.positionalArguments().isEmpty()) {
        std::fputs(qPrintable(parser.helpText()), stderr);
        return 2;
    }

    std::optional<Palette> palette = Palette();
    if (parser.isSet(paletteOption))
        palette = loadPalette(parser.value(paletteOption));
    if (!palette)
        return 1;

    const QString out = parser.value(renderOption);
    if (!paint::sheet(*palette).save(out)) {
        qWarning("%s: cannot write", qUtf8Printable(out));
        return 1;
    }
    return 0;
}
