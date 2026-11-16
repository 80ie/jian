#include "panel/render.h"
#include "paint/font.h"
#include "palette/palette.h"
#include "panel/config.h"
#include "panel/panel.h"
#include "widgets/catalog.h"
#include "widgets/clock.h"
#include "widgets/media.h"
#include "widgets/tasks.h"
#include "widgets/worldview.h"

#include <QPainter>

using namespace Qt::StringLiterals;

namespace {

constexpr int height = 900;
constexpr QSize screen(1600, 900);

// An icon or a capture: a body with a bar across its top, in palette colors so that the render
// follows the palette.
QImage cannedImage(QSize size, const QColor& body, const QColor& bar)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(body);
    QPainter p(&image);
    p.fillRect(0, 0, size.width(), size.height() / 5, bar);
    return image;
}

std::vector<Client> cannedTasks(const Palette& palette)
{
    const auto task = [&palette](quint32 window, const QString& title, const QColor& color) {
        Client client;
        client.window = window;
        client.title = title;
        client.icon = cannedImage(QSize(64, 64), color, palette.ink);
        return client;
    };
    std::vector<Client> tasks { task(1, u"Mail — Inbox (3)"_s, palette.selection),
        task(2, u"Terminal"_s, palette.tone(-6)), task(3, u"Music"_s, palette.fill),
        task(4, u"notes.txt — Editor"_s, palette.tone(4)),
        task(5, u"A title long enough to run out of room"_s, palette.pagerDesk),
        task(6, u"Calculator"_s, palette.tone(-2)), task(7, u"Downloads"_s, palette.fill) };
    const QSize capture(320, 200);
    tasks[0].pinned = true;
    tasks[1].focused = true;
    tasks[2].pinned = true;
    tasks[5].minimized = true;
    tasks[5].minimizedOrder = 2;
    tasks[5].thumbnail = cannedImage(capture, palette.tone(3), palette.tone(-5));
    tasks[6].minimized = true;
    tasks[6].minimizedOrder = 1;
    return tasks;
}

// Paused, so that the trackbar does not follow the clock.
std::vector<Player> cannedPlayers()
{
    Player player;
    player.service = u"org.mpris.MediaPlayer2.vlc"_s;
    player.identity = u"VLC media player"_s;
    player.playback = Playback::Paused;
    player.title = u"Boards of Canada - Roygbiv"_s;
    player.length = std::chrono::seconds(151);
    player.position = std::chrono::seconds(60);
    player.canControl = true;
    player.canGoNext = true;
    player.canSeek = true;
    return { player };
}

Sinks cannedSinks()
{
    return { { { u"analog"_s, u"Built-in Audio Analog Stereo"_s, 0.6, false },
                 { u"hdmi"_s, u"HDMI Output"_s, 1, false } },
        u"analog"_s };
}

QImage cannedArt(const Palette& palette)
{
    QImage image(300, 300, QImage::Format_RGB32);
    QPainter p(&image);
    const QColor rings[] { palette.pagerDesk, palette.tone(-6), palette.fill, palette.pagerDesk };
    for (int i = 0; i < 4; ++i)
        p.fillRect(image.rect().adjusted(i * 38, i * 38, -i * 38, -i * 38), rings[i]);
    return image;
}

} // namespace

QImage renderPanel(const Palette& palette, const Config& config)
{
    // Spelled out, or the image would follow the desktop's fonts.
    const QFont text
        = paint::font(QFont(u"Linux Libertine"_s, -1, QFont::DemiBold), paint::textPoints);
    const QFont fixed(u"Linux Libertine Mono O"_s);
    const QDateTime time(QDate(2026, 11, 13), QTime(9, 41));

    Panel panel(palette, { }, [&](const QString& id, QWidget* parent) {
        ShelfWidget* widget = createWidget(id, palette, config, nullptr, parent);
        if (!widget)
            return widget;
        widget->setFont(qobject_cast<Clock*>(widget) ? fixed : text);
        if (auto* clock = qobject_cast<Clock*>(widget))
            clock->setTime(time);
        if (auto* tasks = qobject_cast<Tasks*>(widget))
            tasks->setClients(cannedTasks(palette));
        if (auto* worldView = qobject_cast<WorldView*>(widget))
            worldView->setScreen(screen);
        if (auto* media = qobject_cast<Media*>(widget)) {
            media->setPlayers(cannedPlayers());
            media->setSinks(cannedSinks());
            media->setArt(cannedArt(palette));
        }
        return widget;
    });
    panel.setSide(config.side);
    panel.setWidth(config.width);
    panel.arrange(config.widgets, { });
    panel.resize(panel.width(), height);
    return panel.grab().toImage();
}
