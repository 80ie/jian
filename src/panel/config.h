#pragma once

#include <QString>
#include <QStringList>

#include <optional>

struct Config {
    Qt::Edge side = Qt::RightEdge;
    int width = 160;
    QStringList widgets { QStringLiteral("clock") };
    // QDateTime formats, drawn at the left and right ends of the clock.
    QString clockTime = QStringLiteral("hh:mmAP");
    QString clockDate = QStringLiteral("ddd MM.dd");

    bool operator==(const Config&) const = default;
};

// Keys the file leaves out keep their defaults.
std::optional<Config> loadConfig(const QString& path);
bool saveConfig(const Config& config, const QString& path);
