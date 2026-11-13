#pragma once

#include <QStringView>

#include <vector>

class QWidget;
class ShelfWidget;
struct Config;
struct Palette;

// A new widget of the kind config.ini calls `id`, or nullptr if there is none.
ShelfWidget* createWidget(
    QStringView id, const Palette& palette, const Config& config, QWidget* parent);
// The widgets config.ini lists, in its order.
std::vector<ShelfWidget*> createWidgets(
    const Palette& palette, const Config& config, QWidget* parent);
