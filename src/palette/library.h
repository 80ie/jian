#pragma once

#include "palette/palette.h"

#include <QDateTime>
#include <QDir>
#include <QList>
#include <QStringList>

#include <optional>

// The palettes kept beside palette.ini in the config directory `dir`: named presets in
// palettes/<name>.ini, and in history/ a copy of each save named for its time, to the second.

// A save past this many copies deletes the oldest.
constexpr qsizetype historyLength = 50;

// Writes palette.ini, and a copy of it into history/ as of `now`.
bool savePaletteWithHistory(const Palette& palette, const QDir& dir, const QDateTime& now);
// Newest first.
QList<QDateTime> historyTimes(const QDir& dir);
std::optional<Palette> loadHistory(const QDir& dir, const QDateTime& time);

// Whether `name` can be a preset's: a file name of its own in palettes/.
bool isPresetName(const QString& name);
// Sorted.
QStringList presetNames(const QDir& dir);
bool savePreset(const Palette& palette, const QDir& dir, const QString& name);
std::optional<Palette> loadPreset(const QDir& dir, const QString& name);
bool deletePreset(const QDir& dir, const QString& name);
