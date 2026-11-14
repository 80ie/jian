#include "panel/order.h"

#include <QTest>

#include <vector>

class OrderTest : public QObject {
    Q_OBJECT

private slots:
    void placed_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("before");
        QTest::addColumn<QStringList>("expected");
        QTest::newRow("up") << "media" << "worldview"
                            << QStringList { "clock", "media", "worldview", "tasks", "tray" };
        QTest::newRow("down") << "clock" << "media"
                              << QStringList { "worldview", "tasks", "clock", "media", "tray" };
        QTest::newRow("last") << "clock" << ""
                              << QStringList { "worldview", "tasks", "media", "tray", "clock" };
        QTest::newRow("before itself")
            << "tasks" << "tasks" << QStringList { "clock", "worldview", "tasks", "media", "tray" };
        QTest::newRow("before the next")
            << "tasks" << "media" << QStringList { "clock", "worldview", "tasks", "media", "tray" };
        QTest::newRow("new") << "notes" << "tasks"
                             << QStringList { "clock", "worldview", "notes", "tasks", "media",
                                    "tray" };
        QTest::newRow("new, before one gone")
            << "notes" << "gone"
            << QStringList { "clock", "worldview", "tasks", "media", "tray", "notes" };
    }

    void placed()
    {
        QFETCH(QString, id);
        QFETCH(QString, before);
        QFETCH(QStringList, expected);
        const QStringList order { "clock", "worldview", "tasks", "media", "tray" };
        QCOMPARE(::placed(order, id, before), expected);
    }

    void unplacedKeepsTheOfferedOrder()
    {
        const QStringList offered { "clock", "worldview", "tasks", "media", "tray" };
        QCOMPARE(::unplaced(offered, { "tray", "tasks", "notes" }),
            (QStringList { "clock", "worldview", "media" }));
        QCOMPARE(::unplaced(offered, offered), QStringList());
    }

    void gapIsBeforeTheFirstSlotWhoseMiddleIsBelow()
    {
        const std::vector<QRect> stack { { 0, 0, 100, 40 }, { 0, 44, 100, 100 } };
        QCOMPARE(gapAt(stack, -5), 0);
        QCOMPARE(gapAt(stack, 18), 0);
        QCOMPARE(gapAt(stack, 19), 1);
        QCOMPARE(gapAt(stack, 92), 1);
        QCOMPARE(gapAt(stack, 93), 2);
        QCOMPARE(gapAt(std::vector<QRect>(), 10), 0);
    }
};

QTEST_GUILESS_MAIN(OrderTest)
#include "order_test.moc"
