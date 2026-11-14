#include "x11/atoms.h"

#include <QtGlobal>

#include <array>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace {

struct Name {
    const char* name;
    xcb_atom_t Atoms::* atom;
};

constexpr Name names[] {
    { "_NET_CLIENT_LIST", &Atoms::clientList },
    { "_NET_CLIENT_LIST_STACKING", &Atoms::clientListStacking },
    { "_NET_ACTIVE_WINDOW", &Atoms::activeWindow },
    { "_NET_CURRENT_DESKTOP", &Atoms::currentDesktop },
    { "_NET_CLOSE_WINDOW", &Atoms::closeWindow },
    { "_NET_WM_NAME", &Atoms::wmName },
    { "_NET_WM_ICON", &Atoms::wmIcon },
    { "_NET_WM_DESKTOP", &Atoms::wmDesktop },
    { "_NET_WM_STATE", &Atoms::wmState },
    { "_NET_WM_WINDOW_TYPE", &Atoms::wmWindowType },
    { "_NET_WM_WINDOW_TYPE_NORMAL", &Atoms::windowTypeNormal },
    { "_NET_WM_PID", &Atoms::wmPid },
    { "_NET_WM_MOVERESIZE", &Atoms::moveResize },
    { "_NET_WM_STATE_HIDDEN", &Atoms::stateHidden },
    { "_NET_WM_STATE_STICKY", &Atoms::stateSticky },
    { "_NET_WM_STATE_SKIP_TASKBAR", &Atoms::stateSkipTaskbar },
    { "_NET_WM_STATE_MAXIMIZED_VERT", &Atoms::stateMaximizedVert },
    { "_NET_WM_STATE_MAXIMIZED_HORZ", &Atoms::stateMaximizedHorz },
    { "WM_CHANGE_STATE", &Atoms::wmChangeState },
    { "UTF8_STRING", &Atoms::utf8String },
};

} // namespace

std::optional<Atoms> internAtoms(xcb_connection_t* connection)
{
    std::array<xcb_intern_atom_cookie_t, std::size(names)> cookies;
    for (std::size_t i = 0; i < cookies.size(); ++i)
        cookies[i] = xcb_intern_atom(connection, false, std::strlen(names[i].name), names[i].name);
    Atoms atoms;
    bool interned = true;
    for (std::size_t i = 0; i < cookies.size(); ++i) {
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookies[i], nullptr);
        interned = interned && reply;
        if (reply)
            atoms.*names[i].atom = reply->atom;
        std::free(reply);
    }
    if (interned)
        return atoms;
    qWarning("cannot intern the window list atoms");
    return std::nullopt;
}
