#include "x11/client.h"

#include <QTest>

#include <vector>

namespace {

const QRect screen(0, 0, 1600, 900);

Client at(const QRect& geometry, quint32 desktop = 0)
{
    Client client;
    client.geometry = geometry;
    client.desktop = desktop;
    return client;
}

// A _NET_WM_ICON image of `size` square, every pixel `pixel`.
void appendIcon(std::vector<quint32>& value, quint32 size, quint32 pixel)
{
    value.push_back(size);
    value.push_back(size);
    value.insert(value.end(), size * size, pixel);
}

} // namespace

class ClientTest : public QObject {
    Q_OBJECT

private slots:
    void isShown_data()
    {
        QTest::addColumn<Client>("client");
        QTest::addColumn<bool>("shown");
        QTest::newRow("on the screen") << at({ 100, 100, 300, 200 }) << true;
        QTest::newRow("on the next page") << at({ 1700, 100, 300, 200 }) << false;
        QTest::newRow("on the page above") << at({ 100, -800, 300, 200 }) << false;
        QTest::newRow("across the edge") << at({ 1500, 100, 300, 200 }) << true;
        QTest::newRow("against the edge") << at({ 1600, 100, 300, 200 }) << false;
        QTest::newRow("on another desktop") << at({ 100, 100, 300, 200 }, 1) << false;
        QTest::newRow("on every desktop") << at({ 100, 100, 300, 200 }, allDesktops) << true;
        Client skipped = at({ 100, 100, 300, 200 });
        skipped.skipTaskbar = true;
        QTest::newRow("skipping the taskbar") << skipped << false;
    }

    void isShown()
    {
        QFETCH(Client, client);
        QFETCH(bool, shown);
        QCOMPARE(::isShown(client, screen, 0), shown);
    }

    void decodeIconNearest_data()
    {
        QTest::addColumn<int>("size");
        QTest::addColumn<int>("width");
        QTest::newRow("exact") << 32 << 32;
        QTest::newRow("between") << 24 << 32;
        QTest::newRow("below the smallest") << 8 << 16;
        QTest::newRow("above the largest") << 64 << 48;
    }

    void decodeIconNearest()
    {
        QFETCH(int, size);
        QFETCH(int, width);
        std::vector<quint32> value;
        for (quint32 s : { 48, 16, 32 })
            appendIcon(value, s, 0xff000000 | s);
        const QImage icon = decodeIcon(value, size);
        QCOMPARE(icon.width(), width);
        QCOMPARE(icon.pixel(0, 0), 0xff000000 | width);
    }

    void decodeIconKeepsAlpha()
    {
        std::vector<quint32> value { 2, 1, 0x80ff0000, 0x00000000 };
        const QImage icon = decodeIcon(value, 16);
        QCOMPARE(icon.size(), QSize(2, 1));
        QCOMPARE(icon.pixel(0, 0), 0x80ff0000u);
        QCOMPARE(qAlpha(icon.pixel(1, 0)), 0);
    }

    void decodeIconStopsAtBadData()
    {
        QVERIFY(decodeIcon({ }, 16).isNull());
        QVERIFY(decodeIcon(std::vector<quint32> { 4, 4, 1, 2, 3 }, 16).isNull());
        QVERIFY(decodeIcon(std::vector<quint32> { 0, 4 }, 16).isNull());
        std::vector<quint32> value;
        appendIcon(value, 16, 1);
        value.insert(value.end(), { 32, 32, 7 });
        QCOMPARE(decodeIcon(value, 32).width(), 16);
    }
};

QTEST_GUILESS_MAIN(ClientTest)
#include "client_test.moc"
