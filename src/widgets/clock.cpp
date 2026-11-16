#include "widgets/clock.h"
#include "palette/palette.h"
#include "widgets/clocklayout.h"

#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>

using namespace Qt::StringLiterals;

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
    return clockLayout(width, font(), m_time, m_date).height;
}

QWidget* Clock::settingsPage()
{
    auto* page = new QWidget;
    auto* time = new QLineEdit(m_timeFormat, page);
    auto* date = new QLineEdit(m_dateFormat, page);
    auto* timePreview = new QLabel(page);
    auto* datePreview = new QLabel(page);
    auto* form = new QFormLayout(page);
    form->addRow(u"Time"_s, time);
    form->addRow(QString(), timePreview);
    form->addRow(u"Date"_s, date);
    form->addRow(QString(), datePreview);
    const auto preview = [time, date, timePreview, datePreview] {
        const QDateTime now = QDateTime::currentDateTime();
        timePreview->setText(now.toString(time->text()));
        datePreview->setText(now.toString(date->text()));
    };
    for (QLineEdit* edit : { time, date }) {
        edit->setToolTip(u"QDateTime format"_s);
        connect(edit, &QLineEdit::textEdited, this, [this, time, date, preview] {
            preview();
            setFormats(time->text(), date->text());
        });
    }
    preview();
    return page;
}

void Clock::setFormats(const QString& timeFormat, const QString& dateFormat)
{
    m_timeFormat = timeFormat;
    m_dateFormat = dateFormat;
    m_seconds = showsSeconds(timeFormat) || showsSeconds(dateFormat);
    if (isVisible())
        tick();
    else
        setTime(QDateTime::currentDateTime());
    emit formatsChosen(timeFormat, dateFormat);
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
    const ClockLayout l = clockLayout(width(), font(), m_time, m_date);
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    p.setPen(m_palette.ink);
    for (const ClockSide* side : { &l.time, &l.date }) {
        p.setFont(side->font);
        for (const ClockLine& line : side->lines)
            p.drawText(line.origin, line.text);
    }
}

void Clock::showEvent(QShowEvent*) { tick(); }

void Clock::hideEvent(QHideEvent*) { m_timer.stop(); }

void Clock::tick()
{
    const QDateTime now = QDateTime::currentDateTime();
    setTime(now);
    m_timer.start(msecsToNextTick(now.time(), m_seconds));
}
