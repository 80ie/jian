#include "media/coverart.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkReply>
#include <QTimer>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

constexpr int timeoutMs = 10'000;
constexpr qint64 largestFile = 8 << 20;
constexpr qint64 chunk = 64 << 10;
constexpr int maxRedirects = 3;
constexpr int cached = 8;
// Covers come as large as 4000 pixels, and the widest panel is 420.
constexpr int largest = 512;

QUrl urlOf(const QString& location)
{
    if (location.isEmpty() || location.contains("://"_L1))
        return QUrl(location);
    return QUrl::fromLocalFile(QFileInfo(location).absoluteFilePath());
}

bool isWeb(const QUrl& url) { return url.scheme() == "http"_L1 || url.scheme() == "https"_L1; }

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
    if (m_reply)
        m_reply->abort();
    const QUrl& url = track.url;
    const auto hit = std::ranges::find(m_cache, url, &std::pair<QUrl, QImage>::first);
    if (hit != m_cache.end()) {
        std::rotate(hit, hit + 1, m_cache.end());
        show(m_cache.back().second);
    } else if (url.isLocalFile()) {
        read(url);
    } else if (isWeb(url)) {
        fetch(url);
    } else {
        // Such as mpris-proxy's coverart:, which nothing can fetch.
        show(QImage());
    }
}

// Not through QNetworkAccessManager, which would open a device or a FIFO as readily.
void CoverArt::read(const QUrl& url)
{
    const QFileInfo info(url.toLocalFile());
    QFile file(info.filePath());
    if (!info.isFile() || info.size() > largestFile || !file.open(QIODevice::ReadOnly))
        return;
    if (const QByteArray data = file.read(largestFile + 1); data.size() <= largestFile)
        received(data, url);
}

// Any web page a browser plays can name the URL, so the size and the time are bounded. Data is
// taken as it arrives: Qt holds a burst back and reports none of it until the sender pauses, so
// a check on progress alone could come many megabytes late.
void CoverArt::fetch(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setMaximumRedirectsAllowed(maxRedirects);
    QNetworkReply* reply = m_network.get(request);
    m_reply = reply;
    m_data.clear();
    reply->setReadBufferSize(chunk);
    QTimer::singleShot(timeoutMs, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::redirected, reply, [reply](const QUrl& to) {
        if (!isWeb(to))
            reply->abort();
    });
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        m_data += reply->readAll();
        if (m_data.size() > largestFile)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        if (reply != m_reply)
            return;
        const QByteArray data = std::exchange(m_data, { }) + reply->readAll();
        if (reply->error() == QNetworkReply::NoError)
            received(data, url);
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
