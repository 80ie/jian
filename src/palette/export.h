#pragma once

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QStringList>

#include <optional>

struct Palette;

struct ExportTarget {
    QString destination;
    QString command;
    bool enabled = false;

    bool operator==(const ExportTarget&) const = default;
};

struct Rendering {
    QString text;
    QString unknown;
};

// `text` with each placeholder replaced by its color: {name}, {name:rgb} or {name:noHash}, where
// a name is a named color, tone-6 to tone+5, or chrome-2 to chrome+1.
Rendering render(QStringView text, const Palette& palette);

std::optional<ExportTarget> loadTarget(const QDir& dir, const QString& name);
bool saveTarget(const ExportTarget& target, const QDir& dir, const QString& name);
QStringList targetNames(const QDir& dir);
// Only into a new exports/, so that a user's edits and deletions there stand.
bool installTargets(const QDir& dir);
// A different file it replaces is first copied into history/<name>/. Runs no command.
bool exportTarget(
    const Palette& palette, const QDir& dir, const QString& name, const QDateTime& now);
