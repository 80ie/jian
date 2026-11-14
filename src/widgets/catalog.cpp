#include "widgets/catalog.h"
#include "panel/config.h"
#include "widgets/clock.h"
#include "widgets/tasks.h"

ShelfWidget* createWidget(QStringView id, const Palette& palette, const Config& config,
    WindowModel* windows, QWidget* parent)
{
    if (id == u"clock")
        return new Clock(palette, config.clockTime, config.clockDate, parent);
    if (id == u"tasks")
        return new Tasks(palette, { config.tasksTile, config.tasksFloor }, windows, parent);
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
