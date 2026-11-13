#pragma once

#include "x11/client.h"

#include <QObject>
#include <QTimer>

#include <array>
#include <set>
#include <utility>
#include <vector>

#include <xcb/xcb.h>

class XConnection;

// The client windows the window manager lists. Each is read once when it appears and then
// changed only by the events that report a change.
class WindowModel : public QObject {
    Q_OBJECT

public:
    explicit WindowModel(XConnection& x);

    // The clients a task list shows, in _NET_CLIENT_LIST order.
    const std::vector<Client>& shown() const { return m_shown; }
    void setThumbnail(quint32 window, const QImage& thumbnail);

    // What a click on its task does: minimizes the window if it is focused and on top, and
    // otherwise brings it forward.
    void toggle(quint32 window) const;
    void activate(quint32 window) const;
    void minimize(quint32 window) const;
    void close(quint32 window) const;
    void setMaximized(quint32 window, bool maximized) const;
    void setPinned(quint32 window, bool pinned) const;

signals:
    // Once for a burst of events that changed shown().
    void changed();

private:
    enum class Field { Title, Icon, State, Desktop };
    struct RootRequest {
        xcb_atom_t atom;
        xcb_get_property_cookie_t cookie;
    };
    struct FieldRequest {
        quint32 window;
        Field field;
        std::array<xcb_get_property_cookie_t, 2> cookies;
    };
    struct PlaceRequest {
        quint32 window;
        xcb_get_geometry_cookie_t size;
        xcb_translate_coordinates_cookie_t origin;
    };

    void handle(const xcb_generic_event_t* event);
    void schedule();
    void update();
    std::vector<RootRequest> requestRoot();
    std::vector<quint32> applyRoot(const std::vector<RootRequest>& requests);
    std::vector<FieldRequest> requestFields(const std::set<std::pair<quint32, Field>>& fields);
    void applyFields(const std::vector<FieldRequest>& requests);
    std::vector<PlaceRequest> requestPlaces(const std::set<quint32>& windows);
    void applyPlaces(const std::vector<PlaceRequest>& requests);
    void add(const std::vector<quint32>& windows);
    std::array<xcb_atom_t, 2> atomsOf(Field field) const;
    void send(quint32 window, xcb_atom_t type, std::array<quint32, 5> data) const;
    Client* find(quint32 window);

    XConnection& m_x;
    QRect m_screen;
    quint32 m_desktop = 0;
    quint32 m_active = 0;
    std::vector<quint32> m_stacking;
    std::vector<Client> m_clients;
    std::vector<Client> m_shown;
    quint64 m_minimizations = 0;
    std::set<xcb_atom_t> m_staleRoot;
    std::set<std::pair<quint32, Field>> m_stale;
    std::set<quint32> m_unplaced;
    QTimer m_update;
};
