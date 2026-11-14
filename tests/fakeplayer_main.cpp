#include "fakeplayer.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusError>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QUrl>

#include <cstdio>

using namespace Qt::StringLiterals;

// A fake player for looking at the panel in fvwm-rig: one track with cover art, playing from
// 40% of the way in. It joins only the bus it is given, never the session's.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    const QCommandLineOption busOption(u"bus"_s, u"Join the bus at <address>."_s, u"address"_s);
    parser.addOption(busOption);
    parser.process(app);
    if (!parser.isSet(busOption)) {
        std::fputs(qPrintable(parser.helpText()), stderr);
        return 2;
    }
    QDBusConnection bus = QDBusConnection::connectToBus(parser.value(busOption), u"fake"_s);
    if (!bus.isConnected()) {
        qWarning("fakeplayer: %s", qUtf8Printable(bus.lastError().message()));
        return 1;
    }

    const QTemporaryDir dir;
    QImage art(300, 300, QImage::Format_RGB32);
    QPainter p(&art);
    const QColor rings[] { QColor(0x5c, 0x30, 0x0e), QColor(0x14, 0x14, 0x08),
        QColor(0x58, 0x4a, 0x0c), QColor(0x5c, 0x30, 0x0e) };
    for (int i = 0; i < 4; ++i)
        p.fillRect(art.rect().adjusted(i * 38, i * 38, -i * 38, -i * 38), rings[i]);
    p.end();
    const QString artPath = dir.filePath(u"art.png"_s);
    art.save(artPath);

    FakePlayer player(bus, u"fake"_s, u"Fake Player"_s);
    player.setMetadata({ { u"xesam:title"_s, u"Roygbiv"_s },
        { u"xesam:artist"_s, QStringList { u"Boards of Canada"_s } },
        { u"mpris:length"_s, qlonglong(240'000'000) },
        { u"mpris:trackid"_s, QVariant::fromValue(QDBusObjectPath(u"/track/1"_s)) },
        { u"mpris:artUrl"_s, QUrl::fromLocalFile(artPath).toString() } });
    player.jump(96'000'000);
    player.setPlaying(true);
    return QCoreApplication::exec();
}
