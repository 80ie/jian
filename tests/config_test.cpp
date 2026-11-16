#include "panel/config.h"

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace {

std::optional<Config> loadText(const QByteArray& text)
{
    const QTemporaryDir dir;
    QFile file(dir.filePath("config.ini"));
    if (!file.open(QIODevice::WriteOnly) || file.write(text) != text.size())
        return std::nullopt;
    file.close();
    return loadConfig(file.fileName());
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
        changed.clockDate = "dddd d MMMM";
        changed.tasksFloor = 40;
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
        QTest::newRow("pop-out position") << QByteArray("[popout]\nmedia=12\n");
        QTest::newRow("floor") << QByteArray("[tasks]\nfloor=0\n");
    }

    void badValuesAreRejected()
    {
        QFETCH(QByteArray, text);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("bad (panel|tasks|popout)/"));
        QVERIFY(!loadText(text));
    }
};

QTEST_GUILESS_MAIN(ConfigTest)
#include "config_test.moc"
