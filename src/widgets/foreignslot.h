#pragma once

#include <QWidget>

#include <xcb/xcb.h>

class QWindow;
class WindowModel;
struct Client;

// A window that belongs to another program, reparented into this widget and kept at its size and
// place until it dies. The pager and the tray hold theirs here; so can windows the panel did not
// launch.
class ForeignSlot : public QWidget {
    Q_OBJECT

public:
    ForeignSlot(WindowModel& windows, QWidget* parent);

    // Embeds the next client to appear whose WM_CLASS instance is `instance`.
    void find(const QString& instance);
    // Embeds `window` in place of the one held.
    void embed(quint32 window);
    // The window held, or 0.
    quint32 window() const { return m_window; }
    // Hands the window held back to the root window, where Qt leaves it unmapped, and returns
    // it.
    quint32 letGo();

signals:
    void embedded();
    // The window held changed its size or place.
    void configured();
    // The window held was destroyed or taken out of the slot.
    void lost();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void offer(const Client& client);
    void handle(const xcb_generic_event_t* event);
    void release();

    WindowModel& m_windows;
    QString m_wanted;
    quint32 m_window = 0;
    QWindow* m_foreign = nullptr;
    QWidget* m_container = nullptr;
};
