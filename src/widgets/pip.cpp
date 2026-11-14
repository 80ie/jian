#include "widgets/pip.h"
#include "widgets/foreignslot.h"
#include "x11/connection.h"
#include "x11/reply.h"
#include "x11/windowmodel.h"

#include <QFile>
#include <QWidget>

#include <algorithm>
#include <array>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

constexpr QSize smallest(160, 90);
constexpr QSize largest(1000, 700);
constexpr double narrowest = 1.2;
constexpr double widest = 2.6;
constexpr std::array<QByteArrayView, 3> browsers { "firefox", "chromium", "chrome" };
constexpr int dropMs = 120;
constexpr int dragMs = 50;
// How far a Win+drag that starts on the slot moves before the window is let go.
constexpr int dragDistance = 4;
constexpr quint16 anyButton = XCB_BUTTON_MASK_1 | XCB_BUTTON_MASK_2 | XCB_BUTTON_MASK_3;

struct Pointer {
    QPoint at;
    quint16 mask;
};

std::optional<Pointer> queryPointer(const XConnection& x)
{
    const auto reply
        = take(x.xcb(), xcb_query_pointer_reply, xcb_query_pointer(x.xcb(), x.screen().root));
    if (!reply)
        return std::nullopt;
    return Pointer { QPoint(reply->root_x, reply->root_y), reply->mask };
}

QRect globalRect(const QWidget& widget)
{
    return QRect(widget.mapToGlobal(QPoint(0, 0)), widget.size());
}

} // namespace

bool pipShaped(QSize size)
{
    if (size.isEmpty())
        return false;
    const double ratio = double(size.width()) / size.height();
    return size.expandedTo(smallest) == size && size.boundedTo(largest) == size
        && ratio >= narrowest && ratio <= widest;
}

PictureInPicture::PictureInPicture(
    WindowModel& windows, QWidget& target, ForeignSlot& slot, QObject* parent)
    : QObject(parent)
    , m_windows(windows)
    , m_target(target)
    , m_slot(slot)
{
    m_drop.setSingleShot(true);
    m_drop.setInterval(dropMs);
    connect(&m_drop, &QTimer::timeout, this, &PictureInPicture::checkDrop);
    m_drag.setInterval(dragMs);
    connect(&m_drag, &QTimer::timeout, this, &PictureInPicture::watchDrag);
    connect(&windows, &WindowModel::appeared, this, &PictureInPicture::offer);
    connect(&windows.connection(), &XConnection::received, this, &PictureInPicture::handle);
    connect(&slot, &ForeignSlot::lost, this, [this] {
        m_size = QSize();
        m_drag.stop();
        emit sizeChanged();
    });
}

void PictureInPicture::letGo()
{
    if (m_size.isValid())
        put(globalRect(m_slot).topLeft());
}

void PictureInPicture::offer(const Client& client)
{
    if (client.window == m_moving) {
        m_moving = 0;
        if (const std::optional<Pointer> pointer = queryPointer(m_windows.connection()))
            m_windows.startMove(client.window, pointer->at);
    }
    if (pipShaped(client.geometry.size()) && fromBrowser(client.window)) {
        m_candidate = client.window;
        m_floating = client.geometry;
        checkDrop();
    }
}

// A round trip, but only for a window already shaped like a video.
bool PictureInPicture::fromBrowser(quint32 window) const
{
    const XConnection& x = m_windows.connection();
    const auto typeCookie
        = xcb_get_property(x.xcb(), false, window, x.atoms().wmWindowType, XCB_ATOM_ATOM, 0, 32);
    const auto pidCookie
        = xcb_get_property(x.xcb(), false, window, x.atoms().wmPid, XCB_ATOM_CARDINAL, 0, 1);
    const PropertyReply type = take(x.xcb(), xcb_get_property_reply, typeCookie);
    const PropertyReply pid = take(x.xcb(), xcb_get_property_reply, pidCookie);
    // A browser's own window is a normal one, which its picture-in-picture never is.
    if (std::ranges::find(values(type), x.atoms().windowTypeNormal) != values(type).end()
        || values(pid).empty())
        return false;
    QFile command(u"/proc/%1/comm"_s.arg(values(pid).front()));
    if (!command.open(QIODevice::ReadOnly))
        return false;
    return std::ranges::find(browsers, command.readAll().trimmed()) != browsers.end();
}

void PictureInPicture::handle(const xcb_generic_event_t* event)
{
    switch (event->response_type & ~0x80) {
    case XCB_CONFIGURE_NOTIFY: {
        const auto* configure = reinterpret_cast<const xcb_configure_notify_event_t*>(event);
        // ICCCM 4.1.5: a window manager that moves a client says where in root coordinates
        // with a synthetic event.
        if (configure->window != m_candidate || !(event->response_type & 0x80))
            break;
        const int border = 2 * configure->border_width;
        m_floating = QRect(
            configure->x, configure->y, configure->width + border, configure->height + border);
        checkDrop();
        break;
    }
    case XCB_DESTROY_NOTIFY:
        if (reinterpret_cast<const xcb_destroy_notify_event_t*>(event)->window == m_candidate) {
            m_candidate = 0;
            m_drop.stop();
        }
        break;
    }
}

// A window carried over the target lands when its button is let go, which X tells only the
// window manager doing the carrying, so a window held over the target is looked at again
// until it lands or moves off.
void PictureInPicture::checkDrop()
{
    if (!m_candidate || m_size.isValid() || !m_target.isVisible()
        || !globalRect(m_target).contains(m_floating.center()))
        return;
    const std::optional<Pointer> pointer = queryPointer(m_windows.connection());
    if (!pointer || pointer->mask & anyButton) {
        m_drop.start();
        return;
    }
    m_size = m_floating.size();
    emit sizeChanged();
    m_slot.embed(std::exchange(m_candidate, 0));
    m_drag.start();
}

// The window manager's Win+drag grabs the pointer on the panel, so a drag that starts on the
// held window reaches neither it nor the panel; only the pointer's state shows it.
void PictureInPicture::watchDrag()
{
    const std::optional<Pointer> pointer = queryPointer(m_windows.connection());
    if (!pointer || !(pointer->mask & XCB_MOD_MASK_4) || !(pointer->mask & XCB_BUTTON_MASK_1)
        || !m_slot.isVisible()) {
        m_dragging = false;
        m_press.reset();
        return;
    }
    if (!std::exchange(m_dragging, true)) {
        if (globalRect(m_slot).contains(pointer->at))
            m_press = pointer->at;
        return;
    }
    if (!m_press || (pointer->at - *m_press).manhattanLength() < dragDistance)
        return;
    m_moving = m_slot.window();
    put(pointer->at - QPoint(m_size.width() / 2, m_size.height() / 2));
    emit sizeChanged();
}

void PictureInPicture::put(QPoint at)
{
    const QSize size = m_size;
    m_size = QSize();
    m_drag.stop();
    m_dragging = false;
    m_press.reset();
    const quint32 window = m_slot.letGo();
    xcb_connection_t* c = m_windows.connection().xcb();
    const quint32 geometry[] { quint32(at.x()), quint32(at.y()), quint32(size.width()),
        quint32(size.height()) };
    discard(c,
        xcb_configure_window_checked(c, window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
                | XCB_CONFIG_WINDOW_HEIGHT,
            geometry));
    discard(c, xcb_map_window_checked(c, window));
    xcb_flush(c);
    m_candidate = window;
    m_floating = QRect(at, size);
}
