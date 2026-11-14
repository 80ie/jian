#include "widgets/foreignslot.h"
#include "x11/connection.h"
#include "x11/reply.h"
#include "x11/windowmodel.h"

#include <QWindow>

#include <utility>

ForeignSlot::ForeignSlot(WindowModel& windows, QWidget* parent)
    : QWidget(parent)
    , m_windows(windows)
{
    connect(&windows, &WindowModel::appeared, this, &ForeignSlot::offer);
    connect(&windows.connection(), &XConnection::received, this, &ForeignSlot::handle);
}

void ForeignSlot::find(const QString& instance) { m_wanted = instance; }

void ForeignSlot::embed(quint32 window)
{
    release();
    xcb_connection_t* c = m_windows.connection().xcb();
    const quint32 events = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    discard(c, xcb_change_window_attributes_checked(c, window, XCB_CW_EVENT_MASK, &events));
    m_window = window;
    m_foreign = QWindow::fromWinId(window);
    m_container = QWidget::createWindowContainer(m_foreign, this);
    m_container->setGeometry(rect());
    m_container->show();
    emit embedded();
}

// Deleted at once rather than later, so that Qt has moved the window before the caller acts
// on it.
quint32 ForeignSlot::letGo()
{
    delete m_container;
    m_container = nullptr;
    m_foreign = nullptr;
    return std::exchange(m_window, 0);
}

void ForeignSlot::resizeEvent(QResizeEvent*)
{
    if (m_container)
        m_container->setGeometry(rect());
}

void ForeignSlot::offer(const Client& client)
{
    if (!m_wanted.isEmpty() && client.instance == m_wanted) {
        m_wanted.clear();
        embed(client.window);
    }
}

void ForeignSlot::handle(const xcb_generic_event_t* event)
{
    if (!m_window)
        return;
    switch (event->response_type & ~0x80) {
    case XCB_DESTROY_NOTIFY: {
        const auto* destroy = reinterpret_cast<const xcb_destroy_notify_event_t*>(event);
        if (destroy->window == m_window) {
            release();
            emit lost();
        }
        break;
    }
    case XCB_REPARENT_NOTIFY: {
        const auto* reparent = reinterpret_cast<const xcb_reparent_notify_event_t*>(event);
        if (reparent->window == m_window && !m_windows.connection().owns(reparent->parent)) {
            release();
            emit lost();
        }
        break;
    }
    case XCB_CONFIGURE_NOTIFY: {
        const auto* configure = reinterpret_cast<const xcb_configure_notify_event_t*>(event);
        if (configure->window != m_window)
            break;
        // stalonetray places itself in root coordinates when it resizes, as if it were still a
        // top-level window, which takes it out of the slot.
        if (const QPoint at = m_foreign->position();
            configure->x != at.x() || configure->y != at.y()) {
            xcb_connection_t* c = m_windows.connection().xcb();
            const quint32 position[] { quint32(at.x()), quint32(at.y()) };
            discard(c,
                xcb_configure_window_checked(
                    c, m_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, position));
            xcb_flush(c);
        }
        emit configured();
        break;
    }
    }
}

void ForeignSlot::release()
{
    if (!m_container)
        return;
    m_container->deleteLater();
    m_container = nullptr;
    m_foreign = nullptr;
    m_window = 0;
}
