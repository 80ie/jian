#include "widgets/clock.h"

#include <QTest>

class ClockTest : public QObject {
    Q_OBJECT

private slots:
    void showsSeconds_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<bool>("seconds");
        QTest::newRow("minutes") << "hh:mmAP" << false;
        QTest::newRow("seconds") << "HH:mm:ss" << true;
        QTest::newRow("quoted") << "h 'seconds'" << false;
        QTest::newRow("after an escaped quote") << "h 'o''clock' ss" << true;
        QTest::newRow("date") << "ddd MM.dd" << false;
    }

    void showsSeconds()
    {
        QFETCH(QString, format);
        QFETCH(bool, seconds);
        QCOMPARE(::showsSeconds(format), seconds);
    }

    void msecsToNextTick_data()
    {
        QTest::addColumn<QTime>("now");
        QTest::addColumn<bool>("seconds");
        QTest::addColumn<int>("msecs");
        QTest::newRow("before a minute") << QTime(11, 59, 59, 500) << false << 500;
        QTest::newRow("on a minute") << QTime(12, 0) << false << 60'000;
        QTest::newRow("within a second") << QTime(12, 0, 0, 250) << true << 750;
        QTest::newRow("before midnight") << QTime(23, 59, 30) << false << 30'000;
    }

    void msecsToNextTick()
    {
        QFETCH(QTime, now);
        QFETCH(bool, seconds);
        QFETCH(int, msecs);
        QCOMPARE(::msecsToNextTick(now, seconds), msecs);
    }
};

QTEST_GUILESS_MAIN(ClockTest)
#include "clock_test.moc"
