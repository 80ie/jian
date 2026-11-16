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

// From the top of the first line's capitals to the last baseline.
int span(const QFontMetrics& metrics, qsizetype lines)
{
    return int(lines - 1) * metrics.lineSpacing() + metrics.capHeight();
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

// The side with fewer lines grows until it would stand taller than the other or no longer fit
// beside it.
int grownPoints(
    int width, const QFont& family, int base, const QStringList& lines, const QStringList& other)
{
    if (lines.size() >= other.size())
        return base;
    const QFontMetrics metrics(paint::font(family, base));
    const int tallest = span(metrics, other.size());
    const int room = width - padLeft - padRight - gap - widest(metrics, other);
    const auto fits = [&](int points) {
        const QFontMetrics grown(paint::font(family, points));
        return span(grown, lines.size()) <= tallest && widest(grown, lines) <= room;
    };
    int points = base;
    while (fits(points + 1))
        ++points;
    return points;
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
    int baseline = (height + span(metrics, lines.size())) / 2
        - int(lines.size() - 1) * metrics.lineSpacing();
    for (const QString& line : lines) {
        side.lines.push_back({ line, QPoint(x(metrics.horizontalAdvance(line)), baseline) });
        baseline += metrics.lineSpacing();
    }
    return side;
}

} // namespace

ClockLayout clockLayout(int width, const QFont& family, const QString& time, const QString& date)
{
    const QStringList timeLines = time.split(u'\n');
    const QStringList dateLines = date.split(u'\n');
    const int base = basePoints(width, family, timeLines, dateLines);
    const int height = qRound(double(stripHeight) * base / referencePoints)
        + int(std::max(timeLines.size(), dateLines.size()) - 1)
            * QFontMetrics(paint::font(family, base)).lineSpacing();
    const QFont timeFont
        = paint::font(family, grownPoints(width, family, base, timeLines, dateLines));
    const QFont dateFont
        = paint::font(family, grownPoints(width, family, base, dateLines, timeLines));
    const QFontMetrics timeMetrics(timeFont);
    const int room = width - padLeft - padRight;
    const QStringList shownTime = elided(timeMetrics, timeLines, room);
    const QStringList shownDate
        = elided(QFontMetrics(dateFont), dateLines, room - gap - widest(timeMetrics, shownTime));
    return { stack(timeFont, shownTime, height, [](int) { return padLeft; }),
        stack(dateFont, shownDate, height,
            [width](int advance) { return width - padRight - advance; }),
        height };
}
