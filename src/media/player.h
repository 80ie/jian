#pragma once

#include <QString>
#include <QVariantMap>

#include <chrono>
#include <optional>
#include <vector>

enum class Playback { Stopped, Paused, Playing };

// One MPRIS player as its properties last described it.
struct Player {
    // The well-known name, and the unique name its signals come from.
    QString service;
    QString owner;
    QString identity;
    Playback playback = Playback::Stopped;
    QString title;
    QString artUrl;
    QString trackId;
    std::optional<std::chrono::microseconds> length;
    // MPRIS signals jumps in position but not its steady advance, so this is where it was at
    // `sampled`.
    std::chrono::microseconds position { 0 };
    std::chrono::steady_clock::time_point sampled;
    double rate = 1;
    bool canControl = false;
    bool canGoNext = false;
    bool canGoPrevious = false;
    bool canSeek = false;
    // When it last started playing, counted in starts across all players; 0 if it never has.
    quint64 started = 0;

    bool operator==(const Player&) const = default;
};

// The part of a well-known name that survives a restart: "vlc" for
// org.mpris.MediaPlayer2.vlc.instance4242.
QString playerKey(QStringView service);
// "Artist - Title", or as much of it as `metadata` has.
QString trackTitle(const QVariantMap& metadata);
// Takes in changed org.mpris.MediaPlayer2.Player properties, with Metadata as a plain map.
void applyProperties(
    Player& player, const QVariantMap& changed, std::chrono::steady_clock::time_point now);
std::chrono::microseconds positionAt(
    const Player& player, std::chrono::steady_clock::time_point now);
// The player the controls act on. Among the players `pinned` names, or all if it is empty: the
// playing one that started last, else the one that started last, else the first.
const Player* choosePlayer(const std::vector<Player>& players, const QString& pinned);
