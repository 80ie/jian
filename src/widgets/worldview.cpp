#include "widgets/worldview.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "widgets/foreignslot.h"
#include "widgets/tasklayout.h"

#include <QFileInfo>
#include <QFontInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QStandardPaths>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace {

// Apart from the ShelfPager the Python shelf starts, so that both can run.
constexpr QLatin1StringView alias = "ShelfWorldView"_L1;
constexpr int desks = 2;
// Far above the numbers configs give their own colorsets.
constexpr int deskColorset = 300;
constexpr int hilightColorset = 301;
constexpr int windowColorset = 302;
constexpr int focusColorset = 303;
// FvwmPager's label bar is the font's height and a pixel either side.
constexpr int labelPadding = 2;
constexpr int settleMs = 50;
// How long a pager that was started has to show its window.
constexpr int deadlineMs = 5000;
constexpr int retryMs = 500;
constexpr int retries = 20;

QRect inside(const QRect& well)
{
    return well.adjusted(wellPadding, wellPadding, -wellPadding, -wellPadding);
}

// The font at the whole pixel size xftName() asks fvwm for, so that both measure alike.
QFont atPixelSize(const QFont& font)
{
    QFont sized = font;
    sized.setPixelSize(QFontInfo(font).pixelSize());
    return sized;
}

} // namespace

bool fvwmReachable()
{
    const QString socket = qEnvironmentVariable("FVWMMFL_SOCKET");
    return !socket.isEmpty() && QFileInfo::exists(socket)
        && !QStandardPaths::findExecutable(u"FvwmCommand"_s).isEmpty();
}

QSize pagerSize(int width, QSize screen, int labelHeight)
{
    const int page = qRound(double(width) * screen.height() / screen.width());
    return QSize(width, desks * (labelHeight + page));
}

QString xftName(const QFont& font)
{
    // fontconfig's names for the weights 100 to 900.
    static constexpr std::array weights { "thin"_L1, "extralight"_L1, "light"_L1, "regular"_L1,
        "medium"_L1, "semibold"_L1, "bold"_L1, "extrabold"_L1, "black"_L1 };
    const QFontInfo info(font);
    const std::size_t weight = std::clamp((info.weight() + 50) / 100, 1, 9) - 1;
    return u"xft:%1:pixelsize=%2:weight=%3%4"_s.arg(info.family(),
        QString::number(info.pixelSize()), weights[weight],
        info.italic() ? u":slant=italic"_s : QString());
}

QStringList pagerColorsets(const Palette& palette)
{
    const auto colorset = [](int number, const QColor& fore, const QColor& back) {
        return u"Colorset %1 fg %2, bg %3, Plain, NoShape"_s.arg(
            QString::number(number), fore.name(), back.name());
    };
    return { colorset(deskColorset, palette.tone(-6), palette.tone(-2)),
        colorset(hilightColorset, palette.ink, palette.tone(0)),
        colorset(windowColorset, palette.tone(-6), palette.tone(-1)),
        colorset(focusColorset, palette.tone(-6), palette.tone(2)) };
}

QStringList pagerLaunch(const Palette& palette, const QString& font, QSize size)
{
    QStringList commands { u"KillModule FvwmPager "_s + alias,
        u"DestroyModuleConfig "_s + alias + u":*"_s };
    for (const QString& option :
        { u"Colorset %1"_s.arg(deskColorset), u"HilightColorset %1"_s.arg(hilightColorset),
            u"WindowColorset %1"_s.arg(windowColorset), u"FocusColorset %1"_s.arg(focusColorset),
            u"Font "_s + font, u"Rows %1"_s.arg(desks), u"MiniIcons"_s, u"SolidSeparators"_s,
            u"HideSmallWindows"_s, u"UseSkipList"_s,
            u"Geometry %1x%2"_s.arg(size.width()).arg(size.height()) })
        commands.append(u"*"_s + alias + u": "_s + option);
    commands.append(pagerColorsets(palette));
    commands.append(u"Module FvwmPager %1 0 %2"_s.arg(alias).arg(desks - 1));
    return commands;
}

WorldView::WorldView(const Palette& palette, WindowModel* windows, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
    , m_screen(QGuiApplication::primaryScreen()->virtualSize())
{
    connect(QGuiApplication::primaryScreen(), &QScreen::virtualGeometryChanged, this,
        [this](const QRect& geometry) { setScreen(geometry.size()); });
    if (!windows)
        return;
    m_slot = new ForeignSlot(*windows, this);
    m_settle.setSingleShot(true);
    m_settle.setInterval(settleMs);
    connect(&m_settle, &QTimer::timeout, this, &WorldView::launch);
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(deadlineMs);
    connect(&m_deadline, &QTimer::timeout, this, [this] { failed("FvwmPager showed no window"); });
    connect(m_slot, &ForeignSlot::embedded, this, [this] {
        m_deadline.stop();
        m_launching = false;
        m_retries = 0;
        m_settle.start();
    });
    // Losing the pager otherwise than to its successor means fvwm restarted or the module died.
    connect(m_slot, &ForeignSlot::lost, this, [this] {
        if (!m_launching)
            relaunchSoon();
    });

    m_fvwm.setProgram(u"FvwmCommand"_s);
    m_fvwm.setArguments({ u"-c"_s });
    m_fvwm.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_fvwm, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QByteArray output = "FvwmCommand failed: " + m_fvwm.readAll().trimmed();
        if (status != QProcess::NormalExit || code != 0) {
            if (m_launching)
                failed(output);
            else
                qWarning("worldview: %s", output.constData());
        }
        flush();
    });
}

WorldView::~WorldView()
{
    if (m_slot)
        QProcess::startDetached(u"FvwmCommand"_s, { u"KillModule FvwmPager "_s + alias });
}

QString WorldView::id() const { return u"worldview"_s; }

int WorldView::heightForWidth(int width) const
{
    const int label = QFontMetrics(atPixelSize(font())).height() + labelPadding;
    return pagerSize(width - 2 * wellPadding, m_screen, label).height() + 2 * wellPadding;
}

void WorldView::setScreen(QSize size)
{
    m_screen = size;
    emit heightChanged();
}

void WorldView::paletteSettled()
{
    if (m_slot)
        send(pagerColorsets(m_palette));
}

void WorldView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    paint::sunken(p, rect(), m_palette, m_palette.tone(-1));
}

void WorldView::resizeEvent(QResizeEvent*)
{
    if (!m_slot)
        return;
    m_slot->setGeometry(inside(rect()));
    m_settle.start();
}

// One launch at a time, or the window of the first could be taken and then killed by the second.
// A size that changes meanwhile is picked up when the window arrives.
void WorldView::launch()
{
    const QSize size = m_slot->size();
    if (m_launching || size == m_launched || size.isEmpty())
        return;
    m_launched = size;
    m_launching = true;
    m_deadline.start();
    m_slot->find(alias);
    send(pagerLaunch(m_palette, xftName(atPixelSize(font())), size));
}

// A launch comes to nothing while fvwm restarts, which is also when it kills the pager.
void WorldView::failed(const QByteArray& why)
{
    m_deadline.stop();
    m_launching = false;
    if (m_retries == retries) {
        qWarning("worldview: %s", why.constData());
        return;
    }
    ++m_retries;
    relaunchSoon();
}

void WorldView::relaunchSoon()
{
    m_launched = QSize();
    QTimer::singleShot(retryMs, this, &WorldView::launch);
}

void WorldView::send(const QStringList& commands)
{
    m_queued.append(commands);
    if (m_fvwm.state() == QProcess::NotRunning)
        flush();
}

// One FvwmCommand at a time, so that fvwm takes the commands in the order they were sent.
void WorldView::flush()
{
    if (m_queued.isEmpty())
        return;
    m_fvwm.start();
    m_fvwm.write((m_queued.join(u'\n') + u'\n').toUtf8());
    m_fvwm.closeWriteChannel();
    m_queued.clear();
}
