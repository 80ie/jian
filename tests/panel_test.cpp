#include "palette/palette.h"
#include "panel/config.h"
#include "panel/render.h"

#include <QTest>

#include <optional>

class PanelTest : public QObject {
    Q_OBJECT

private slots:
    void matchesGolden_data()
    {
        QTest::addColumn<Separation>("separation");
        QTest::newRow("tiles") << Separation::Tiles;
        QTest::newRow("bays") << Separation::Bays;
        QTest::newRow("grooves") << Separation::Grooves;
    }

    void matchesGolden()
    {
        QFETCH(Separation, separation);
        const std::optional<Palette> palette = loadPalette(QFINDTESTDATA("golden/palette.ini"));
        QVERIFY(palette);
        const QString name = QStringLiteral("panel-%1.png").arg(QTest::currentDataTag());
        const QImage golden
            = QImage(QFINDTESTDATA("golden/" + name)).convertToFormat(QImage::Format_RGB32);
        Config config;
        config.separation = separation;
        const QImage panel = renderPanel(*palette, config).convertToFormat(QImage::Format_RGB32);
        if (panel != golden)
            panel.save("actual-" + name);
        QVERIFY2(
            panel == golden, qPrintable("differs from golden/" + name + ", see actual-" + name));
    }
};

QTEST_MAIN(PanelTest)
#include "panel_test.moc"
