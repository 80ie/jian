#include "widgets/clock.h"
#include "palette/palette.h"

#include <QFontDatabase>
#include <QPainter>

#include <algorithm>

using namespace Qt::StringLiterals;

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

} // namespace

bool showsSeconds(QStringView format)
{
    bool quoted = false;
    for (QChar c : format) {
        if (c == u'\'')
            quoted = !quoted;
        else if (c == u's' && !quoted)
            return true;
    }
    return false;
}

int msecsToNextTick(const QTime& now, bool seconds)
{
    const int period = seconds ? 1000 : 60'000;
    return period - now.msecsSinceStartOfDay() % period;
}

Clock::Clock(
    const Palette& palette, const QString& timeFormat, const QString& dateFormat, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
    , m_timeFormat(timeFormat)
    , m_dateFormat(dateFormat)
    , m_seconds(showsSeconds(timeFormat) || showsSeconds(dateFormat))
{
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &Clock::tick);
}

QString Clock::id() const { return u"clock"_s; }

int Clock::heightForWidth(int width) const
{
    return qRound(double(stripHeight) * fontFor(width).pointSize() / referencePoints);
}

void Clock::setTime(const QDateTime& time)
{
    const int before = heightForWidth(width());
    m_time = time.toString(m_timeFormat);
    m_date = time.toString(m_dateFormat);
    if (heightForWidth(width()) != before)
        emit heightChanged();
    update();
}

void Clock::paintEvent(QPaintEvent*)
{
    const QFont font = fontFor(width());
    const QFontMetrics metrics(font, this);
    const int room = width() - padLeft - padRight;
    const QString time = metrics.elidedText(m_time, Qt::ElideRight, room);
    const QString date
        = metrics.elidedText(m_date, Qt::ElideRight, room - gap - metrics.horizontalAdvance(time));
    const int baseline = (height() + metrics.capHeight()) / 2;

    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    p.setFont(font);
    p.setPen(m_palette.ink);
    p.drawText(padLeft, baseline, time);
    p.drawText(width() - padRight - metrics.horizontalAdvance(date), baseline, date);
}

void Clock::showEvent(QShowEvent*) { tick(); }

void Clock::hideEvent(QHideEvent*) { m_timer.stop(); }

// Scaling with the text as well would blow a short format up to fill the strip, so the width
// sets the size and the text can only step it down.
QFont Clock::fontFor(int width) const
{
    QFont font = this->font();
    const auto fits = [&](int points) {
        font.setPointSize(points);
        const QFontMetrics metrics(font, this);
        return metrics.horizontalAdvance(m_time) + metrics.horizontalAdvance(m_date)
            <= width - padLeft - padRight - gap;
    };
    int points = qRound(double(referencePoints) * width / referenceWidth);
    while (points > minimumPoints && !fits(points))
        --points;
    font.setPointSize(std::max(points, minimumPoints));
    return font;
}

void Clock::tick()
{
    const QDateTime now = QDateTime::currentDateTime();
    setTime(now);
    m_timer.start(msecsToNextTick(now.time(), m_seconds));
}
