#include "panel/layout.h"

#include <QTest>

namespace {

constexpr int gap = 4;

std::vector<QRect> layout(int height, const std::vector<SlotHeights>& heights)
{
    return layoutSlots(QRect(0, 0, 100, height), gap, heights);
}

} // namespace

class LayoutTest : public QObject {
    Q_OBJECT

private slots:
    void naturalHeightsFromTheTop()
    {
        const auto rects = layout(200, { { 30, 20, false }, { 40, 40, false } });
        QCOMPARE(rects, (std::vector<QRect> { { 0, 0, 100, 30 }, { 0, 34, 100, 40 } }));
    }

    void spareSplitsEvenly()
    {
        // 85 left over: the first taker gets the odd pixel, and the last slot ends at the bottom.
        const auto rects = layout(201,
            { { 30, 30, false }, { 10, 5, true }, { 20, 20, false }, { 10, 5, true },
                { 30, 30, false } });
        QCOMPARE(rects,
            (std::vector<QRect> { { 0, 0, 100, 30 }, { 0, 34, 100, 53 }, { 0, 91, 100, 20 },
                { 0, 115, 100, 52 }, { 0, 171, 100, 30 } }));
    }

    void spareTakersShrinkFirst()
    {
        const auto rects = layout(100, { { 40, 30, false }, { 50, 20, true }, { 40, 10, true } });
        QCOMPARE(rects,
            (std::vector<QRect> { { 0, 0, 100, 40 }, { 0, 44, 100, 31 }, { 0, 79, 100, 21 } }));
    }

    void othersShrinkOnceSpareTakersAreAtMinimum()
    {
        // 38 too tall: the taker gives its 5, the first slot its last 10, the third the rest.
        const auto rects = layout(100, { { 40, 30, false }, { 50, 45, true }, { 40, 10, false } });
        QCOMPARE(rects,
            (std::vector<QRect> { { 0, 0, 100, 30 }, { 0, 34, 100, 45 }, { 0, 83, 100, 17 } }));
    }

    void overflowRunsPastTheBottom()
    {
        const auto rects = layout(100, { { 40, 30, false }, { 50, 45, true }, { 40, 35, false } });
        QCOMPARE(rects,
            (std::vector<QRect> { { 0, 0, 100, 30 }, { 0, 34, 100, 45 }, { 0, 83, 100, 35 } }));
    }
};

QTEST_GUILESS_MAIN(LayoutTest)
#include "layout_test.moc"
