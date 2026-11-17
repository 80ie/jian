#include "panel/config.h"

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace {

bool writeText(const QString& path, const QByteArray& text)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(text) == text.size();
}

std::optional<Config> loadText(const QByteArray& text)
{
    const QTemporaryDir dir;
    const QString path = dir.filePath("config.ini");
    return writeText(path, text) ? loadConfig(path) : std::nullopt;
}

} // namespace

class ConfigTest : public QObject {
    Q_OBJECT

private slots:
    void roundTrip_data()
    {
        QTest::addColumn<Config>("config");
        QTest::newRow("defaults") << Config();
        Config changed;
        changed.side = Qt::LeftEdge;
        changed.width = 233;
        changed.widgets = QStringList { "tasks", "clock", "media" };
        changed.poppedOut = QStringList { "media" };
        changed.windowPositions = { { "media", QPoint(1220, 300) }, { "clock", QPoint(-4, 0) } };
        changed.clockTime = "HH:mm:ss";
        changed.clockDate = "MM.dd.yyyy\ndddd";
        changed.clockTimeSize = 150;
        changed.clockDateSize = 80;
        changed.tasksTile = 60;
        changed.tasksThumbnailFloor = 40;
        changed.mediaSource = "vlc";
        changed.exportOnSave = true;
        QTest::newRow("changed") << changed;
        Config empty;
        empty.widgets.clear();
        QTest::newRow("no widgets") << empty;
    }

    void roundTrip()
    {
        QFETCH(Config, config);
        const QTemporaryDir dir;
        const QString path = dir.filePath("config.ini");
        QVERIFY(saveConfig(config, path));
        const std::optional<Config> loaded = loadConfig(path);
        QVERIFY(loaded);
        QVERIFY(*loaded == config);
    }

    void missingKeysKeepDefaults()
    {
        Config expected;
        expected.side = Qt::LeftEdge;
        const std::optional<Config> loaded = loadText("[panel]\nside=left\n");
        QVERIFY(loaded);
        QVERIFY(*loaded == expected);
    }

    void badValuesAreRejected_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("side") << QByteArray("[panel]\nside=top\n");
        QTest::newRow("width") << QByteArray("[panel]\nwidth=wide\n");
        QTest::newRow("tile") << QByteArray("[tasks]\ntile=big\n");
        QTest::newRow("pop-out position") << QByteArray("[popout]\nmedia=12\n");
        QTest::newRow("floor") << QByteArray("[tasks]\nfloor=0\n");
        QTest::newRow("time size") << QByteArray("[clock]\ntimeSize=big\n");
        QTest::newRow("date size") << QByteArray("[clock]\ndateSize=0\n");
        QTest::newRow("format") << QByteArray("[panel]\nformat=new\n");
    }

    void badValuesAreRejected()
    {
        QFETCH(QByteArray, text);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("bad (panel|tasks|popout|clock)/"));
        QVERIFY(!loadText(text));
    }

    void soundFollowsMedia_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QStringList>("widgets");
        QTest::newRow("after media") << QByteArray("[panel]\nwidgets=clock, media, tray\n")
                                     << QStringList { "clock", "media", "sound", "tray" };
        QTest::newRow("no media") << QByteArray("[panel]\nwidgets=clock, tray\n")
                                  << QStringList { "clock", "tray" };
        QTest::newRow("placed already") << QByteArray("[panel]\nwidgets=sound, clock, media\n")
                                        << QStringList { "sound", "clock", "media" };
    }

    void soundFollowsMedia()
    {
        QFETCH(QByteArray, text);
        QFETCH(QStringList, widgets);
        const std::optional<Config> loaded = loadText(text);
        QVERIFY(loaded);
        QCOMPARE(loaded->widgets, widgets);
    }

    void soundStaysDockedBesidePoppedMedia()
    {
        const std::optional<Config> loaded
            = loadText("[panel]\nwidgets=media, tray\npopped=media\n");
        QVERIFY(loaded);
        QCOMPARE(loaded->widgets, (QStringList { "media", "sound", "tray" }));
        QCOMPARE(loaded->poppedOut, QStringList { "media" });
    }

    void removedSoundStaysRemoved()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("config.ini");
        QVERIFY(writeText(path, "[panel]\nwidgets=clock, media\n"));
        std::optional<Config> config = loadConfig(path);
        QVERIFY(config);
        config->widgets.removeAll("sound");
        QVERIFY(saveConfig(*config, path));
        const std::optional<Config> loaded = loadConfig(path);
        QVERIFY(loaded);
        QCOMPARE(loaded->widgets, (QStringList { "clock", "media" }));
    }
};

QTEST_GUILESS_MAIN(ConfigTest)
#include "config_test.moc"
