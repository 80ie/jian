#include "panel/render.h"
#include "panel/config.h"
#include "panel/panel.h"
#include "widgets/catalog.h"
#include "widgets/clock.h"

using namespace Qt::StringLiterals;

namespace {

constexpr int height = 120;

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
    std::vector<ShelfWidget*> widgets = createWidgets(palette, config, &panel);
    for (ShelfWidget* widget : widgets) {
        widget->setFont(font);
        if (auto* clock = qobject_cast<Clock*>(widget))
            clock->setTime(time);
    }
    panel.setWidgets(std::move(widgets));
    panel.resize(panel.width(), height);
    return panel.grab().toImage();
}
