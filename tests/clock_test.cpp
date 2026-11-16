#include "widgets/clock.h"
#include "widgets/clocklayout.h"

#include <QFontMetrics>
#include <QTest>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

// The family the golden image draws the clock in.
QFont mono() { return QFont(u"Linux Libertine Mono O"_s); }

// From the top of the first line's capitals to the last baseline.
std::pair<int, int> span(const ClockSide& side)
{
    return { side.lines.front().origin.y() - QFontMetrics(side.font).capHeight(),
        side.lines.back().origin.y() };
}

// Every line on the strip, the time's lines clear of the date's, and the side with fewer lines
// within the other's span.
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
    const bool timeShorter = l.time.lines.size() < l.date.lines.size();
    const auto [top, bottom] = span(timeShorter ? l.time : l.date);
    const auto [tallTop, tallBottom] = span(timeShorter ? l.date : l.time);
    return timeRight < dateLeft && top >= tallTop && bottom <= tallBottom;
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

    void stackedDateKeepsItsSize()
    {
        const ClockLayout one = clockLayout(180, mono(), u"09:41AM"_s, u"11.16.2026"_s);
        const ClockLayout two = clockLayout(180, mono(), u"09:41AM"_s, u"11.16.2026\nMonday"_s);
        QCOMPARE(two.date.lines.size(), 2);
        QCOMPARE(two.date.font.pointSize(), one.date.font.pointSize());
        QVERIFY(two.height > one.height);
        QVERIFY(two.time.font.pointSize() > one.time.font.pointSize());
        QVERIFY(two.date.lines[1].origin.y() > two.date.lines[0].origin.y());
    }

    void widthCapsGrowth()
    {
        const ClockLayout narrow = clockLayout(173, mono(), u"09:41AM"_s, u"11.16.2026\nMonday"_s);
        const ClockLayout wide = clockLayout(190, mono(), u"09:41AM"_s, u"11.16.2026\nMonday"_s);
        QCOMPARE(narrow.date.font.pointSize(), wide.date.font.pointSize());
        QVERIFY(narrow.time.font.pointSize() < wide.time.font.pointSize());
    }

    void equalLinesDoNotGrow()
    {
        const ClockLayout l = clockLayout(180, mono(), u"09:41\nAM"_s, u"11.16\nMon"_s);
        QCOMPARE(l.time.font.pointSize(), l.date.font.pointSize());
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
        QTest::newRow("stacked date") << 180 << u"09:41AM"_s << u"11.16.2026\nMonday"_s;
        QTest::newRow("stacked time") << 180 << u"09:41\nAM"_s << u"Mon 11.16"_s;
        QTest::newRow("short time") << 180 << u"9"_s << u"11.16.2026\nMonday"_s;
        QTest::newRow("narrow") << 120 << u"09:41AM"_s << u"Monday 11.16.2026\nweek 47\nQ4"_s;
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
