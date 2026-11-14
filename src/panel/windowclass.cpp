#include "panel/windowclass.h"

#include <QGuiApplication>

#include <xcb/xcb.h>

// Qt gives every window of a program the same WM_CLASS as it creates it, and the window manager
// reads it at map time, so it is replaced in between. fvwm matches Style names with case, so the
// class "Shelf" keeps the panel's `Style shelf` line off these windows.
void setWindowClass(WId window, const QString& instance)
{
    if (QGuiApplication::platformName() != u"xcb")
        return;
    xcb_connection_t* c
        = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->connection();
    const QByteArray value = instance.toUtf8() + '\0' + "Shelf" + '\0';
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, quint32(window), XCB_ATOM_WM_CLASS,
        XCB_ATOM_STRING, 8, value.size(), value.constData());
    xcb_flush(c);
}
