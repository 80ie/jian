#pragma once

#include <QtTypes>

class XConnection;

// What a pager asks of the window manager about a client, as EWMH has it sent to the root.

void activateClient(const XConnection& x, quint32 window);
void minimizeClient(const XConnection& x, quint32 window);
void closeClient(const XConnection& x, quint32 window);
void setClientMaximized(const XConnection& x, quint32 window, bool maximized);
void setClientPinned(const XConnection& x, quint32 window, bool pinned);
