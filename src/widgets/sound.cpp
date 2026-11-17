#include "widgets/sound.h"
#include "paint/bevel.h"
#include "paint/dropdown.h"
#include "palette/palette.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygon>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace {

constexpr double wheelStep = 0.05;
// A drag finer than this is not worth a write.
constexpr double volumeGrain = 0.005;

// Drawn rather than taken from an icon theme, which a render cannot count on.
void speaker(QPainter& p, const QRect& r)
{
    const int y = r.center().y();
    p.drawPolygon(QPolygon({ QPoint(r.left() + 2, y - 3), QPoint(r.left() + 6, y - 3),
        QPoint(r.left() + 11, r.top() + 2), QPoint(r.left() + 11, r.bottom() - 2),
        QPoint(r.left() + 6, y + 3), QPoint(r.left() + 2, y + 3) }));
}

// The speaker, the groove with its scale, and the thumb at `volume`.
void drawVolume(
    QPainter& p, const SoundLayout& l, const Palette& palette, double volume, bool muted)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    p.setBrush(muted ? palette.tone(-4) : palette.ink);
    speaker(p, l.speaker);
    p.restore();
    paint::groove(p, l.groove, palette);
    // Five marks, ends included, spaced by the groove's width rather than a fixed pitch.
    const int travel = l.groove.width() - thumbSize.width();
    for (int i = 0; i < 5; ++i)
        paint::tick(p,
            { l.groove.left() + thumbSize.width() / 2 + qRound(travel * i / 4.0),
                l.thumbTop + thumbSize.height() - 1 },
            palette);
    paint::thumb(p, QRect(QPoint(thumbLeft(l.groove, volume), l.thumbTop), thumbSize), palette);
}

} // namespace

Sound::Sound(const Palette& palette, WindowModel* windows, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
{
    if (!windows)
        return;
    m_pulse = new Pulse(this);
    connect(m_pulse, &Pulse::changed, this, [this] { setSinks(m_pulse->sinks()); });
}

QString Sound::id() const { return u"sound"_s; }

int Sound::heightForWidth(int width) const { return soundLayout(width).height; }

void Sound::setSinks(const Sinks& sinks)
{
    m_sinks = sinks;
    update();
}

void Sound::paintEvent(QPaintEvent*)
{
    const SoundLayout l = layout();
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    const Output* output = m_sinks.current();
    drawVolume(p, l, m_palette, std::min(1.0, m_volume.value_or(output ? output->volume : 0)),
        !output || output->muted);
    paint::dropDown(p, l.output, m_palette, output ? output->description : u"No output device"_s,
        !m_sinks.outputs.empty());
}

void Sound::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QPoint pos = event->position().toPoint();
    const SoundLayout l = layout();
    const QRect volumeBand(
        l.groove.left() - 6, l.thumbTop - 2, l.groove.width() + 12, thumbSize.height() + 4);
    if (l.output.contains(pos)) {
        chooseOutput(l.output);
    } else if (l.speaker.contains(pos) && m_pulse) {
        m_pulse->toggleMute();
    } else if (volumeBand.contains(pos) && m_sinks.current()) {
        m_volume = m_sinks.current()->volume;
        dragVolume(pos.x());
    }
}

void Sound::mouseMoveEvent(QMouseEvent* event)
{
    if (m_volume)
        dragVolume(event->position().toPoint().x());
}

void Sound::mouseReleaseEvent(QMouseEvent*)
{
    if (m_volume) {
        m_volume.reset();
        update();
    }
}

void Sound::wheelEvent(QWheelEvent* event)
{
    const Output* output = m_sinks.current();
    if (!m_pulse || !output || event->angleDelta().y() == 0)
        return;
    m_pulse->setVolume(output->volume + (event->angleDelta().y() > 0 ? wheelStep : -wheelStep));
}

SoundLayout Sound::layout() const { return soundLayout(width()); }

void Sound::chooseOutput(const QRect& field)
{
    if (m_sinks.outputs.empty())
        return;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    for (const Output& output : m_sinks.outputs) {
        QAction* action = menu->addAction(output.description, this, [this, name = output.name] {
            if (m_pulse)
                m_pulse->setDefault(name);
        });
        action->setCheckable(true);
        action->setChecked(output.name == m_sinks.defaultName);
    }
    menu->popup(mapToGlobal(field.bottomLeft()));
}

void Sound::dragVolume(int x)
{
    const double volume = volumeAt(layout().groove, x);
    if (std::abs(volume - *m_volume) < volumeGrain)
        return;
    m_volume = volume;
    if (m_pulse)
        m_pulse->setVolume(volume);
    update();
}
