#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <QTimer>

#include <deque>
#include <memory>
#include <vector>

#include <xcb/xcb.h>

class WindowModel;
class XConnection;

// The window in a GetImage reply, scaled to thumbnail size and apart from the reply. Null for a
// depth other than 24 or 32.
QImage thumbnailFrom(xcb_get_image_reply_t& reply, int width, int height);

class Thumbnails : public QObject {
    Q_OBJECT

public:
    static std::unique_ptr<Thumbnails> open(XConnection& x, WindowModel& model);
    Thumbnails(XConnection& x, WindowModel& model);

private:
    struct Capture {
        quint32 window = 0;
        // Names the window's storage, or is 0 to name it when the capture is taken.
        quint32 pixmap = 0;
    };
    void update();
    void follow(quint32 window, QSize size);
    void queue(const Capture& capture);
    void captureNext();
    quint32 name(quint32 window) const;

    XConnection& m_x;
    WindowModel& m_model;
    std::vector<quint32> m_shown;
    // The focused window's storage, held so that it can still be read once the window is
    // minimized, which comes before the focus moves.
    Capture m_focused;
    QSize m_focusedSize;
    std::deque<Capture> m_queue;
    QTimer m_next;
};
