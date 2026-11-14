#pragma once

#include <QObject>
#include <QSize>
#include <QTimer>

#include <deque>
#include <map>
#include <memory>
#include <vector>

#include <xcb/xcb.h>

class WindowModel;
class XConnection;

// Captures of the listed windows through the Composite extension, scaled down and handed to the
// window model: of a window when it loses the focus, and of a pinned one as it changes, at most
// about once a second.
class Thumbnails : public QObject {
    Q_OBJECT

public:
    // Nothing without the Composite and Damage extensions.
    static std::unique_ptr<Thumbnails> open(XConnection& x, WindowModel& model);
    Thumbnails(XConnection& x, WindowModel& model, quint8 damageNotify);

private:
    struct Capture {
        quint32 window = 0;
        // Names the window's storage, or is 0 to name it when the capture is taken.
        quint32 pixmap = 0;
    };
    struct Watch {
        quint32 damage;
        bool damaged;
    };

    void update();
    void follow(quint32 window, QSize size);
    void watch(const std::vector<quint32>& windows);
    void handle(const xcb_generic_event_t* event);
    void captureDamaged();
    void queue(const Capture& capture);
    void captureNext();
    quint32 name(quint32 window) const;

    XConnection& m_x;
    WindowModel& m_model;
    quint8 m_damageNotify;
    std::vector<quint32> m_shown;
    // The focused window's storage, held so that it can still be read once the window is
    // minimized, which comes before the focus moves.
    Capture m_focused;
    QSize m_focusedSize;
    std::map<quint32, Watch> m_watched;
    std::deque<Capture> m_queue;
    QTimer m_next;
    QTimer m_pace;
};
