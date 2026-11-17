#include "widgets/soundlayout.h"

#include <QTest>

class SoundLayoutTest : public QObject {
    Q_OBJECT

private slots:
    void volume()
    {
        const QRect groove = soundLayout(153, 0).groove;
        for (double v : { 0.0, 0.25, 0.6, 1.0 })
            QVERIFY(std::abs(volumeAt(groove, thumbLeft(groove, v) + thumbSize.width() / 2) - v)
                < 0.01);
        QCOMPARE(volumeAt(groove, 0), 0.0);
        QCOMPARE(volumeAt(groove, 1000), 1.0);
    }
};

QTEST_GUILESS_MAIN(SoundLayoutTest)
#include "soundlayout_test.moc"
