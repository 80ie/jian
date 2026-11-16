#include "widgets/catalog.h"
#include "panel/config.h"
#include "widgets/clock.h"
#include "widgets/media.h"
#include "widgets/tasks.h"
#include "widgets/tray.h"
#include "widgets/worldview.h"

#include <QStandardPaths>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace {

struct Entry {
    QStringView id;
    QStringView name;
};

constexpr std::array<Entry, 5> entries { { { u"clock", u"Clock" }, { u"worldview", u"World View" },
    { u"tasks", u"Tasks" }, { u"media", u"Media" }, { u"tray", u"Tray" } } };

// Why the widget `id` cannot run here, or null if it can.
const char* missing(QStringView id, const WindowModel* windows)
{
    if (!windows)
        return nullptr;
    if (id == u"worldview" && !fvwmReachable())
        return "no fvwm to reach through FvwmCommand";
    if (id == u"tray" && QStandardPaths::findExecutable(u"stalonetray"_s).isEmpty())
        return "stalonetray is not installed";
    return nullptr;
}

} // namespace

QStringList offeredWidgets(const WindowModel* windows)
{
    QStringList offered;
    for (const Entry& entry : entries)
        if (!missing(entry.id, windows))
            offered.append(entry.id.toString());
    return offered;
}

QString widgetName(QStringView id)
{
    const auto entry = std::ranges::find(entries, id, &Entry::id);
    return entry == entries.end() ? id.toString() : entry->name.toString();
}

ShelfWidget* createWidget(QStringView id, const Palette& palette, const Config& config,
    WindowModel* windows, QWidget* parent)
{
    if (const char* why = missing(id, windows)) {
        qWarning("%s: %s", qUtf8Printable(id.toString()), why);
        return nullptr;
    }
    if (id == u"clock")
        return new Clock(palette, config.clockTime, config.clockDate, parent);
    if (id == u"worldview")
        return new WorldView(palette, windows, parent);
    if (id == u"media")
        return new Media(palette, config.mediaSource, windows, parent);
    if (id == u"tasks")
        return new Tasks(palette, config.tasksFloor, windows, parent);
    if (id == u"tray")
        return new Tray(palette, windows, parent);
    qWarning("no widget called %s", qUtf8Printable(id.toString()));
    return nullptr;
}
