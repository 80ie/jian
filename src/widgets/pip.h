#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QTimer>

#include <xcb/xcb.h>

#include <optional>

class ForeignSlot;
class QWidget;
class WindowModel;
struct Client;

// Whether a window `size` is shaped like a browser's picture-in-picture: a video's proportions
// at a size a floating player takes.
bool pipShaped(QSize size);

// A browser's picture-in-picture window. Dropped on `target`, it is held in `slot`; dragged out
// with the window manager's Win+drag, it is let go and handed back to be moved.
class PictureInPicture : public QObject {
    Q_OBJECT

public:
    PictureInPicture(WindowModel& windows, QWidget& target, ForeignSlot& slot, QObject* parent);

    // The held window's size before the slot took it; empty when none is held.
    QSize size() const { return m_size; }
    // Puts a held window back on the root where the slot is, at its own size. Nothing is
    // signalled, so that the target can call it as it goes.
    void letGo();

signals:
    void sizeChanged();

private:
    void offer(const Client& client);
    bool fromBrowser(quint32 window) const;
    void handle(const xcb_generic_event_t* event);
    void checkDrop();
    void watchDrag();
    void put(QPoint at);

    WindowModel& m_windows;
    QWidget& m_target;
    ForeignSlot& m_slot;
    // The window that would be held if dropped, and where it floats, in root coordinates.
    quint32 m_candidate = 0;
    QRect m_floating;
    QSize m_size;
    // Let go by a drag: when the window manager has it again, it is handed over to be moved.
    quint32 m_moving = 0;
    QTimer m_drop;
    QTimer m_drag;
    // A Win+drag is under way, and where it began if that was on the slot.
    bool m_dragging = false;
    std::optional<QPoint> m_press;
};
