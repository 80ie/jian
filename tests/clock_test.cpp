#include "widgets/clock.h"
#include "widgets/clocklayout.h"

#include <QFontMetrics>
#include <QTest>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

// The family the golden image draws the clock in.
QFont mono() { return QFont(u"Linux Libertine Mono O"_s); }

// Every line on the strip, and the time's lines clear of the date's.
bool staysInside(const ClockLayout& l, int width)
{
    int timeRight = 0;
    int dateLeft = width;
    for (const ClockSide* side : { &l.time, &l.date }) {
        const QFontMetrics metrics(side->font);
        for (const ClockLine& line : side->lines) {
            const int left = line.origin.x();
            const int right = left + metrics.horizontalAdvance(line.text);
            const int top = line.origin.y() - metrics.capHeight();
            if (left < 0 || right > width || top < 0 || line.origin.y() > l.height)
                return false;
            if (side == &l.time)
                timeRight = std::max(timeRight, right);
            else if (!line.text.isEmpty())
                dateLeft = std::min(dateLeft, left);
        }
    }
    return timeRight < dateLeft;
}

} // namespace

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

    void sizeFollowsWidth_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("points");
        QTest::addColumn<int>("height");
        QTest::newRow("reference") << 200 << 11 << 30;
        QTest::newRow("narrower") << 180 << 10 << 27;
        QTest::newRow("at the floor") << 120 << 8 << 22;
    }

    void sizeFollowsWidth()
    {
        QFETCH(int, width);
        QFETCH(int, points);
        QFETCH(int, height);
        const ClockLayout l = clockLayout(width, mono(), u"09:41AM"_s, u"Fri 11.13"_s);
        QCOMPARE(l.time.font.pointSize(), points);
        QCOMPARE(l.date.font.pointSize(), points);
        QCOMPARE(l.height, height);
    }

    void longFormatStepsDown()
    {
        const ClockLayout l = clockLayout(180, mono(), u"09:41:07 AM"_s, u"Fri 11.13"_s);
        QVERIFY(l.time.font.pointSize() < 10);
        QCOMPARE(l.date.font.pointSize(), l.time.font.pointSize());
        QVERIFY(l.height < 27);
        QCOMPARE(l.time.lines.front().text, u"09:41:07 AM"_s);
        QCOMPARE(l.date.lines.front().text, u"Fri 11.13"_s);
    }

    void staysInside_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<QString>("time");
        QTest::addColumn<QString>("date");
        QTest::newRow("default") << 160 << u"09:41AM"_s << u"Fri 11.13"_s;
        QTest::newRow("stepped down") << 180 << u"09:41:07 AM"_s << u"Fri 11.13"_s;
        QTest::newRow("date elided") << 120 << u"09:41AM"_s << u"Wednesday 11.16.2026"_s;
        QTest::newRow("time elided") << 120 << u"Friday 09:41:07 AM 2026"_s << u"Fri"_s;
        QTest::newRow("empty") << 160 << QString() << QString();
    }

    void staysInside()
    {
        QFETCH(int, width);
        QFETCH(QString, time);
        QFETCH(QString, date);
        QVERIFY(::staysInside(clockLayout(width, mono(), time, date), width));
    }
};

QTEST_MAIN(ClockTest)
#include "clock_test.moc"
