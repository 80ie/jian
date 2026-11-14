#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QStringList>
#include <QVariantMap>

// An MPRIS player for the tests and the rig. It plays nothing, but it keeps a position, answers
// calls and signals its changes as a player does.
class FakePlayer : public QObject {
    Q_OBJECT

public:
    // Takes org.mpris.MediaPlayer2.`name` on `bus`, which holds no other player.
    FakePlayer(const QDBusConnection& bus, const QString& name, const QString& identity);
    ~FakePlayer() override;

    QString connectionName() const { return m_bus.name(); }
    QString identity() const { return m_identity; }
    QString status() const { return m_status; }
    QVariantMap metadata() const { return m_metadata; }
    qlonglong position() const;
    bool canSeek() const { return m_canSeek; }

    void setMetadata(const QVariantMap& metadata);
    void setPlaying(bool playing);
    void setCanSeek(bool canSeek);
    // Moves the position as a user would in the player's own window.
    void jump(qlonglong position);

    void playPause();
    void stop();
    void seek(qlonglong offset);
    void setPosition(const QDBusObjectPath& track, qlonglong position);

    // The methods called on it, with their arguments, oldest first.
    QStringList calls;

signals:
    void seeked(qlonglong position);

private:
    void notify(const QVariantMap& changed);

    QDBusConnection m_bus;
    QString m_service;
    QString m_identity;
    QVariantMap m_metadata;
    bool m_playing = false;
    QString m_status = QStringLiteral("Stopped");
    bool m_canSeek = true;
    // Where the position was when m_since started.
    qlonglong m_position = 0;
    QElapsedTimer m_since;
};

class FakeRoot : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(bool CanQuit READ no)
    Q_PROPERTY(bool CanRaise READ no)
    Q_PROPERTY(bool HasTrackList READ no)

public:
    explicit FakeRoot(FakePlayer* player);
    QString identity() const { return m_player->identity(); }
    bool no() const { return false; }

public slots:
    void Raise() { }
    void Quit() { }

private:
    FakePlayer* m_player;
};

class FakeControls : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double Rate READ rate)
    Q_PROPERTY(bool CanControl READ yes)
    Q_PROPERTY(bool CanPlay READ yes)
    Q_PROPERTY(bool CanPause READ yes)
    Q_PROPERTY(bool CanGoNext READ yes)
    Q_PROPERTY(bool CanGoPrevious READ yes)
    Q_PROPERTY(bool CanSeek READ canSeek)

public:
    explicit FakeControls(FakePlayer* player);
    QString playbackStatus() const { return m_player->status(); }
    QVariantMap metadata() const { return m_player->metadata(); }
    qlonglong position() const { return m_player->position(); }
    double rate() const { return 1; }
    bool yes() const { return true; }
    bool canSeek() const { return m_player->canSeek(); }

public slots:
    void PlayPause() { m_player->playPause(); }
    void Play();
    void Pause();
    void Stop() { m_player->stop(); }
    void Next();
    void Previous();
    void Seek(qlonglong offset) { m_player->seek(offset); }
    void SetPosition(const QDBusObjectPath& track, qlonglong position)
    {
        m_player->setPosition(track, position);
    }

signals:
    void Seeked(qlonglong position);

private:
    FakePlayer* m_player;
};
