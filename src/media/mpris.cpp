#include "media/mpris.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace std::chrono;

namespace {

constexpr QLatin1StringView prefix = "org.mpris.MediaPlayer2."_L1;
constexpr QLatin1StringView objectPath = "/org/mpris/MediaPlayer2"_L1;
constexpr QLatin1StringView rootInterface = "org.mpris.MediaPlayer2"_L1;
constexpr QLatin1StringView playerInterface = "org.mpris.MediaPlayer2.Player"_L1;
constexpr QLatin1StringView propertiesInterface = "org.freedesktop.DBus.Properties"_L1;
constexpr QLatin1StringView noTrack = "/org/mpris/MediaPlayer2/TrackList/NoTrack"_L1;

// QtDBus leaves a dictionary marshalled when it arrives inside a variant or a message.
QVariantMap plainMap(const QVariant& value)
{
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<QVariantMap>(value.value<QDBusArgument>());
    return value.toMap();
}

QDBusMessage getProperty(const QString& owner, QLatin1StringView interface, const QString& name)
{
    QDBusMessage message
        = QDBusMessage::createMethodCall(owner, objectPath, propertiesInterface, u"Get"_s);
    message.setArguments({ QString(interface), name });
    return message;
}

// The value of a reply to Properties.Get, or nothing if the call failed.
std::optional<QVariant> gotten(const QDBusMessage& reply)
{
    if (reply.type() != QDBusMessage::ReplyMessage)
        return std::nullopt;
    return reply.arguments().value(0).value<QDBusVariant>().variant();
}

template <typename Handle>
void onReply(const QDBusPendingCall& call, QObject* context, Handle handle)
{
    auto* watcher = new QDBusPendingCallWatcher(call, context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, handle] {
        watcher->deleteLater();
        handle(watcher->reply());
    });
}

} // namespace

Mpris::Mpris(const QDBusConnection& bus, QObject* parent)
    : QObject(parent)
    , m_bus(bus)
    // Qt makes the pattern an arg0namespace match, which the bus refuses with a trailing dot.
    , m_watcher(rootInterface + u'*', bus, QDBusServiceWatcher::WatchForOwnerChange)
{
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &Mpris::ownerChanged);
    // An empty service matches any sender, so each rule covers players yet to start as well.
    m_bus.connect(QString(), objectPath, propertiesInterface, u"PropertiesChanged"_s, this,
        SLOT(propertiesChanged(QDBusMessage)));
    m_bus.connect(
        QString(), objectPath, playerInterface, u"Seeked"_s, this, SLOT(seeked(QDBusMessage)));
    onReply(m_bus.interface()->asyncCall(u"ListNames"_s), this,
        [this](const QDBusMessage& reply) { listed(reply.arguments().value(0).toStringList()); });
}

void Mpris::playPause(const QString& owner) { call(owner, u"PlayPause"_s); }

void Mpris::stop(const QString& owner) { call(owner, u"Stop"_s); }

void Mpris::next(const QString& owner) { call(owner, u"Next"_s); }

void Mpris::previous(const QString& owner) { call(owner, u"Previous"_s); }

void Mpris::setPosition(const QString& owner, microseconds position)
{
    Player* player = find(owner);
    if (!player)
        return;
    const steady_clock::time_point now = steady_clock::now();
    // SetPosition needs the track's id, which is optional; Seek goes by a distance instead.
    if (player->trackId.startsWith(u'/') && player->trackId != noTrack)
        call(owner, u"SetPosition"_s,
            { QVariant::fromValue(QDBusObjectPath(player->trackId)), qlonglong(position.count()) });
    else
        call(owner, u"Seek"_s, { qlonglong((position - positionAt(*player, now)).count()) });
    // Not every player answers with Seeked.
    player->position = position;
    player->sampled = now;
    emit changed();
}

void Mpris::propertiesChanged(const QDBusMessage& message)
{
    const QList<QVariant> arguments = message.arguments();
    const QString owner = message.service();
    const QString interface = arguments.value(0).toString();
    const QVariantMap changed = plainMap(arguments.value(1));
    if (!find(owner))
        return;
    if (interface == playerInterface) {
        apply(owner, changed);
        if (!arguments.value(2).toStringList().isEmpty())
            fetch(owner);
    } else if (interface == rootInterface && changed.contains(u"Identity"_s)) {
        setIdentity(owner, changed.value(u"Identity"_s).toString());
    }
}

void Mpris::seeked(const QDBusMessage& message)
{
    if (find(message.service()))
        apply(message.service(), { { u"Position"_s, message.arguments().value(0) } });
}

void Mpris::listed(const QStringList& names)
{
    for (const QString& name : names) {
        if (!name.startsWith(prefix))
            continue;
        onReply(m_bus.interface()->asyncCall(u"GetNameOwner"_s, name), this,
            [this, name](const QDBusMessage& reply) {
                if (reply.type() == QDBusMessage::ReplyMessage)
                    add(name, reply.arguments().value(0).toString());
            });
    }
}

void Mpris::ownerChanged(const QString& service, const QString& before, const QString& after)
{
    if (!service.startsWith(prefix))
        return;
    if (!before.isEmpty() && std::erase_if(m_players, [&](const Player& player) {
            return player.service == service;
        }) > 0)
        emit changed();
    if (!after.isEmpty())
        add(service, after);
}

void Mpris::add(const QString& service, const QString& owner)
{
    const auto known = std::ranges::find(m_players, service, &Player::service);
    if (known != m_players.end() && known->owner == owner)
        return;
    if (known != m_players.end())
        m_players.erase(known);
    Player player;
    player.service = service;
    player.owner = owner;
    m_players.push_back(player);
    fetch(owner);
}

void Mpris::remove(const QString& owner)
{
    if (std::erase_if(m_players, [&](const Player& player) { return player.owner == owner; }) > 0)
        emit changed();
}

void Mpris::fetch(const QString& owner)
{
    QDBusMessage getAll
        = QDBusMessage::createMethodCall(owner, objectPath, propertiesInterface, u"GetAll"_s);
    getAll.setArguments({ QString(playerInterface) });
    onReply(m_bus.asyncCall(getAll), this, [this, owner](const QDBusMessage& reply) {
        if (reply.type() == QDBusMessage::ReplyMessage) {
            apply(owner, plainMap(reply.arguments().value(0)));
            return;
        }
        qWarning("media: player %s does not answer: %s", qUtf8Printable(owner),
            qUtf8Printable(reply.errorMessage()));
        remove(owner);
    });
    onReply(m_bus.asyncCall(getProperty(owner, rootInterface, u"Identity"_s)), this,
        [this, owner](const QDBusMessage& reply) {
            if (const std::optional<QVariant> identity = gotten(reply))
                setIdentity(owner, identity->toString());
        });
}

void Mpris::readPosition(const QString& owner)
{
    onReply(m_bus.asyncCall(getProperty(owner, playerInterface, u"Position"_s)), this,
        [this, owner](const QDBusMessage& reply) {
            if (const std::optional<QVariant> position = gotten(reply))
                apply(owner, { { u"Position"_s, *position } });
        });
}

void Mpris::setIdentity(const QString& owner, const QString& identity)
{
    for (Player& player : m_players)
        if (player.owner == owner)
            player.identity = identity;
    emit changed();
}

void Mpris::apply(const QString& owner, QVariantMap changed)
{
    if (!find(owner))
        return;
    if (const auto metadata = changed.find(u"Metadata"_s); metadata != changed.end())
        *metadata = plainMap(*metadata);
    const steady_clock::time_point now = steady_clock::now();
    for (Player& player : m_players) {
        if (player.owner != owner)
            continue;
        const bool playing = player.playback == Playback::Playing;
        applyProperties(player, changed, now);
        if (!playing && player.playback == Playback::Playing)
            player.started = ++m_starts;
    }
    // Starting, stopping and changing track can each move the position without a Seeked.
    if (!changed.contains(u"Position"_s)
        && (changed.contains(u"PlaybackStatus"_s) || changed.contains(u"Metadata"_s)
            || changed.contains(u"Rate"_s)))
        readPosition(owner);
    emit this->changed();
}

void Mpris::call(const QString& owner, const QString& method, const QVariantList& arguments)
{
    QDBusMessage message
        = QDBusMessage::createMethodCall(owner, objectPath, playerInterface, method);
    message.setArguments(arguments);
    m_bus.send(message);
}

Player* Mpris::find(const QString& owner)
{
    const auto player = std::ranges::find(m_players, owner, &Player::owner);
    return player == m_players.end() ? nullptr : &*player;
}
