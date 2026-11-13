#include "x11/connection.h"

#include <QGuiApplication>

#include <cstdlib>

namespace {

const xcb_screen_t* defaultScreen(xcb_connection_t* connection)
{
    char* host = nullptr;
    int display = 0;
    int number = 0;
    const bool parsed = xcb_parse_display(nullptr, &host, &display, &number);
    std::free(host);
    if (!parsed)
        return nullptr;
    xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(connection));
    for (; screens.rem; --number, xcb_screen_next(&screens))
        if (number == 0)
            return screens.data;
    return nullptr;
}

// Event masks are per client, and Qt has its own on the root, so ours is added to it.
bool selectRootEvents(xcb_connection_t* connection, xcb_window_t root, quint32 events)
{
    xcb_get_window_attributes_reply_t* reply = xcb_get_window_attributes_reply(
        connection, xcb_get_window_attributes(connection, root), nullptr);
    if (!reply)
        return false;
    const quint32 mask = reply->your_event_mask | events;
    std::free(reply);
    xcb_change_window_attributes(connection, root, XCB_CW_EVENT_MASK, &mask);
    xcb_flush(connection);
    return true;
}

} // namespace

std::unique_ptr<XConnection> XConnection::open()
{
    if (QGuiApplication::platformName() != u"xcb")
        return nullptr;
    xcb_connection_t* connection
        = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->connection();
    const xcb_screen_t* screen = defaultScreen(connection);
    if (!screen || !selectRootEvents(connection, screen->root, XCB_EVENT_MASK_PROPERTY_CHANGE)) {
        qWarning("cannot watch the root window");
        return nullptr;
    }
    const std::optional<Atoms> atoms = internAtoms(connection);
    if (!atoms)
        return nullptr;
    return std::make_unique<XConnection>(connection, *screen, *atoms);
}

XConnection::XConnection(
    xcb_connection_t* connection, const xcb_screen_t& screen, const Atoms& atoms)
    : m_connection(connection)
    , m_screen(screen)
    , m_atoms(atoms)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

bool XConnection::nativeEventFilter(const QByteArray& type, void* message, qintptr*)
{
    if (type == "xcb_generic_event_t")
        emit received(static_cast<const xcb_generic_event_t*>(message));
    return false;
}
