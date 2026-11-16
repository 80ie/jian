#include "widgets/tray.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "widgets/foreignslot.h"
#include "widgets/tasklayout.h"
#include "x11/connection.h"
#include "x11/reply.h"
#include "x11/windowmodel.h"

#include <QPainter>

#include <algorithm>
#include <csignal>
#include <vector>

#include <sys/prctl.h>

using namespace Qt::StringLiterals;

namespace {

constexpr int iconSize = 22;
constexpr int slotSize = 26;

QRect inside(const QRect& well)
{
    return well.adjusted(wellPadding, wellPadding, -wellPadding, -wellPadding);
}

} // namespace

QStringList trayArguments(const Palette& palette, int columns)
{
    return { u"--config"_s, u"/dev/null"_s, u"--background"_s, palette.tone(-1).name(),
        u"--geometry"_s, u"%1x1"_s.arg(columns), u"--max-geometry"_s, u"%1x0"_s.arg(columns),
        u"--icon-size"_s, QString::number(iconSize), u"--slot-size"_s, QString::number(slotSize),
        u"--kludges"_s, u"force_icons_size"_s, u"--skip-taskbar"_s };
}

int trayColumns(int width) { return std::max(1, (width - 2 * wellPadding) / slotSize); }

int trayHeight(int width, int icons)
{
    const int columns = trayColumns(width);
    return std::max(1, (icons + columns - 1) / columns) * slotSize + 2 * wellPadding;
}

int countIcons(std::span<const QRect> geometries)
{
    return int(std::ranges::count_if(geometries, [](const QRect& r) {
        return r.x() >= 0 && r.y() >= 0 && r.width() > 1 && r.height() > 1;
    }));
}

Tray::Tray(const Palette& palette, WindowModel* windows, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
    , m_windows(windows)
{
    if (!m_windows)
        return;
    m_slot = new ForeignSlot(*m_windows, this);
    m_measure.setSingleShot(true);
    connect(&m_measure, &QTimer::timeout, this, &Tray::measure);
    connect(m_slot, &ForeignSlot::embedded, &m_measure, qOverload<>(&QTimer::start));
    connect(m_slot, &ForeignSlot::configured, &m_measure, qOverload<>(&QTimer::start));
    connect(m_slot, &ForeignSlot::lost, this, [this] { setIcons(0); });

    m_process.setProgram(u"stalonetray"_s);
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    // A signal or a broken X connection ends the panel without running ~Tray. Not SIGTERM:
    // stalonetray catches it and, while idle, sleeps on.
    m_process.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGKILL); });
    connect(&m_process, &QProcess::finished, this, [this] {
        if (!m_restarting) {
            qWarning("tray: stalonetray exited");
            return;
        }
        m_restarting = false;
        start();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            qWarning("tray: %s", qUtf8Printable(m_process.errorString()));
    });
}

Tray::~Tray()
{
    m_process.disconnect(this);
    m_process.terminate();
    m_process.waitForFinished(500);
}

QString Tray::id() const { return u"tray"_s; }

int Tray::heightForWidth(int width) const { return trayHeight(width, m_icons); }

void Tray::paletteSettled()
{
    if (m_slot && m_palette.tone(-1) != m_background)
        restart();
}

void Tray::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    paint::sunken(p, rect(), m_palette, m_palette.tone(-1));
}

void Tray::resizeEvent(QResizeEvent*)
{
    if (!m_slot)
        return;
    m_slot->setGeometry(inside(rect()));
    if (trayColumns(width()) != m_columns)
        restart();
}

// The old tray has to let go of the selection before the new one starts.
void Tray::restart()
{
    m_columns = trayColumns(width());
    if (m_process.state() == QProcess::NotRunning) {
        start();
        return;
    }
    m_restarting = true;
    m_process.terminate();
}

void Tray::start()
{
    m_slot->find(u"stalonetray"_s);
    m_background = m_palette.tone(-1);
    m_process.setArguments(trayArguments(m_palette, m_columns));
    m_process.start();
}

// stalonetray resizes its window when it gains or loses a row, which is when this runs.
void Tray::measure()
{
    if (!m_slot->window())
        return;
    xcb_connection_t* c = m_windows->connection().xcb();
    const auto tree = take(c, xcb_query_tree_reply, xcb_query_tree(c, m_slot->window()));
    if (!tree)
        return;
    const std::span<const xcb_window_t> children(
        xcb_query_tree_children(tree.get()), xcb_query_tree_children_length(tree.get()));
    std::vector<xcb_get_geometry_cookie_t> cookies;
    for (xcb_window_t child : children)
        cookies.push_back(xcb_get_geometry(c, child));
    std::vector<QRect> geometries;
    for (xcb_get_geometry_cookie_t cookie : cookies)
        if (const auto geometry = take(c, xcb_get_geometry_reply, cookie))
            geometries.emplace_back(geometry->x, geometry->y, geometry->width, geometry->height);
    setIcons(countIcons(geometries));
}

void Tray::setIcons(int icons)
{
    const int before = heightForWidth(width());
    m_icons = icons;
    if (heightForWidth(width()) != before)
        emit heightChanged();
}
