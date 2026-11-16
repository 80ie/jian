#include "widgets/medialayout.h"

#include <QTest>

using namespace std::chrono;

class MediaLayoutTest : public QObject {
    Q_OBJECT

private slots:
    void height_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<QSize>("art");
        QTest::addColumn<bool>("track");
        QTest::addColumn<int>("height");
        QTest::newRow("bare") << 153 << QSize() << false << 121;
        QTest::newRow("trackbar") << 153 << QSize() << true << 134;
        // The well is as wide as the fields, a divider and two gaps under it.
        QTest::newRow("square art") << 153 << QSize(300, 300) << false << 121 + 145 + 8;
        QTest::newRow("wide art") << 150 << QSize(320, 180) << true << 134 + 80 + 8;
        QTest::newRow("tall art stays square") << 150 << QSize(100, 400) << false << 121 + 142 + 8;
    }

    void height()
    {
        QFETCH(int, width);
        QFETCH(QSize, art);
        QFETCH(bool, track);
        QFETCH(int, height);
        const MediaLayout l = mediaLayout(width, art, track);
        QCOMPARE(l.height, height);
        QCOMPARE(l.art.isNull(), art.isEmpty());
        QCOMPARE(l.track.isNull(), !track);
    }

    void buttonsSpanTheFields()
    {
        const MediaLayout l = mediaLayout(153, QSize(), false);
        QCOMPARE(l.buttons.front().left(), l.title.left());
        QCOMPARE(l.buttons.back().right(), l.title.right());
        for (const QRect& button : l.buttons)
            QVERIFY(std::abs(button.width() - l.buttons.front().width()) <= 1);
    }

    void volume()
    {
        const QRect groove = mediaLayout(153, QSize(), false).groove;
        for (double v : { 0.0, 0.25, 0.6, 1.0 })
            QVERIFY(std::abs(volumeAt(groove, thumbLeft(groove, v) + thumbSize.width() / 2) - v)
                < 0.01);
        QCOMPARE(volumeAt(groove, 0), 0.0);
        QCOMPARE(volumeAt(groove, 1000), 1.0);
    }

    void track()
    {
        const QRect track = mediaLayout(153, QSize(), true).track;
        const QRect inside = trackInside(track);
        QCOMPARE(trackFill(track, 60s, 120s), qRound(inside.width() / 2.0));
        QCOMPARE(trackFill(track, 200s, 120s), inside.width());
        QCOMPARE(trackPosition(track, inside.left(), 120s), 0s);
        QCOMPARE(trackPosition(track, inside.left() + inside.width(), 120s), 120s);
        QCOMPARE(trackPosition(track, -50, 120s), 0s);
    }

    void trackStep()
    {
        const QRect track = mediaLayout(153, QSize(), true).track;
        const int pixels = trackInside(track).width();
        QCOMPARE(::trackStep(track, seconds(pixels), 1), 1000ms);
        QCOMPARE(::trackStep(track, seconds(pixels), 2), 500ms);
        QCOMPARE(::trackStep(track, 1s, 1), 40ms);
    }

    void marquee()
    {
        QCOMPARE(marqueeStep(0, 0), 0);
        QCOMPARE(marqueeStep(5, 10), 6);
        QCOMPARE(marqueeStep(9, 10), 0);
    }
};

QTEST_GUILESS_MAIN(MediaLayoutTest)
#include "medialayout_test.moc"
