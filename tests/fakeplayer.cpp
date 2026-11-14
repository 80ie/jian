#include "fakeplayer.h"

#include <QDBusMessage>

using namespace Qt::StringLiterals;

namespace {

const QString objectPath = u"/org/mpris/MediaPlayer2"_s;

} // namespace

FakePlayer::FakePlayer(const QDBusConnection& bus, const QString& name, const QString& identity)
    : m_bus(bus)
    , m_service(u"org.mpris.MediaPlayer2."_s + name)
    , m_identity(identity)
{
    m_since.start();
    new FakeRoot(this);
    auto* controls = new FakeControls(this);
    connect(this, &FakePlayer::seeked, controls, &FakeControls::Seeked);
    if (!m_bus.registerObject(objectPath, this, QDBusConnection::ExportAdaptors)
        || !m_bus.registerService(m_service))
        qWarning("fakeplayer: cannot register %s", qUtf8Printable(m_service));
}

FakePlayer::~FakePlayer()
{
    m_bus.unregisterService(m_service);
    m_bus.unregisterObject(objectPath);
}

qlonglong FakePlayer::position() const
{
    return m_playing ? m_position + m_since.elapsed() * 1000 : m_position;
}

void FakePlayer::setMetadata(const QVariantMap& metadata)
{
    m_metadata = metadata;
    m_position = 0;
    m_since.restart();
    notify({ { u"Metadata"_s, metadata } });
}

void FakePlayer::setPlaying(bool playing)
{
    m_position = position();
    m_since.restart();
    m_playing = playing;
    m_status = playing ? u"Playing"_s : u"Paused"_s;
    notify({ { u"PlaybackStatus"_s, m_status } });
}

void FakePlayer::setCanSeek(bool canSeek)
{
    m_canSeek = canSeek;
    notify({ { u"CanSeek"_s, canSeek } });
}

void FakePlayer::jump(qlonglong position)
{
    m_position = position;
    m_since.restart();
    emit seeked(position);
}

void FakePlayer::playPause()
{
    calls << u"PlayPause"_s;
    setPlaying(!m_playing);
}

void FakePlayer::stop()
{
    calls << u"Stop"_s;
    m_playing = false;
    m_status = u"Stopped"_s;
    m_position = 0;
    notify({ { u"PlaybackStatus"_s, m_status } });
}

void FakePlayer::seek(qlonglong offset)
{
    calls << u"Seek %1"_s.arg(offset);
    jump(position() + offset);
}

void FakePlayer::setPosition(const QDBusObjectPath& track, qlonglong position)
{
    calls << u"SetPosition %1 %2"_s.arg(track.path()).arg(position);
    jump(position);
}

void FakePlayer::notify(const QVariantMap& changed)
{
    QDBusMessage message = QDBusMessage::createSignal(
        objectPath, u"org.freedesktop.DBus.Properties"_s, u"PropertiesChanged"_s);
    message.setArguments({ u"org.mpris.MediaPlayer2.Player"_s, changed, QStringList() });
    m_bus.send(message);
}

FakeRoot::FakeRoot(FakePlayer* player)
    : QDBusAbstractAdaptor(player)
    , m_player(player)
{
}

FakeControls::FakeControls(FakePlayer* player)
    : QDBusAbstractAdaptor(player)
    , m_player(player)
{
}

void FakeControls::Play()
{
    m_player->calls << u"Play"_s;
    m_player->setPlaying(true);
}

void FakeControls::Pause()
{
    m_player->calls << u"Pause"_s;
    m_player->setPlaying(false);
}

void FakeControls::Next() { m_player->calls << u"Next"_s; }

void FakeControls::Previous() { m_player->calls << u"Previous"_s; }
