#pragma once

#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <utility>
#include <vector>

// The current track's cover, fetched when a track names one, from a file or over HTTP. A fetch
// ends with its image or after its timeout. A failed one leaves the last good image up, and
// one that lands after the track has changed is dropped.
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

    void received(const QByteArray& data, const QUrl& url);
    void show(const QImage& image);

    QNetworkAccessManager m_network;
    Track m_track;
    QImage m_image;
    // The most recently shown last.
    std::vector<std::pair<QUrl, QImage>> m_cache;
};
