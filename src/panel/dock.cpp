#include "panel/dock.h"

#include <QGuiApplication>

#include <xcb/xcb.h>

#include <cstdlib>
#include <cstring>
#include <iterator>

std::array<quint32, 12> strutPartial(Qt::Edge side, const QRect& panel, const QRect& root)
{
    std::array<quint32, 12> strut { };
    if (side == Qt::LeftEdge) {
        strut[0] = panel.right() + 1 - root.left();
        strut[4] = panel.top();
        strut[5] = panel.bottom();
    } else {
        strut[1] = root.right() + 1 - panel.left();
        strut[6] = panel.top();
        strut[7] = panel.bottom();
    }
    return strut;
}

DockHints::DockHints(WId window)
{
    // The offscreen platform answers to QX11Application as well, but its windows are not X
    // windows and its connection, if it has one, is to $DISPLAY.
    if (QGuiApplication::platformName() != u"xcb")
        return;
    xcb_connection_t* connection
        = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->connection();
    const char* const names[] { "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DOCK", "_NET_WM_STRUT",
        "_NET_WM_STRUT_PARTIAL" };
    xcb_intern_atom_cookie_t cookies[std::size(names)];
    for (std::size_t i = 0; i < std::size(names); ++i)
        cookies[i] = xcb_intern_atom(connection, false, std::strlen(names[i]), names[i]);
    xcb_atom_t atoms[std::size(names)] { };
    bool interned = true;
    for (std::size_t i = 0; i < std::size(names); ++i) {
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookies[i], nullptr);
        interned = interned && reply;
        if (reply)
            atoms[i] = reply->atom;
        std::free(reply);
    }
    if (!interned) {
        qWarning("cannot intern the atoms to dock the panel");
        return;
    }

    m_connection = connection;
    m_window = window;
    m_strut = atoms[2];
    m_strutPartial = atoms[3];
    xcb_change_property(
        connection, XCB_PROP_MODE_REPLACE, m_window, atoms[0], XCB_ATOM_ATOM, 32, 1, &atoms[1]);
    xcb_flush(connection);
}

void DockHints::setStrut(Qt::Edge side, const QRect& panel, const QRect& root) const
{
    if (!m_connection)
        return;
    const std::array<quint32, 12> strut = strutPartial(side, panel, root);
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_window, m_strutPartial,
        XCB_ATOM_CARDINAL, 32, strut.size(), strut.data());
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_window, m_strut, XCB_ATOM_CARDINAL,
        32, 4, strut.data());
    xcb_flush(m_connection);
}
