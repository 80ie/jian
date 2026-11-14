#pragma once

#include "media/player.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>

class QDBusMessage;

// The MPRIS players on a bus, kept current by their signals: players come and go with their
// names, and their state arrives in PropertiesChanged and Seeked. Nothing here polls.
class Mpris : public QObject {
    Q_OBJECT

public:
    Mpris(const QDBusConnection& bus, QObject* parent);

    const std::vector<Player>& players() const { return m_players; }
    // Calls on the player whose unique name is `owner`, none of which waits for its reply.
    void playPause(const QString& owner);
    void stop(const QString& owner);
    void next(const QString& owner);
    void previous(const QString& owner);
    void setPosition(const QString& owner, std::chrono::microseconds position);

signals:
    void changed();

private slots:
    // QtDBus connects a bus signal only to a slot named by its signature.
    void propertiesChanged(const QDBusMessage& message);
    void seeked(const QDBusMessage& message);

private:
    void listed(const QStringList& names);
    void ownerChanged(const QString& service, const QString& before, const QString& after);
    void add(const QString& service, const QString& owner);
    void remove(const QString& owner);
    void fetch(const QString& owner);
    void readPosition(const QString& owner);
    void setIdentity(const QString& owner, const QString& identity);
    void apply(const QString& owner, QVariantMap changed);
    void call(const QString& owner, const QString& method, const QVariantList& arguments = { });
    Player* find(const QString& owner);

    QDBusConnection m_bus;
    QDBusServiceWatcher m_watcher;
    std::vector<Player> m_players;
    quint64 m_starts = 0;
};
