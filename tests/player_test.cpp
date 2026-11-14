#include "media/player.h"

#include <QDBusObjectPath>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace std::chrono;

namespace {

Player player(const QString& service, Playback playback, quint64 started)
{
    Player player;
    player.service = service;
    player.playback = playback;
    player.started = started;
    return player;
}

} // namespace

class PlayerTest : public QObject {
    Q_OBJECT

private slots:
    void key_data()
    {
        QTest::addColumn<QString>("service");
        QTest::addColumn<QString>("key");
        QTest::newRow("plain") << u"org.mpris.MediaPlayer2.vlc"_s << u"vlc"_s;
        QTest::newRow("pid") << u"org.mpris.MediaPlayer2.chromium.instance4242"_s << u"chromium"_s;
        QTest::newRow("firefox") << u"org.mpris.MediaPlayer2.firefox.instance_1_84"_s
                                 << u"firefox"_s;
        QTest::newRow("dotted") << u"org.mpris.MediaPlayer2.kde.dragonplayer"_s
                                << u"kde.dragonplayer"_s;
    }

    void key()
    {
        QFETCH(QString, service);
        QFETCH(QString, key);
        QCOMPARE(playerKey(service), key);
    }

    void title_data()
    {
        QTest::addColumn<QVariantMap>("metadata");
        QTest::addColumn<QString>("title");
        QTest::newRow("both") << QVariantMap { { u"xesam:title"_s, u" Roygbiv "_s },
            { u"xesam:artist"_s, QStringList { u"Boards of Canada"_s } } }
                              << u"Boards of Canada - Roygbiv"_s;
        QTest::newRow("two artists") << QVariantMap { { u"xesam:title"_s, u"T"_s },
            { u"xesam:artist"_s, QStringList { u"A"_s, QString(), u"B"_s } } }
                                     << u"A, B - T"_s;
        QTest::newRow("artist as a string") << QVariantMap { { u"xesam:title"_s, u"T"_s },
            { u"xesam:artist"_s, u"A"_s } } << u"A - T"_s;
        QTest::newRow("title only") << QVariantMap { { u"xesam:title"_s, u"T"_s } } << u"T"_s;
        QTest::newRow("url only") << QVariantMap { { u"xesam:artist"_s, u"A"_s },
            { u"xesam:url"_s, u"file:///music/My%20Song.ogg"_s } }
                                  << u"My Song.ogg"_s;
        QTest::newRow("nothing") << QVariantMap() << QString();
    }

    void title()
    {
        QFETCH(QVariantMap, metadata);
        QFETCH(QString, title);
        QCOMPARE(trackTitle(metadata), title);
    }

    void metadata()
    {
        Player p;
        const QVariantMap metadata { { u"mpris:trackid"_s,
                                         QVariant::fromValue(QDBusObjectPath(u"/t/1"_s)) },
            { u"mpris:length"_s, qlonglong(240'000'000) },
            { u"mpris:artUrl"_s, u"file:///a.png"_s }, { u"xesam:title"_s, u"T"_s } };
        applyProperties(p, { { u"Metadata"_s, metadata }, { u"CanSeek"_s, true } }, { });
        QCOMPARE(p.trackId, u"/t/1"_s);
        QCOMPARE(p.length, microseconds(240'000'000));
        QCOMPARE(p.artUrl, u"file:///a.png"_s);
        QVERIFY(p.canSeek);

        applyProperties(
            p, { { u"Metadata"_s, QVariantMap { { u"mpris:trackid"_s, u"/t/2"_s } } } }, { });
        QCOMPARE(p.trackId, u"/t/2"_s);
        QCOMPARE(p.length, std::nullopt);
        QCOMPARE(p.title, QString());
    }

    void position()
    {
        const steady_clock::time_point start;
        Player p;
        p.length = 100s;
        applyProperties(p,
            { { u"PlaybackStatus"_s, u"Playing"_s }, { u"Position"_s, qlonglong(10'000'000) } },
            start);
        QCOMPARE(positionAt(p, start), 10s);
        QCOMPARE(positionAt(p, start + 5s), 15s);
        QCOMPARE(positionAt(p, start + 500s), 100s);

        applyProperties(p, { { u"Rate"_s, 2.0 } }, start + 5s);
        QCOMPARE(positionAt(p, start + 7s), 19s);

        applyProperties(p, { { u"PlaybackStatus"_s, u"Paused"_s } }, start + 8s);
        QCOMPARE(positionAt(p, start + 8s), 21s);
        QCOMPARE(positionAt(p, start + 60s), 21s);
    }

    void unknownLength()
    {
        Player p;
        applyProperties(p, { { u"PlaybackStatus"_s, u"Playing"_s } }, { });
        QCOMPARE(positionAt(p, steady_clock::time_point() + 1h), 1h);
    }

    void choice()
    {
        const QString vlc = u"org.mpris.MediaPlayer2.vlc"_s;
        const QString mpv = u"org.mpris.MediaPlayer2.mpv"_s;
        const QString firefox = u"org.mpris.MediaPlayer2.firefox.instance_1_9"_s;

        QCOMPARE(choosePlayer({ }, QString()), nullptr);

        std::vector<Player> players { player(vlc, Playback::Stopped, 0),
            player(mpv, Playback::Stopped, 0) };
        QCOMPARE(choosePlayer(players, QString()), &players[0]);

        players = { player(vlc, Playback::Paused, 3), player(mpv, Playback::Playing, 1) };
        QCOMPARE(choosePlayer(players, QString()), &players[1]);

        players = { player(vlc, Playback::Playing, 3), player(mpv, Playback::Playing, 4) };
        QCOMPARE(choosePlayer(players, QString()), &players[1]);

        players = { player(vlc, Playback::Paused, 5), player(mpv, Playback::Stopped, 4) };
        QCOMPARE(choosePlayer(players, QString()), &players[0]);

        players = { player(vlc, Playback::Playing, 5), player(firefox, Playback::Paused, 4),
            player(firefox, Playback::Paused, 2) };
        QCOMPARE(choosePlayer(players, u"firefox"_s), &players[1]);
        QCOMPARE(choosePlayer(players, u"mpv"_s), nullptr);
    }
};

QTEST_GUILESS_MAIN(PlayerTest)
#include "player_test.moc"
