#include "palette/library.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

Palette withFace(int grey)
{
    Palette palette;
    palette.face = QColor(grey, grey, grey);
    return palette;
}

const QDateTime start(QDate(2026, 11, 14), QTime(9, 0));

} // namespace

class LibraryTest : public QObject {
    Q_OBJECT

private slots:
    void presetsRoundTrip()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        QVERIFY(presetNames(dir).isEmpty());
        QVERIFY(savePreset(withFace(0x90), dir, "slate"));
        QVERIFY(savePreset(withFace(0xa0), dir, "beige paper"));
        QCOMPARE(presetNames(dir), QStringList({ "beige paper", "slate" }));
        QVERIFY(loadPreset(dir, "slate") == withFace(0x90));

        QVERIFY(savePreset(withFace(0xb0), dir, "slate"));
        QVERIFY(loadPreset(dir, "slate") == withFace(0xb0));
        QVERIFY(deletePreset(dir, "slate"));
        QCOMPARE(presetNames(dir), QStringList({ "beige paper" }));
    }

    void badPresetNamesAreRefused_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("empty") << QString();
        QTest::newRow("hidden") << ".slate";
        QTest::newRow("parent") << "..";
        QTest::newRow("path") << "../palette";
    }

    void badPresetNamesAreRefused()
    {
        QFETCH(QString, name);
        const QTemporaryDir temporary;
        QVERIFY(!savePreset(Palette(), QDir(temporary.path()), name));
    }

    void savesGoIntoHistory()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        for (int i = 0; i < 3; ++i)
            QVERIFY(savePaletteWithHistory(withFace(0x80 + i), dir, start.addSecs(i)));
        QVERIFY(loadPalette(dir.filePath("palette.ini")) == withFace(0x82));
        QCOMPARE(
            historyTimes(dir), QList<QDateTime>({ start.addSecs(2), start.addSecs(1), start }));
        for (int i = 0; i < 3; ++i)
            QVERIFY(loadHistory(dir, start.addSecs(i)) == withFace(0x80 + i));
    }

    void historyKeepsTheNewest()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        for (int i = 0; i < historyLength + 3; ++i)
            QVERIFY(savePaletteWithHistory(withFace(i), dir, start.addSecs(i)));
        const QList<QDateTime> times = historyTimes(dir);
        QCOMPARE(times.size(), historyLength);
        QCOMPARE(times.front(), start.addSecs(historyLength + 2));
        QCOMPARE(times.back(), start.addSecs(3));
    }

    void otherFilesInHistoryAreLeftAlone()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        QVERIFY(savePaletteWithHistory(Palette(), dir, start));
        QFile other(dir.filePath("history/notes.ini"));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();
        QCOMPARE(historyTimes(dir), QList<QDateTime>({ start }));
    }
};

QTEST_GUILESS_MAIN(LibraryTest)
#include "library_test.moc"
