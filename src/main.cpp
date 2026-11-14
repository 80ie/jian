#include "paint/bevel.h"
#include "paint/sheet.h"
#include "palette/palette.h"
#include "panel/config.h"
#include "panel/panel.h"
#include "panel/render.h"
#include "widgets/catalog.h"
#include "widgets/media.h"
#include "widgets/shelfwidget.h"
#include "x11/connection.h"
#include "x11/thumbnails.h"
#include "x11/trace.h"
#include "x11/windowmodel.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QLockFile>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>
#include <memory>
#include <optional>

using namespace Qt::StringLiterals;

namespace {

// A file that is not there gives the defaults; one that is there but bad gives nothing.
std::optional<Palette> readPalette(const QString& path)
{
    return QFile::exists(path) ? loadPalette(path) : Palette();
}

std::optional<Config> readConfig(const QString& path)
{
    return QFile::exists(path) ? loadConfig(path) : Config();
}

int saveImage(const QImage& image, const QString& path)
{
    if (image.save(path))
        return 0;
    qWarning("%s: cannot write", qUtf8Printable(path));
    return 1;
}

int runWidget(QStringView id, const Palette& palette, const Config& config, WindowModel* windows)
{
    const std::unique_ptr<ShelfWidget> widget(createWidget(id, palette, config, windows, nullptr));
    if (!widget)
        return 1;
    const int width = config.width - paint::frameWidth;
    widget->resize(width, widget->heightForWidth(width));
    QObject::connect(widget.get(), &ShelfWidget::heightChanged, widget.get(),
        [&widget] { widget->resize(widget->width(), widget->heightForWidth(widget->width())); });
    widget->show();
    return QApplication::exec();
}

int runPanel(const QDir& dir, WindowModel* windows)
{
    const QString configPath = dir.filePath(u"config.ini"_s);
    const QString palettePath = dir.filePath(u"palette.ini"_s);
    Config config = readConfig(configPath).value_or(Config());
    Palette palette = readPalette(palettePath).value_or(Palette());

    Panel panel(palette);
    const auto apply = [&] {
        panel.setSide(config.side);
        panel.setWidth(config.width);
        std::vector<ShelfWidget*> widgets = createWidgets(palette, config, windows, &panel);
        for (ShelfWidget* widget : widgets)
            if (auto* media = qobject_cast<Media*>(widget))
                QObject::connect(media, &Media::sourceChosen, &panel, [&](const QString& source) {
                    config.mediaSource = source;
                    saveConfig(config, configPath);
                });
        panel.setWidgets(std::move(widgets));
    };
    apply();
    QObject::connect(&panel, &Panel::widthDragged, &panel, [&](int width) {
        config.width = width;
        saveConfig(config, configPath);
    });
    QObject::connect(&panel, &Panel::sideSwitched, &panel, [&](Qt::Edge side) {
        config.side = side;
        saveConfig(config, configPath);
    });

    QFileSystemWatcher watcher({ dir.path() });
    const auto rewatch = [&] {
        // A file replaced by a rename keeps its path in the list but loses its watch.
        if (!watcher.files().isEmpty())
            watcher.removePaths(watcher.files());
        for (const QString& path : { configPath, palettePath })
            if (QFile::exists(path))
                watcher.addPath(path);
    };
    rewatch();
    // Gathers the burst of changes one save makes, from an editor or from QSettings.
    QTimer settle;
    settle.setSingleShot(true);
    settle.setInterval(50);
    QObject::connect(
        &watcher, &QFileSystemWatcher::directoryChanged, &settle, qOverload<>(&QTimer::start));
    QObject::connect(
        &watcher, &QFileSystemWatcher::fileChanged, &settle, qOverload<>(&QTimer::start));
    QObject::connect(&settle, &QTimer::timeout, &panel, [&] {
        rewatch();
        if (const std::optional<Palette> next = readPalette(palettePath);
            next && *next != palette) {
            palette = *next;
            panel.paletteChanged();
        }
        if (const std::optional<Config> next = readConfig(configPath); next && *next != config) {
            config = *next;
            apply();
        }
    });

    panel.show();
    return QApplication::exec();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"shelf"_s);
    QCommandLineParser parser;
    const QCommandLineOption renderOption(
        u"render"_s, u"Draw the panel with canned data to <png>."_s, u"png"_s);
    const QCommandLineOption sheetOption(
        u"render-sheet"_s, u"Draw the painter sheet to <png>."_s, u"png"_s);
    const QCommandLineOption paletteOption(
        u"palette"_s, u"Draw either with the palette in <ini>."_s, u"ini"_s);
    const QCommandLineOption onlyOption(
        u"only"_s, u"Run one widget as a plain window."_s, u"widget"_s);
    const QCommandLineOption traceOption(
        u"trace-x"_s, u"Log X events and task repaints with their times to stderr."_s);
    parser.addOptions({ renderOption, sheetOption, paletteOption, onlyOption, traceOption });
    const bool parsed = parser.parse(app.arguments());
    const bool rendering = parser.isSet(renderOption) || parser.isSet(sheetOption);
    if (!parsed || !parser.positionalArguments().isEmpty()
        || parser.isSet(renderOption) + parser.isSet(sheetOption) + parser.isSet(onlyOption) > 1
        || (parser.isSet(paletteOption) && !rendering)
        || (parser.isSet(traceOption) && rendering)) {
        std::fputs(qPrintable(parser.helpText()), stderr);
        return 2;
    }

    if (rendering) {
        const std::optional<Palette> palette
            = parser.isSet(paletteOption) ? loadPalette(parser.value(paletteOption)) : Palette();
        if (!palette)
            return 1;
        if (parser.isSet(renderOption))
            return saveImage(renderPanel(*palette, Config()), parser.value(renderOption));
        return saveImage(paint::sheet(*palette), parser.value(sheetOption));
    }

    if (parser.isSet(traceOption))
        QLoggingCategory::setFilterRules(u"shelf.trace-x.info=true"_s);
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    const std::unique_ptr<XConnection> x = XConnection::open();
    const std::unique_ptr<WindowModel> windows = x ? std::make_unique<WindowModel>(*x) : nullptr;
    const std::unique_ptr<Thumbnails> thumbnails = x ? Thumbnails::open(*x, *windows) : nullptr;
    if (parser.isSet(onlyOption))
        return runWidget(parser.value(onlyOption),
            readPalette(dir.filePath(u"palette.ini"_s)).value_or(Palette()),
            readConfig(dir.filePath(u"config.ini"_s)).value_or(Config()), windows.get());

    QLockFile lock(QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
            .filePath(u"shelf.lock"_s));
    if (!lock.tryLock()) {
        qWarning("shelf is already running");
        return 1;
    }
    if (!QDir().mkpath(dir.path())) {
        qWarning("%s: cannot create", qUtf8Printable(dir.path()));
        return 1;
    }
    return runPanel(dir, windows.get());
}
