#include "panel/config.h"

#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace {

// QSettings would write an empty list as @Invalid().
QVariant listValue(const QStringList& list)
{
    return list.isEmpty() ? QVariant(QString()) : QVariant(list);
}

struct SeparationName {
    Separation separation;
    QLatin1StringView name;
};

constexpr std::array<SeparationName, 3> separationNames { { { Separation::Tiles, "tiles"_L1 },
    { Separation::Bays, "bays"_L1 }, { Separation::Grooves, "grooves"_L1 } } };

} // namespace

std::optional<Config> loadConfig(const QString& path)
{
    if (!QFileInfo(path).isReadable()) {
        qWarning("%s: cannot read", qUtf8Printable(path));
        return std::nullopt;
    }
    const auto bad = [&path](const char* key) {
        qWarning("%s: bad %s", qUtf8Printable(path), key);
        return std::nullopt;
    };

    QSettings ini(path, QSettings::IniFormat);
    Config config;
    const QString side = ini.value("panel/side").toString();
    if (side == "left"_L1)
        config.side = Qt::LeftEdge;
    else if (!side.isEmpty() && side != "right"_L1)
        return bad("panel/side");
    bool ok = false;
    config.width = ini.value("panel/width", config.width).toInt(&ok);
    if (!ok)
        return bad("panel/width");
    const QString separation = ini.value("panel/separation").toString();
    if (!separation.isEmpty()) {
        const auto named = std::ranges::find(separationNames, separation, &SeparationName::name);
        if (named == separationNames.end())
            return bad("panel/separation");
        config.separation = named->separation;
    }
    config.widgets = ini.value("panel/widgets", config.widgets).toStringList();
    config.widgets.removeAll(QString());
    config.poppedOut = ini.value("panel/popped").toStringList();
    config.poppedOut.removeAll(QString());
    ini.beginGroup("popout");
    for (const QString& id : ini.childKeys()) {
        const QStringList at = ini.value(id).toStringList();
        bool okX = false;
        bool okY = false;
        if (at.size() == 2)
            config.windowPositions[id] = QPoint(at[0].toInt(&okX), at[1].toInt(&okY));
        if (!okX || !okY)
            return bad(qPrintable(u"popout/"_s + id));
    }
    ini.endGroup();
    config.clockTime = ini.value("clock/time", config.clockTime).toString();
    config.clockDate = ini.value("clock/date", config.clockDate).toString();
    config.tasksTile = ini.value("tasks/tile", config.tasksTile).toInt(&ok);
    if (!ok || config.tasksTile < 1)
        return bad("tasks/tile");
    config.tasksFloor = ini.value("tasks/floor", config.tasksFloor).toInt(&ok);
    if (!ok || config.tasksFloor < 1)
        return bad("tasks/floor");
    config.mediaSource = ini.value("media/source").toString();
    return config;
}

bool saveConfig(const Config& config, const QString& path)
{
    QSettings ini(path, QSettings::IniFormat);
    ini.setValue("panel/side", config.side == Qt::LeftEdge ? u"left"_s : u"right"_s);
    ini.setValue("panel/width", config.width);
    ini.setValue("panel/separation",
        std::ranges::find(separationNames, config.separation, &SeparationName::separation)
            ->name.toString());
    ini.setValue("panel/widgets", listValue(config.widgets));
    ini.setValue("panel/popped", listValue(config.poppedOut));
    ini.remove("popout");
    for (const auto& [id, at] : config.windowPositions)
        ini.setValue(
            u"popout/"_s + id, QStringList { QString::number(at.x()), QString::number(at.y()) });
    ini.setValue("clock/time", config.clockTime);
    ini.setValue("clock/date", config.clockDate);
    ini.setValue("tasks/tile", config.tasksTile);
    ini.setValue("tasks/floor", config.tasksFloor);
    ini.setValue("media/source", config.mediaSource);
    ini.sync();
    if (ini.status() == QSettings::NoError)
        return true;
    qWarning("%s: cannot write", qUtf8Printable(path));
    return false;
}
