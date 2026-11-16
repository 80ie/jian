#include "widgets/clock.h"
#include "palette/palette.h"

#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTextDocument>
#include <QtMath>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

// Big enough for `lines` lines `width` wide without a scroll bar. QPlainTextEdit rounds each line
// up to a whole pixel and wants a pixel to spare beyond the margins.
QSize fitting(const QPlainTextEdit* edit, qreal width, int lines)
{
    const int line = qCeil(QFontMetricsF(edit->font()).lineSpacing());
    const int margin = edit->frameWidth() + qCeil(edit->document()->documentMargin());
    return { qCeil(width) + 2 * margin + 1, lines * line + 2 * margin + 1 };
}

QPlainTextEdit* formatEdit(const QString& format, QWidget* parent)
{
    auto* edit = new QPlainTextEdit(format, parent);
    edit->setToolTip(u"QDateTime format"_s);
    edit->setTabChangesFocus(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setFixedHeight(fitting(edit, 0, 3).height());
    return edit;
}

QPlainTextEdit* syntaxWell(QWidget* parent)
{
    auto* well = new QPlainTextEdit(parent);
    well->setReadOnly(true);
    well->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    well->setLineWrapMode(QPlainTextEdit::NoWrap);
    well->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    well->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return well;
}

void showSyntax(QPlainTextEdit* well, const QDateTime& now)
{
    const QString sheet = syntaxSheet(now);
    const QStringList lines = sheet.split(u'\n');
    const QFontMetricsF metrics(well->font());
    qreal widest = 0;
    for (const QString& line : lines)
        widest = std::max(widest, metrics.horizontalAdvance(line));
    well->setPlainText(sheet);
    well->setFixedSize(fitting(well, widest, int(lines.size())));
}

QHBoxLayout* withSize(QPlainTextEdit* edit, QSpinBox* size)
{
    auto* row = new QHBoxLayout;
    row->addWidget(edit);
    row->addWidget(size, 0, Qt::AlignTop);
    return row;
}

QSpinBox* sizeBox(int percent, QWidget* parent)
{
    auto* box = new QSpinBox(parent);
    box->setToolTip(u"Size"_s);
    box->setRange(50, 300);
    box->setSingleStep(10);
    box->setSuffix(u" %"_s);
    box->setValue(percent);
    return box;
}

} // namespace

QString syntaxSheet(const QDateTime& now)
{
    constexpr qsizetype rows = 8;
    const QStringList tokens { u"h AP"_s, u"hh AP"_s, u"H"_s, u"HH"_s, u"mm"_s, u"ss"_s, u"AP"_s,
        u"ap"_s, u"d"_s, u"dd"_s, u"ddd"_s, u"dddd"_s, u"M"_s, u"MM"_s, u"MMM"_s, u"MMMM"_s,
        u"yy"_s, u"yyyy"_s, u"'at'"_s };
    QList<std::pair<QString, QString>> cells;
    for (const QString& token : tokens)
        cells.emplace_back(token, now.toString(token));
    cells.emplace_back(u"Enter"_s, u"new line"_s);
    QStringList lines(rows);
    qsizetype left = 0;
    for (qsizetype first = 0; first < cells.size(); first += rows) {
        const QList<std::pair<QString, QString>> column = cells.mid(first, rows);
        qsizetype tokenWidth = 0;
        qsizetype givesWidth = 0;
        for (const auto& [token, gives] : column) {
            tokenWidth = std::max(tokenWidth, token.size());
            givesWidth = std::max(givesWidth, gives.size());
        }
        for (qsizetype row = 0; row < column.size(); ++row)
            lines[row] = lines[row].leftJustified(left)
                + column[row].first.leftJustified(tokenWidth + 2) + column[row].second;
        left += tokenWidth + 2 + givesWidth + 4;
    }
    return lines.join(u'\n');
}

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

Clock::Clock(const Palette& palette, const QString& timeFormat, const QString& dateFormat,
    const ClockSizes& sizes, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
    , m_timeFormat(timeFormat)
    , m_dateFormat(dateFormat)
    , m_sizes(sizes)
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
    return clockLayout(width, font(), m_time, m_date, m_sizes).height;
}

QWidget* Clock::settingsPage()
{
    auto* page = new QWidget;
    auto* time = formatEdit(m_timeFormat, page);
    auto* date = formatEdit(m_dateFormat, page);
    auto* timeSize = sizeBox(m_sizes.time, page);
    auto* dateSize = sizeBox(m_sizes.date, page);
    auto* timePreview = new QLabel(page);
    auto* datePreview = new QLabel(page);
    auto* syntax = syntaxWell(page);
    auto* form = new QFormLayout(page);
    form->addRow(u"Time"_s, withSize(time, timeSize));
    form->addRow(QString(), timePreview);
    form->addRow(u"Date"_s, withSize(date, dateSize));
    form->addRow(QString(), datePreview);
    form->addRow(u"Syntax"_s, syntax);
    const auto preview = [time, date, timePreview, datePreview, syntax] {
        const QDateTime now = QDateTime::currentDateTime();
        timePreview->setText(now.toString(time->toPlainText()));
        datePreview->setText(now.toString(date->toPlainText()));
        showSyntax(syntax, now);
    };
    for (QPlainTextEdit* edit : { time, date })
        connect(edit, &QPlainTextEdit::textChanged, this, [this, time, date, preview] {
            preview();
            setFormats(time->toPlainText(), date->toPlainText());
        });
    for (QSpinBox* box : { timeSize, dateSize })
        connect(box, &QSpinBox::valueChanged, this,
            [this, timeSize, dateSize] { setSizes({ timeSize->value(), dateSize->value() }); });
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

void Clock::setSizes(const ClockSizes& sizes)
{
    m_sizes = sizes;
    emit heightChanged();
    update();
    emit sizesChosen(sizes);
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
    const ClockLayout l = clockLayout(width(), font(), m_time, m_date, m_sizes);
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
