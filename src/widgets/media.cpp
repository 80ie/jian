#include "widgets/media.h"
#include "media/coverart.h"
#include "media/mpris.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "widgets/foreignslot.h"
#include "widgets/pip.h"

#include <QComboBox>
#include <QDBusConnection>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;
using namespace std::chrono;

namespace {

constexpr int marqueeMs = 60;
constexpr double wheelStep = 0.05;
// A drag finer than this is not worth a write.
constexpr double volumeGrain = 0.005;

QString labelOf(const Player& player)
{
    return player.identity.isEmpty() ? playerKey(player.service) : player.identity;
}

} // namespace

Media::Media(const Palette& palette, const QString& source, WindowModel* windows, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
    , m_source(source)
{
    m_marquee.setInterval(marqueeMs);
    connect(&m_marquee, &QTimer::timeout, this, [this] {
        m_scroll = marqueeStep(m_scroll, m_span);
        update(layout().title);
    });
    connect(&m_progress, &QTimer::timeout, this, [this] { update(layout().track); });
    m_title = titleText();
    if (!windows)
        return;

    if (const QDBusConnection bus = QDBusConnection::sessionBus(); bus.isConnected()) {
        m_mpris = new Mpris(bus, this);
        connect(m_mpris, &Mpris::changed, this, [this] { setPlayers(m_mpris->players()); });
    } else {
        qWarning("media: no session bus");
    }
    m_pulse = new Pulse(this);
    connect(m_pulse, &Pulse::changed, this, [this] { setSinks(m_pulse->sinks()); });
    m_cover = new CoverArt(this);
    connect(m_cover, &CoverArt::changed, this, [this] { setArt(m_cover->image()); });
    m_slot = new ForeignSlot(*windows, this);
    m_slot->hide();
    m_pip = new PictureInPicture(*windows, *this, *m_slot, this);
    connect(m_pip, &PictureInPicture::sizeChanged, this, [this] {
        const int before = heightForWidth(width());
        m_pipSize = m_pip->size();
        refresh(before);
    });
}

// The held window would otherwise go down with the slot's X window.
Media::~Media()
{
    if (m_pip)
        m_pip->letGo();
}

QString Media::id() const { return u"media"_s; }

int Media::heightForWidth(int width) const
{
    return mediaLayout(width, wellSize(), showsTrack()).height;
}

void Media::setPlayers(const std::vector<Player>& players)
{
    const int before = heightForWidth(width());
    m_players = players;
    refresh(before);
}

void Media::setSinks(const Sinks& sinks)
{
    m_sinks = sinks;
    update();
}

void Media::setArt(const QImage& art)
{
    const int before = heightForWidth(width());
    m_art = art;
    refresh(before);
}

void Media::paintEvent(QPaintEvent* event)
{
    const MediaLayout l = layout();
    const Player* current = player();
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));

    const QString source = current ? labelOf(*current) : m_source;
    drawSelector(
        p, l.source, m_palette, source.isEmpty() ? u"No player"_s : source, current != nullptr);
    if (!l.art.isNull()) {
        // The marquee repaints its field many times a second, and scaling the art is not free.
        if (event->rect().intersects(l.art))
            drawArt(p, l.art, m_palette, m_pipSize.isValid() ? QImage() : m_art);
        paint::divider(p, QRect(0, l.artRule, width(), 2), m_palette);
    }
    drawTitle(
        p, l.title, m_palette, m_title, current && !current->title.isEmpty(), m_scroll, m_span);
    if (!l.track.isNull())
        drawTrack(p, l.track, m_palette,
            trackFill(l.track, m_seek.value_or(positionAt(*current, steady_clock::now())),
                *current->length));
    const bool live = current && current->canControl;
    drawTransport(p, l.buttons, m_palette,
        { live && current->canGoPrevious, live, live, live && current->canGoNext }, m_pressed,
        current && current->playback == Playback::Playing);
    paint::divider(p, QRect(0, l.rule, width(), 2), m_palette);

    const Output* output = m_sinks.current();
    drawVolume(p, l, m_palette, std::min(1.0, m_volume.value_or(output ? output->volume : 0)),
        !output || output->muted);
    drawSelector(p, l.output, m_palette, output ? output->description : u"No output device"_s,
        !m_sinks.outputs.empty());
}

void Media::resizeEvent(QResizeEvent*)
{
    if (m_slot)
        m_slot->setGeometry(artBox(layout().art));
    restartTimers();
}

void Media::showEvent(QShowEvent*) { restartTimers(); }

void Media::hideEvent(QHideEvent*) { restartTimers(); }

void Media::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QPoint pos = event->position().toPoint();
    const MediaLayout l = layout();
    for (int i = 0; i < 4; ++i) {
        if (l.buttons[i].contains(pos)) {
            m_pressed = Transport(i);
            update(l.buttons[i]);
            return;
        }
    }
    const QRect volumeBand(
        l.groove.left() - 6, l.thumbTop - 2, l.groove.width() + 12, thumbSize.height() + 4);
    if (l.source.contains(pos)) {
        chooseSource(l.source);
    } else if (l.output.contains(pos)) {
        chooseOutput(l.output);
    } else if (l.speaker.contains(pos) && m_pulse) {
        m_pulse->toggleMute();
    } else if (volumeBand.contains(pos) && m_sinks.current()) {
        m_volume = m_sinks.current()->volume;
        dragVolume(pos.x());
    } else if (l.track.contains(pos)) {
        m_seek = trackPosition(l.track, pos.x(), *player()->length);
        restartTimers();
        update(l.track);
    }
}

void Media::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    const MediaLayout l = layout();
    if (m_volume) {
        dragVolume(pos.x());
    } else if (m_seek && showsTrack()) {
        m_seek = trackPosition(l.track, pos.x(), *player()->length);
        update(l.track);
    } else if (m_pressed && !l.buttons[int(*m_pressed)].contains(pos)) {
        update(l.buttons[int(*m_pressed)]);
        m_pressed.reset();
    }
}

void Media::mouseReleaseEvent(QMouseEvent* event)
{
    const MediaLayout l = layout();
    if (m_volume) {
        m_volume.reset();
        update();
    } else if (m_seek) {
        if (const Player* current = player(); m_mpris && current && showsTrack())
            m_mpris->setPosition(current->owner, *m_seek);
        m_seek.reset();
        restartTimers();
        update(l.track);
    } else if (m_pressed) {
        const Transport which = *std::exchange(m_pressed, std::nullopt);
        update(l.buttons[int(which)]);
        if (l.buttons[int(which)].contains(event->position().toPoint()))
            act(which);
    }
}

void Media::wheelEvent(QWheelEvent* event)
{
    const Output* output = m_sinks.current();
    if (!m_pulse || !output || event->angleDelta().y() == 0)
        return;
    m_pulse->setVolume(output->volume + (event->angleDelta().y() > 0 ? wheelStep : -wheelStep));
}

const Player* Media::player() const { return choosePlayer(m_players, m_source); }

bool Media::showsTrack() const
{
    const Player* current = player();
    return current && current->canSeek && current->length;
}

QSize Media::wellSize() const { return m_pipSize.isValid() ? m_pipSize : m_art.size(); }

MediaLayout Media::layout() const { return mediaLayout(width(), wellSize(), showsTrack()); }

QString Media::titleText() const
{
    const Player* current = player();
    return current && !current->title.isEmpty() ? current->title : u"—"_s;
}

void Media::refresh(int heightBefore)
{
    const Player* current = player();
    if (m_cover && current)
        m_cover->setTrack(current->owner, current->trackId, current->artUrl);
    else if (m_cover)
        m_cover->setTrack(QString(), QString(), QString());
    if (const QString title = titleText(); title != m_title) {
        m_title = title;
        m_scroll = 0;
    }
    if (heightForWidth(width()) != heightBefore)
        emit heightChanged();
    if (m_slot) {
        m_slot->setGeometry(artBox(layout().art));
        m_slot->setVisible(m_pipSize.isValid());
    }
    restartTimers();
    update();
}

// The marquee runs while the title overflows, the trackbar while it moves; neither while hidden.
void Media::restartTimers()
{
    const MediaLayout l = layout();
    const QFontMetrics metrics = fontMetrics();
    m_span = metrics.horizontalAdvance(m_title) > titleRoom(l.title)
        ? metrics.horizontalAdvance(m_title + marqueeGap)
        : 0;
    m_scroll = m_span > 0 ? m_scroll % m_span : 0;
    if (m_span == 0 || !isVisible())
        m_marquee.stop();
    else if (!m_marquee.isActive())
        m_marquee.start();

    const Player* current = player();
    if (isVisible() && showsTrack() && current->playback == Playback::Playing && current->rate > 0
        && !m_seek) {
        const milliseconds step = trackStep(l.track, *current->length, current->rate);
        if (!m_progress.isActive() || m_progress.intervalAsDuration() != step)
            m_progress.start(step);
    } else {
        m_progress.stop();
    }
}

void Media::setSource(const QString& source)
{
    const int before = heightForWidth(width());
    m_source = source;
    emit sourceChosen(source);
    refresh(before);
}

std::vector<Media::Source> Media::sources() const
{
    std::vector<Source> sources { { u"Automatic"_s, QString() } };
    const auto listed = [&sources](const QString& key) {
        return std::ranges::find(sources, key, &Source::key) != sources.end();
    };
    for (const Player& player : m_players)
        if (const QString key = playerKey(player.service); !listed(key))
            sources.push_back({ labelOf(player), key });
    if (!listed(m_source))
        sources.push_back({ m_source, m_source });
    return sources;
}

void Media::chooseSource(const QRect& field)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    for (const auto& [label, key] : sources()) {
        QAction* action = menu->addAction(label, this, [this, key] { setSource(key); });
        action->setCheckable(true);
        action->setChecked(key == m_source);
        if (key.isEmpty())
            menu->addSeparator();
    }
    menu->popup(mapToGlobal(field.bottomLeft()));
}

QWidget* Media::settingsPage()
{
    auto* page = new QWidget;
    auto* source = new QComboBox(page);
    for (const auto& [label, key] : sources()) {
        source->addItem(label, key);
        if (key == m_source)
            source->setCurrentIndex(source->count() - 1);
    }
    auto* hint = new QLabel(u"Automatic follows whichever player started playing last. A player "
                            "chosen by name keeps the controls while it is not running."_s,
        page);
    hint->setWordWrap(true);
    auto* form = new QFormLayout(page);
    form->addRow(u"Player"_s, source);
    form->addRow(hint);
    connect(source, &QComboBox::activated, this,
        [this, source] { setSource(source->currentData().toString()); });
    return page;
}

void Media::chooseOutput(const QRect& field)
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

void Media::dragVolume(int x)
{
    const double volume = volumeAt(layout().groove, x);
    if (std::abs(volume - *m_volume) < volumeGrain)
        return;
    m_volume = volume;
    if (m_pulse)
        m_pulse->setVolume(volume);
    update();
}

void Media::act(Transport which)
{
    const Player* current = player();
    if (!m_mpris || !current || !current->canControl)
        return;
    switch (which) {
    case Transport::Previous:
        if (current->canGoPrevious)
            m_mpris->previous(current->owner);
        break;
    case Transport::Stop:
        m_mpris->stop(current->owner);
        break;
    case Transport::PlayPause:
        m_mpris->playPause(current->owner);
        break;
    case Transport::Next:
        if (current->canGoNext)
            m_mpris->next(current->owner);
        break;
    }
}
