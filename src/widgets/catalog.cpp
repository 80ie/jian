#include "widgets/catalog.h"
#include "panel/config.h"
#include "widgets/clock.h"
#include "widgets/media.h"
#include "widgets/tasks.h"
#include "widgets/tray.h"
#include "widgets/worldview.h"

#include <QStandardPaths>

using namespace Qt::StringLiterals;

ShelfWidget* createWidget(QStringView id, const Palette& palette, const Config& config,
    WindowModel* windows, QWidget* parent)
{
    if (id == u"clock")
        return new Clock(palette, config.clockTime, config.clockDate, parent);
    if (id == u"worldview") {
        if (windows && !fvwmReachable()) {
            qWarning("worldview: no fvwm to reach through FvwmCommand");
            return nullptr;
        }
        return new WorldView(palette, windows, parent);
    }
    if (id == u"media")
        return new Media(palette, config.mediaSource, windows, parent);
    if (id == u"tasks")
        return new Tasks(palette, { config.tasksTile, config.tasksFloor }, windows, parent);
    if (id == u"tray") {
        if (windows && QStandardPaths::findExecutable(u"stalonetray"_s).isEmpty()) {
            qWarning("tray: stalonetray is not installed");
            return nullptr;
        }
        return new Tray(palette, windows, parent);
    }
    qWarning("no widget called %s", qUtf8Printable(id.toString()));
    return nullptr;
}

std::vector<ShelfWidget*> createWidgets(
    const Palette& palette, const Config& config, WindowModel* windows, QWidget* parent)
{
    std::vector<ShelfWidget*> widgets;
    for (const QString& id : config.widgets)
        if (ShelfWidget* widget = createWidget(id, palette, config, windows, parent))
            widgets.push_back(widget);
    return widgets;
}
