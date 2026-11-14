#include "media/player.h"

#include <QDBusObjectPath>
#include <QUrl>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;
using namespace std::chrono;

namespace {

constexpr QLatin1StringView prefix = "org.mpris.MediaPlayer2."_L1;

constexpr std::pair<QLatin1StringView, bool Player::*> flags[] {
    { "CanControl"_L1, &Player::canControl },
    { "CanGoNext"_L1, &Player::canGoNext },
    { "CanGoPrevious"_L1, &Player::canGoPrevious },
    { "CanSeek"_L1, &Player::canSeek },
};

Playback playbackFrom(const QString& status)
{
    if (status == "Playing"_L1)
        return Playback::Playing;
    if (status == "Paused"_L1)
        return Playback::Paused;
    return Playback::Stopped;
}

// The spec makes the track id an object path, but some players send it as a string.
QString pathOf(const QVariant& value)
{
    if (value.metaType() == QMetaType::fromType<QDBusObjectPath>())
        return value.value<QDBusObjectPath>().path();
    return value.toString();
}

void applyMetadata(Player& player, const QVariantMap& metadata)
{
    player.title = trackTitle(metadata);
    player.artUrl = metadata.value(u"mpris:artUrl"_s).toString();
    player.trackId = pathOf(metadata.value(u"mpris:trackid"_s));
    bool ok = false;
    const qlonglong length = metadata.value(u"mpris:length"_s).toLongLong(&ok);
    player.length = ok && length > 0 ? std::optional(microseconds(length)) : std::nullopt;
}

} // namespace

QString playerKey(QStringView service)
{
    if (service.startsWith(prefix))
        service = service.sliced(prefix.size());
    const qsizetype instance = service.indexOf(u".instance");
    return (instance < 0 ? service : service.first(instance)).toString();
}

QString trackTitle(const QVariantMap& metadata)
{
    const QString title = metadata.value(u"xesam:title"_s).toString().trimmed();
    QStringList artists = metadata.value(u"xesam:artist"_s).toStringList();
    artists.removeAll(QString());
    const QString artist = artists.join(", "_L1).trimmed();
    if (!title.isEmpty() && !artist.isEmpty())
        return artist + " - "_L1 + title;
    if (!title.isEmpty())
        return title;
    return QUrl(metadata.value(u"xesam:url"_s).toString()).fileName();
}

// The position is carried up to `now` first, so that a change of state or rate starts from
// where playback had got to.
void applyProperties(Player& player, const QVariantMap& changed, steady_clock::time_point now)
{
    player.position = positionAt(player, now);
    player.sampled = now;
    for (const auto& [key, value] : changed.asKeyValueRange()) {
        if (key == "PlaybackStatus"_L1)
            player.playback = playbackFrom(value.toString());
        else if (key == "Metadata"_L1)
            applyMetadata(player, value.toMap());
        else if (key == "Position"_L1)
            player.position = microseconds(value.toLongLong());
        else if (key == "Rate"_L1)
            player.rate = value.toDouble();
        for (const auto& [name, flag] : flags)
            if (key == name)
                player.*flag = value.toBool();
    }
}

microseconds positionAt(const Player& player, steady_clock::time_point now)
{
    if (player.playback != Playback::Playing)
        return player.position;
    const duration<double, std::micro> elapsed = now - player.sampled;
    const microseconds position
        = player.position + duration_cast<microseconds>(elapsed * player.rate);
    return std::clamp(position, microseconds(0), player.length.value_or(microseconds::max()));
}

const Player* choosePlayer(const std::vector<Player>& players, const QString& pinned)
{
    const auto rank = [](const Player& player) {
        return std::pair(player.playback == Playback::Playing, player.started);
    };
    const Player* chosen = nullptr;
    for (const Player& player : players)
        if ((pinned.isEmpty() || playerKey(player.service) == pinned)
            && (!chosen || rank(player) > rank(*chosen)))
            chosen = &player;
    return chosen;
}
