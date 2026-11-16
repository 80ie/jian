#include "widgets/clock.h"
#include "palette/palette.h"
#include "widgets/clocklayout.h"

#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QtMath>

#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace {

QPlainTextEdit* formatEdit(const QString& format, QWidget* parent)
{
    auto* edit = new QPlainTextEdit(format, parent);
    edit->setToolTip(u"QDateTime format"_s);
    edit->setTabChangesFocus(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    // Three lines without a scroll bar. QPlainTextEdit rounds each line up to a whole pixel and
    // wants a pixel to spare below the margins.
    const int line = qCeil(QFontMetricsF(edit->font()).lineSpacing());
    const int margin = edit->frameWidth() + qCeil(edit->document()->documentMargin());
    edit->setFixedHeight(3 * line + 2 * margin + 1);
    return edit;
}

// Each token beside what it gives, a column to every eight rows. Returns the labels that show
// what the tokens give, each with its token.
std::vector<std::pair<QString, QLabel*>> fillSyntax(QGridLayout* grid, QWidget* parent)
{
    constexpr int rows = 8;
    const QFont fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const int gap = QFontMetrics(fixed).horizontalAdvance(u"  "_s);
    const auto place = [&](int i, QLabel* token, QLabel* gives) {
        gives->setContentsMargins(0, 0, gap, 0);
        grid->addWidget(token, i % rows, 2 * (i / rows));
        grid->addWidget(gives, i % rows, 2 * (i / rows) + 1);
    };
    const QStringList tokens { u"h AP"_s, u"hh AP"_s, u"H"_s, u"HH"_s, u"mm"_s, u"ss"_s, u"AP"_s,
        u"ap"_s, u"d"_s, u"dd"_s, u"ddd"_s, u"dddd"_s, u"M"_s, u"MM"_s, u"MMM"_s, u"MMMM"_s,
        u"yy"_s, u"yyyy"_s, u"'at'"_s };
    std::vector<std::pair<QString, QLabel*>> examples;
    for (const QString& token : tokens) {
        auto* label = new QLabel(token, parent);
        label->setFont(fixed);
        examples.emplace_back(token, new QLabel(parent));
        place(int(examples.size()) - 1, label, examples.back().second);
    }
    place(int(tokens.size()), new QLabel(u"Enter"_s, parent), new QLabel(u"new line"_s, parent));
    grid->setColumnStretch(grid->columnCount(), 1);
    return examples;
}

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
    return clockLayout(width, font(), m_time, m_date, { }).height;
}

QWidget* Clock::settingsPage()
{
    auto* page = new QWidget;
    auto* time = formatEdit(m_timeFormat, page);
    auto* date = formatEdit(m_dateFormat, page);
    auto* timePreview = new QLabel(page);
    auto* datePreview = new QLabel(page);
    auto* syntax = new QGridLayout;
    auto* form = new QFormLayout(page);
    form->addRow(u"Time"_s, time);
    form->addRow(QString(), timePreview);
    form->addRow(u"Date"_s, date);
    form->addRow(QString(), datePreview);
    form->addRow(u"Syntax"_s, syntax);
    const std::vector<std::pair<QString, QLabel*>> examples = fillSyntax(syntax, page);
    const auto preview = [time, date, timePreview, datePreview, examples] {
        const QDateTime now = QDateTime::currentDateTime();
        timePreview->setText(now.toString(time->toPlainText()));
        datePreview->setText(now.toString(date->toPlainText()));
        for (const auto& [token, label] : examples)
            label->setText(now.toString(token));
    };
    for (QPlainTextEdit* edit : { time, date })
        connect(edit, &QPlainTextEdit::textChanged, this, [this, time, date, preview] {
            preview();
            setFormats(time->toPlainText(), date->toPlainText());
        });
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
    const ClockLayout l = clockLayout(width(), font(), m_time, m_date, { });
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
