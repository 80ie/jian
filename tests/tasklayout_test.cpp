#include "widgets/tasklayout.h"

#include <QTest>

namespace {

Client task(quint32 window, bool pinned, bool minimized, quint64 minimizedOrder = 0)
{
    Client client;
    client.window = window;
    client.pinned = pinned;
    client.minimized = minimized;
    client.minimizedOrder = minimizedOrder;
    return client;
}

Client captured(quint32 window, quint64 minimizedOrder)
{
    Client client = task(window, false, true, minimizedOrder);
    client.thumbnail = QImage(4, 4, QImage::Format_RGB32);
    return client;
}

// Two pinned, three rows, two minimized with captures: 11 of wells, 46 of tiles, 125 of rows.
const TaskCounts counts { .pinned = 2, .rows = 3, .thumbnails = 2 };
const TaskSizes sizes { .tile = 48, .floor = 64 };

} // namespace

class TaskLayoutTest : public QObject {
    Q_OBJECT

private slots:
    void orderPutsPinnedFirstAndMinimizedLast()
    {
        const std::vector<Client> order = taskOrder({ task(1, false, true), task(2, false, false),
            task(3, true, true), task(4, false, false), task(5, true, false) });
        std::vector<quint32> windows;
        for (const Client& client : order)
            windows.push_back(client.window);
        QCOMPARE(windows, (std::vector<quint32> { 3, 5, 2, 4, 1 }));
    }

    void countSortsMinimizedByCapture()
    {
        const TaskCounts counted
            = countTasks({ task(1, true, true), task(2, false, true), captured(3, 1) });
        QCOMPARE(counted.pinned, 1);
        QCOMPARE(counted.rows, 1);
        QCOMPARE(counted.thumbnails, 1);
    }

    void naturalAndMinimum()
    {
        QCOMPARE(naturalHeight(counts, sizes), 11 + 46 + 125 + 2 * (18 + 136));
        QCOMPARE(minimumHeight(counts), 11 + 30 + 25);
        QCOMPARE(naturalHeight({ }, sizes), 4);
        QCOMPARE(naturalHeight({ .pinned = 3 }, sizes), 4 + 94);
    }

    void fit_data()
    {
        QTest::addColumn<int>("height");
        QTest::addColumn<TaskFit>("fit");
        QTest::newRow("room to spare") << 600 << TaskFit { 48, 136, 2, 125 + 2 * 154, false };
        QTest::newRow("natural") << 490 << TaskFit { 48, 136, 2, 125 + 2 * 154, false };
        QTest::newRow("thumbnails shrink together")
            << 400 << TaskFit { 48, 91, 2, 125 + 2 * 109, false };
        QTest::newRow("one collapses below the floor")
            << 345 << TaskFit { 48, 136, 1, 125 + 154, false };
        QTest::newRow("the last at the floor") << 264 << TaskFit { 48, 64, 1, 125 + 82, false };
        QTest::newRow("all collapsed") << 263 << TaskFit { 48, 0, 0, 125, false };
        QTest::newRow("rows and tiles just fit") << 182 << TaskFit { 48, 0, 0, 125, false };
        QTest::newRow("tiles drop to icons") << 181 << TaskFit { 32, 0, 0, 125, false };
        QTest::newRow("rows scroll") << 165 << TaskFit { 32, 0, 0, 125, true };
    }

    void fit()
    {
        QFETCH(int, height);
        QFETCH(TaskFit, fit);
        QCOMPARE(fitTasks(height, counts, sizes), fit);
    }

    void keptAreTheMostRecentlyMinimized()
    {
        const std::vector<Client> tasks { captured(1, 5), task(2, false, true, 7), captured(3, 2),
            captured(4, 9) };
        QCOMPARE(keptThumbnails(tasks, 2), (std::vector<quint32> { 4, 1 }));
        QCOMPARE(keptThumbnails(tasks, 0), std::vector<quint32> { });
    }
};

QTEST_GUILESS_MAIN(TaskLayoutTest)
#include "tasklayout_test.moc"
