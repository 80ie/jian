#pragma once

#include <QImage>
#include <QRect>
#include <QString>

#include <span>

// The _NET_WM_DESKTOP of a window on every desktop.
constexpr quint32 allDesktops = 0xffffffff;

struct Client {
    quint32 window = 0;
    // In root coordinates, which change when a window manager changes page by moving windows.
    QRect geometry;
    quint32 desktop = allDesktops;
    QString title;
    QString instance;
    QString windowClass;
    QImage icon;
    QImage thumbnail;
    bool focused = false;
    bool minimized = false;
    bool maximized = false;
    bool pinned = false;
    bool skipTaskbar = false;
    // Grows with each window seen becoming minimized, so smaller was minimized longer ago.
    quint64 minimizedOrder = 0;

    bool operator==(const Client&) const = default;
};

// Whether a task list shows the client: on the screen and desktop in view, and not asking to be
// left out.
bool isShown(const Client& client, const QRect& screen, quint32 desktop);

// The image in a _NET_WM_ICON value nearest `size`: the smallest at least that wide, or else the
// largest. A null image if the value holds none.
QImage decodeIcon(std::span<const quint32> value, int size);
