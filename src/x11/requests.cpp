#include "x11/requests.h"
#include "x11/connection.h"

#include <algorithm>
#include <array>

namespace {

// ICCCM 4.1.4, and the EWMH source indication of a pager.
constexpr quint32 iconicState = 3;
constexpr quint32 fromPager = 2;
constexpr quint32 removeState = 0;
constexpr quint32 addState = 1;

void send(const XConnection& x, quint32 window, xcb_atom_t type, std::array<quint32, 5> data)
{
    xcb_client_message_event_t message { };
    message.response_type = XCB_CLIENT_MESSAGE;
    message.format = 32;
    message.window = window;
    message.type = type;
    std::ranges::copy(data, message.data.data32);
    xcb_send_event(x.xcb(), false, x.screen().root,
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
        reinterpret_cast<const char*>(&message));
    xcb_flush(x.xcb());
}

} // namespace

void activateClient(const XConnection& x, quint32 window)
{
    send(x, window, x.atoms().activeWindow, { fromPager, XCB_CURRENT_TIME, 0, 0, 0 });
}

void minimizeClient(const XConnection& x, quint32 window)
{
    send(x, window, x.atoms().wmChangeState, { iconicState, 0, 0, 0, 0 });
}

void closeClient(const XConnection& x, quint32 window)
{
    send(x, window, x.atoms().closeWindow, { XCB_CURRENT_TIME, fromPager, 0, 0, 0 });
}

void setClientMaximized(const XConnection& x, quint32 window, bool maximized)
{
    const Atoms& atoms = x.atoms();
    send(x, window, atoms.wmState,
        { maximized ? addState : removeState, atoms.stateMaximizedVert, atoms.stateMaximizedHorz,
            fromPager, 0 });
}

void setClientPinned(const XConnection& x, quint32 window, bool pinned)
{
    const Atoms& atoms = x.atoms();
    send(x, window, atoms.wmState,
        { pinned ? addState : removeState, atoms.stateSticky, 0, fromPager, 0 });
}
