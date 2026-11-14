#include "widgets/pip.h"

#include <QTest>

class PipTest : public QObject {
    Q_OBJECT

private slots:
    void shape_data()
    {
        QTest::addColumn<QSize>("size");
        QTest::addColumn<bool>("shaped");
        QTest::newRow("firefox default") << QSize(480, 270) << true;
        QTest::newRow("smallest") << QSize(160, 90) << true;
        QTest::newRow("too small") << QSize(150, 90) << false;
        QTest::newRow("too large") << QSize(1280, 720) << false;
        QTest::newRow("square") << QSize(400, 400) << false;
        QTest::newRow("a strip") << QSize(900, 300) << false;
        QTest::newRow("empty") << QSize() << false;
    }

    void shape()
    {
        QFETCH(QSize, size);
        QFETCH(bool, shaped);
        QCOMPARE(pipShaped(size), shaped);
    }
};

QTEST_GUILESS_MAIN(PipTest)
#include "pip_test.moc"
