#include "fakeplayer.h"
#include "media/mpris.h"

#include <QDBusMessage>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono;

namespace {

// A session bus with no service activation, listening only inside the test's own directory.
constexpr QLatin1StringView busConfig = R"(<busconfig>
  <type>session</type>
  <listen>unix:dir=%1</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
)"_L1;

const Player* keyed(const Mpris& mpris, const QString& key)
{
    for (const Player& player : mpris.players())
        if (playerKey(player.service) == key)
            return &player;
    return nullptr;
}

QString titleOf(const Mpris& mpris, const QString& key)
{
    const Player* player = keyed(mpris, key);
    return player ? player->title : QString();
}

QVariantMap track(const QString& title, const QString& id = QString())
{
    QVariantMap metadata { { u"xesam:title"_s, title }, { u"mpris:length"_s, qlonglong(240e6) } };
    if (!id.isEmpty())
        metadata[u"mpris:trackid"_s] = QVariant::fromValue(QDBusObjectPath(id));
    return metadata;
}

} // namespace

class MprisTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QFile config(m_dir.filePath(u"bus.conf"_s));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write(QString(busConfig).arg(m_dir.path()).toUtf8());
        config.close();
        m_daemon.start(u"dbus-daemon"_s,
            { u"--config-file="_s + config.fileName(), u"--nofork"_s, u"--print-address"_s });
        QVERIFY2(m_daemon.waitForReadyRead(5000), "dbus-daemon did not start");
        m_address = QString::fromUtf8(m_daemon.readLine()).trimmed();
        // Nothing here may reach the user's session bus, where real players run.
        QVERIFY(m_address.startsWith(u"unix:path="_s + m_dir.path()));
        qputenv("DBUS_SESSION_BUS_ADDRESS", m_address.toUtf8());
    }

    void cleanupTestCase()
    {
        m_daemon.kill();
        m_daemon.waitForFinished();
    }

    void discovery()
    {
        auto vlc = player(u"vlc"_s, u"VLC media player"_s);
        vlc->setMetadata(track(u"Before"_s));
        Mpris mpris(connection(), nullptr);
        QTRY_COMPARE(titleOf(mpris, u"vlc"_s), u"Before"_s);
        QCOMPARE(keyed(mpris, u"vlc"_s)->identity, u"VLC media player"_s);

        auto mpv = player(u"mpv.instance77"_s, u"mpv"_s);
        QTRY_VERIFY(keyed(mpris, u"mpv"_s));
        QTRY_COMPARE(keyed(mpris, u"mpv"_s)->identity, u"mpv"_s);
        vlc.reset();
        QTRY_COMPARE(mpris.players().size(), 1u);
        QVERIFY(keyed(mpris, u"mpv"_s));
    }

    void signalsByOwner()
    {
        Mpris mpris(connection(), nullptr);
        auto vlc = player(u"vlc"_s, u"VLC"_s);
        auto mpv = player(u"mpv"_s, u"mpv"_s);
        QTRY_COMPARE(mpris.players().size(), 2u);
        vlc->setMetadata(track(u"a"_s));
        mpv->setMetadata(track(u"b"_s));
        QTRY_COMPARE(titleOf(mpris, u"vlc"_s), u"a"_s);
        QTRY_COMPARE(titleOf(mpris, u"mpv"_s), u"b"_s);

        // From a connection that owns no player's name, at a player's path.
        QDBusConnection stranger = connection();
        QDBusMessage spoof = QDBusMessage::createSignal(u"/org/mpris/MediaPlayer2"_s,
            u"org.freedesktop.DBus.Properties"_s, u"PropertiesChanged"_s);
        spoof.setArguments({ u"org.mpris.MediaPlayer2.Player"_s,
            QVariantMap { { u"Metadata"_s, track(u"spoof"_s) } }, QStringList() });
        QVERIFY(stranger.send(spoof));
        vlc->setMetadata(track(u"a2"_s));
        QTRY_COMPARE(titleOf(mpris, u"vlc"_s), u"a2"_s);
        QTest::qWait(50);
        QCOMPARE(titleOf(mpris, u"mpv"_s), u"b"_s);
    }

    void transport()
    {
        Mpris mpris(connection(), nullptr);
        auto vlc = player(u"vlc"_s, u"VLC"_s);
        vlc->setPlaying(true);
        QTRY_COMPARE(keyed(mpris, u"vlc"_s) ? keyed(mpris, u"vlc"_s)->playback : Playback::Stopped,
            Playback::Playing);
        const QString owner = keyed(mpris, u"vlc"_s)->owner;
        mpris.playPause(owner);
        QTRY_COMPARE(keyed(mpris, u"vlc"_s)->playback, Playback::Paused);
        mpris.next(owner);
        mpris.previous(owner);
        mpris.stop(owner);
        QTRY_COMPARE(keyed(mpris, u"vlc"_s)->playback, Playback::Stopped);
        QCOMPARE(vlc->calls, (QStringList { u"PlayPause"_s, u"Next"_s, u"Previous"_s, u"Stop"_s }));
    }

    void position()
    {
        Mpris mpris(connection(), nullptr);
        auto vlc = player(u"vlc"_s, u"VLC"_s);
        vlc->setMetadata(track(u"t"_s, u"/track/1"_s));
        vlc->setPlaying(true);
        QTRY_COMPARE(
            keyed(mpris, u"vlc"_s) ? keyed(mpris, u"vlc"_s)->trackId : QString(), u"/track/1"_s);
        QCOMPARE(keyed(mpris, u"vlc"_s)->length, 240s);

        vlc->jump(60'000'000);
        QTRY_VERIFY(keyed(mpris, u"vlc"_s)->position >= 60s);
        const auto at
            = [&mpris] { return positionAt(*keyed(mpris, u"vlc"_s), steady_clock::now()); };
        QVERIFY(at() < 62s);

        const QString owner = keyed(mpris, u"vlc"_s)->owner;
        mpris.setPosition(owner, 30s);
        QTRY_COMPARE(vlc->calls.value(0), u"SetPosition /track/1 30000000"_s);
        QVERIFY(at() >= 30s && at() < 32s);

        // A pause brings no Seeked, but the position is read again.
        vlc->jump(90'000'000);
        QTRY_VERIFY(at() >= 90s);
        vlc->setPlaying(false);
        QTRY_COMPARE(keyed(mpris, u"vlc"_s)->playback, Playback::Paused);
        QTRY_COMPARE(keyed(mpris, u"vlc"_s)->position, microseconds(vlc->position()));
    }

    void seekWithoutTrackId()
    {
        Mpris mpris(connection(), nullptr);
        auto vlc = player(u"vlc"_s, u"VLC"_s);
        vlc->setMetadata(track(u"t"_s));
        vlc->jump(100'000'000);
        QTRY_COMPARE(keyed(mpris, u"vlc"_s) ? keyed(mpris, u"vlc"_s)->position : 0s, 100s);
        mpris.setPosition(keyed(mpris, u"vlc"_s)->owner, 40s);
        QTRY_COMPARE(vlc->calls.value(0), u"Seek -60000000"_s);
    }

    void restart()
    {
        Mpris mpris(connection(), nullptr);
        auto vlc = player(u"vlc.instance1"_s, u"VLC"_s);
        QTRY_COMPARE(
            keyed(mpris, u"vlc"_s) ? keyed(mpris, u"vlc"_s)->identity : QString(), u"VLC"_s);
        const QString before = keyed(mpris, u"vlc"_s)->owner;
        vlc.reset();
        QTRY_VERIFY(!keyed(mpris, u"vlc"_s));
        QCOMPARE(choosePlayer(mpris.players(), u"vlc"_s), nullptr);

        vlc = player(u"vlc.instance2"_s, u"VLC"_s);
        vlc->setPlaying(true);
        QTRY_VERIFY(choosePlayer(mpris.players(), u"vlc"_s));
        QVERIFY(choosePlayer(mpris.players(), u"vlc"_s)->owner != before);
    }

private:
    QDBusConnection connection()
    {
        return QDBusConnection::connectToBus(m_address, u"test%1"_s.arg(++m_connections));
    }

    // A player on a connection of its own, which it leaves when it goes, as a player that quits.
    std::unique_ptr<FakePlayer, void (*)(FakePlayer*)> player(
        const QString& name, const QString& identity)
    {
        return { new FakePlayer(connection(), name, identity), [](FakePlayer* player) {
                    const QString connection = player->connectionName();
                    delete player;
                    QDBusConnection::disconnectFromBus(connection);
                } };
    }

    QTemporaryDir m_dir;
    QProcess m_daemon;
    QString m_address;
    int m_connections = 0;
};

QTEST_GUILESS_MAIN(MprisTest)
#include "mpris_test.moc"
