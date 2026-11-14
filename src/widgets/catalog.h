#pragma once

#include <QStringList>

class QWidget;
class ShelfWidget;
class WindowModel;
struct Config;
struct Palette;

// The widgets that can run here, in the panel's default order. World View needs fvwm and the tray
// stalonetray, when there is X to run them on.
QStringList offeredWidgets(const WindowModel* windows);
// What edit mode calls the widget `id`.
QString widgetName(QStringView id);
// A new widget of the kind config.ini calls `id`, or nullptr if there is none. `windows` is null
// without X.
ShelfWidget* createWidget(QStringView id, const Palette& palette, const Config& config,
    WindowModel* windows, QWidget* parent);
