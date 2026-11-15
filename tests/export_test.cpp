#include "palette/export.h"
#include "palette/palette.h"

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {

const QDateTime first(QDate(2026, 11, 15), QTime(9, 0));

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return QDir().mkpath(QFileInfo(path).path()) && file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A target `name` in `dir` that writes `text` into out/<name>.
bool makeTarget(const QDir& dir, const QString& name, const QByteArray& text)
{
    const ExportTarget target { dir.filePath(u"out/"_s + name), QString(), true };
    return QDir(dir).mkpath(u"exports/"_s + name) && saveTarget(target, dir, name)
        && writeFile(dir.filePath(u"exports/%1/template"_s.arg(name)), text);
}

} // namespace

class ExportTest : public QObject {
    Q_OBJECT

private slots:
    void placeholders_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("rendered");
        Palette palette;
        QTest::newRow("plain") << u"no colors, $[x] (y)"_s << u"no colors, $[x] (y)"_s;
        QTest::newRow("named") << u"bg {face};"_s << u"bg #a2948a;"_s;
        QTest::newRow("rgb") << u"{ink:rgb}"_s << u"2,2,0"_s;
        QTest::newRow("no hash") << u"#ff{selection:noHash}"_s << u"#ff7a110f"_s;
        QTest::newRow("tones") << u"{tone-6} {tone+5}"_s
                               << palette.tone(-6).name() + u' ' + palette.tone(5).name();
        QTest::newRow("chrome") << u"{chrome} {chrome+1}"_s
                                << u"#78340a "_s + palette.chromeTone(1).name();
        QTest::newRow("pager desk") << u"{pager_desk}{fill}"_s << u"#5c300e#584a0c"_s;
    }

    void placeholders()
    {
        QFETCH(QString, text);
        QFETCH(QString, rendered);
        const Rendering rendering = render(text, Palette());
        QCOMPARE(rendering.unknown, QString());
        QCOMPARE(rendering.text, rendered);
    }

    void unknownPlaceholders_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("unknown");
        QTest::newRow("name") << u"a {face} {paper} {ink}"_s << u"{paper}"_s;
        QTest::newRow("past the ramp") << u"{tone+6}"_s << u"{tone+6}"_s;
        QTest::newRow("tone 0") << u"{tone0}"_s << u"{tone0}"_s;
        QTest::newRow("unsigned") << u"{tone2}"_s << u"{tone2}"_s;
        QTest::newRow("format") << u"{face:hsl}"_s << u"{face:hsl}"_s;
        QTest::newRow("empty format") << u"{face:}"_s << u"{face:}"_s;
        QTest::newRow("unclosed") << u"x {face\n}"_s << u"{face"_s;
        QTest::newRow("css rule") << u"window { color: red; }"_s << u"{ color: red; }"_s;
    }

    void unknownPlaceholders()
    {
        QFETCH(QString, text);
        QFETCH(QString, unknown);
        QCOMPARE(render(text, Palette()).unknown, unknown);
    }

    void targetRoundTrip()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        QVERIFY(dir.mkpath(u"exports/gtk3"_s));
        const ExportTarget target { u"~/.themes/Redmond97 Deer/gtk-3.0/gtk.css"_s,
            u"FvwmCommand 'Read colorsets', \"then\"; done"_s, true };
        QVERIFY(saveTarget(target, dir, u"gtk3"_s));
        QCOMPARE(loadTarget(dir, u"gtk3"_s), target);
        QCOMPARE(targetNames(dir), QStringList { u"gtk3"_s });
    }

    void targetWithoutDestinationIsRefused()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        QVERIFY(writeFile(dir.filePath(u"exports/kde/target.ini"_s), "[target]\ncommand=x\n"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"target.ini: no destination"_s));
        QVERIFY(!loadTarget(dir, u"kde"_s));
    }

    void exportKeepsWhatItReplaces()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        QVERIFY(makeTarget(dir, u"qt6ct"_s, "Window={face}\n"));
        const QString out = dir.filePath(u"out/qt6ct"_s);
        QVERIFY(writeFile(out, "mine\n"));

        QVERIFY(exportTarget(Palette(), dir, u"qt6ct"_s, first));
        QCOMPARE(readFile(out), "Window=#a2948a\n");
        const QString backup = dir.filePath(u"history/qt6ct/20261115-090000-qt6ct"_s);
        QCOMPARE(readFile(backup), "mine\n");

        // The same contents again replace nothing, so they keep nothing.
        QVERIFY(exportTarget(Palette(), dir, u"qt6ct"_s, first.addSecs(1)));
        QCOMPARE(QDir(dir.filePath(u"history/qt6ct"_s)).entryList(QDir::Files).size(), 1);
    }

    void exportMakesItsDirectory()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        QVERIFY(makeTarget(dir, u"gtk4"_s, "{ink}"));
        QVERIFY(exportTarget(Palette(), dir, u"gtk4"_s, first));
        QCOMPARE(readFile(dir.filePath(u"out/gtk4"_s)), "#020200");
        QVERIFY(!dir.exists(u"history"_s));
    }

    void unknownPlaceholderWritesNothing()
    {
        const QTemporaryDir temporary;
        const QDir dir(temporary.path());
        QVERIFY(makeTarget(dir, u"fvwm"_s, "{face}\n{paper}\n"));
        QTest::ignoreMessage(
            QtWarningMsg, QRegularExpression(u"template: no color for \\{paper\\}"_s));
        QVERIFY(!exportTarget(Palette(), dir, u"fvwm"_s, first));
        QVERIFY(!QFile::exists(dir.filePath(u"out/fvwm"_s)));
    }
};

QTEST_GUILESS_MAIN(ExportTest)
#include "export_test.moc"
