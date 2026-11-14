#include "palette/extract.h"
#include "palette/oklab.h"

#include <QImage>
#include <QPainter>
#include <QTest>

#include <cmath>
#include <numbers>

namespace {

const QColor red(0xb0, 0x30, 0x30);
const QColor blue(0x30, 0x50, 0xb0);

double chroma(const QColor& color)
{
    const Oklab c = toOklab(color);
    return std::hypot(c.a, c.b);
}

// In degrees, from 0 to 180.
double hueApart(const QColor& p, const QColor& q)
{
    const Oklab a = toOklab(p);
    const Oklab b = toOklab(q);
    const double turn
        = std::abs(std::atan2(a.b, a.a) - std::atan2(b.b, b.a)) * 180 / std::numbers::pi;
    return turn > 180 ? 360 - turn : turn;
}

double apart(const QColor& p, const QColor& q)
{
    const Oklab a = toOklab(p);
    const Oklab b = toOklab(q);
    return std::hypot(a.lightness - b.lightness, a.a - b.a, a.b - b.b);
}

QImage halves(const QColor& left, const QColor& right)
{
    QImage image(400, 200, QImage::Format_ARGB32);
    image.fill(right);
    QPainter p(&image);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(0, 0, 200, 200, left);
    return image;
}

// Grey with a ripple of a few levels, so that it does not cluster to one point.
QImage noisyGrey()
{
    QImage image(300, 200, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const int level = 0x80 + (x * 7 + y * 13) % 9 - 4;
            image.setPixel(x, y, qRgb(level, level, level));
        }
    return image;
}

QImage pattern()
{
    QImage image(640, 360, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            image.setPixel(x, y,
                qRgb((x * 3) % 256, (y * 5 + x) % 256,
                    int(127 + 127 * std::sin(x * 0.05 + y * 0.03))));
    return image;
}

} // namespace

class ExtractTest : public QObject {
    Q_OBJECT

private slots:
    void twoColorsAreFound()
    {
        const std::vector<ImageColor> colors = imageColors(halves(red, blue));
        QVERIFY(colors.size() >= 2);
        for (std::size_t i = 0; i < 2; ++i) {
            QVERIFY(std::abs(colors[i].share - 0.5) < 0.02);
            QVERIFY(std::min(apart(colors[i].color, red), apart(colors[i].color, blue)) < 0.01);
        }
        QVERIFY(apart(colors[0].color, colors[1].color) > 0.2);
    }

    void twoColorsGiveTheirHues()
    {
        const std::vector<Palette> palettes = candidates(imageColors(halves(red, blue)));
        QCOMPARE(palettes.size(), 2);
        for (const Palette& palette : palettes) {
            const bool reddish = hueApart(palette.face, red) < 10;
            QVERIFY(reddish || hueApart(palette.face, blue) < 10);
            QVERIFY2(hueApart(palette.selection, reddish ? blue : red) < 10,
                "the selection comes from the other color");
        }
        QVERIFY(hueApart(palettes[0].face, palettes[1].face) > 90);
    }

    void greyGivesOneNeutralCandidate()
    {
        const std::vector<Palette> palettes = candidates(imageColors(noisyGrey()));
        QCOMPARE(palettes.size(), 1);
        QVERIFY(chroma(palettes[0].face) < 0.01);
    }

    void facesLeaveRoomOnBothSides_data()
    {
        QTest::addColumn<QColor>("color");
        QTest::newRow("black") << QColor(0x08, 0x08, 0x08);
        QTest::newRow("white") << QColor(0xfc, 0xfc, 0xfc);
        QTest::newRow("loud green") << QColor(0x00, 0xff, 0x00);
    }

    void facesLeaveRoomOnBothSides()
    {
        QFETCH(QColor, color);
        const std::vector<Palette> palettes = candidates(imageColors(halves(color, color)));
        QCOMPARE(palettes.size(), 1);
        const Palette& palette = palettes[0];
        QVERIFY(toOklab(palette.tone(darkestTone)).lightness > 0.19);
        QVERIFY(toOklab(palette.tone(lightestTone)).lightness < 0.98);
        QVERIFY(chroma(palette.face) < 0.06);
    }

    void transparentPixelsAreLeftOut()
    {
        const std::vector<ImageColor> colors = imageColors(halves(Qt::transparent, blue));
        QCOMPARE(colors.size(), 1);
        QCOMPARE(colors[0].share, 1.0);
    }

    void sameImageSameColors()
    {
        const QImage image = pattern();
        const std::vector<ImageColor> first = imageColors(image);
        QVERIFY(first.size() > 4);
        QVERIFY(first == imageColors(image.copy()));
        QVERIFY(candidates(first) == candidates(imageColors(image.copy())));
    }
};

QTEST_GUILESS_MAIN(ExtractTest)
#include "extract_test.moc"
