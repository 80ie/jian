#include "panel/config.h"

#include <QFileInfo>
#include <QSettings>

using namespace Qt::StringLiterals;

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

    const QSettings ini(path, QSettings::IniFormat);
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
    config.widgets = ini.value("panel/widgets", config.widgets).toStringList();
    config.widgets.removeAll(QString());
    config.clockTime = ini.value("clock/time", config.clockTime).toString();
    config.clockDate = ini.value("clock/date", config.clockDate).toString();
    return config;
}

bool saveConfig(const Config& config, const QString& path)
{
    QSettings ini(path, QSettings::IniFormat);
    ini.setValue("panel/side", config.side == Qt::LeftEdge ? u"left"_s : u"right"_s);
    ini.setValue("panel/width", config.width);
    // QSettings would write an empty list as @Invalid().
    ini.setValue(
        "panel/widgets", config.widgets.isEmpty() ? QVariant(QString()) : QVariant(config.widgets));
    ini.setValue("clock/time", config.clockTime);
    ini.setValue("clock/date", config.clockDate);
    ini.sync();
    if (ini.status() == QSettings::NoError)
        return true;
    qWarning("%s: cannot write", qUtf8Printable(path));
    return false;
}
