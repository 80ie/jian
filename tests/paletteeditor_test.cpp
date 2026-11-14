#include "palette/library.h"
#include "settings/paletteeditor.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

Palette withFace(int grey)
{
    Palette palette;
    palette.face = QColor(grey, grey, grey);
    return palette;
}

} // namespace

class PaletteEditorTest : public QObject {
    Q_OBJECT

private slots:
    void editShowsAtOnceUnsaved()
    {
        const QTemporaryDir dir;
        Palette live;
        PaletteEditor editor(live, QDir(dir.path()));
        QSignalSpy redrawn(&editor, &PaletteEditor::liveChanged);
        editor.edit(withFace(0x70));
        QVERIFY(live == withFace(0x70));
        QCOMPARE(redrawn.size(), 1);
        QVERIFY(editor.unsaved());
        editor.revert();
        QVERIFY(live == Palette());
        QVERIFY(!editor.unsaved());
    }

    void saveWritesTheFileAndHistory()
    {
        const QTemporaryDir dir;
        Palette live;
        PaletteEditor editor(live, QDir(dir.path()));
        editor.edit(withFace(0x70));
        QVERIFY(editor.save());
        QVERIFY(!editor.unsaved());
        QVERIFY(loadPalette(QDir(dir.path()).filePath("palette.ini")) == withFace(0x70));
        QCOMPARE(historyTimes(QDir(dir.path())).size(), 1);
    }

    void fileChangesKeepUnsavedEdits()
    {
        const QTemporaryDir dir;
        Palette live;
        PaletteEditor editor(live, QDir(dir.path()));
        editor.edit(withFace(0x70));
        editor.load(withFace(0x90));
        QVERIFY(live == withFace(0x70));
        editor.revert();
        QVERIFY(live == withFace(0x90));
    }

    void fileChangesShowWithoutEdits()
    {
        const QTemporaryDir dir;
        Palette live;
        PaletteEditor editor(live, QDir(dir.path()));
        editor.load(withFace(0x90));
        QVERIFY(live == withFace(0x90));
        QVERIFY(!editor.unsaved());
    }

    void showingAToneIsNoEdit()
    {
        const QTemporaryDir dir;
        Palette live;
        PaletteEditor editor(live, QDir(dir.path()));
        editor.showTone(-2);
        QVERIFY(live.tone(-2) != Palette().tone(-2));
        QVERIFY(!editor.unsaved());
        editor.edit(withFace(0x70));
        QVERIFY(live == withToneShown(withFace(0x70), -2));
        editor.showTone(std::nullopt);
        QVERIFY(live == withFace(0x70));
    }
};

QTEST_GUILESS_MAIN(PaletteEditorTest)
#include "paletteeditor_test.moc"
