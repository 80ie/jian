#include "panel/render.h"
#include "palette/palette.h"
#include "panel/config.h"
#include "panel/panel.h"
#include "widgets/catalog.h"
#include "widgets/clock.h"
#include "widgets/tasks.h"

#include <QPainter>

using namespace Qt::StringLiterals;

namespace {

constexpr int height = 360;

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
        client.icon = cannedImage(QSize(32, 32), color, palette.ink);
        return client;
    };
    std::vector<Client> tasks { task(1, u"Mail — Inbox (3)"_s, palette.selection),
        task(2, u"Terminal"_s, palette.tone(-6)), task(3, u"Music"_s, palette.fill),
        task(4, u"notes.txt — Editor"_s, palette.tone(4)),
        task(5, u"A title long enough to run out of room"_s, palette.pagerDesk),
        task(6, u"Calculator"_s, palette.tone(-2)), task(7, u"Downloads"_s, palette.fill) };
    const QSize capture(320, 200);
    tasks[0].pinned = true;
    tasks[0].thumbnail = cannedImage(capture, palette.tone(5), palette.selection);
    tasks[1].focused = true;
    tasks[2].pinned = true;
    tasks[5].minimized = true;
    tasks[5].minimizedOrder = 2;
    tasks[5].thumbnail = cannedImage(capture, palette.tone(3), palette.tone(-5));
    tasks[6].minimized = true;
    tasks[6].minimizedOrder = 1;
    return tasks;
}

} // namespace

QImage renderPanel(const Palette& palette, const Config& config)
{
    // Spelled out in full, or the image would follow the desktop's font settings.
    QFont font(u"DejaVu Sans Mono"_s);
    font.setWeight(QFont::Normal);
    font.setStyleName(QString());
    const QDateTime time(QDate(2026, 11, 13), QTime(9, 41));

    Panel panel(palette);
    panel.setSide(config.side);
    panel.setWidth(config.width);
    std::vector<ShelfWidget*> widgets = createWidgets(palette, config, nullptr, &panel);
    for (ShelfWidget* widget : widgets) {
        widget->setFont(font);
        if (auto* clock = qobject_cast<Clock*>(widget))
            clock->setTime(time);
        if (auto* tasks = qobject_cast<Tasks*>(widget))
            tasks->setClients(cannedTasks(palette));
    }
    panel.setWidgets(std::move(widgets));
    panel.resize(panel.width(), height);
    return panel.grab().toImage();
}
