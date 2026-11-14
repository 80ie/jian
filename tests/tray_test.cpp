#include "palette/palette.h"
#include "widgets/tray.h"

#include <QTest>

#include <vector>

class TrayTest : public QObject {
    Q_OBJECT

private slots:
    void arguments()
    {
        Palette palette;
        palette.face = QColor(0x8a, 0x9a, 0xa2);
        const QStringList arguments = trayArguments(palette, 4);
        const auto value = [&arguments](const char* option) {
            const qsizetype at = arguments.indexOf(QLatin1StringView(option));
            return at < 0 ? QString() : arguments.value(at + 1);
        };
        QCOMPARE(value("--config"), "/dev/null");
        QCOMPARE(value("--background"), palette.tone(-1).name());
        QCOMPARE(value("--geometry"), "4x1");
        QCOMPARE(value("--max-geometry"), "4x0");
    }

    void columns_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("columns");
        QTest::newRow("panel default") << 153 << 5;
        QTest::newRow("exactly three") << 82 << 3;
        QTest::newRow("a pixel short of three") << 81 << 2;
        QTest::newRow("narrower than an icon") << 20 << 1;
    }

    void columns()
    {
        QFETCH(int, width);
        QFETCH(int, columns);
        QCOMPARE(trayColumns(width), columns);
    }

    void height_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("icons");
        QTest::addColumn<int>("height");
        QTest::newRow("empty") << 153 << 0 << 30;
        QTest::newRow("one full row") << 153 << 5 << 30;
        QTest::newRow("one over") << 153 << 6 << 56;
        QTest::newRow("one column") << 20 << 3 << 82;
    }

    void height()
    {
        QFETCH(int, width);
        QFETCH(int, icons);
        QFETCH(int, height);
        QCOMPARE(trayHeight(width, icons), height);
    }

    void icons()
    {
        const std::vector<QRect> children { QRect(2, 28, 22, 22), QRect(28, 2, 22, 22),
            QRect(2, 2, 22, 22), QRect(-1, -1, 1, 1), QRect(0, 0, 1, 1) };
        QCOMPARE(countIcons(children), 3);
        QCOMPARE(countIcons({ }), 0);
    }
};

QTEST_GUILESS_MAIN(TrayTest)
#include "tray_test.moc"
