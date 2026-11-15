#include "palette/palette.h"
#include "panel/config.h"
#include "panel/panel.h"
#include "panel/render.h"
#include "widgets/shelfwidget.h"

#include <QTest>

#include <optional>

using namespace Qt::StringLiterals;

namespace {

class Counting : public ShelfWidget {
public:
    using ShelfWidget::ShelfWidget;

    QString id() const override { return u"counting"_s; }
    int heightForWidth(int) const override { return 10; }
    void paletteChanged() override { ++changed; }
    void paletteSettled() override { ++settled; }

    int changed = 0;
    int settled = 0;
};

} // namespace

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
            panel.save("actual-panel.png");
        QVERIFY2(panel == golden, "differs from golden/panel.png, see actual-panel.png");
    }

    void paletteSettlesOnce()
    {
        const Palette palette;
        Counting* counting = nullptr;
        Panel panel(palette, { }, [&counting](const QString&, QWidget* parent) {
            counting = new Counting(parent);
            return counting;
        });
        panel.arrange({ u"counting"_s }, { });
        QVERIFY(counting);
        for (int i = 0; i < 10; ++i)
            panel.paletteChanged();
        QCOMPARE(counting->changed, 10);
        QCOMPARE(counting->settled, 0);
        QTRY_COMPARE(counting->settled, 1);
        QTest::qWait(500);
        QCOMPARE(counting->settled, 1);
    }
};

QTEST_MAIN(PanelTest)
#include "panel_test.moc"
