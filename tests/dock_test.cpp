#include "panel/dock.h"

#include <QTest>

using Strut = std::array<quint32, 12>;

class DockTest : public QObject {
    Q_OBJECT

private slots:
    void strutPartialValues()
    {
        const QRect screen(0, 0, 1600, 900);
        QCOMPARE(strutPartial(Qt::RightEdge, QRect(1440, 0, 160, 900), screen),
            (Strut { 0, 160, 0, 0, 0, 0, 0, 899, 0, 0, 0, 0 }));
        QCOMPARE(strutPartial(Qt::LeftEdge, QRect(0, 0, 160, 900), screen),
            (Strut { 160, 0, 0, 0, 0, 899, 0, 0, 0, 0, 0, 0 }));
    }

    // A strut is measured from the edge of the root window, so a panel on the right of the left
    // of two monitors reserves the whole of the right one as well.
    void strutPartialBesideAnotherMonitor()
    {
        QCOMPARE(strutPartial(Qt::RightEdge, QRect(1760, 0, 160, 1080), QRect(0, 0, 3840, 1080)),
            (Strut { 0, 2080, 0, 0, 0, 0, 0, 1079, 0, 0, 0, 0 }));
    }
};

QTEST_GUILESS_MAIN(DockTest)
#include "dock_test.moc"
