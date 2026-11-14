#include "widgets/medialayout.h"

#include <QTest>

class MediaLayoutTest : public QObject {
    Q_OBJECT

private slots:
    void height_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<QSize>("art");
        QTest::addColumn<int>("height");
        QTest::newRow("bare") << 153 << QSize() << 113;
        // The well is as wide as the widget, a divider and two gaps under it.
        QTest::newRow("square art") << 153 << QSize(300, 300) << 113 + 153 + 8;
        QTest::newRow("wide art") << 150 << QSize(320, 180) << 113 + 84 + 8;
        QTest::newRow("tall art stays square") << 150 << QSize(100, 400) << 113 + 150 + 8;
    }

    void height()
    {
        QFETCH(int, width);
        QFETCH(QSize, art);
        QFETCH(int, height);
        const MediaLayout l = mediaLayout(width, art);
        QCOMPARE(l.height, height);
        QCOMPARE(l.art.isNull(), art.isEmpty());
    }

    void buttonsSpanTheWidth()
    {
        const MediaLayout l = mediaLayout(153, QSize());
        QCOMPARE(l.buttons.front().left(), 0);
        QCOMPARE(l.buttons.back().right(), 152);
        for (const QRect& button : l.buttons)
            QVERIFY(std::abs(button.width() - l.buttons.front().width()) <= 1);
    }

    void volume()
    {
        const QRect groove = mediaLayout(153, QSize()).groove;
        for (double v : { 0.0, 0.25, 0.6, 1.0 })
            QVERIFY(std::abs(volumeAt(groove, thumbLeft(groove, v) + thumbSize.width() / 2) - v)
                < 0.01);
        QCOMPARE(volumeAt(groove, 0), 0.0);
        QCOMPARE(volumeAt(groove, 1000), 1.0);
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
