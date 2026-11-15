#pragma once

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QStringList>

#include <optional>

struct Palette;

// Export targets in the config directory `dir` (spec 9): exports/<name>/ holds target.ini and
// a template. Exporting writes the template, its placeholders filled from the palette, to the
// destination, and keeps the file it replaces in history/<name>/.

struct ExportTarget {
    // A leading ~/ stands for the home directory.
    QString destination;
    // A shell command run after a successful export, or empty.
    QString command;
    bool enabled = false;

    bool operator==(const ExportTarget&) const = default;
};

struct Rendering {
    QString text;
    // The first placeholder that names no color, empty when every one did. `text` is then
    // incomplete.
    QString unknown;
};

// `text` with each placeholder replaced by its color: {name}, {name:rgb} or {name:noHash}, where
// a name is a named color, tone-6 to tone+5, or chrome-2 to chrome+1.
Rendering render(QStringView text, const Palette& palette);

std::optional<ExportTarget> loadTarget(const QDir& dir, const QString& name);
bool saveTarget(const ExportTarget& target, const QDir& dir, const QString& name);
// Sorted.
QStringList targetNames(const QDir& dir);
// Copies the targets that come with the program into exports/, unless it is already there.
bool installTargets(const QDir& dir);
// Writes the target's template, filled from `palette`, to its destination. A different file
// already there is first copied into history/<name>/, named for `now`. Runs no command.
bool exportTarget(
    const Palette& palette, const QDir& dir, const QString& name, const QDateTime& now);
