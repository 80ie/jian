#include "palette/scheme.h"

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

class SchemeTest : public QObject {
    Q_OBJECT

private slots:
    void mapsAsRolesDid()
    {
        const std::optional<Palette> palette = importScheme(QFINDTESTDATA("data/scheme.json"));
        QVERIFY(palette);
        Palette expected;
        expected.face = QColor(0xb0, 0xa8, 0x98);
        expected.ink = QColor(0x04, 0x02, 0x01);
        expected.fill = QColor(0x4a, 0x6a, 0x2a);
        expected.selection = QColor(0x33, 0x66, 0x99);
        expected.pagerDesk = QColor(0x60, 0x38, 0x20);
        expected.chrome = QColor(0x8a, 0x30, 0x20);
        QVERIFY(*palette == expected);
    }

    void badSchemesAreRefused_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("not json") << QByteArray("colors: none");
        QTest::newRow("no face") << QByteArray("{\"colors\": {\"color0\": \"#000000\"}}");
    }

    void badSchemesAreRefused()
    {
        QFETCH(QByteArray, text);
        const QTemporaryDir dir;
        QFile file(dir.filePath("scheme.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
        file.close();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("scheme.json: "));
        QVERIFY(!importScheme(file.fileName()));
    }
};

QTEST_GUILESS_MAIN(SchemeTest)
#include "scheme_test.moc"
