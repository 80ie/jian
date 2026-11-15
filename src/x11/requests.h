#pragma once

#include <QtTypes>

class XConnection;

void activateClient(const XConnection& x, quint32 window);
void minimizeClient(const XConnection& x, quint32 window);
void closeClient(const XConnection& x, quint32 window);
void setClientMaximized(const XConnection& x, quint32 window, bool maximized);
void setClientPinned(const XConnection& x, quint32 window, bool pinned);
