#include "palette/palette.h"
#include "widgets/worldview.h"

#include <QRegularExpression>
#include <QTest>

#include <algorithm>

using namespace Qt::StringLiterals;

class WorldViewTest : public QObject {
    Q_OBJECT

private slots:
    void size_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<QSize>("screen");
        QTest::addColumn<int>("label");
        QTest::addColumn<QSize>("size");
        // Measured in fvwm-rig: FvwmPager drew 19-pixel labels over 95-pixel desks.
        QTest::newRow("rig") << 169 << QSize(1600, 900) << 19 << QSize(169, 228);
        QTest::newRow("rounded page") << 149 << QSize(1600, 900) << 19 << QSize(149, 206);
        QTest::newRow("portrait") << 90 << QSize(900, 1600) << 10 << QSize(90, 340);
    }

    void size()
    {
        QFETCH(int, width);
        QFETCH(QSize, screen);
        QFETCH(int, label);
        QFETCH(QSize, size);
        QCOMPARE(pagerSize(width, screen, label), size);
    }

    void fontName()
    {
        QFont font(u"DejaVu Sans"_s);
        font.setPixelSize(13);
        QCOMPARE(xftName(font), u"xft:DejaVu Sans:pixelsize=13:weight=regular"_s);
        font.setWeight(QFont::Bold);
        font.setItalic(true);
        QCOMPARE(xftName(font), u"xft:DejaVu Sans:pixelsize=13:weight=bold:slant=italic"_s);
    }

    // FvwmPager starts once, from the last line, with its old instance gone and everything it
    // reads already sent.
    void launchOrder()
    {
        const QStringList commands = pagerLaunch(Palette(), u"xft:Sans"_s, QSize(149, 206));
        QVERIFY(commands.first().startsWith(u"KillModule FvwmPager "_s));
        QVERIFY(commands.last().startsWith(u"Module FvwmPager "_s));
        QCOMPARE(std::ranges::count_if(commands,
                     [](const QString& c) { return c.startsWith(u"Module FvwmPager "_s); }),
            1);
        QVERIFY(commands.contains(u"*ShelfWorldView: Geometry 149x206"_s));
        QVERIFY(commands.contains(u"*ShelfWorldView: Font xft:Sans"_s));
    }

    void colorsetsNamedAreSent()
    {
        const QStringList colorsets = pagerColorsets(Palette());
        const QRegularExpression named(u"^\\*ShelfWorldView: \\w*Colorset (\\d+)$"_s);
        int count = 0;
        for (const QString& command : pagerLaunch(Palette(), u"xft:Sans"_s, QSize(149, 206))) {
            const QRegularExpressionMatch match = named.match(command);
            if (!match.hasMatch())
                continue;
            ++count;
            const QString defined = u"Colorset "_s + match.captured(1) + u' ';
            QVERIFY2(std::ranges::any_of(
                         colorsets, [&defined](const QString& c) { return c.startsWith(defined); }),
                qPrintable(command));
        }
        QCOMPARE(count, 4);
    }

    void colorsetsFollowThePalette()
    {
        Palette palette;
        const QStringList before = pagerColorsets(palette);
        palette.face = QColor(0x8a, 0x9a, 0xa2);
        const QStringList after = pagerColorsets(palette);
        for (qsizetype i = 0; i < before.size(); ++i)
            QVERIFY(before[i] != after[i]);
    }
};

QTEST_MAIN(WorldViewTest)
#include "worldview_test.moc"
