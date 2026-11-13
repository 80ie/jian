#pragma once

#include <QImage>

struct Config;
struct Palette;

// The panel as `config` lays it out, drawn offscreen with canned data in place of anything
// live, the time and the font among it.
QImage renderPanel(const Palette& palette, const Config& config);
