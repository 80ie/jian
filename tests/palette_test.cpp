#include "palette/oklab.h"
#include "palette/palette.h"

#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

// Spec 8.1: the ramp's OKLab lightness for a face at 0.676.
constexpr std::array specLightness { 0.30, 0.40, 0.50, 0.57, 0.61, 0.645, 0.676, 0.71, 0.77, 0.82,
    0.876, 0.91 };

double lightness(const QColor& color) { return toOklab(color).lightness; }

Palette withFace(const QColor& face)
{
    Palette palette;
    palette.face = face;
    return palette;
}

} // namespace

class PaletteTest : public QObject {
    Q_OBJECT

private slots:
    void toneZeroIsFace_data()
    {
        QTest::addColumn<QColor>("face");
        QTest::newRow("stock") << QColor(0xa2, 0x94, 0x8a);
        QTest::newRow("slate") << QColor(0x33, 0x66, 0x99);
        QTest::newRow("red") << QColor(0xff, 0x00, 0x00);
        QTest::newRow("green") << QColor(0x00, 0xff, 0x00);
        QTest::newRow("white") << QColor(0xff, 0xff, 0xff);
        QTest::newRow("black") << QColor(0x00, 0x00, 0x00);
    }

    void toneZeroIsFace()
    {
        QFETCH(QColor, face);
        QCOMPARE(withFace(face).tone(0), face);
    }

    void lightnessRises_data()
    {
        QTest::addColumn<QColor>("face");
        QTest::newRow("stock") << QColor(0xa2, 0x94, 0x8a);
        QTest::newRow("slate") << QColor(0x33, 0x66, 0x99);
        QTest::newRow("red") << QColor(0xff, 0x00, 0x00);
    }

    void lightnessRises()
    {
        QFETCH(QColor, face);
        const Palette palette = withFace(face);
        for (int tone = darkestTone + 1; tone <= lightestTone; ++tone)
            QVERIFY2(lightness(palette.tone(tone)) > lightness(palette.tone(tone - 1)),
                qPrintable(QString::number(tone)));
    }

    // The saturated faces leave sRGB at some tones, and lightness has to survive the way back in.
    void lightnessFollowsSpec_data()
    {
        QTest::addColumn<QColor>("face");
        QTest::newRow("stock") << QColor(0xa2, 0x94, 0x8a);
        QTest::newRow("red") << QColor(0xff, 0x00, 0x00);
        QTest::newRow("green") << QColor(0x00, 0xff, 0x00);
        QTest::newRow("blue") << QColor(0x00, 0x00, 0xff);
        QTest::newRow("magenta") << QColor(0xff, 0x00, 0xff);
    }

    void lightnessFollowsSpec()
    {
        QFETCH(QColor, face);
        const Palette palette = withFace(face);
        for (int tone = darkestTone; tone <= lightestTone; ++tone) {
            const double offset = specLightness[tone - darkestTone] - 0.676;
            const double expected = std::clamp(lightness(face) + offset, 0.0, 1.0);
            QVERIFY2(std::abs(lightness(palette.tone(tone)) - expected) < 0.003,
                qPrintable(QString::number(tone)));
        }
    }

    void overridesWin()
    {
        const Palette plain;
        Palette palette;
        palette.overrides[-2] = QColor(0x12, 0x34, 0x56);
        for (int tone = darkestTone; tone <= lightestTone; ++tone)
            QCOMPARE(palette.tone(tone), tone == -2 ? QColor(0x12, 0x34, 0x56) : plain.tone(tone));
    }

    void shownToneStandsOut_data()
    {
        QTest::addColumn<QColor>("face");
        QTest::newRow("stock") << QColor(0xa2, 0x94, 0x8a);
        QTest::newRow("grey") << QColor(0x99, 0x99, 0x99);
        QTest::newRow("slate") << QColor(0x6a, 0x7a, 0x99);
    }

    void shownToneStandsOut()
    {
        QFETCH(QColor, face);
        const Palette palette = withFace(face);
        const Palette shown = withToneShown(palette, -2);
        const Oklab loud = toOklab(shown.tone(-2));
        for (int tone = darkestTone; tone <= lightestTone; ++tone) {
            if (tone == -2)
                continue;
            QCOMPARE(shown.tone(tone), palette.tone(tone));
            const Oklab other = toOklab(palette.tone(tone));
            QVERIFY(std::hypot(loud.a - other.a, loud.b - other.b) > 0.1);
        }
    }

    void chromeFollowsRoles_data()
    {
        QTest::addColumn<int>("tone");
        QTest::addColumn<QColor>("role");
        QTest::newRow("chrome_bg") << -2 << QColor(0x30, 0x15, 0x04);
        QTest::newRow("chrome_bg_lift") << -1 << QColor(0x48, 0x1f, 0x06);
        QTest::newRow("chrome_alert") << 0 << QColor(0x78, 0x34, 0x0a);
        QTest::newRow("chrome_glow") << 1 << QColor(0xe4, 0x74, 0x34);
    }

    void chromeFollowsRoles()
    {
        QFETCH(int, tone);
        QFETCH(QColor, role);
        const QColor chrome = Palette().chromeTone(tone);
        for (const auto channel : { &QColor::red, &QColor::green, &QColor::blue })
            QVERIFY2(
                std::abs((chrome.*channel)() - (role.*channel)()) <= 6, qPrintable(chrome.name()));
    }

    void zeroContrastIsFlat()
    {
        Palette palette;
        palette.contrast = 0;
        for (int tone = darkestTone; tone <= lightestTone; ++tone)
            QCOMPARE(palette.tone(tone), palette.face);
    }

    void iniRoundTrip()
    {
        Palette palette;
        palette.face = QColor(0x33, 0x66, 0x99);
        palette.contrast = 0.85;
        palette.ink = QColor(0x10, 0x10, 0x10);
        palette.fill = QColor(0x20, 0x40, 0x60);
        palette.selection = QColor(0xc0, 0x30, 0x30);
        palette.pagerDesk = QColor(0x40, 0x20, 0x10);
        palette.chrome = QColor(0x50, 0x30, 0x60);
        palette.overrides = { { -2, QColor(0x8c, 0x80, 0x78) }, { 3, QColor(0xfe, 0xfe, 0xfe) } };

        const QTemporaryDir dir;
        const QString path = dir.filePath("palette.ini");
        QVERIFY(savePalette(palette, path));
        const std::optional<Palette> loaded = loadPalette(path);
        QVERIFY(loaded);
        QVERIFY(*loaded == palette);
    }
};

QTEST_GUILESS_MAIN(PaletteTest)
#include "palette_test.moc"
