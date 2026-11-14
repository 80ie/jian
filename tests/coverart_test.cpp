#include "media/coverart.h"

#include <QBuffer>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

using namespace Qt::StringLiterals;

namespace {

constexpr qint64 largestFile = 8 << 20;

} // namespace

class CoverArtTest : public QObject {
    Q_OBJECT

private slots:
    void reads()
    {
        QTemporaryDir dir;
        const QString square = dir.filePath(u"square.png"_s);
        const QString wide = dir.filePath(u"wide.png"_s);
        QVERIFY(QImage(40, 40, QImage::Format_RGB32).save(square));
        QVERIFY(QImage(2000, 1000, QImage::Format_RGB32).save(wide));

        CoverArt art(nullptr);
        art.setTrack(u":1.1"_s, u"/1"_s, square);
        QCOMPARE(art.image().size(), QSize(40, 40));
        art.setTrack(u":1.1"_s, u"/2"_s, QUrl::fromLocalFile(wide).toString());
        QCOMPARE(art.image().size(), QSize(512, 256));

        // A cover that cannot be read leaves the last one up.
        art.setTrack(u":1.1"_s, u"/3"_s, dir.filePath(u"missing.png"_s));
        QCOMPARE(art.image().size(), QSize(512, 256));
        art.setTrack(u":1.1"_s, u"/4"_s, QString());
        QVERIFY(art.image().isNull());
        art.setTrack(u":1.1"_s, u"/5"_s, u"coverart://elsewhere"_s);
        QVERIFY(art.image().isNull());
    }

    void readsOnlyRegularFiles()
    {
        QTemporaryDir dir;
        const QString fifo = dir.filePath(u"fifo"_s);
        QCOMPARE(mkfifo(qPrintable(fifo), 0600), 0);
        CoverArt art(nullptr);
        art.setTrack(u":1.1"_s, u"/1"_s, fifo);
        art.setTrack(u":1.1"_s, u"/2"_s, dir.path());
        QVERIFY(art.image().isNull());
    }

    // Uncompressed, so that the image's size sets the file's.
    void capsFiles()
    {
        QTemporaryDir dir;
        const QString under = dir.filePath(u"under.bmp"_s);
        const QString over = dir.filePath(u"over.bmp"_s);
        QVERIFY(QImage(1600, 1600, QImage::Format_RGB888).save(under));
        QVERIFY(QImage(1700, 1700, QImage::Format_RGB888).save(over));
        QVERIFY(QFileInfo(under).size() < largestFile && QFileInfo(over).size() > largestFile);

        CoverArt art(nullptr);
        art.setTrack(u":1.1"_s, u"/1"_s, under);
        QCOMPARE(art.image().size(), QSize(512, 512));
        art.setTrack(u":1.1"_s, u"/2"_s, QString());
        art.setTrack(u":1.1"_s, u"/3"_s, over);
        QVERIFY(art.image().isNull());
    }

    void capsDownloads()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        qint64 sent = 0;
        bool closed = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            QTcpSocket* socket = server.nextPendingConnection();
            const QByteArray chunk(1 << 16, 'x');
            const auto more = [socket, chunk, &sent] {
                if (socket->bytesToWrite() < chunk.size())
                    sent += socket->write(chunk);
            };
            // A body with no length, which runs until the client gives up.
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n\r\n");
            more();
            connect(socket, &QTcpSocket::bytesWritten, socket, more);
            connect(socket, &QTcpSocket::disconnected, this, [&closed] { closed = true; });
        });

        CoverArt art(nullptr);
        art.setTrack(
            u":1.1"_s, u"/1"_s, u"http://127.0.0.1:%1/cover.png"_s.arg(server.serverPort()));
        QTRY_VERIFY_WITH_TIMEOUT(closed, 5000);
        QVERIFY(sent < 2 * largestFile);
        QVERIFY(art.image().isNull());
    }

    void dropsTheOldTrack()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QTcpSocket* socket = nullptr;
        connect(&server, &QTcpServer::newConnection, this,
            [&] { socket = server.nextPendingConnection(); });
        CoverArt art(nullptr);
        art.setTrack(
            u":1.1"_s, u"/1"_s, u"http://127.0.0.1:%1/cover.png"_s.arg(server.serverPort()));
        QTRY_VERIFY(socket && socket->bytesAvailable() > 0);
        art.setTrack(u":1.1"_s, u"/2"_s, QString());

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        QVERIFY(QImage(20, 20, QImage::Format_RGB32).save(&buffer, "PNG"));
        socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(png.size())
            + "\r\n\r\n" + png);
        QSignalSpy changed(&art, &CoverArt::changed);
        QVERIFY(!changed.wait(300));
        QVERIFY(art.image().isNull());
    }
};

QTEST_GUILESS_MAIN(CoverArtTest)
#include "coverart_test.moc"
