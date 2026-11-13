#pragma once

#include <QRect>
#include <qwindowdefs.h>

#include <array>

struct xcb_connection_t;

// _NET_WM_STRUT_PARTIAL for a panel at `panel` docked to `side` of the root window `root`: the
// reservation along the left, right, top and bottom, then where each starts and ends.
std::array<quint32, 12> strutPartial(Qt::Edge side, const QRect& panel, const QRect& root);

// The X properties that make a window a panel: the dock window type and the strut. Without an
// X connection they do nothing.
class DockHints {
public:
    explicit DockHints(WId window);
    void setStrut(Qt::Edge side, const QRect& panel, const QRect& root) const;

private:
    xcb_connection_t* m_connection = nullptr;
    quint32 m_window = 0;
    quint32 m_strut = 0;
    quint32 m_strutPartial = 0;
};
