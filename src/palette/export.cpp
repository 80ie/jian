#include "palette/export.h"
#include "palette/palette.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

using namespace Qt::StringLiterals;

namespace {

constexpr QLatin1StringView timeFormat = "yyyyMMdd-HHmmss"_L1;

constexpr std::pair<QLatin1StringView, QColor Palette::*> namedColors[] {
    { "face"_L1, &Palette::face },
    { "ink"_L1, &Palette::ink },
    { "fill"_L1, &Palette::fill },
    { "selection"_L1, &Palette::selection },
    { "pager_desk"_L1, &Palette::pagerDesk },
    { "chrome"_L1, &Palette::chrome },
};

// The signed, nonzero number after `prefix` in `name`, if it lies within the range.
std::optional<int> step(QStringView name, QLatin1StringView prefix, int darkest, int lightest)
{
    if (!name.startsWith(prefix))
        return std::nullopt;
    const QStringView number = name.sliced(prefix.size());
    if (number.isEmpty() || (number.front() != u'-' && number.front() != u'+'))
        return std::nullopt;
    bool ok = false;
    const int n = number.toInt(&ok);
    if (!ok || n == 0 || n < darkest || n > lightest)
        return std::nullopt;
    return n;
}

std::optional<QColor> colorNamed(QStringView name, const Palette& palette)
{
    for (const auto& [key, member] : namedColors)
        if (name == key)
            return palette.*member;
    if (const std::optional<int> tone = step(name, "tone"_L1, darkestTone, lightestTone))
        return palette.tone(*tone);
    if (const std::optional<int> tone = step(name, "chrome"_L1, darkestChrome, lightestChrome))
        return palette.chromeTone(*tone);
    return std::nullopt;
}

// What the placeholder `inner`, without its braces, stands for.
std::optional<QString> fill(QStringView inner, const Palette& palette)
{
    const qsizetype colon = inner.indexOf(u':');
    const std::optional<QColor> color = colorNamed(colon < 0 ? inner : inner.first(colon), palette);
    if (!color)
        return std::nullopt;
    if (colon < 0)
        return color->name();
    const QStringView format = inner.sliced(colon + 1);
    if (format == u"rgb")
        return u"%1,%2,%3"_s.arg(color->red()).arg(color->green()).arg(color->blue());
    if (format == u"noHash")
        return color->name().mid(1);
    return std::nullopt;
}

QString targetPath(const QDir& dir, const QString& name, const QString& file)
{
    return dir.filePath(u"exports/"_s + name + u'/' + file);
}

QString expandHome(const QString& path)
{
    return path.startsWith("~/"_L1) ? QDir::home().filePath(path.mid(2)) : path;
}

bool makePath(const QString& path)
{
    if (QDir().mkpath(path))
        return true;
    qWarning("%s: cannot create", qUtf8Printable(path));
    return false;
}

// A second backup within the same second replaces the first.
bool backUp(const QString& path, const QString& backups, const QDateTime& now)
{
    const QString copy
        = QDir(backups).filePath(now.toString(timeFormat) + u'-' + QFileInfo(path).fileName());
    if (makePath(backups) && (!QFile::exists(copy) || QFile::remove(copy))
        && QFile::copy(path, copy))
        return true;
    qWarning("%s: cannot back up to %s", qUtf8Printable(path), qUtf8Printable(copy));
    return false;
}

std::optional<QByteArray> readFile(const QString& path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
        return file.readAll();
    qWarning("%s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
    return std::nullopt;
}

} // namespace

Rendering render(QStringView text, const Palette& palette)
{
    Rendering rendering;
    qsizetype at = 0;
    while (true) {
        const qsizetype open = text.indexOf(u'{', at);
        rendering.text += text.sliced(at, (open < 0 ? text.size() : open) - at);
        if (open < 0)
            return rendering;
        const qsizetype close = text.indexOf(u'}', open);
        const qsizetype end = text.indexOf(u'\n', open);
        if (close < 0 || (end >= 0 && end < close)) {
            rendering.unknown = text.sliced(open, (end < 0 ? text.size() : end) - open).toString();
            return rendering;
        }
        const std::optional<QString> value = fill(text.sliced(open + 1, close - open - 1), palette);
        if (!value) {
            rendering.unknown = text.sliced(open, close + 1 - open).toString();
            return rendering;
        }
        rendering.text += *value;
        at = close + 1;
    }
}

std::optional<ExportTarget> loadTarget(const QDir& dir, const QString& name)
{
    const QString path = targetPath(dir, name, u"target.ini"_s);
    if (!QFileInfo(path).isReadable()) {
        qWarning("%s: cannot read", qUtf8Printable(path));
        return std::nullopt;
    }
    const QSettings ini(path, QSettings::IniFormat);
    ExportTarget target;
    target.destination = ini.value("target/destination").toString();
    target.command = ini.value("target/command").toString();
    target.enabled = ini.value("target/enabled").toBool();
    if (target.destination.isEmpty()) {
        qWarning("%s: no destination", qUtf8Printable(path));
        return std::nullopt;
    }
    return target;
}

bool saveTarget(const ExportTarget& target, const QDir& dir, const QString& name)
{
    const QString path = targetPath(dir, name, u"target.ini"_s);
    QSettings ini(path, QSettings::IniFormat);
    ini.clear();
    ini.setValue("target/destination", target.destination);
    ini.setValue("target/command", target.command);
    ini.setValue("target/enabled", target.enabled);
    ini.sync();
    if (ini.status() == QSettings::NoError)
        return true;
    qWarning("%s: cannot write", qUtf8Printable(path));
    return false;
}

QStringList targetNames(const QDir& dir)
{
    QStringList names;
    const QDir exports(dir.filePath(u"exports"_s));
    for (const QString& name : exports.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        if (QFile::exists(targetPath(dir, name, u"target.ini"_s)))
            names.append(name);
    return names;
}

bool installTargets(const QDir& dir)
{
    if (dir.exists(u"exports"_s))
        return true;
    const QDir shipped(u":/exports"_s);
    for (const QString& name : shipped.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!makePath(targetPath(dir, name, QString())))
            return false;
        for (const QString& file : { u"target.ini"_s, u"template"_s }) {
            const QString copy = targetPath(dir, name, file);
            // A file copied out of the resources is read-only, as they are.
            if (!QFile::copy(shipped.filePath(name + u'/' + file), copy)
                || !QFile::setPermissions(copy,
                    QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther)) {
                qWarning("%s: cannot write", qUtf8Printable(copy));
                return false;
            }
        }
    }
    return true;
}

bool exportTarget(
    const Palette& palette, const QDir& dir, const QString& name, const QDateTime& now)
{
    const std::optional<ExportTarget> target = loadTarget(dir, name);
    const QString templatePath = targetPath(dir, name, u"template"_s);
    const std::optional<QByteArray> text = readFile(templatePath);
    if (!target || !text)
        return false;
    const Rendering rendering = render(QString::fromUtf8(*text), palette);
    if (!rendering.unknown.isEmpty()) {
        qWarning(
            "%s: no color for %s", qUtf8Printable(templatePath), qUtf8Printable(rendering.unknown));
        return false;
    }
    const QByteArray bytes = rendering.text.toUtf8();
    const QString path = expandHome(target->destination);
    if (QFile::exists(path)) {
        const std::optional<QByteArray> before = readFile(path);
        if (!before)
            return false;
        if (*before == bytes)
            return true;
        if (!backUp(path, dir.filePath(u"history/"_s + name), now))
            return false;
    }
    if (!makePath(QFileInfo(path).path()))
        return false;
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit())
        return true;
    qWarning("%s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
    return false;
}
