#include "palette/library.h"

#include <QFile>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr QLatin1StringView timeFormat = "yyyyMMdd-HHmmss"_L1;

QString historyPath(const QDir& dir, const QDateTime& time)
{
    return dir.filePath(u"history/"_s + time.toString(timeFormat) + u".ini"_s);
}

QString presetPath(const QDir& dir, const QString& name)
{
    return dir.filePath(u"palettes/"_s + name + u".ini"_s);
}

bool makeDirectory(const QDir& dir, const QString& name)
{
    if (dir.mkpath(name))
        return true;
    qWarning("%s: cannot create", qUtf8Printable(dir.filePath(name)));
    return false;
}

// Base names of the .ini files in `dir`'s subdirectory `name`, sorted.
QStringList iniNames(const QDir& dir, const QString& name)
{
    QStringList names = QDir(dir.filePath(name)).entryList({ u"*.ini"_s }, QDir::Files, QDir::Name);
    for (QString& file : names)
        file.chop(4);
    return names;
}

} // namespace

bool savePaletteWithHistory(const Palette& palette, const QDir& dir, const QDateTime& now)
{
    if (!savePalette(palette, dir.filePath(u"palette.ini"_s)) || !makeDirectory(dir, u"history"_s)
        || !savePalette(palette, historyPath(dir, now)))
        return false;
    const QList<QDateTime> times = historyTimes(dir);
    for (qsizetype i = historyLength; i < times.size(); ++i)
        QFile::remove(historyPath(dir, times[i]));
    return true;
}

QList<QDateTime> historyTimes(const QDir& dir)
{
    QList<QDateTime> times;
    for (const QString& name : iniNames(dir, u"history"_s))
        if (const QDateTime time = QDateTime::fromString(name, timeFormat); time.isValid())
            times.append(time);
    std::ranges::sort(times, std::greater { });
    return times;
}

std::optional<Palette> loadHistory(const QDir& dir, const QDateTime& time)
{
    return loadPalette(historyPath(dir, time));
}

bool isPresetName(const QString& name)
{
    return !name.isEmpty() && !name.startsWith(u'.') && !name.contains(u'/')
        && !name.contains(QChar::Null);
}

QStringList presetNames(const QDir& dir) { return iniNames(dir, u"palettes"_s); }

bool savePreset(const Palette& palette, const QDir& dir, const QString& name)
{
    return isPresetName(name) && makeDirectory(dir, u"palettes"_s)
        && savePalette(palette, presetPath(dir, name));
}

std::optional<Palette> loadPreset(const QDir& dir, const QString& name)
{
    return loadPalette(presetPath(dir, name));
}

bool deletePreset(const QDir& dir, const QString& name)
{
    if (QFile::remove(presetPath(dir, name)))
        return true;
    qWarning("%s: cannot delete", qUtf8Printable(presetPath(dir, name)));
    return false;
}
