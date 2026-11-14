#pragma once

#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <utility>
#include <vector>

// The current track's cover, read when a track names one, from a file or over HTTP. A fetch
// ends with its image, or is abandoned when it runs too long or too large or the track changes.
// A failed one leaves the last good image up.
class CoverArt : public QObject {
    Q_OBJECT

public:
    explicit CoverArt(QObject* parent);

    // `location` is what mpris:artUrl holds: a URL, or the path some players give instead.
    void setTrack(const QString& owner, const QString& trackId, const QString& location);
    const QImage& image() const { return m_image; }

signals:
    void changed();

private:
    struct Track {
        QString owner;
        QString trackId;
        QUrl url;

        bool operator==(const Track&) const = default;
    };

    void read(const QUrl& url);
    void fetch(const QUrl& url);
    void received(const QByteArray& data, const QUrl& url);
    void show(const QImage& image);

    QNetworkAccessManager m_network;
    // The one fetch under way, and what it has brought so far.
    QPointer<QNetworkReply> m_reply;
    QByteArray m_data;
    Track m_track;
    QImage m_image;
    // The most recently shown last.
    std::vector<std::pair<QUrl, QImage>> m_cache;
};
