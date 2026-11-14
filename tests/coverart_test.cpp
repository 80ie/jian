#include "media/coverart.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class CoverArtTest : public QObject {
    Q_OBJECT

private slots:
    void fetches()
    {
        QTemporaryDir dir;
        const QString square = dir.filePath(u"square.png"_s);
        const QString huge = dir.filePath(u"huge.png"_s);
        QVERIFY(QImage(40, 40, QImage::Format_RGB32).save(square));
        QVERIFY(QImage(2000, 1000, QImage::Format_RGB32).save(huge));

        CoverArt art(nullptr);
        art.setTrack(u":1.1"_s, u"/1"_s, square);
        QTRY_COMPARE(art.image().size(), QSize(40, 40));

        art.setTrack(u":1.1"_s, u"/2"_s, QUrl::fromLocalFile(huge).toString());
        QTRY_COMPARE(art.image().size(), QSize(512, 256));

        // A cover that cannot be read leaves the last one up.
        QSignalSpy changed(&art, &CoverArt::changed);
        art.setTrack(u":1.1"_s, u"/3"_s, dir.filePath(u"missing.png"_s));
        QVERIFY(!changed.wait(200));
        QCOMPARE(art.image().size(), QSize(512, 256));

        art.setTrack(u":1.1"_s, u"/4"_s, QString());
        QVERIFY(art.image().isNull());
        art.setTrack(u":1.1"_s, u"/1"_s, square);
        QCOMPARE(art.image().size(), QSize(40, 40));
    }

    void dropsStaleReplies()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"cover.png"_s);
        QVERIFY(QImage(30, 30, QImage::Format_RGB32).save(path));
        CoverArt art(nullptr);
        QSignalSpy changed(&art, &CoverArt::changed);
        art.setTrack(u":1.1"_s, u"/1"_s, path);
        art.setTrack(u":1.1"_s, u"/2"_s, u"coverart://elsewhere"_s);
        QVERIFY(!changed.wait(200));
        QVERIFY(art.image().isNull());
    }
};

QTEST_GUILESS_MAIN(CoverArtTest)
#include "coverart_test.moc"
