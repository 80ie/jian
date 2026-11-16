#include "widgets/clocklayout.h"
#include "paint/font.h"

#include <QFontMetrics>
#include <QStringList>

#include <algorithm>

namespace {

// At referenceWidth the text is referencePoints and the strip stripHeight tall; both scale with
// the width.
constexpr int referenceWidth = 200;
constexpr int referencePoints = 11;
constexpr int minimumPoints = 8;
constexpr int stripHeight = 30;
constexpr int padLeft = 4;
constexpr int padRight = 3;
constexpr int gap = 10;

int widest(const QFontMetrics& metrics, const QStringList& lines)
{
    int widest = 0;
    for (const QString& line : lines)
        widest = std::max(widest, metrics.horizontalAdvance(line));
    return widest;
}

// Scaling with the text as well would blow a short format up to fill the strip, so the width
// sets the size and the text can only step it down.
int basePoints(int width, const QFont& family, const QStringList& time, const QStringList& date)
{
    const auto fits = [&](int points) {
        const QFontMetrics metrics(paint::font(family, points));
        return widest(metrics, time) + widest(metrics, date) <= width - padLeft - padRight - gap;
    };
    int points = qRound(double(referencePoints) * width / referenceWidth);
    while (points > minimumPoints && !fits(points))
        --points;
    return std::max(points, minimumPoints);
}

QStringList elided(const QFontMetrics& metrics, QStringList lines, int room)
{
    for (QString& line : lines)
        line = metrics.elidedText(line, Qt::ElideRight, room);
    return lines;
}

// `lines` in `font` on baselines centered in `height`, each starting where `x` puts a line of
// its advance.
ClockSide stack(const QFont& font, const QStringList& lines, int height, const auto& x)
{
    const QFontMetrics metrics(font);
    ClockSide side { font, { } };
    for (const QString& line : lines)
        side.lines.push_back({ line,
            QPoint(x(metrics.horizontalAdvance(line)), (height + metrics.capHeight()) / 2) });
    return side;
}

} // namespace

ClockLayout clockLayout(int width, const QFont& family, const QString& time, const QString& date)
{
    const QStringList timeLines { time };
    const QStringList dateLines { date };
    const QFont font = paint::font(family, basePoints(width, family, timeLines, dateLines));
    const QFontMetrics metrics(font);
    const int height = qRound(double(stripHeight) * font.pointSize() / referencePoints);
    const int room = width - padLeft - padRight;
    const QStringList shownTime = elided(metrics, timeLines, room);
    const QStringList shownDate
        = elided(metrics, dateLines, room - gap - widest(metrics, shownTime));
    return { stack(font, shownTime, height, [](int) { return padLeft; }),
        stack(font, shownDate, height, [width](int advance) { return width - padRight - advance; }),
        height };
}
