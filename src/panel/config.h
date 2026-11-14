#pragma once

#include <QPoint>
#include <QString>
#include <QStringList>

#include <map>
#include <optional>

struct Config {
    Qt::Edge side = Qt::RightEdge;
    int width = 160;
    QStringList widgets { QStringLiteral("clock"), QStringLiteral("worldview"),
        QStringLiteral("tasks"), QStringLiteral("media"), QStringLiteral("tray") };
    // Widgets out of the panel in windows of their own. Each keeps its place in `widgets`, where
    // it docks again.
    QStringList poppedOut;
    // Where each widget's window was last, kept once it docks for the next time it pops out.
    std::map<QString, QPoint> windowPositions;
    // QDateTime formats, drawn at the left and right ends of the clock.
    QString clockTime = QStringLiteral("hh:mmAP");
    QString clockDate = QStringLiteral("ddd MM.dd");
    // In pixels: the height of a pinned tile showing a capture, and the least a minimized
    // window's capture shrinks to before it gives way to a plain row.
    int tasksTile = 48;
    int tasksFloor = 64;
    // The key of the player the media controls keep to; empty follows whichever started playing
    // last.
    QString mediaSource;

    bool operator==(const Config&) const = default;
};

// Keys the file leaves out keep their defaults.
std::optional<Config> loadConfig(const QString& path);
bool saveConfig(const Config& config, const QString& path);
