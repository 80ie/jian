#pragma once

#include <QStringView>

#include <vector>

class QWidget;
class ShelfWidget;
class WindowModel;
struct Config;
struct Palette;

// A new widget of the kind config.ini calls `id`, or nullptr if there is none. `windows` is null
// without X.
ShelfWidget* createWidget(QStringView id, const Palette& palette, const Config& config,
    WindowModel* windows, QWidget* parent);
// The widgets config.ini lists, in its order.
std::vector<ShelfWidget*> createWidgets(
    const Palette& palette, const Config& config, WindowModel* windows, QWidget* parent);
