#include "widgets/catalog.h"
#include "panel/config.h"
#include "widgets/clock.h"

ShelfWidget* createWidget(
    QStringView id, const Palette& palette, const Config& config, QWidget* parent)
{
    if (id == u"clock")
        return new Clock(palette, config.clockTime, config.clockDate, parent);
    qWarning("no widget called %s", qUtf8Printable(id.toString()));
    return nullptr;
}

std::vector<ShelfWidget*> createWidgets(
    const Palette& palette, const Config& config, QWidget* parent)
{
    std::vector<ShelfWidget*> widgets;
    for (const QString& id : config.widgets)
        if (ShelfWidget* widget = createWidget(id, palette, config, parent))
            widgets.push_back(widget);
    return widgets;
}
