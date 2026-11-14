#include "x11/reply.h"
#include "x11/thumbnails.h"

#include <QTest>

#include <algorithm>

namespace {

// A GetImage reply as xcb hands one out: the header, then the pixels.
Reply<xcb_get_image_reply_t> reply(quint8 depth, int width, int height, quint32 pixel)
{
    const std::size_t pixels = std::size_t(width) * height;
    auto* reply = static_cast<xcb_get_image_reply_t*>(
        std::calloc(1, sizeof(xcb_get_image_reply_t) + pixels * sizeof(quint32)));
    reply->depth = depth;
    reply->length = pixels;
    std::fill_n(reinterpret_cast<quint32*>(xcb_get_image_data(reply)), pixels, pixel);
    return Reply<xcb_get_image_reply_t>(reply);
}

} // namespace

class ThumbnailsTest : public QObject {
    Q_OBJECT

private slots:
    void smallCaptureIsACopy()
    {
        const auto captured = reply(24, 4, 2, 0x00336699);
        const QImage thumbnail = thumbnailFrom(*captured, 4, 2);
        const uchar* data = xcb_get_image_data(captured.get());
        QVERIFY(thumbnail.constBits() < data || thumbnail.constBits() >= data + 4 * 2 * 4);
        QCOMPARE(thumbnail.size(), QSize(4, 2));
        QCOMPARE(thumbnail.pixel(3, 1), 0xff336699u);
    }

    void largeCaptureIsScaled()
    {
        const auto captured = reply(32, 800, 400, 0xff000000);
        QCOMPARE(thumbnailFrom(*captured, 800, 400).size(), QSize(400, 200));
    }

    void otherDepthsAreRefused()
    {
        const auto captured = reply(16, 4, 2, 0);
        QVERIFY(thumbnailFrom(*captured, 4, 2).isNull());
    }
};

QTEST_GUILESS_MAIN(ThumbnailsTest)
#include "thumbnails_test.moc"
