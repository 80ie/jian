#include "media/coverart.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkReply>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr int timeoutMs = 8000;
constexpr int cached = 8;
// Covers come as large as 4000 pixels, and the widest panel is 420.
constexpr int largest = 512;

QUrl urlOf(const QString& location)
{
    if (location.isEmpty() || location.contains("://"_L1))
        return QUrl(location);
    return QUrl::fromLocalFile(QFileInfo(location).absoluteFilePath());
}

} // namespace

CoverArt::CoverArt(QObject* parent)
    : QObject(parent)
{
}

void CoverArt::setTrack(const QString& owner, const QString& trackId, const QString& location)
{
    const Track track { owner, trackId, urlOf(location) };
    if (track == m_track)
        return;
    m_track = track;
    const QUrl& url = track.url;
    const auto hit = std::ranges::find(m_cache, url, &std::pair<QUrl, QImage>::first);
    if (hit != m_cache.end()) {
        std::rotate(hit, hit + 1, m_cache.end());
        show(m_cache.back().second);
        return;
    }
    // Other schemes, such as mpris-proxy's coverart:, have nothing to fetch them with.
    if (!url.isLocalFile() && url.scheme() != "http"_L1 && url.scheme() != "https"_L1) {
        show(QImage());
        return;
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(timeoutMs);
    QNetworkReply* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, track] {
        reply->deleteLater();
        if (track == m_track)
            received(reply->readAll(), track.url);
    });
}

void CoverArt::received(const QByteArray& data, const QUrl& url)
{
    QBuffer buffer;
    buffer.setData(data);
    QImageReader reader(&buffer);
    if (const QSize size = reader.size(); std::max(size.width(), size.height()) > largest)
        reader.setScaledSize(size.scaled(largest, largest, Qt::KeepAspectRatio));
    const QImage image = reader.read();
    if (image.isNull())
        return;
    m_cache.emplace_back(url, image);
    if (m_cache.size() > cached)
        m_cache.erase(m_cache.begin());
    show(image);
}

void CoverArt::show(const QImage& image)
{
    if (image.isNull() && m_image.isNull())
        return;
    m_image = image;
    emit changed();
}
