#include "settings/rampstrip.h"
#include "palette/oklab.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include <array>

using namespace Qt::StringLiterals;

namespace {

constexpr int toneCount = lightestTone - darkestTone + 1;
constexpr int swatchHeight = 40;
// The face stands this much taller than the other swatches, above and below.
constexpr int faceRise = 6;
constexpr int numberHeight = 18;
constexpr int markSize = 8;
constexpr int markInset = 6;
constexpr int swatchWidth = 40;

} // namespace

QString toneName(int tone)
{
    if (tone == 0)
        return u"face"_s;
    return tone > 0 ? u"+%1"_s.arg(tone) : u"−%1"_s.arg(-tone);
}

QPixmap paletteStrip(const Palette& palette, QSize size)
{
    const std::array named { palette.ink, palette.fill, palette.selection, palette.pagerDesk };
    const int units = toneCount + 1 + int(named.size());
    const int inset = size.height() / 5;
    const auto column = [&size, units](int unit, const QColor& color, int inset, QPainter& p) {
        const int left = unit * size.width() / units;
        p.fillRect(left, inset, (unit + 1) * size.width() / units - left, size.height() - 2 * inset,
            color);
    };
    QPixmap picture(size);
    picture.fill(Qt::transparent);
    QPainter p(&picture);
    for (int tone = darkestTone; tone <= lightestTone; ++tone)
        column(tone - darkestTone, palette.tone(tone), tone == 0 ? 0 : inset, p);
    for (std::size_t i = 0; i < named.size(); ++i)
        column(toneCount + 1 + int(i), named[i], inset, p);
    return picture;
}

RampStrip::RampStrip(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RampStrip::setColors(const Palette& palette)
{
    m_palette = palette;
    hover(m_hovered);
    update();
}

void RampStrip::setCurrent(std::optional<int> tone)
{
    m_current = tone;
    update();
}

QSize RampStrip::sizeHint() const
{
    return { toneCount * swatchWidth, swatchHeight + 2 * faceRise + numberHeight };
}

void RampStrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    for (int tone = darkestTone; tone <= lightestTone; ++tone) {
        const QRect r = swatch(tone);
        const QColor color = m_palette.tone(tone);
        p.fillRect(r, color);
        if (m_palette.overrides.contains(tone)) {
            p.setPen(Qt::NoPen);
            p.setBrush(
                toOklab(color).lightness > 0.5 ? m_palette.ink : m_palette.tone(lightestTone));
            p.drawEllipse(QRect(0, 0, markSize, markSize)
                    .translated(r.center().x() - markSize / 2, r.bottom() - markSize - markInset));
        }
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRect(r.left(), height() - numberHeight, r.width(), numberHeight),
            Qt::AlignCenter, toneName(tone));
    }
    if (m_current) {
        p.setPen(QPen(palette().color(QPalette::Highlight), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(swatch(*m_current).adjusted(1, 1, -1, -1));
    }
}

void RampStrip::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (const std::optional<int> tone = toneAt(event->position().toPoint()))
        emit clicked(*tone);
}

void RampStrip::mouseMoveEvent(QMouseEvent* event) { hover(toneAt(event->position().toPoint())); }

void RampStrip::leaveEvent(QEvent*) { hover(std::nullopt); }

void RampStrip::contextMenuEvent(QContextMenuEvent* event)
{
    const std::optional<int> tone = toneAt(event->pos());
    if (!tone || !m_palette.overrides.contains(*tone))
        return;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(u"Reset"_s, this, [this, tone] { emit resetRequested(*tone); });
    menu->popup(event->globalPos());
}

QRect RampStrip::swatch(int tone) const
{
    const int i = tone - darkestTone;
    const int left = i * width() / toneCount;
    const QRect r(left, faceRise, (i + 1) * width() / toneCount - left, swatchHeight);
    return tone == 0 ? r.adjusted(0, -faceRise, 0, faceRise) : r;
}

std::optional<int> RampStrip::toneAt(QPoint pos) const
{
    for (int tone = darkestTone; tone <= lightestTone; ++tone)
        if (swatch(tone).contains(pos))
            return tone;
    return std::nullopt;
}

// The tooltip says where the color under the pointer comes from.
void RampStrip::hover(std::optional<int> tone)
{
    if (tone != m_hovered) {
        m_hovered = tone;
        emit hovered(tone);
    }
    if (!tone) {
        setToolTip(QString());
        return;
    }
    if (*tone == 0)
        setToolTip(u"Face"_s);
    else if (m_palette.overrides.contains(*tone))
        setToolTip(u"Set by hand"_s);
    else
        setToolTip(u"From the face"_s);
}
