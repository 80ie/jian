#include "palette/palette.h"
#include "panel/config.h"
#include "panel/render.h"

#include <QTest>

#include <optional>

class PanelTest : public QObject {
    Q_OBJECT

private slots:
    void matchesGolden()
    {
        const std::optional<Palette> palette = loadPalette(QFINDTESTDATA("golden/palette.ini"));
        QVERIFY(palette);
        const QImage golden
            = QImage(QFINDTESTDATA("golden/panel.png")).convertToFormat(QImage::Format_RGB32);
        const QImage panel = renderPanel(*palette, Config()).convertToFormat(QImage::Format_RGB32);
        if (panel != golden)
            panel.save("panel-actual.png");
        QVERIFY2(panel == golden, "differs from golden/panel.png, see panel-actual.png");
    }
};

QTEST_MAIN(PanelTest)
#include "panel_test.moc"
