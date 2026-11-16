#include "widgets/clock.h"
#include "widgets/clocklayout.h"

#include <QFontMetrics>
#include <QTest>

#include <algorithm>
#include <cstdlib>

using namespace Qt::StringLiterals;

namespace {

// The family the golden image draws the clock in.
QFont mono() { return QFont(u"Linux Libertine Mono O"_s); }

// Where each column starts in `line`: at its start and after each run of two spaces or more.
QList<qsizetype> columnStarts(const QString& line)
{
    QList<qsizetype> starts { 0 };
    for (qsizetype i = 2; i < line.size(); ++i)
        if (line[i] != u' ' && line[i - 1] == u' ' && line[i - 2] == u' ')
            starts.append(i);
    return starts;
}

// Every line on the strip, the time's lines clear of the date's, and each side centred.
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
        const int top = side->lines.front().origin.y() - metrics.capHeight();
        if (std::abs(top + side->lines.back().origin.y() - l.height) > 1)
            return false;
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

    void syntaxColumnsLineUp_data()
    {
        QTest::addColumn<QDate>("day");
        QTest::newRow("May") << QDate(2026, 5, 6);
        QTest::newRow("November") << QDate(2026, 11, 18);
    }

    void syntaxColumnsLineUp()
    {
        QFETCH(QDate, day);
        const QStringList lines = syntaxSheet(QDateTime(day, QTime(9, 5))).split(u'\n');
        QCOMPARE(lines.size(), 8);
        const QList<qsizetype> starts = columnStarts(lines.front());
        QCOMPARE(starts.size(), 6);
        for (const QString& line : lines)
            QCOMPARE(starts.first(columnStarts(line).size()), columnStarts(line));
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
        const ClockLayout l = clockLayout(width, mono(), u"09:41AM"_s, u"Fri 11.13"_s, { });
        QCOMPARE(l.time.font.pointSize(), points);
        QCOMPARE(l.date.font.pointSize(), points);
        QCOMPARE(l.height, height);
    }

    void longFormatStepsDown()
    {
        const ClockLayout l = clockLayout(180, mono(), u"09:41:07 AM"_s, u"Fri 11.13"_s, { });
        QVERIFY(l.time.font.pointSize() < 10);
        QCOMPARE(l.date.font.pointSize(), l.time.font.pointSize());
        QVERIFY(l.height < 27);
        QCOMPARE(l.time.lines.front().text, u"09:41:07 AM"_s);
        QCOMPARE(l.date.lines.front().text, u"Fri 11.13"_s);
    }

    void timeAtOneAndAHalf()
    {
        const ClockLayout l = clockLayout(180, mono(), u"09:41AM"_s, u"11.16\nMon"_s, { 150, 100 });
        QCOMPARE(l.time.font.pointSize(), 15);
        QCOMPARE(l.date.font.pointSize(), 10);
    }

    void stepDownKeepsTheRatio()
    {
        const ClockLayout l
            = clockLayout(260, mono(), u"Fri 09:41:07 AM"_s, u"11.16\nMon"_s, { 150, 100 });
        QCOMPARE(l.time.font.pointSize(), 15);
        QCOMPARE(l.date.font.pointSize(), 10);
        QCOMPARE(l.time.lines.front().text, u"Fri 09:41:07 AM"_s);
    }

    void nothingBelowTheFloor()
    {
        const ClockLayout l
            = clockLayout(120, mono(), u"Friday 09:41:07 AM"_s, u"11.16\nMon"_s, { 150, 50 });
        QCOMPARE(l.time.font.pointSize(), 8);
        QCOMPARE(l.date.font.pointSize(), 8);
    }

    void largerSideSetsHeight_data()
    {
        QTest::addColumn<QString>("date");
        QTest::addColumn<int>("height");
        QTest::newRow("time taller") << u"Mon"_s << 41;
        QTest::newRow("date taller") << u"11.16\nMon"_s << 42;
    }

    void largerSideSetsHeight()
    {
        QFETCH(QString, date);
        QFETCH(int, height);
        QCOMPARE(clockLayout(180, mono(), u"09:41AM"_s, date, { 150, 100 }).height, height);
    }

    void largerTimeCutsNothing()
    {
        for (int width = 140; width <= 260; ++width) {
            const ClockLayout l
                = clockLayout(width, mono(), u"12:55PM"_s, u"11.16\nMon"_s, { 150, 100 });
            QCOMPARE(l.time.lines.front().text, u"12:55PM"_s);
            QCOMPARE(l.date.lines.front().text, u"11.16"_s);
        }
    }

    void staysInside_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<QString>("time");
        QTest::addColumn<QString>("date");
        QTest::addColumn<ClockSizes>("sizes");
        QTest::newRow("default") << 160 << u"09:41AM"_s << u"Fri 11.13"_s << ClockSizes();
        QTest::newRow("stepped down") << 180 << u"09:41:07 AM"_s << u"Fri 11.13"_s << ClockSizes();
        QTest::newRow("date elided")
            << 120 << u"09:41AM"_s << u"Wednesday 11.16.2026"_s << ClockSizes();
        QTest::newRow("time elided")
            << 120 << u"Friday 09:41:07 AM 2026"_s << u"Fri"_s << ClockSizes();
        QTest::newRow("empty") << 160 << QString() << QString() << ClockSizes();
        QTest::newRow("stacked date")
            << 180 << u"09:41AM"_s << u"11.16.2026\nMonday"_s << ClockSizes();
        QTest::newRow("stacked time") << 180 << u"09:41\nAM"_s << u"Mon 11.16"_s << ClockSizes();
        QTest::newRow("short time") << 180 << u"9"_s << u"11.16.2026\nMonday"_s << ClockSizes();
        QTest::newRow("narrow") << 120 << u"09:41AM"_s << u"Monday 11.16.2026\nweek 47\nQ4"_s
                                << ClockSizes();
        QTest::newRow("larger time")
            << 180 << u"09:41AM"_s << u"11.16\nMon"_s << ClockSizes { 150, 100 };
        QTest::newRow("larger date")
            << 180 << u"09:41\nAM"_s << u"Monday 11.16"_s << ClockSizes { 70, 200 };
        QTest::newRow("both at the most")
            << 220 << u"09:41AM"_s << u"Fri 11.13"_s << ClockSizes { 300, 300 };
    }

    void staysInside()
    {
        QFETCH(int, width);
        QFETCH(QString, time);
        QFETCH(QString, date);
        QFETCH(ClockSizes, sizes);
        QVERIFY(::staysInside(clockLayout(width, mono(), time, date, sizes), width));
    }
};

QTEST_MAIN(ClockTest)
#include "clock_test.moc"
