#include "paint/sheet.h"
#include "palette/palette.h"

#include <QTest>

#include <optional>

class SheetTest : public QObject {
    Q_OBJECT

private slots:
    void matchesGolden()
    {
        const std::optional<Palette> palette = loadPalette(QFINDTESTDATA("golden/palette.ini"));
        QVERIFY(palette);
        const QImage golden
            = QImage(QFINDTESTDATA("golden/sheet.png")).convertToFormat(QImage::Format_RGB32);
        const QImage sheet = paint::sheet(*palette);
        if (sheet != golden)
            sheet.save("sheet-actual.png");
        QVERIFY2(sheet == golden, "differs from golden/sheet.png, see sheet-actual.png");
    }
};

QTEST_MAIN(SheetTest)
#include "sheet_test.moc"
